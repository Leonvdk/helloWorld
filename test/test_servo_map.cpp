// Servo calibration: pulse widths, trim, gearing and travel limits.
#include <limits>

#include "core/servo_map.h"
#include "test_framework.h"

using namespace windclock;

namespace {

// The shipped build: a 180 degree servo on the needle, no gearing.
ServoCalibration direct() { return ServoCalibration{500, 2500, 180.0f, 1.0f, 0.0f, false}; }

// The roomier alternative: a 270 degree servo, sweep centred with trim.
ServoCalibration roomy() {
  ServoCalibration cal = direct();
  cal.travelDeg = 270.0f;
  cal.trimDeg = 45.0f;
  return cal;
}

DialSpec spec() { return DialSpec{}; } // -90 .. +90 degrees

constexpr float kTol = 0.01f;

} // namespace

TEST(ServoMap, PulseEndpointsMatchCalibration) {
  const ServoCalibration cal = direct();
  CHECK_EQ(shaftDegToPulseUs(cal, 0.0f), 500);
  CHECK_EQ(shaftDegToPulseUs(cal, 180.0f), 2500);
  CHECK_EQ(shaftDegToPulseUs(cal, 90.0f), 1500);
}

TEST(ServoMap, PulseIsClampedToTravel) {
  const ServoCalibration cal = direct();
  CHECK_EQ(shaftDegToPulseUs(cal, -90.0f), 500);
  CHECK_EQ(shaftDegToPulseUs(cal, 720.0f), 2500);
}

TEST(ServoMap, ShaftAngleIsMeasuredFromTheCalmEndOfTheDial) {
  // The dial runs -90..+90, but the servo shaft still runs 0..180: the
  // needle's 9 o'clock is the servo's zero.
  const ServoCalibration cal = direct();
  CHECK_NEAR(dialDegToShaftDeg(cal, spec(), -90.0f), 0.0f, kTol);
  CHECK_NEAR(dialDegToShaftDeg(cal, spec(), 0.0f), 90.0f, kTol);
  CHECK_NEAR(dialDegToShaftDeg(cal, spec(), 90.0f), 180.0f, kTol);
}

TEST(ServoMap, TrimOffsetsTheWholeSweep) {
  const ServoCalibration cal = roomy();
  CHECK_NEAR(dialDegToShaftDeg(cal, spec(), -90.0f), 45.0f, kTol);
  CHECK_NEAR(dialDegToShaftDeg(cal, spec(), 90.0f), 225.0f, kTol);
  // Centred, with 45 degrees spare at each end.
  CHECK_NEAR(cal.travelDeg - 225.0f, 45.0f, kTol);
  CHECK_TRUE(dialFitsCalibration(cal, spec()));
}

TEST(ServoMap, GearRatioScalesTheShaftAngle) {
  ServoCalibration cal = direct();
  cal.gearRatio = 2.0f; // 2 needle degrees per shaft degree
  CHECK_NEAR(dialDegToShaftDeg(cal, spec(), -90.0f), 0.0f, kTol);
  CHECK_NEAR(dialDegToShaftDeg(cal, spec(), 90.0f), 90.0f, kTol);
  CHECK_NEAR(requiredShaftDeg(cal, spec()), 90.0f, kTol);
}

TEST(ServoMap, ReversedServoMirrorsThePulse) {
  ServoCalibration cal = direct();
  cal.reversed = true;
  CHECK_EQ(shaftDegToPulseUs(cal, 0.0f), 2500);
  CHECK_EQ(shaftDegToPulseUs(cal, 180.0f), 500);
  CHECK_EQ(shaftDegToPulseUs(cal, 90.0f), 1500);
  // Calm now sits at the long pulse rather than the short one.
  CHECK_EQ(windKphToPulseUs(cal, spec(), 0.0f), 2500);
  CHECK_EQ(windKphToPulseUs(cal, spec(), 100.0f), 500);
}

TEST(ServoMap, ShippedBuildUsesTheServosFullTravel) {
  CHECK_TRUE(dialFitsCalibration(direct(), spec()));
  CHECK_NEAR(requiredShaftDeg(direct(), spec()), 180.0f, kTol);
  // Exactly 180 of 180: no trim margin left, which is the trade for
  // dropping the gear train.
  CHECK_NEAR(direct().travelDeg - requiredShaftDeg(direct(), spec()), 0.0f,
             kTol);
}

TEST(ServoMap, ServoThatFallsShortOfOneEightyIsRejected) {
  ServoCalibration cal = direct();
  cal.travelDeg = 170.0f;
  CHECK_FALSE(dialFitsCalibration(cal, spec()));
}

TEST(ServoMap, TrimThatPushesTheSweepPastTheEndStopIsRejected) {
  ServoCalibration cal = direct();
  cal.trimDeg = 10.0f; // 10 + 180 = 190 > 180
  CHECK_FALSE(dialFitsCalibration(cal, spec()));
}

TEST(ServoMap, NegativeTrimIsRejected) {
  ServoCalibration cal = direct();
  cal.trimDeg = -5.0f;
  CHECK_FALSE(dialFitsCalibration(cal, spec()));
}

TEST(ServoMap, RejectsUnusableCalibrations) {
  CHECK_TRUE(calibrationIsValid(direct()));

  ServoCalibration backwardsPulses = direct();
  backwardsPulses.minPulseUs = 2500;
  backwardsPulses.maxPulseUs = 500;
  CHECK_FALSE(calibrationIsValid(backwardsPulses));

  ServoCalibration zeroTravel = direct();
  zeroTravel.travelDeg = 0.0f;
  CHECK_FALSE(calibrationIsValid(zeroTravel));

  ServoCalibration zeroGear = direct();
  zeroGear.gearRatio = 0.0f;
  CHECK_FALSE(calibrationIsValid(zeroGear));

  ServoCalibration nanTrim = direct();
  nanTrim.trimDeg = std::numeric_limits<float>::quiet_NaN();
  CHECK_FALSE(calibrationIsValid(nanTrim));
}

TEST(ServoMap, InvalidCalibrationYieldsNoPulse) {
  ServoCalibration cal = direct();
  cal.gearRatio = -1.0f;
  CHECK_EQ(shaftDegToPulseUs(cal, 90.0f), 0);
  CHECK_EQ(windKphToPulseUs(cal, spec(), 50.0f), 0);
}

TEST(ServoMap, InvalidDialYieldsTheShaftZero) {
  DialSpec broken = spec();
  broken.maxDialDeg = broken.minDialDeg;
  CHECK_NEAR(dialDegToShaftDeg(direct(), broken, 0.0f), 0.0f, kTol);
}

TEST(ServoMap, NonFiniteAngleFallsBackToTheTrimPosition) {
  const float nan = std::numeric_limits<float>::quiet_NaN();
  CHECK_NEAR(dialDegToShaftDeg(roomy(), spec(), nan), 45.0f, kTol);
  CHECK_NEAR(dialDegToShaftDeg(direct(), spec(), nan), 0.0f, kTol);
}

TEST(ServoMap, WindMapsStraightToAPulseWidth) {
  const ServoCalibration cal = direct();
  CHECK_EQ(windKphToPulseUs(cal, spec(), 0.0f), 500);   // 9 o'clock
  CHECK_EQ(windKphToPulseUs(cal, spec(), 50.0f), 1500); // 12 o'clock
  CHECK_EQ(windKphToPulseUs(cal, spec(), 100.0f), 2500); // 3 o'clock
  // 20 us per km/h, right across the scale.
  CHECK_EQ(windKphToPulseUs(cal, spec(), 25.0f), 1000);
  CHECK_EQ(windKphToPulseUs(cal, spec(), 75.0f), 2000);
}

TEST(ServoMap, RoomyBuildGivesTheSameDialOverASmallerPulseSpan) {
  const ServoCalibration cal = roomy();
  // 45..225 of 270 degrees maps to the middle two thirds of the pulses.
  CHECK_EQ(windKphToPulseUs(cal, spec(), 0.0f), 833);
  CHECK_EQ(windKphToPulseUs(cal, spec(), 50.0f), 1500);
  CHECK_EQ(windKphToPulseUs(cal, spec(), 100.0f), 2167);
}

TEST(ServoMap, PulseWidthRisesWithWindAndStaysInRange) {
  const ServoCalibration cal = direct();
  uint16_t previous = 0;
  for (float kph = 0.0f; kph <= 100.0f; kph += 1.0f) {
    const uint16_t pulse = windKphToPulseUs(cal, spec(), kph);
    CHECK_TRUE(pulse >= previous);
    CHECK_TRUE(pulse >= cal.minPulseUs);
    CHECK_TRUE(pulse <= cal.maxPulseUs);
    previous = pulse;
  }
  CHECK_EQ(previous, 2500);
}

TEST(ServoMap, OverspeedWindDoesNotDriveTheServoPastItsEndStop) {
  const ServoCalibration cal = direct();
  CHECK_EQ(windKphToPulseUs(cal, spec(), 250.0f), 2500);
}

TEST(ServoMap, NarrowPulseRangeServoStillSpansTheDial) {
  // Some servos only honour 1000..2000 us.
  ServoCalibration cal = direct();
  cal.minPulseUs = 1000;
  cal.maxPulseUs = 2000;
  CHECK_EQ(windKphToPulseUs(cal, spec(), 0.0f), 1000);
  CHECK_EQ(windKphToPulseUs(cal, spec(), 50.0f), 1500);
  CHECK_EQ(windKphToPulseUs(cal, spec(), 100.0f), 2000);
}
