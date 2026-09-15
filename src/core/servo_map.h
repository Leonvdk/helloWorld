// Turns a position on the clock face into a servo pulse width.
//
// The 9-to-3 dial needs 180 degrees of needle travel, so a standard hobby
// servo drives the needle directly (travelDeg = 180, gearRatio = 1). The
// gearing and travel are still configurable, for a servo that does not
// quite make its nominal 180, or a 270-degree servo used with trim to
// centre the sweep.
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
  // Shaft angle that puts the needle at the calm end of the dial, i.e. at
  // DialSpec::minDialDeg. Use it to take up the slop after assembly, and
  // to centre a sweep inside a servo with travel to spare.
  float trimDeg = 0.0f;
  // True if increasing pulse width turns the needle anticlockwise.
  bool reversed = false;
};

bool calibrationIsValid(const ServoCalibration &cal);

// Shaft degrees the needle needs for the full dial sweep.
float requiredShaftDeg(const ServoCalibration &cal, const DialSpec &spec);

// True if the servo can reach both ends of the dial, trim included.
bool dialFitsCalibration(const ServoCalibration &cal, const DialSpec &spec);

// Needle angle -> shaft angle, clamped into the servo's travel. Measured
// from the calm end of the dial, so the servo's zero lines up with
// DialSpec::minDialDeg wherever that sits on the face.
float dialDegToShaftDeg(const ServoCalibration &cal, const DialSpec &spec,
                        float dialDeg);

// Shaft angle -> pulse width, clamped into the servo's pulse range.
uint16_t shaftDegToPulseUs(const ServoCalibration &cal, float shaftDeg);

// The whole chain: wind speed -> pulse width to hand the servo driver.
uint16_t windKphToPulseUs(const ServoCalibration &cal, const DialSpec &spec,
                          float windKph);

} // namespace windclock

#endif // WINDCLOCK_CORE_SERVO_MAP_H
