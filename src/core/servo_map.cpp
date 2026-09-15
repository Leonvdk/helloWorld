#include "servo_map.h"

#include <cmath>

namespace windclock {
namespace {

float clampf(float v, float lo, float hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

} // namespace

bool calibrationIsValid(const ServoCalibration &cal) {
  if (cal.maxPulseUs <= cal.minPulseUs) return false;
  if (!std::isfinite(cal.travelDeg) || cal.travelDeg <= 0.0f) return false;
  if (!std::isfinite(cal.gearRatio) || cal.gearRatio <= 0.0f) return false;
  if (!std::isfinite(cal.trimDeg)) return false;
  return true;
}

float requiredShaftDeg(const ServoCalibration &cal, const DialSpec &spec) {
  if (!calibrationIsValid(cal) || !dialSpecIsValid(spec)) return 0.0f;
  return std::fabs(dialSweepDeg(spec)) / cal.gearRatio;
}

bool dialFitsCalibration(const ServoCalibration &cal, const DialSpec &spec) {
  if (!calibrationIsValid(cal) || !dialSpecIsValid(spec)) return false;
  if (cal.trimDeg < 0.0f || cal.trimDeg > cal.travelDeg) return false;
  // Allow a hair of tolerance so a 2:1 gear train on a 165-degree
  // requirement isn't rejected by float noise.
  constexpr float kEpsilonDeg = 1e-3f;
  return cal.trimDeg + requiredShaftDeg(cal, spec) <= cal.travelDeg + kEpsilonDeg;
}

float dialDegToShaftDeg(const ServoCalibration &cal, float dialDeg) {
  if (!calibrationIsValid(cal)) return 0.0f;
  if (!std::isfinite(dialDeg)) return clampf(cal.trimDeg, 0.0f, cal.travelDeg);
  const float shaft = cal.trimDeg + dialDeg / cal.gearRatio;
  return clampf(shaft, 0.0f, cal.travelDeg);
}

uint16_t shaftDegToPulseUs(const ServoCalibration &cal, float shaftDeg) {
  if (!calibrationIsValid(cal)) return 0;
  const float clamped =
      std::isfinite(shaftDeg) ? clampf(shaftDeg, 0.0f, cal.travelDeg) : 0.0f;
  const float fraction =
      (cal.reversed ? cal.travelDeg - clamped : clamped) / cal.travelDeg;
  const float span =
      static_cast<float>(cal.maxPulseUs) - static_cast<float>(cal.minPulseUs);
  const float pulse = static_cast<float>(cal.minPulseUs) + fraction * span;
  return static_cast<uint16_t>(
      std::lround(clampf(pulse, static_cast<float>(cal.minPulseUs),
                         static_cast<float>(cal.maxPulseUs))));
}

uint16_t windKphToPulseUs(const ServoCalibration &cal, const DialSpec &spec,
                          float windKph) {
  return shaftDegToPulseUs(cal,
                           dialDegToShaftDeg(cal, windToDialDeg(spec, windKph)));
}

} // namespace windclock
