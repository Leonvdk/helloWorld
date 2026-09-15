#include "wind_dial.h"

#include <cmath>

namespace windclock {
namespace {

bool finite(float v) { return std::isfinite(v); }

} // namespace

bool dialSpecIsValid(const DialSpec &spec) {
  if (!finite(spec.minWindKph) || !finite(spec.maxWindKph)) return false;
  if (!finite(spec.minDialDeg) || !finite(spec.maxDialDeg)) return false;
  if (spec.maxWindKph <= spec.minWindKph) return false;
  if (spec.maxDialDeg == spec.minDialDeg) return false;
  return true;
}

float clampWindKph(const DialSpec &spec, float windKph) {
  if (!finite(windKph)) return spec.minWindKph;
  if (windKph < spec.minWindKph) return spec.minWindKph;
  if (windKph > spec.maxWindKph) return spec.maxWindKph;
  return windKph;
}

float dialSweepDeg(const DialSpec &spec) {
  return spec.maxDialDeg - spec.minDialDeg;
}

float windToDialDeg(const DialSpec &spec, float windKph) {
  if (!dialSpecIsValid(spec)) return spec.minDialDeg;
  const float clamped = clampWindKph(spec, windKph);
  const float fraction =
      (clamped - spec.minWindKph) / (spec.maxWindKph - spec.minWindKph);
  return spec.minDialDeg + fraction * dialSweepDeg(spec);
}

float dialDegToWindKph(const DialSpec &spec, float dialDeg) {
  if (!dialSpecIsValid(spec)) return spec.minWindKph;
  if (!finite(dialDeg)) return spec.minWindKph;
  const float fraction = (dialDeg - spec.minDialDeg) / dialSweepDeg(spec);
  const float wind =
      spec.minWindKph + fraction * (spec.maxWindKph - spec.minWindKph);
  return clampWindKph(spec, wind);
}

float dialDegToClockHour(float dialDeg) {
  if (!finite(dialDeg)) return 0.0f;
  float hour = std::fmod(dialDeg / kDegreesPerClockHour, 12.0f);
  if (hour < 0.0f) hour += 12.0f;
  return hour;
}

} // namespace windclock
