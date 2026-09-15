// Maps a wind speed onto an angle on the clock face.
//
// The dial is read like a clock, and the needle travels *clockwise* as the
// wind picks up: 0 km/h at 9 o'clock, half scale straight up at 12, and
// 100 km/h at 3 o'clock. That is a 180 degree sweep across the top of the
// face, which is exactly what a standard servo turns.
//
// Angles are signed degrees clockwise from 12 o'clock, so 9 o'clock is
// -90 and 3 o'clock is +90.
#ifndef WINDCLOCK_CORE_WIND_DIAL_H
#define WINDCLOCK_CORE_WIND_DIAL_H

namespace windclock {

// One hour on a clock face.
constexpr float kDegreesPerClockHour = 30.0f;

struct DialSpec {
  float minWindKph = 0.0f;
  float maxWindKph = 100.0f;
  // Signed degrees clockwise from 12 o'clock. minDialDeg is where the calm
  // end of the scale sits and must be the lower of the two -- if the
  // needle has to travel the other way round, reverse the servo rather
  // than the dial (see ServoCalibration::reversed).
  float minDialDeg = -3.0f * kDegreesPerClockHour; // 9 o'clock == -90 deg
  float maxDialDeg = 3.0f * kDegreesPerClockHour;  // 3 o'clock == +90 deg
};

// A spec is usable only if the wind range and the angle range are both
// finite and ascending. Everything downstream assumes this holds.
bool dialSpecIsValid(const DialSpec &spec);

// Clamps to [minWindKph, maxWindKph]. A non-finite input clamps to the low
// end so a bad reading can never fling the needle across the face.
float clampWindKph(const DialSpec &spec, float windKph);

// Wind speed -> signed degrees clockwise from 12 o'clock. Out-of-range
// winds are pinned to the ends of the dial rather than running off it.
float windToDialDeg(const DialSpec &spec, float windKph);

// Inverse of windToDialDeg, for tests and for reading back what the needle
// is currently showing.
float dialDegToWindKph(const DialSpec &spec, float dialDeg);

// Signed degrees clockwise from 12 o'clock -> position on the clock face,
// in hours (0..12). -90 deg reads back as 9.0, +90 as 3.0.
float dialDegToClockHour(float dialDeg);

// Total sweep the needle has to cover, in degrees.
float dialSweepDeg(const DialSpec &spec);

} // namespace windclock

#endif // WINDCLOCK_CORE_WIND_DIAL_H
