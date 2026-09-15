// Wind clock firmware for the ESP32.
//
// The needle reads 0 km/h at 9 o'clock, half scale straight up at 12, and
// 100 km/h at 3 o'clock.
//
// One wake cycle:
//   1. read the battery, decide whether the servo may run at all;
//   2. join WiFi and fetch the hourly wind forecast;
//   3. fold the number into the needle filter;
//   4. if the needle needs to move, power the servo, drive it, cut power;
//   5. deep sleep until the next update.
//
// Everything worth testing lives in src/core and is exercised by the
// native test suite; this file is the glue that talks to the hardware.

#include <Arduino.h>
#include <ESP32Servo.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <driver/gpio.h>
#include <esp_sleep.h>

#include "config.h"
#include "core/forecast_url.h"
#include "core/needle_filter.h"
#include "core/power_policy.h"
#include "core/servo_map.h"
#include "core/wind_dial.h"
#include "core/wind_forecast.h"

using namespace windclock;

// Survives deep sleep; cleared by a power cycle or the reset button.
RTC_DATA_ATTR NeedleState gNeedle;
RTC_DATA_ATTR PowerMode gPowerMode = PowerMode::Normal;
RTC_DATA_ATTR uint32_t gSecondsSinceGoodUpdate = 0;
RTC_DATA_ATTR int gConsecutiveFailures = 0;
RTC_DATA_ATTR uint32_t gWakeCount = 0;

namespace {

constexpr size_t kResponseLimitBytes = 8192;

void servoPower(bool on) {
  const bool level = config::kServoPowerActiveHigh ? on : !on;
  digitalWrite(config::kServoPowerPin, level ? HIGH : LOW);
}

float readBatteryVolts() {
  if (!config::kBatterySenseFitted) return config::kAssumedBatteryVolts;

  digitalWrite(config::kBatterySenseEnablePin, HIGH);
  delay(5); // let the divider settle

  // Average a handful of samples; the ADC on the ESP32 is noisy.
  uint32_t total = 0;
  constexpr int kSamples = 8;
  for (int i = 0; i < kSamples; ++i) {
    total += analogReadMilliVolts(config::kBatterySensePin);
  }
  digitalWrite(config::kBatterySenseEnablePin, LOW);

  const float pinVolts = (total / static_cast<float>(kSamples)) / 1000.0f;
  return adcToBatteryVolts(pinVolts, config::kBatteryDividerRatio);
}

bool connectWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WINDCLOCK_WIFI_SSID, WINDCLOCK_WIFI_PASSWORD);

  const uint32_t deadline = millis() + config::kWifiTimeoutMs;
  while (WiFi.status() != WL_CONNECTED && millis() < deadline) {
    delay(100);
  }
  return WiFi.status() == WL_CONNECTED;
}

void shutdownWifi() {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
}

// Fetches the forecast into `body`. Returns true on a 200 with a payload.
bool fetchForecast(String &body) {
  char url[256];
  if (buildOpenMeteoUrl(url, sizeof(url), config::kLatitude, config::kLongitude,
                        config::kForecast) < 0) {
    Serial.println(F("[wind] could not build request URL"));
    return false;
  }

  WiFiClientSecure client;
  // Open-Meteo is a public, read-only endpoint and the payload is a wind
  // speed, so the root-CA bundle is not worth the flash or the expiry
  // maintenance here. Pin a certificate if that trade-off changes.
  client.setInsecure();

  HTTPClient http;
  http.setTimeout(config::kHttpTimeoutMs);
  if (!http.begin(client, url)) {
    Serial.println(F("[wind] http.begin failed"));
    return false;
  }

  const int status = http.GET();
  if (status != HTTP_CODE_OK) {
    Serial.printf("[wind] HTTP %d\n", status);
    http.end();
    return false;
  }

  const int length = http.getSize();
  if (length > static_cast<int>(kResponseLimitBytes)) {
    Serial.printf("[wind] response too large (%d bytes)\n", length);
    http.end();
    return false;
  }

  body = http.getString();
  http.end();
  return body.length() > 0;
}

// Drives the needle to `windKph` and cuts the servo's power again.
void moveNeedle(float windKph) {
  const uint16_t pulseUs =
      windKphToPulseUs(config::kServo, config::kDial, windKph);
  Serial.printf("[wind] needle -> %.1f km/h (%.0f deg, %u us)\n", windKph,
                windToDialDeg(config::kDial, windKph),
                static_cast<unsigned>(pulseUs));

  servoPower(true);
  delay(config::kServoPowerRiseMs);

  // ESP32Servo needs an LEDC timer before the first attach().
  ESP32PWM::allocateTimer(0);
  Servo servo;
  servo.setPeriodHertz(config::kServoPwmHz);
  servo.attach(config::kServoSignalPin, config::kServo.minPulseUs,
               config::kServo.maxPulseUs);
  servo.writeMicroseconds(pulseUs);
  delay(config::kServoSettleMs);
  servo.detach();

  servoPower(false);
}

#ifdef WINDCLOCK_BENCH_MODE
// Walks the needle over the whole scale and keeps doing it. No WiFi, no
// deep sleep -- for fitting the needle and checking it reaches both end
// stops. Never returns. Built by `pio run -e bench -t upload`.
void runBenchSweep() {
  static const float kStops[] = {0.0f, 25.0f, 50.0f, 75.0f, 100.0f, 50.0f};
  Serial.println(F("[bench] sweeping the dial -- no WiFi, no sleep."));
  Serial.println(F("[bench] servo power is cut between stops, so the horn "
                   "can be repositioned by hand."));
  for (;;) {
    for (const float kph : kStops) {
      moveNeedle(kph);
      delay(2000);
    }
  }
}
#endif

void sleepFor(uint32_t seconds) {
  if (seconds == 0) seconds = 1;
  Serial.printf("[wind] sleeping %u s\n", static_cast<unsigned>(seconds));
  Serial.flush();

  gSecondsSinceGoodUpdate += seconds;
  shutdownWifi();

  // Hold the servo rail off and the sense divider disconnected through
  // sleep. Both pins are RTC-capable, which is what makes the hold stick.
  servoPower(false);
  digitalWrite(config::kBatterySenseEnablePin, LOW);
  gpio_hold_en(static_cast<gpio_num_t>(config::kServoPowerPin));
  gpio_hold_en(static_cast<gpio_num_t>(config::kBatterySenseEnablePin));
  gpio_deep_sleep_hold_en();

  esp_sleep_enable_timer_wakeup(static_cast<uint64_t>(seconds) * 1000000ULL);
  esp_deep_sleep_start();
}

} // namespace

void setup() {
  Serial.begin(115200);
  delay(50);
  ++gWakeCount;
  Serial.printf("\n[wind] wake #%u\n", static_cast<unsigned>(gWakeCount));

  gpio_deep_sleep_hold_dis();
  gpio_hold_dis(static_cast<gpio_num_t>(config::kServoPowerPin));
  gpio_hold_dis(static_cast<gpio_num_t>(config::kBatterySenseEnablePin));
  pinMode(config::kServoPowerPin, OUTPUT);
  pinMode(config::kBatterySenseEnablePin, OUTPUT);
  servoPower(false);
  digitalWrite(config::kBatterySenseEnablePin, LOW);

  // A dial the servo cannot physically reach is a build error, not a
  // runtime condition -- say so loudly instead of showing a wrong number.
  if (!dialFitsCalibration(config::kServo, config::kDial)) {
    Serial.println(F("[wind] FATAL: servo travel cannot cover the dial; "
                     "check kServo.travelDeg / trimDeg in config.h"));
    sleepFor(config::kPower.criticalSleepSeconds);
    return;
  }

#ifdef WINDCLOCK_BENCH_MODE
  runBenchSweep(); // never returns
#endif

  const float volts = readBatteryVolts();
  gPowerMode = classifyBattery(config::kPower, volts, gPowerMode);
  Serial.printf("[wind] battery %.2f V, mode %d\n", volts,
                static_cast<int>(gPowerMode));

  if (!servoAllowed(gPowerMode)) {
    // Park at zero once, then stop touching the servo until the battery
    // recovers. gNeedle is reset so the park counts as a fresh start.
    if (gNeedle.initialised && gNeedle.displayedKph != 0.0f) {
      Serial.println(F("[wind] battery critical: parking needle"));
      moveNeedle(0.0f);
      gNeedle = NeedleState{};
    }
    sleepFor(sleepSecondsFor(config::kPower, gPowerMode));
    return;
  }

  bool updated = false;
  for (int attempt = 0; attempt < config::kMaxAttemptsPerWake && !updated;
       ++attempt) {
    if (!connectWifi()) {
      Serial.println(F("[wind] WiFi failed"));
      continue;
    }

    String body;
    const bool fetched = fetchForecast(body);
    shutdownWifi();
    if (!fetched) continue;

    const ForecastResult forecast =
        parseOpenMeteo(body.c_str(), config::kForecast);
    if (!forecast.valid) {
      Serial.println(F("[wind] forecast unusable"));
      continue;
    }
    if (forecast.fellBackToSustained) {
      Serial.println(F("[wind] no gust data; using sustained wind"));
    }

    Serial.printf("[wind] forecast %.1f km/h (%d samples, index %d)\n",
                  forecast.windKph, forecast.sampleCount, forecast.chosenIndex);

    if (updateNeedle(gNeedle, config::kNeedle, forecast.windKph)) {
      moveNeedle(gNeedle.displayedKph);
    } else {
      Serial.println(F("[wind] change within deadband; needle stays put"));
    }
    updated = true;
  }

  if (updated) {
    gConsecutiveFailures = 0;
    gSecondsSinceGoodUpdate = 0;
    sleepFor(sleepSecondsFor(config::kPower, gPowerMode));
    return;
  }

  // No usable reading this cycle.
  ++gConsecutiveFailures;
  if (readingIsStale(config::kPower, gSecondsSinceGoodUpdate) &&
      gNeedle.initialised) {
    // Better a needle at zero than one confidently showing yesterday's gale.
    Serial.println(F("[wind] reading stale: parking needle"));
    moveNeedle(0.0f);
    gNeedle = NeedleState{};
  }

  const uint32_t backoff =
      retryBackoffSeconds(gConsecutiveFailures - 1,
                          config::kRetryBackoffBaseSeconds,
                          config::kRetryBackoffMaxSeconds);
  const uint32_t scheduled = sleepSecondsFor(config::kPower, gPowerMode);
  sleepFor(backoff < scheduled ? backoff : scheduled);
}

void loop() {
  // Never reached: setup() always ends in deep sleep.
}
