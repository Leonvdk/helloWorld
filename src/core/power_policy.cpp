#include "power_policy.h"

#include <cmath>

namespace windclock {

bool powerPolicyIsValid(const PowerPolicy &policy) {
  if (!std::isfinite(policy.lowBatteryVolts)) return false;
  if (!std::isfinite(policy.criticalBatteryVolts)) return false;
  if (!std::isfinite(policy.recoverVolts)) return false;
  if (policy.criticalBatteryVolts >= policy.lowBatteryVolts) return false;
  if (policy.recoverVolts < policy.lowBatteryVolts) return false;
  if (policy.normalSleepSeconds == 0) return false;
  if (policy.lowBatterySleepSeconds < policy.normalSleepSeconds) return false;
  if (policy.criticalSleepSeconds < policy.lowBatterySleepSeconds) return false;
  return true;
}

PowerMode classifyBattery(const PowerPolicy &policy, float volts,
                          PowerMode previous) {
  if (!std::isfinite(volts)) return previous;
  if (!powerPolicyIsValid(policy)) return previous;

  if (volts < policy.criticalBatteryVolts) return PowerMode::Critical;
  if (volts < policy.lowBatteryVolts) return PowerMode::LowBattery;
  if (previous == PowerMode::Normal) return PowerMode::Normal;
  // Coming back up out of a degraded mode needs the recovery margin.
  return volts >= policy.recoverVolts ? PowerMode::Normal : PowerMode::LowBattery;
}

uint32_t sleepSecondsFor(const PowerPolicy &policy, PowerMode mode) {
  switch (mode) {
    case PowerMode::LowBattery: return policy.lowBatterySleepSeconds;
    case PowerMode::Critical: return policy.criticalSleepSeconds;
    case PowerMode::Normal: break;
  }
  return policy.normalSleepSeconds;
}

bool servoAllowed(PowerMode mode) { return mode != PowerMode::Critical; }

float adcToBatteryVolts(float pinVolts, float dividerRatio) {
  if (!std::isfinite(pinVolts) || !std::isfinite(dividerRatio)) return 0.0f;
  if (dividerRatio <= 0.0f || pinVolts < 0.0f) return 0.0f;
  return pinVolts * dividerRatio;
}

uint32_t retryBackoffSeconds(int attempt, uint32_t baseSeconds,
                             uint32_t maxSeconds) {
  if (baseSeconds == 0) return 0;
  if (attempt <= 0) return baseSeconds > maxSeconds ? maxSeconds : baseSeconds;
  uint32_t seconds = baseSeconds;
  for (int i = 0; i < attempt; ++i) {
    if (seconds > maxSeconds / 2) return maxSeconds;
    seconds *= 2;
  }
  return seconds > maxSeconds ? maxSeconds : seconds;
}

bool readingIsStale(const PowerPolicy &policy, uint32_t secondsSinceUpdate) {
  if (policy.staleAfterSeconds == 0) return false;
  return secondsSinceUpdate >= policy.staleAfterSeconds;
}

} // namespace windclock
