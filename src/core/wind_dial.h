// Maps a wind speed onto an angle on the clock face.
//
// The dial is read like a clock: 0 km/h parks the needle straight up at
// 12 o'clock and the needle travels *clockwise* as the wind picks up, so
// 100 km/h lands on 11 o'clock -- 330 degrees of sweep.
#ifndef WINDCLOCK_CORE_WIND_DIAL_H
#define WINDCLOCK_CORE_WIND_DIAL_H

namespace windclock {

// One hour on a clock face.
constexpr float kDegreesPerClockHour = 30.0f;

struct DialSpec {
  float minWindKph = 0.0f;
  float maxWindKph = 100.0f;
  // Degrees clockwise from 12 o'clock.
  float minDialDeg = 0.0f;                        // 12 o'clock
  float maxDialDeg = 11.0f * kDegreesPerClockHour; // 11 o'clock == 330 deg
};

// A spec is usable only if both the wind range and the angle range are
// non-empty and finite. Everything downstream assumes this holds.
bool dialSpecIsValid(const DialSpec &spec);

// Clamps to [minWindKph, maxWindKph]. A non-finite input clamps to the low
// end so a bad reading can never fling the needle across the face.
float clampWindKph(const DialSpec &spec, float windKph);

// Wind speed -> degrees clockwise from 12 o'clock. Out-of-range winds are
// pinned to the ends of the dial rather than running off it.
float windToDialDeg(const DialSpec &spec, float windKph);

// Inverse of windToDialDeg, for tests and for reading back what the needle
// is currently showing.
float dialDegToWindKph(const DialSpec &spec, float dialDeg);

// Degrees clockwise from 12 o'clock -> position on the clock face, in hours
// (0..12). 330 deg reads back as 11.0.
float dialDegToClockHour(float dialDeg);

// Total sweep the needle has to cover, in degrees.
float dialSweepDeg(const DialSpec &spec);

} // namespace windclock

#endif // WINDCLOCK_CORE_WIND_DIAL_H
