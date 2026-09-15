// End to end: an HTTP response body in, a servo pulse width out, using the
// real configuration the firmware ships with.
#include <cstdio>
#include <string>

#include "config.h"
#include "core/forecast_url.h"
#include "core/needle_filter.h"
#include "core/power_policy.h"
#include "core/servo_map.h"
#include "core/wind_dial.h"
#include "core/wind_forecast.h"
#include "test_framework.h"

using namespace windclock;

namespace {

// A dead calm across every hour.
const char *kCalmResponse =
    R"({"hourly_units":{"wind_speed_10m":"km/h"},)"
    R"("hourly":{"wind_speed_10m":[0.0,0.0,0.0]}})";

// Builds a response whose strongest hour is `peakKph`. The filler hours
// are low, so `peakKph` is what MaxOverHorizon picks -- keep it above 2.
std::string responseWithPeak(float peakKph) {
  char buffer[256];
  std::snprintf(buffer, sizeof(buffer),
                R"({"hourly_units":{"wind_speed_10m":"km/h"},)"
                R"("hourly":{"wind_speed_10m":[1.0,%.2f,2.0]}})",
                static_cast<double>(peakKph));
  return buffer;
}

// One full wake cycle against a canned response, minus the hardware.
struct CycleResult {
  bool moved = false;
  uint16_t pulseUs = 0;
  float displayedKph = 0.0f;
  float dialDeg = 0.0f;
};

CycleResult runCycle(NeedleState &needle, const std::string &body) {
  CycleResult out;
  const ForecastResult forecast =
      parseOpenMeteo(body.c_str(), config::kForecast);
  if (!forecast.valid) return out;

  out.moved = updateNeedle(needle, config::kNeedle, forecast.windKph);
  out.displayedKph = needle.displayedKph;
  out.dialDeg = windToDialDeg(config::kDial, needle.displayedKph);
  out.pulseUs =
      windKphToPulseUs(config::kServo, config::kDial, needle.displayedKph);
  return out;
}

constexpr float kTol = 0.01f;

} // namespace

TEST(Integration, ShippedConfigurationIsCoherent) {
  // If any of these fail the firmware will refuse to run, so catch it here.
  CHECK_TRUE(dialSpecIsValid(config::kDial));
  CHECK_TRUE(calibrationIsValid(config::kServo));
  CHECK_TRUE(needlePolicyIsValid(config::kNeedle));
  CHECK_TRUE(powerPolicyIsValid(config::kPower));
  CHECK_TRUE(dialFitsCalibration(config::kServo, config::kDial));
}

TEST(Integration, ShippedDialRunsFromNineThroughTwelveToThree) {
  CHECK_NEAR(dialDegToClockHour(windToDialDeg(config::kDial, 0.0f)), 9.0f, kTol);
  // 12 o'clock reads back as 0 hours.
  CHECK_NEAR(dialDegToClockHour(windToDialDeg(config::kDial, 50.0f)), 0.0f,
             kTol);
  CHECK_NEAR(dialDegToClockHour(windToDialDeg(config::kDial, 100.0f)), 3.0f,
             kTol);
  CHECK_NEAR(dialSweepDeg(config::kDial), 180.0f, kTol);
}

TEST(Integration, CalmResponseParksTheNeedleAtNineOClock) {
  NeedleState needle;
  const CycleResult cycle = runCycle(needle, kCalmResponse);
  CHECK_TRUE(cycle.moved);
  CHECK_NEAR(cycle.displayedKph, 0.0f, kTol);
  CHECK_NEAR(cycle.dialDeg, -90.0f, kTol);
  CHECK_NEAR(dialDegToClockHour(cycle.dialDeg), 9.0f, kTol);
  CHECK_EQ(cycle.pulseUs, config::kServo.minPulseUs);
}

TEST(Integration, StormResponseDrivesTheNeedleToThreeOClock) {
  NeedleState needle;
  // Cold start: the first reading goes straight to the target, no slew cap.
  const CycleResult cycle = runCycle(needle, responseWithPeak(100.0f));
  CHECK_TRUE(cycle.moved);
  CHECK_NEAR(cycle.displayedKph, 100.0f, kTol);
  CHECK_NEAR(dialDegToClockHour(cycle.dialDeg), 3.0f, kTol);
  CHECK_EQ(cycle.pulseUs, config::kServo.maxPulseUs);
}

TEST(Integration, WindBeyondTheScaleStillReadsThreeOClock) {
  NeedleState needle;
  const CycleResult cycle = runCycle(needle, responseWithPeak(180.0f));
  CHECK_TRUE(cycle.moved);
  CHECK_NEAR(dialDegToClockHour(cycle.dialDeg), 3.0f, kTol);
  CHECK_EQ(cycle.pulseUs, config::kServo.maxPulseUs);
}

TEST(Integration, HalfScaleStandsTheNeedleStraightUp) {
  NeedleState needle;
  const CycleResult cycle = runCycle(needle, responseWithPeak(50.0f));
  CHECK_TRUE(cycle.moved);
  CHECK_NEAR(cycle.dialDeg, 0.0f, kTol);
  CHECK_EQ(cycle.pulseUs, 1500);
}

TEST(Integration, TypicalAfternoonBreezeLandsLeftOfTwelve) {
  NeedleState needle;
  const CycleResult cycle = runCycle(needle, responseWithPeak(30.0f));
  CHECK_TRUE(cycle.moved);
  // 30% of a 180 degree sweep, starting from -90.
  CHECK_NEAR(cycle.dialDeg, -36.0f, kTol);
  CHECK_NEAR(dialDegToClockHour(cycle.dialDeg), 10.8f, kTol);
  CHECK_EQ(cycle.pulseUs, 1100);
}

TEST(Integration, SecondCycleWithSameWindDoesNotMoveTheServo) {
  NeedleState needle;
  runCycle(needle, responseWithPeak(30.0f));
  const CycleResult second = runCycle(needle, responseWithPeak(30.3f));
  CHECK_FALSE(second.moved);
}

TEST(Integration, GustFrontIsSlewLimitedAcrossCycles) {
  NeedleState needle;
  runCycle(needle, responseWithPeak(10.0f));
  const CycleResult jump = runCycle(needle, responseWithPeak(90.0f));
  CHECK_TRUE(jump.moved);
  // 0.6 smoothing on an 80 km/h step is 48, capped at 25.
  CHECK_NEAR(jump.displayedKph, 35.0f, kTol);
  CHECK_TRUE(jump.displayedKph < 90.0f);
}

TEST(Integration, BadResponseLeavesTheNeedleWhereItIs) {
  NeedleState needle;
  runCycle(needle, responseWithPeak(45.0f));
  const float before = needle.displayedKph;

  const CycleResult broken = runCycle(needle, "{\"error\":true}");
  CHECK_FALSE(broken.moved);
  CHECK_NEAR(needle.displayedKph, before, kTol);
}

TEST(Integration, UrlAndParserAgreeOnTheFieldNames) {
  char url[256];
  const int written = buildOpenMeteoUrl(url, sizeof(url), config::kLatitude,
                                        config::kLongitude, config::kForecast);
  CHECK_TRUE(written > 0);
  // The array the parser looks for is the one the URL asks for.
  CHECK_TRUE(std::string(url).find("hourly=wind_speed_10m") != std::string::npos);

  const ForecastResult parsed =
      parseOpenMeteo(responseWithPeak(20.0f).c_str(), config::kForecast);
  CHECK_TRUE(parsed.valid);
  CHECK_NEAR(parsed.windKph, 20.0f, kTol);
}

TEST(Integration, FlatBatteryParksTheNeedleAndStopsTheServo) {
  const PowerMode mode =
      classifyBattery(config::kPower, 3.05f, PowerMode::Normal);
  CHECK_TRUE(mode == PowerMode::Critical);
  CHECK_FALSE(servoAllowed(mode));
  // Parked means 9 o'clock, at the calm end of the needle's travel.
  CHECK_NEAR(windToDialDeg(config::kDial, 0.0f), -90.0f, kTol);
  CHECK_EQ(windKphToPulseUs(config::kServo, config::kDial, 0.0f),
           config::kServo.minPulseUs);
}

TEST(Integration, DialSweepUsesTheServoTravelExactly) {
  const float atZero = dialDegToShaftDeg(
      config::kServo, config::kDial, windToDialDeg(config::kDial, 0.0f));
  const float atFull = dialDegToShaftDeg(
      config::kServo, config::kDial, windToDialDeg(config::kDial, 100.0f));
  CHECK_NEAR(atZero, 0.0f, kTol);
  CHECK_NEAR(atFull, config::kServo.travelDeg, kTol);
  CHECK_NEAR(atFull - atZero, 180.0f, kTol);
}

TEST(Integration, EveryWindSpeedOnTheScaleGivesAReachablePulse) {
  for (float kph = 0.0f; kph <= 100.0f; kph += 0.5f) {
    const uint16_t pulse =
        windKphToPulseUs(config::kServo, config::kDial, kph);
    CHECK_TRUE(pulse >= config::kServo.minPulseUs);
    CHECK_TRUE(pulse <= config::kServo.maxPulseUs);
  }
}

TEST(Integration, DialResolutionIsFineEnoughToRead) {
  // 1 km/h is 1.8 degrees of needle across a 180 degree sweep.
  const float oneKph = windToDialDeg(config::kDial, 1.0f) -
                       windToDialDeg(config::kDial, 0.0f);
  CHECK_NEAR(oneKph, 1.8f, kTol);

  // And a distinct pulse width, so the servo can actually resolve it.
  CHECK_TRUE(windKphToPulseUs(config::kServo, config::kDial, 1.0f) >
             windKphToPulseUs(config::kServo, config::kDial, 0.0f));
}
