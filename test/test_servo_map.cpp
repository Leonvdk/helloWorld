// Servo calibration: pulse widths, gearing, trim and travel limits.
#include <limits>

#include "core/servo_map.h"
#include "test_framework.h"

using namespace windclock;

namespace {

// The default build: 180 degree servo, geared 2:1 for a 330 degree needle.
ServoCalibration geared() {
  ServoCalibration cal;
  cal.travelDeg = 180.0f;
  cal.gearRatio = 2.0f;
  cal.trimDeg = 0.0f;
  return cal;
}

// The alternative build: a 360 degree positional servo, direct drive.
ServoCalibration direct() {
  ServoCalibration cal;
  cal.travelDeg = 360.0f;
  cal.gearRatio = 1.0f;
  return cal;
}

DialSpec spec() { return DialSpec{}; }

constexpr float kTol = 0.01f;

} // namespace

TEST(ServoMap, PulseEndpointsMatchCalibration) {
  const ServoCalibration cal = direct();
  CHECK_EQ(shaftDegToPulseUs(cal, 0.0f), 500);
  CHECK_EQ(shaftDegToPulseUs(cal, 360.0f), 2500);
  CHECK_EQ(shaftDegToPulseUs(cal, 180.0f), 1500);
}

TEST(ServoMap, PulseIsClampedToTravel) {
  const ServoCalibration cal = direct();
  CHECK_EQ(shaftDegToPulseUs(cal, -90.0f), 500);
  CHECK_EQ(shaftDegToPulseUs(cal, 720.0f), 2500);
}

TEST(ServoMap, GearRatioHalvesTheShaftAngle) {
  const ServoCalibration cal = geared();
  // 330 degrees of needle needs 165 degrees of shaft at 2:1.
  CHECK_NEAR(dialDegToShaftDeg(cal, 330.0f), 165.0f, kTol);
  CHECK_NEAR(dialDegToShaftDeg(cal, 0.0f), 0.0f, kTol);
  CHECK_NEAR(dialDegToShaftDeg(cal, 165.0f), 82.5f, kTol);
}

TEST(ServoMap, TrimOffsetsTheWholeSweep) {
  ServoCalibration cal = geared();
  cal.trimDeg = 7.5f;
  CHECK_NEAR(dialDegToShaftDeg(cal, 0.0f), 7.5f, kTol);
  CHECK_NEAR(dialDegToShaftDeg(cal, 330.0f), 172.5f, kTol);
  // Still inside a 180 degree servo's travel.
  CHECK_TRUE(dialFitsCalibration(cal, spec()));
}

TEST(ServoMap, ReversedServoMirrorsThePulse) {
  ServoCalibration cal = direct();
  cal.reversed = true;
  CHECK_EQ(shaftDegToPulseUs(cal, 0.0f), 2500);
  CHECK_EQ(shaftDegToPulseUs(cal, 360.0f), 500);
  CHECK_EQ(shaftDegToPulseUs(cal, 180.0f), 1500);
}

TEST(ServoMap, DefaultGearedBuildCoversTheDial) {
  CHECK_TRUE(dialFitsCalibration(geared(), spec()));
  CHECK_NEAR(requiredShaftDeg(geared(), spec()), 165.0f, kTol);
}

TEST(ServoMap, DirectDriveNeedsThreeHundredAndSixtyDegreeServo) {
  CHECK_TRUE(dialFitsCalibration(direct(), spec()));
  CHECK_NEAR(requiredShaftDeg(direct(), spec()), 330.0f, kTol);
}

TEST(ServoMap, PlainHobbyServoWithoutGearingIsRejected) {
  // This is the trap the whole design turns on: a 180 degree servo driven
  // directly cannot reach 11 o'clock, and must be caught at startup.
  ServoCalibration cal = direct();
  cal.travelDeg = 180.0f;
  cal.gearRatio = 1.0f;
  CHECK_FALSE(dialFitsCalibration(cal, spec()));
}

TEST(ServoMap, TrimThatPushesTheSweepPastTheEndStopIsRejected) {
  ServoCalibration cal = geared();
  cal.trimDeg = 20.0f; // 20 + 165 = 185 > 180
  CHECK_FALSE(dialFitsCalibration(cal, spec()));
}

TEST(ServoMap, NegativeTrimIsRejected) {
  ServoCalibration cal = geared();
  cal.trimDeg = -5.0f;
  CHECK_FALSE(dialFitsCalibration(cal, spec()));
}

TEST(ServoMap, RejectsUnusableCalibrations) {
  ServoCalibration cal = direct();
  CHECK_TRUE(calibrationIsValid(cal));

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

TEST(ServoMap, NonFiniteAngleFallsBackToTheTrimPosition) {
  ServoCalibration cal = geared();
  cal.trimDeg = 7.5f;
  const float nan = std::numeric_limits<float>::quiet_NaN();
  CHECK_NEAR(dialDegToShaftDeg(cal, nan), 7.5f, kTol);
}

TEST(ServoMap, WindMapsStraightToAPulseWidth) {
  const ServoCalibration cal = geared();
  // 0 km/h -> 0 deg shaft -> minimum pulse.
  CHECK_EQ(windKphToPulseUs(cal, spec(), 0.0f), 500);
  // 100 km/h -> 165 of 180 deg -> 500 + (165/180) * 2000 = 2333 us.
  CHECK_EQ(windKphToPulseUs(cal, spec(), 100.0f), 2333);
  // 50 km/h -> 82.5 of 180 deg -> 1417 us.
  CHECK_EQ(windKphToPulseUs(cal, spec(), 50.0f), 1417);
}

TEST(ServoMap, PulseWidthRisesWithWindAndStaysInRange) {
  const ServoCalibration cal = geared();
  uint16_t previous = 0;
  for (float kph = 0.0f; kph <= 100.0f; kph += 1.0f) {
    const uint16_t pulse = windKphToPulseUs(cal, spec(), kph);
    CHECK_TRUE(pulse >= previous);
    CHECK_TRUE(pulse >= cal.minPulseUs);
    CHECK_TRUE(pulse <= cal.maxPulseUs);
    previous = pulse;
  }
  CHECK_TRUE(previous > 2000);
}

TEST(ServoMap, OverspeedWindDoesNotDriveTheServoPastItsEndStop) {
  const ServoCalibration cal = geared();
  const uint16_t atFullScale = windKphToPulseUs(cal, spec(), 100.0f);
  CHECK_EQ(windKphToPulseUs(cal, spec(), 250.0f), atFullScale);
}

TEST(ServoMap, NarrowPulseRangeServoStillSpansTheDial) {
  // Some servos only honour 1000..2000 us.
  ServoCalibration cal = geared();
  cal.minPulseUs = 1000;
  cal.maxPulseUs = 2000;
  CHECK_EQ(windKphToPulseUs(cal, spec(), 0.0f), 1000);
  CHECK_EQ(windKphToPulseUs(cal, spec(), 100.0f), 1917); // 1000 + 165/180*1000
}
