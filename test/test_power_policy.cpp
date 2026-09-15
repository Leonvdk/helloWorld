// Battery classification, sleep scheduling and retry backoff.
#include <limits>

#include "core/power_policy.h"
#include "test_framework.h"

using namespace windclock;

namespace {
PowerPolicy policy() { return PowerPolicy{}; }
constexpr float kTol = 0.001f;
} // namespace

TEST(PowerPolicy, HealthyBatteryRunsNormally) {
  CHECK_TRUE(classifyBattery(policy(), 4.05f, PowerMode::Normal) ==
             PowerMode::Normal);
  CHECK_TRUE(classifyBattery(policy(), 3.51f, PowerMode::Normal) ==
             PowerMode::Normal);
}

TEST(PowerPolicy, LowBatteryStretchesTheUpdateInterval) {
  const PowerMode mode = classifyBattery(policy(), 3.40f, PowerMode::Normal);
  CHECK_TRUE(mode == PowerMode::LowBattery);
  CHECK_EQ(sleepSecondsFor(policy(), mode), 3u * 60u * 60u);
  CHECK_TRUE(servoAllowed(mode));
}

TEST(PowerPolicy, CriticalBatteryStopsDrivingTheServo) {
  const PowerMode mode = classifyBattery(policy(), 3.10f, PowerMode::Normal);
  CHECK_TRUE(mode == PowerMode::Critical);
  CHECK_FALSE(servoAllowed(mode));
  CHECK_EQ(sleepSecondsFor(policy(), mode), 12u * 60u * 60u);
}

TEST(PowerPolicy, RecoveryNeedsTheHysteresisMargin) {
  // Back above the low threshold but not above the recovery voltage: a
  // sagging cell must not flap in and out of low-battery mode.
  CHECK_TRUE(classifyBattery(policy(), 3.55f, PowerMode::LowBattery) ==
             PowerMode::LowBattery);
  CHECK_TRUE(classifyBattery(policy(), 3.70f, PowerMode::LowBattery) ==
             PowerMode::Normal);
}

TEST(PowerPolicy, CriticalRecoversThroughLowBatteryFirst) {
  CHECK_TRUE(classifyBattery(policy(), 3.45f, PowerMode::Critical) ==
             PowerMode::LowBattery);
  CHECK_TRUE(classifyBattery(policy(), 3.90f, PowerMode::Critical) ==
             PowerMode::Normal);
}

TEST(PowerPolicy, ThresholdsAreInclusiveAtTheTop) {
  // Exactly at the low threshold is still healthy.
  CHECK_TRUE(classifyBattery(policy(), 3.50f, PowerMode::Normal) ==
             PowerMode::Normal);
  // Exactly at the critical threshold is low, not critical.
  CHECK_TRUE(classifyBattery(policy(), 3.20f, PowerMode::Normal) ==
             PowerMode::LowBattery);
}

TEST(PowerPolicy, UnreadableBatteryKeepsThePreviousMode) {
  const float nan = std::numeric_limits<float>::quiet_NaN();
  CHECK_TRUE(classifyBattery(policy(), nan, PowerMode::LowBattery) ==
             PowerMode::LowBattery);
  CHECK_TRUE(classifyBattery(policy(), nan, PowerMode::Normal) ==
             PowerMode::Normal);
}

TEST(PowerPolicy, NormalSleepIsHalfAnHour) {
  CHECK_EQ(sleepSecondsFor(policy(), PowerMode::Normal), 30u * 60u);
}

TEST(PowerPolicy, RejectsIncoherentPolicies) {
  CHECK_TRUE(powerPolicyIsValid(policy()));

  PowerPolicy crossedThresholds = policy();
  crossedThresholds.criticalBatteryVolts = 3.8f; // above the low threshold
  CHECK_FALSE(powerPolicyIsValid(crossedThresholds));

  PowerPolicy noHysteresis = policy();
  noHysteresis.recoverVolts = 3.2f; // below the low threshold
  CHECK_FALSE(powerPolicyIsValid(noHysteresis));

  PowerPolicy shorterWhenLow = policy();
  shorterWhenLow.lowBatterySleepSeconds = 60; // shorter than normal
  CHECK_FALSE(powerPolicyIsValid(shorterWhenLow));

  PowerPolicy zeroSleep = policy();
  zeroSleep.normalSleepSeconds = 0;
  CHECK_FALSE(powerPolicyIsValid(zeroSleep));
}

TEST(PowerPolicy, InvalidPolicyKeepsThePreviousMode) {
  PowerPolicy broken = policy();
  broken.criticalBatteryVolts = 4.0f;
  CHECK_TRUE(classifyBattery(broken, 3.0f, PowerMode::Normal) ==
             PowerMode::Normal);
}

TEST(PowerPolicy, DividerScalesThePinVoltage) {
  CHECK_NEAR(adcToBatteryVolts(1.85f, 2.0f), 3.70f, kTol);
  CHECK_NEAR(adcToBatteryVolts(2.10f, 2.0f), 4.20f, kTol);
}

TEST(PowerPolicy, NonsenseAdcReadingsGiveZeroVolts) {
  CHECK_NEAR(adcToBatteryVolts(-1.0f, 2.0f), 0.0f, kTol);
  CHECK_NEAR(adcToBatteryVolts(1.85f, 0.0f), 0.0f, kTol);
  CHECK_NEAR(adcToBatteryVolts(std::numeric_limits<float>::quiet_NaN(), 2.0f),
             0.0f, kTol);
}

TEST(PowerPolicy, BackoffDoublesPerFailure) {
  CHECK_EQ(retryBackoffSeconds(0, 300, 3600), 300u);
  CHECK_EQ(retryBackoffSeconds(1, 300, 3600), 600u);
  CHECK_EQ(retryBackoffSeconds(2, 300, 3600), 1200u);
  CHECK_EQ(retryBackoffSeconds(3, 300, 3600), 2400u);
}

TEST(PowerPolicy, BackoffStopsAtTheCap) {
  CHECK_EQ(retryBackoffSeconds(4, 300, 3600), 3600u);
  CHECK_EQ(retryBackoffSeconds(50, 300, 3600), 3600u);
  // No overflow even at an absurd attempt count.
  CHECK_EQ(retryBackoffSeconds(1000, 300, 3600), 3600u);
}

TEST(PowerPolicy, BackoffHandlesDegenerateArguments) {
  CHECK_EQ(retryBackoffSeconds(-1, 300, 3600), 300u);
  CHECK_EQ(retryBackoffSeconds(3, 0, 3600), 0u);
  CHECK_EQ(retryBackoffSeconds(0, 7200, 3600), 3600u);
}

TEST(PowerPolicy, ReadingGoesStaleAfterSixHours) {
  CHECK_FALSE(readingIsStale(policy(), 0));
  CHECK_FALSE(readingIsStale(policy(), 5 * 60 * 60));
  CHECK_TRUE(readingIsStale(policy(), 6 * 60 * 60));
  CHECK_TRUE(readingIsStale(policy(), 24 * 60 * 60));
}

TEST(PowerPolicy, StalenessCanBeDisabled) {
  PowerPolicy never = policy();
  never.staleAfterSeconds = 0;
  CHECK_FALSE(readingIsStale(never, 1000u * 60u * 60u));
}
