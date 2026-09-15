// The headline requirement: 0 km/h at 9 o'clock, half scale at 12 o'clock,
// 100 km/h at 3 o'clock, travelling clockwise across the top of the face.
#include <cmath>
#include <limits>

#include "core/wind_dial.h"
#include "test_framework.h"

using namespace windclock;

namespace {
DialSpec spec() { return DialSpec{}; } // 0..100 km/h over -90..+90 deg
constexpr float kTol = 0.01f;
} // namespace

TEST(WindDial, CalmParksAtNineOClock) {
  const float deg = windToDialDeg(spec(), 0.0f);
  CHECK_NEAR(deg, -90.0f, kTol);
  CHECK_NEAR(dialDegToClockHour(deg), 9.0f, kTol);
}

TEST(WindDial, HalfScaleStandsStraightUpAtTwelve) {
  const float deg = windToDialDeg(spec(), 50.0f);
  CHECK_NEAR(deg, 0.0f, kTol);
  CHECK_NEAR(dialDegToClockHour(deg), 0.0f, kTol); // 0 hours == 12 o'clock
}

TEST(WindDial, HundredKphLandsOnThreeOClock) {
  const float deg = windToDialDeg(spec(), 100.0f);
  CHECK_NEAR(deg, 90.0f, kTol);
  CHECK_NEAR(dialDegToClockHour(deg), 3.0f, kTol);
}

TEST(WindDial, KnownWindSpeedsHitTheExpectedHours) {
  struct Sample { float kph; float hour; };
  // 1.8 degrees per km/h, so 10 km/h is 18 degrees, i.e. 0.6 of an hour.
  const Sample samples[] = {
      {0.0f, 9.0f},   {10.0f, 9.6f},  {25.0f, 10.5f},
      {50.0f, 12.0f}, {75.0f, 1.5f},  {100.0f, 3.0f},
  };
  for (const Sample &s : samples) {
    const float hour = dialDegToClockHour(windToDialDeg(spec(), s.kph));
    // 12 o'clock reads back as 0 hours.
    const float expected = (s.hour == 12.0f) ? 0.0f : s.hour;
    CHECK_NEAR(hour, expected, kTol);
  }
}

TEST(WindDial, NeedleAdvancesClockwiseWithWind) {
  float previous = -1000.0f;
  for (float kph = 0.0f; kph <= 100.0f; kph += 2.5f) {
    const float deg = windToDialDeg(spec(), kph);
    CHECK_TRUE(deg > previous);
    previous = deg;
  }
}

TEST(WindDial, TheSweepIsSymmetricAboutTwelveOClock) {
  // Anything below half scale is on the left of the face, anything above
  // is on the right, and equal distances from 50 km/h mirror each other.
  CHECK_TRUE(windToDialDeg(spec(), 20.0f) < 0.0f);
  CHECK_TRUE(windToDialDeg(spec(), 80.0f) > 0.0f);
  CHECK_NEAR(windToDialDeg(spec(), 20.0f), -windToDialDeg(spec(), 80.0f), kTol);
}

TEST(WindDial, StrongerThanScalePinsAtThreeOClock) {
  CHECK_NEAR(windToDialDeg(spec(), 140.0f), 90.0f, kTol);
  CHECK_NEAR(windToDialDeg(spec(), 1.0e6f), 90.0f, kTol);
  // The needle must never carry on past 3 and down the right-hand side.
  CHECK_NEAR(dialDegToClockHour(windToDialDeg(spec(), 140.0f)), 3.0f, kTol);
}

TEST(WindDial, NegativeWindPinsAtNineOClock) {
  CHECK_NEAR(windToDialDeg(spec(), -5.0f), -90.0f, kTol);
}

TEST(WindDial, NonFiniteWindPinsAtNineOClock) {
  // NaN and both infinities mean the reading is broken, not that a
  // hurricane arrived, so the needle goes to the calm end.
  const float nan = std::numeric_limits<float>::quiet_NaN();
  const float inf = std::numeric_limits<float>::infinity();
  CHECK_NEAR(windToDialDeg(spec(), nan), -90.0f, kTol);
  CHECK_NEAR(windToDialDeg(spec(), -inf), -90.0f, kTol);
  CHECK_NEAR(windToDialDeg(spec(), inf), -90.0f, kTol);
}

TEST(WindDial, AngleConvertsBackToWindSpeed) {
  for (float kph = 0.0f; kph <= 100.0f; kph += 7.0f) {
    const float roundTrip = dialDegToWindKph(spec(), windToDialDeg(spec(), kph));
    CHECK_NEAR(roundTrip, kph, 0.05f);
  }
}

TEST(WindDial, SweepIsOneHundredAndEightyDegrees) {
  CHECK_NEAR(dialSweepDeg(spec()), 180.0f, kTol);
  CHECK_NEAR(dialSweepDeg(spec()), 6.0f * kDegreesPerClockHour, kTol);
}

TEST(WindDial, RejectsUnusableSpecs) {
  CHECK_TRUE(dialSpecIsValid(spec()));

  DialSpec invertedWind = spec();
  invertedWind.maxWindKph = 0.0f;
  CHECK_FALSE(dialSpecIsValid(invertedWind));

  DialSpec flatDial = spec();
  flatDial.maxDialDeg = flatDial.minDialDeg;
  CHECK_FALSE(dialSpecIsValid(flatDial));

  // A dial written the wrong way round: use ServoCalibration::reversed
  // instead, so the shaft angle stays measured from the calm end.
  DialSpec descending = spec();
  descending.minDialDeg = 90.0f;
  descending.maxDialDeg = -90.0f;
  CHECK_FALSE(dialSpecIsValid(descending));

  DialSpec nanSpec = spec();
  nanSpec.maxWindKph = std::numeric_limits<float>::quiet_NaN();
  CHECK_FALSE(dialSpecIsValid(nanSpec));
}

TEST(WindDial, InvalidSpecFallsBackToTheCalmEnd) {
  DialSpec broken = spec();
  broken.maxWindKph = broken.minWindKph;
  CHECK_NEAR(windToDialDeg(broken, 50.0f), broken.minDialDeg, kTol);
}

TEST(WindDial, CustomScaleStillMaps) {
  // A 0..60 km/h dial over the same 9-to-3 sweep, for a sheltered spot.
  DialSpec gentle = spec();
  gentle.maxWindKph = 60.0f;
  CHECK_NEAR(windToDialDeg(gentle, 60.0f), 90.0f, kTol);
  CHECK_NEAR(dialDegToClockHour(windToDialDeg(gentle, 30.0f)), 0.0f, kTol);
}

TEST(WindDial, NarrowerSweepStillMaps) {
  // 10 o'clock to 2 o'clock, for a smaller face.
  DialSpec narrow = spec();
  narrow.minDialDeg = -60.0f;
  narrow.maxDialDeg = 60.0f;
  CHECK_TRUE(dialSpecIsValid(narrow));
  CHECK_NEAR(dialDegToClockHour(windToDialDeg(narrow, 0.0f)), 10.0f, kTol);
  CHECK_NEAR(dialDegToClockHour(windToDialDeg(narrow, 100.0f)), 2.0f, kTol);
  CHECK_NEAR(dialSweepDeg(narrow), 120.0f, kTol);
}

TEST(WindDial, ClockHourWrapsWithinTwelve) {
  CHECK_NEAR(dialDegToClockHour(0.0f), 0.0f, kTol);
  CHECK_NEAR(dialDegToClockHour(-90.0f), 9.0f, kTol);
  CHECK_NEAR(dialDegToClockHour(90.0f), 3.0f, kTol);
  CHECK_NEAR(dialDegToClockHour(360.0f), 0.0f, kTol);
  CHECK_NEAR(dialDegToClockHour(-30.0f), 11.0f, kTol);
}
