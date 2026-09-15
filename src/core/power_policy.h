// Battery bookkeeping: how long to sleep, and when to stop moving the servo.
//
// A servo stalling against its end stop on a flat LiPo is the fastest way
// to kill both the cell and the gear train, so the clock parks the needle
// and stops driving it well before the battery is empty.
#ifndef WINDCLOCK_CORE_POWER_POLICY_H
#define WINDCLOCK_CORE_POWER_POLICY_H

#include <cstdint>

namespace windclock {

enum class PowerMode {
  Normal,     // update on the usual schedule
  LowBattery, // still update, but far less often
  Critical,   // park the needle at zero and stop driving the servo
};

struct PowerPolicy {
  uint32_t normalSleepSeconds = 30 * 60;
  uint32_t lowBatterySleepSeconds = 3 * 60 * 60;
  uint32_t criticalSleepSeconds = 12 * 60 * 60;
  // Single-cell LiPo thresholds, measured at the battery.
  float lowBatteryVolts = 3.50f;
  float criticalBatteryVolts = 3.20f;
  // Must climb back above this to leave a degraded mode. The gap stops the
  // clock flapping between modes as the servo loads the cell.
  float recoverVolts = 3.65f;
  // Longest the clock may go without a successful update before it gives
  // up on the current reading and parks the needle.
  uint32_t staleAfterSeconds = 6 * 60 * 60;
};

bool powerPolicyIsValid(const PowerPolicy &policy);

// Classifies the battery, given the mode from the previous wake so the
// hysteresis has something to work from. A non-finite reading keeps the
// previous mode rather than inventing one.
PowerMode classifyBattery(const PowerPolicy &policy, float volts,
                          PowerMode previous);

uint32_t sleepSecondsFor(const PowerPolicy &policy, PowerMode mode);

// True when the servo may be powered at all.
bool servoAllowed(PowerMode mode);

// Battery volts from the measured ADC pin voltage and the divider ratio
// (Vbatt / Vpin), e.g. 2.0 for two equal resistors.
float adcToBatteryVolts(float pinVolts, float dividerRatio);

// Backoff after a failed update, so a dead WiFi router does not flatten
// the battery with retries. Doubles per attempt, capped.
uint32_t retryBackoffSeconds(int attempt, uint32_t baseSeconds,
                             uint32_t maxSeconds);

// True when the last good reading is too old to keep showing.
bool readingIsStale(const PowerPolicy &policy, uint32_t secondsSinceUpdate);

} // namespace windclock

#endif // WINDCLOCK_CORE_POWER_POLICY_H
