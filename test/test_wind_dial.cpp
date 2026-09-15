// The headline requirement: 0 km/h at 12 o'clock, 100 km/h at 11 o'clock.
#include <cmath>
#include <limits>

#include "core/wind_dial.h"
#include "test_framework.h"

using namespace windclock;

namespace {
DialSpec spec() { return DialSpec{}; } // 0..100 km/h over 0..330 deg
constexpr float kTol = 0.01f;
} // namespace

TEST(WindDial, CalmParksAtTwelveOClock) {
  CHECK_NEAR(windToDialDeg(spec(), 0.0f), 0.0f, kTol);
  CHECK_NEAR(dialDegToClockHour(windToDialDeg(spec(), 0.0f)), 0.0f, kTol);
}

TEST(WindDial, HundredKphLandsOnElevenOClock) {
  const float deg = windToDialDeg(spec(), 100.0f);
  CHECK_NEAR(deg, 330.0f, kTol);
  CHECK_NEAR(dialDegToClockHour(deg), 11.0f, kTol);
}

TEST(WindDial, HalfScaleSitsAtHalfPastFive) {
  // 50 km/h is halfway round a 330 degree sweep: 165 degrees, which is
  // five and a half hours past twelve.
  const float deg = windToDialDeg(spec(), 50.0f);
  CHECK_NEAR(deg, 165.0f, kTol);
  CHECK_NEAR(dialDegToClockHour(deg), 5.5f, kTol);
}

TEST(WindDial, KnownWindSpeedsHitTheExpectedHours) {
  struct Sample { float kph; float hour; };
  // 10 km/h per 33 degrees, i.e. 1.1 hours on the face.
  const Sample samples[] = {
      {0.0f, 0.0f},  {10.0f, 1.1f},  {25.0f, 2.75f},
      {40.0f, 4.4f}, {75.0f, 8.25f}, {100.0f, 11.0f},
  };
  for (const Sample &s : samples) {
    CHECK_NEAR(dialDegToClockHour(windToDialDeg(spec(), s.kph)), s.hour, kTol);
  }
}

TEST(WindDial, NeedleAdvancesClockwiseWithWind) {
  float previous = -1.0f;
  for (float kph = 0.0f; kph <= 100.0f; kph += 2.5f) {
    const float deg = windToDialDeg(spec(), kph);
    CHECK_TRUE(deg > previous);
    previous = deg;
  }
}

TEST(WindDial, StrongerThanScalePinsAtElevenOClock) {
  CHECK_NEAR(windToDialDeg(spec(), 140.0f), 330.0f, kTol);
  CHECK_NEAR(windToDialDeg(spec(), 1.0e6f), 330.0f, kTol);
  // The needle must never wrap past 11 back towards 12 and read as calm.
  CHECK_TRUE(dialDegToClockHour(windToDialDeg(spec(), 140.0f)) > 10.0f);
}

TEST(WindDial, NegativeWindPinsAtTwelveOClock) {
  CHECK_NEAR(windToDialDeg(spec(), -5.0f), 0.0f, kTol);
}

TEST(WindDial, NonFiniteWindPinsAtTwelveOClock) {
  // NaN and both infinities mean the reading is broken, not that a
  // hurricane arrived, so the needle goes to the calm end rather than
  // slamming into the 11 o'clock stop.
  const float nan = std::numeric_limits<float>::quiet_NaN();
  const float inf = std::numeric_limits<float>::infinity();
  CHECK_NEAR(windToDialDeg(spec(), nan), 0.0f, kTol);
  CHECK_NEAR(windToDialDeg(spec(), -inf), 0.0f, kTol);
  CHECK_NEAR(windToDialDeg(spec(), inf), 0.0f, kTol);
}

TEST(WindDial, AngleConvertsBackToWindSpeed) {
  for (float kph = 0.0f; kph <= 100.0f; kph += 7.0f) {
    const float roundTrip = dialDegToWindKph(spec(), windToDialDeg(spec(), kph));
    CHECK_NEAR(roundTrip, kph, 0.05f);
  }
}

TEST(WindDial, SweepIsThreeHundredAndThirtyDegrees) {
  CHECK_NEAR(dialSweepDeg(spec()), 330.0f, kTol);
  CHECK_NEAR(dialSweepDeg(spec()), 11.0f * kDegreesPerClockHour, kTol);
}

TEST(WindDial, RejectsUnusableSpecs) {
  DialSpec inverted = spec();
  inverted.maxWindKph = 0.0f;
  CHECK_FALSE(dialSpecIsValid(inverted));

  DialSpec flatDial = spec();
  flatDial.maxDialDeg = flatDial.minDialDeg;
  CHECK_FALSE(dialSpecIsValid(flatDial));

  DialSpec nanSpec = spec();
  nanSpec.maxWindKph = std::numeric_limits<float>::quiet_NaN();
  CHECK_FALSE(dialSpecIsValid(nanSpec));

  CHECK_TRUE(dialSpecIsValid(spec()));
}

TEST(WindDial, InvalidSpecFallsBackToTwelveOClock) {
  DialSpec broken = spec();
  broken.maxWindKph = broken.minWindKph;
  CHECK_NEAR(windToDialDeg(broken, 50.0f), broken.minDialDeg, kTol);
}

TEST(WindDial, CustomScaleStillMaps) {
  // A 0..60 km/h dial ending at 9 o'clock, for a sheltered spot.
  DialSpec gentle;
  gentle.maxWindKph = 60.0f;
  gentle.maxDialDeg = 9.0f * kDegreesPerClockHour;
  CHECK_NEAR(windToDialDeg(gentle, 60.0f), 270.0f, kTol);
  CHECK_NEAR(dialDegToClockHour(windToDialDeg(gentle, 30.0f)), 4.5f, kTol);
}

TEST(WindDial, ClockHourWrapsWithinTwelve) {
  CHECK_NEAR(dialDegToClockHour(0.0f), 0.0f, kTol);
  CHECK_NEAR(dialDegToClockHour(360.0f), 0.0f, kTol);
  CHECK_NEAR(dialDegToClockHour(390.0f), 1.0f, kTol);
  CHECK_NEAR(dialDegToClockHour(-30.0f), 11.0f, kTol);
}
