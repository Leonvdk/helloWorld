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
// 0 km/h at 12 o'clock, 100 km/h at 11 o'clock, clockwise.
constexpr DialSpec kDial = {
    /*minWindKph=*/0.0f,
    /*maxWindKph=*/100.0f,
    /*minDialDeg=*/0.0f,
    /*maxDialDeg=*/330.0f,
};

// ------------------------------------------------------------------- servo
// Default build: a 180-degree servo geared 2:1, so 165 degrees of shaft
// swings the needle the full 330 degrees. For a 360-degree positional
// servo driving the needle directly, use travelDeg 360 / gearRatio 1.
constexpr ServoCalibration kServo = {
    /*minPulseUs=*/500,
    /*maxPulseUs=*/2500,
    /*travelDeg=*/180.0f,
    /*gearRatio=*/2.0f,
    /*trimDeg=*/7.5f,
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
