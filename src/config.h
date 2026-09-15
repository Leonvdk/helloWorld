// Every tunable for the wind clock lives here.
#ifndef WINDCLOCK_CONFIG_H
#define WINDCLOCK_CONFIG_H

#include "core/needle_filter.h"
#include "core/power_policy.h"
#include "core/servo_map.h"
#include "core/wind_dial.h"
#include "core/wind_forecast.h"

namespace windclock {
namespace config {

// ---------------------------------------------------------------- location
// Where the wind is being forecast for. Defaults to Aljezur, Portugal.
constexpr float kLatitude = 37.3167f;
constexpr float kLongitude = -8.8000f;

// ------------------------------------------------------------------- wifi
// Override at build time, e.g.
//   -DWINDCLOCK_WIFI_SSID='"my-network"' -DWINDCLOCK_WIFI_PASSWORD='"hunter2"'
#ifndef WINDCLOCK_WIFI_SSID
#define WINDCLOCK_WIFI_SSID "set-your-ssid"
#endif
#ifndef WINDCLOCK_WIFI_PASSWORD
#define WINDCLOCK_WIFI_PASSWORD "set-your-password"
#endif

constexpr uint32_t kWifiTimeoutMs = 20000;
constexpr uint32_t kHttpTimeoutMs = 10000;
constexpr int kMaxAttemptsPerWake = 3;

// -------------------------------------------------------------------- pins
constexpr int kServoSignalPin = 18;
// Drives a P-MOSFET / load switch feeding the servo's supply rail. The
// servo draws milliamps even when idle, which matters over a month of
// deep sleep, so its power is cut between updates. Must be an RTC-capable
// GPIO, or the level will not hold through deep sleep.
constexpr int kServoPowerPin = 25;
constexpr bool kServoPowerActiveHigh = true;
// Battery sense divider, gated by kBatterySenseEnablePin so the divider
// itself is not a permanent load. The sense pin must be on ADC1 -- ADC2
// is unusable while WiFi is running. The enable pin must be RTC-capable.
constexpr int kBatterySensePin = 34;
constexpr int kBatterySenseEnablePin = 26;
constexpr float kBatteryDividerRatio = 2.0f;

// Milliseconds to hold the servo powered after commanding a position.
// Long enough for the needle to settle across the full sweep.
constexpr uint32_t kServoSettleMs = 1200;
constexpr uint32_t kServoPowerRiseMs = 50;

// -------------------------------------------------------------------- dial
// 0 km/h at 9 o'clock, 50 straight up at 12, 100 km/h at 3 o'clock.
// A 180 degree sweep clockwise across the top of the face.
constexpr DialSpec kDial = {
    /*minWindKph=*/0.0f,
    /*maxWindKph=*/100.0f,
    /*minDialDeg=*/-90.0f, // 9 o'clock
    /*maxDialDeg=*/90.0f,  // 3 o'clock
};

// ------------------------------------------------------------------- servo
// A standard 180 degree servo on the needle shaft, no gearing. The sweep
// uses the servo's full travel, so there is no trim margin: if your servo
// falls a little short of its nominal 180, either widen the pulse range or
// drop travelDeg to what it really turns and accept a slightly compressed
// scale. A 270 degree servo with travelDeg 270 and trimDeg 45 gives the
// same dial with room to spare at both ends.
constexpr ServoCalibration kServo = {
    /*minPulseUs=*/500,
    /*maxPulseUs=*/2500,
    /*travelDeg=*/180.0f,
    /*gearRatio=*/1.0f,
    /*trimDeg=*/0.0f,
    /*reversed=*/false,
};

constexpr int kServoPwmHz = 50;

// ---------------------------------------------------------------- forecast
constexpr ForecastQuery kForecast = {
    /*mode=*/ForecastMode::MaxOverHorizon,
    /*horizonHours=*/12,
    /*useGusts=*/false,
    /*unit=*/WindUnit::Kph,
};

// ------------------------------------------------------------------ needle
constexpr NeedlePolicy kNeedle = {
    /*deadbandKph=*/1.5f,
    /*maxStepKph=*/25.0f,
    /*smoothing=*/0.6f,
};

// ------------------------------------------------------------------- power
constexpr PowerPolicy kPower = {
    /*normalSleepSeconds=*/30 * 60,
    /*lowBatterySleepSeconds=*/3 * 60 * 60,
    /*criticalSleepSeconds=*/12 * 60 * 60,
    /*lowBatteryVolts=*/3.50f,
    /*criticalBatteryVolts=*/3.20f,
    /*recoverVolts=*/3.65f,
    /*staleAfterSeconds=*/6 * 60 * 60,
};

constexpr uint32_t kRetryBackoffBaseSeconds = 5 * 60;
constexpr uint32_t kRetryBackoffMaxSeconds = 60 * 60;

} // namespace config
} // namespace windclock

#endif // WINDCLOCK_CONFIG_H
