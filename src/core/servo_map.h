// Turns a position on the clock face into a servo pulse width.
//
// The dial needs 330 degrees of needle travel, which is more than a plain
// hobby servo can do. Two ways out, both covered here:
//
//   * a 360-degree *positional* servo driving the needle directly
//     (travelDeg = 360, gearRatio = 1);
//   * a 180- or 270-degree servo geared up, e.g. a 2:1 pulley pair so
//     165 degrees of shaft becomes 330 degrees of needle
//     (travelDeg = 180, gearRatio = 2).
//
// dialFitsCalibration() says whether the hardware can actually reach the
// whole scale, so a mismatch shows up at startup instead of as a needle
// silently stuck at 80 km/h.
#ifndef WINDCLOCK_CORE_SERVO_MAP_H
#define WINDCLOCK_CORE_SERVO_MAP_H

#include <cstdint>

#include "wind_dial.h"

namespace windclock {

struct ServoCalibration {
  // Pulse widths at the two ends of the servo's travel.
  uint16_t minPulseUs = 500;
  uint16_t maxPulseUs = 2500;
  // Shaft rotation between minPulseUs and maxPulseUs.
  float travelDeg = 360.0f;
  // Needle degrees per shaft degree. 1.0 for a direct drive.
  float gearRatio = 1.0f;
  // Shaft angle that puts the needle at the 12 o'clock end of the dial.
  // Use it to take up the slop after assembling the gear train.
  float trimDeg = 0.0f;
  // True if increasing pulse width turns the needle anticlockwise.
  bool reversed = false;
};

bool calibrationIsValid(const ServoCalibration &cal);

// Shaft degrees the needle needs for the full dial sweep.
float requiredShaftDeg(const ServoCalibration &cal, const DialSpec &spec);

// True if the servo can reach both ends of the dial, trim included.
bool dialFitsCalibration(const ServoCalibration &cal, const DialSpec &spec);

// Needle angle -> shaft angle, clamped into the servo's travel.
float dialDegToShaftDeg(const ServoCalibration &cal, float dialDeg);

// Shaft angle -> pulse width, clamped into the servo's pulse range.
uint16_t shaftDegToPulseUs(const ServoCalibration &cal, float shaftDeg);

// The whole chain: wind speed -> pulse width to hand the servo driver.
uint16_t windKphToPulseUs(const ServoCalibration &cal, const DialSpec &spec,
                          float windKph);

} // namespace windclock

#endif // WINDCLOCK_CORE_SERVO_MAP_H
