// Decides whether the needle is worth moving.
//
// On batteries, the expensive part of a wake cycle is powering the servo,
// so a 0.3 km/h wobble in the forecast must not cost a servo move. The
// deadband holds the needle still until the change is big enough to see,
// and the slew limit stops a storm front from slamming it across the face.
#ifndef WINDCLOCK_CORE_NEEDLE_FILTER_H
#define WINDCLOCK_CORE_NEEDLE_FILTER_H

namespace windclock {

struct NeedlePolicy {
  // Smallest change that earns a servo move, in km/h. A change of
  // exactly this much moves; anything smaller does not.
  float deadbandKph = 1.5f;
  // Largest change allowed in one wake cycle, in km/h.
  float maxStepKph = 25.0f;
  // Exponential smoothing on the step. 1.0 goes straight to the target.
  float smoothing = 0.6f;
};

struct NeedleState {
  bool initialised = false;
  float displayedKph = 0.0f;
};

// Folds a new reading into the state. Returns true if the needle should be
// driven to state.displayedKph, false if it should be left alone. The first
// reading after a cold start always moves, so the needle is never showing a
// number nobody chose.
bool updateNeedle(NeedleState &state, const NeedlePolicy &policy,
                  float measuredKph);

bool needlePolicyIsValid(const NeedlePolicy &policy);

} // namespace windclock

#endif // WINDCLOCK_CORE_NEEDLE_FILTER_H
