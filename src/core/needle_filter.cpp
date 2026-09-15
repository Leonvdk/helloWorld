#include "needle_filter.h"

#include <cmath>

namespace windclock {

bool needlePolicyIsValid(const NeedlePolicy &policy) {
  if (!std::isfinite(policy.deadbandKph) || policy.deadbandKph < 0.0f) return false;
  if (!std::isfinite(policy.maxStepKph) || policy.maxStepKph <= 0.0f) return false;
  if (!std::isfinite(policy.smoothing) || policy.smoothing <= 0.0f ||
      policy.smoothing > 1.0f) {
    return false;
  }
  return true;
}

bool updateNeedle(NeedleState &state, const NeedlePolicy &policy,
                  float measuredKph) {
  if (!std::isfinite(measuredKph)) return false;
  if (!needlePolicyIsValid(policy)) return false;

  if (!state.initialised) {
    state.initialised = true;
    state.displayedKph = measuredKph;
    return true;
  }

  const float rawDelta = measuredKph - state.displayedKph;
  // An unchanged reading is never worth a servo move, even with the
  // deadband turned off.
  if (rawDelta == 0.0f) return false;
  // Deadband is measured against the real reading, not the smoothed step,
  // so a slow drift still eventually crosses it. A change of exactly the
  // deadband counts as big enough.
  if (std::fabs(rawDelta) < policy.deadbandKph) return false;

  float step = rawDelta * policy.smoothing;
  if (step > policy.maxStepKph) step = policy.maxStepKph;
  if (step < -policy.maxStepKph) step = -policy.maxStepKph;

  state.displayedKph += step;
  return true;
}

} // namespace windclock
