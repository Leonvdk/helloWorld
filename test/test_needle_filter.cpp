// The deadband and slew limit that keep the servo -- and the battery -- calm.
#include <limits>

#include "core/needle_filter.h"
#include "test_framework.h"

using namespace windclock;

namespace {

NeedlePolicy policy() { return NeedlePolicy{}; } // 1.5 km/h, 25 km/h, 0.6

// A policy that moves straight to the target, for testing the deadband on
// its own.
NeedlePolicy unsmoothed() {
  NeedlePolicy p;
  p.smoothing = 1.0f;
  return p;
}

constexpr float kTol = 0.01f;

} // namespace

TEST(NeedleFilter, FirstReadingAlwaysMovesTheNeedle) {
  NeedleState state;
  CHECK_TRUE(updateNeedle(state, policy(), 23.0f));
  CHECK_NEAR(state.displayedKph, 23.0f, kTol);
  CHECK_TRUE(state.initialised);
}

TEST(NeedleFilter, FirstReadingOfZeroStillCountsAsAMove) {
  // Otherwise a cold start in calm weather would leave the needle wherever
  // it happened to be parked.
  NeedleState state;
  CHECK_TRUE(updateNeedle(state, policy(), 0.0f));
  CHECK_TRUE(state.initialised);
}

TEST(NeedleFilter, SmallChangesAreIgnored) {
  NeedleState state;
  updateNeedle(state, policy(), 20.0f);
  CHECK_FALSE(updateNeedle(state, policy(), 20.4f));
  CHECK_FALSE(updateNeedle(state, policy(), 19.2f));
  CHECK_NEAR(state.displayedKph, 20.0f, kTol);
}

TEST(NeedleFilter, DeadbandEdgeIsInclusive) {
  NeedleState state;
  updateNeedle(state, unsmoothed(), 20.0f);
  // Just under the deadband holds; exactly the deadband moves.
  CHECK_FALSE(updateNeedle(state, unsmoothed(), 21.4f));
  CHECK_TRUE(updateNeedle(state, unsmoothed(), 21.5f));
  CHECK_NEAR(state.displayedKph, 21.5f, kTol);
}

TEST(NeedleFilter, LargeChangesMoveTheNeedle) {
  NeedleState state;
  updateNeedle(state, unsmoothed(), 20.0f);
  CHECK_TRUE(updateNeedle(state, unsmoothed(), 35.0f));
  CHECK_NEAR(state.displayedKph, 35.0f, kTol);
}

TEST(NeedleFilter, SmoothingMovesPartWayToTheTarget) {
  NeedleState state;
  updateNeedle(state, policy(), 20.0f);
  // 0.6 of a 10 km/h step.
  CHECK_TRUE(updateNeedle(state, policy(), 30.0f));
  CHECK_NEAR(state.displayedKph, 26.0f, kTol);
}

TEST(NeedleFilter, SlewLimitCapsASuddenGale) {
  NeedlePolicy p = unsmoothed();
  p.maxStepKph = 25.0f;
  NeedleState state;
  updateNeedle(state, p, 5.0f);
  CHECK_TRUE(updateNeedle(state, p, 95.0f));
  CHECK_NEAR(state.displayedKph, 30.0f, kTol); // 5 + 25, not 95
}

TEST(NeedleFilter, SlewLimitAppliesWhenTheWindDrops) {
  NeedlePolicy p = unsmoothed();
  p.maxStepKph = 25.0f;
  NeedleState state;
  updateNeedle(state, p, 90.0f);
  CHECK_TRUE(updateNeedle(state, p, 2.0f));
  CHECK_NEAR(state.displayedKph, 65.0f, kTol);
}

TEST(NeedleFilter, RepeatedUpdatesConvergeOnTheTarget) {
  NeedleState state;
  updateNeedle(state, policy(), 10.0f);
  for (int i = 0; i < 40; ++i) {
    updateNeedle(state, policy(), 80.0f);
  }
  // Converges to within the deadband, which is as close as it should get.
  CHECK_NEAR(state.displayedKph, 80.0f, policy().deadbandKph);
}

TEST(NeedleFilter, SlowDriftEventuallyCrossesTheDeadband) {
  // The deadband is measured against the real reading, so a wind creeping
  // up 0.5 km/h an hour is not ignored forever.
  NeedleState state;
  updateNeedle(state, policy(), 20.0f);
  bool moved = false;
  for (int i = 1; i <= 5 && !moved; ++i) {
    moved = updateNeedle(state, policy(), 20.0f + 0.5f * static_cast<float>(i));
  }
  CHECK_TRUE(moved);
}

TEST(NeedleFilter, SteadyWindNeverWakesTheServo) {
  NeedleState state;
  updateNeedle(state, policy(), 42.0f);
  int moves = 0;
  for (int i = 0; i < 50; ++i) {
    if (updateNeedle(state, policy(), 42.0f)) ++moves;
  }
  CHECK_EQ(moves, 0);
}

TEST(NeedleFilter, NonFiniteReadingsAreIgnored) {
  NeedleState state;
  updateNeedle(state, policy(), 30.0f);
  CHECK_FALSE(updateNeedle(state, policy(),
                           std::numeric_limits<float>::quiet_NaN()));
  CHECK_FALSE(
      updateNeedle(state, policy(), std::numeric_limits<float>::infinity()));
  CHECK_NEAR(state.displayedKph, 30.0f, kTol);
}

TEST(NeedleFilter, NonFiniteReadingCannotInitialiseTheState) {
  NeedleState state;
  CHECK_FALSE(
      updateNeedle(state, policy(), std::numeric_limits<float>::quiet_NaN()));
  CHECK_FALSE(state.initialised);
}

TEST(NeedleFilter, RejectsUnusablePolicies) {
  CHECK_TRUE(needlePolicyIsValid(policy()));

  NeedlePolicy negativeDeadband = policy();
  negativeDeadband.deadbandKph = -1.0f;
  CHECK_FALSE(needlePolicyIsValid(negativeDeadband));

  NeedlePolicy zeroStep = policy();
  zeroStep.maxStepKph = 0.0f;
  CHECK_FALSE(needlePolicyIsValid(zeroStep));

  NeedlePolicy overSmoothed = policy();
  overSmoothed.smoothing = 1.4f;
  CHECK_FALSE(needlePolicyIsValid(overSmoothed));

  NeedlePolicy noSmoothing = policy();
  noSmoothing.smoothing = 0.0f;
  CHECK_FALSE(needlePolicyIsValid(noSmoothing));
}

TEST(NeedleFilter, InvalidPolicyHoldsTheNeedleStill) {
  NeedlePolicy broken = policy();
  broken.maxStepKph = 0.0f;
  NeedleState state;
  CHECK_FALSE(updateNeedle(state, broken, 30.0f));
  CHECK_FALSE(state.initialised);
}

TEST(NeedleFilter, ZeroDeadbandLetsEveryChangeThrough) {
  NeedlePolicy p = unsmoothed();
  p.deadbandKph = 0.0f;
  NeedleState state;
  updateNeedle(state, p, 20.0f);
  CHECK_TRUE(updateNeedle(state, p, 20.01f));
  // But an identical reading is still not a change, deadband or no
  // deadband -- otherwise a dead calm would drive the servo every cycle.
  CHECK_FALSE(updateNeedle(state, p, 20.01f));
}
