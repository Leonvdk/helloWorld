// Parsing the Open-Meteo response, including the ways it can go wrong.
#include <cmath>

#include "core/wind_forecast.h"
#include "test_framework.h"

using namespace windclock;

namespace {

const char *kTypicalResponse = R"({
  "latitude": 37.3,
  "longitude": -8.8,
  "generationtime_ms": 0.12,
  "utc_offset_seconds": 0,
  "hourly_units": {"time": "unixtime", "wind_speed_10m": "km/h"},
  "hourly": {
    "time": [1757894400, 1757898000, 1757901600, 1757905200],
    "wind_speed_10m": [12.4, 18.9, 31.7, 27.2]
  }
})";

const char *kWithGusts = R"({
  "hourly_units": {"wind_speed_10m": "km/h", "wind_gusts_10m": "km/h"},
  "hourly": {
    "wind_speed_10m": [12.4, 18.9, 31.7],
    "wind_gusts_10m": [20.1, 34.6, 58.3]
  }
})";

ForecastQuery maxOver(int hours) {
  ForecastQuery q;
  q.mode = ForecastMode::MaxOverHorizon;
  q.horizonHours = hours;
  return q;
}

constexpr float kTol = 0.01f;

} // namespace

TEST(WindForecast, ReadsTheStrongestHourInTheHorizon) {
  const ForecastResult r = parseOpenMeteo(kTypicalResponse, maxOver(12));
  CHECK_TRUE(r.valid);
  CHECK_NEAR(r.windKph, 31.7f, kTol);
  CHECK_EQ(r.chosenIndex, 2);
  CHECK_EQ(r.sampleCount, 4);
}

TEST(WindForecast, HorizonLimitsHowFarAheadItLooks) {
  // Only the first two hours: the 31.7 spike is beyond the horizon.
  const ForecastResult r = parseOpenMeteo(kTypicalResponse, maxOver(2));
  CHECK_TRUE(r.valid);
  CHECK_NEAR(r.windKph, 18.9f, kTol);
  CHECK_EQ(r.sampleCount, 2);
}

TEST(WindForecast, NextHourModeTakesTheFirstSample) {
  ForecastQuery q;
  q.mode = ForecastMode::NextHour;
  const ForecastResult r = parseOpenMeteo(kTypicalResponse, q);
  CHECK_TRUE(r.valid);
  CHECK_NEAR(r.windKph, 12.4f, kTol);
  CHECK_EQ(r.chosenIndex, 0);
  CHECK_EQ(r.sampleCount, 1);
}

TEST(WindForecast, HorizonLongerThanTheResponseUsesWhatIsThere) {
  const ForecastResult r = parseOpenMeteo(kTypicalResponse, maxOver(48));
  CHECK_TRUE(r.valid);
  CHECK_NEAR(r.windKph, 31.7f, kTol);
  CHECK_EQ(r.sampleCount, 4);
}

TEST(WindForecast, GustsArePreferredWhenAskedFor) {
  ForecastQuery q = maxOver(12);
  q.useGusts = true;
  const ForecastResult r = parseOpenMeteo(kWithGusts, q);
  CHECK_TRUE(r.valid);
  CHECK_NEAR(r.windKph, 58.3f, kTol);
  CHECK_FALSE(r.fellBackToSustained);
}

TEST(WindForecast, MissingGustsFallBackToSustainedWind) {
  ForecastQuery q = maxOver(12);
  q.useGusts = true;
  const ForecastResult r = parseOpenMeteo(kTypicalResponse, q);
  CHECK_TRUE(r.valid);
  CHECK_NEAR(r.windKph, 31.7f, kTol);
  CHECK_TRUE(r.fellBackToSustained);
}

TEST(WindForecast, UnitStringsInHourlyUnitsAreNotMistakenForData) {
  // "wind_speed_10m" appears twice: once as a unit string, once as the
  // array. Only the array is data.
  const ForecastResult r = parseOpenMeteo(kTypicalResponse, maxOver(12));
  CHECK_TRUE(r.valid);
  CHECK_NEAR(r.windKph, 31.7f, kTol);
}

TEST(WindForecast, NullSamplesAreSkipped) {
  const char *json = R"({"hourly":{"wind_speed_10m":[null,22.5,null,19.0]}})";
  const ForecastResult r = parseOpenMeteo(json, maxOver(12));
  CHECK_TRUE(r.valid);
  CHECK_NEAR(r.windKph, 22.5f, kTol);
  CHECK_EQ(r.sampleCount, 2);
  CHECK_EQ(r.chosenIndex, 1);
}

TEST(WindForecast, AllNullSamplesAreRejected) {
  const char *json = R"({"hourly":{"wind_speed_10m":[null,null,null]}})";
  CHECK_FALSE(parseOpenMeteo(json, maxOver(12)).valid);
}

TEST(WindForecast, NullInTheCurrentHourFailsNextHourMode) {
  const char *json = R"({"hourly":{"wind_speed_10m":[null,22.5]}})";
  ForecastQuery q;
  q.mode = ForecastMode::NextHour;
  CHECK_FALSE(parseOpenMeteo(json, q).valid);
}

TEST(WindForecast, EmptyArrayIsRejected) {
  const char *json = R"({"hourly":{"wind_speed_10m":[]}})";
  CHECK_FALSE(parseOpenMeteo(json, maxOver(12)).valid);
}

TEST(WindForecast, MissingArrayIsRejected) {
  const char *json = R"({"hourly":{"temperature_2m":[14.0,15.0]}})";
  CHECK_FALSE(parseOpenMeteo(json, maxOver(12)).valid);
}

TEST(WindForecast, ApiErrorPayloadIsRejected) {
  const char *json =
      R"({"error":true,"reason":"Value cannot be negative for latitude"})";
  CHECK_FALSE(parseOpenMeteo(json, maxOver(12)).valid);
}

TEST(WindForecast, EmptyAndNullInputAreRejected) {
  CHECK_FALSE(parseOpenMeteo("", maxOver(12)).valid);
  CHECK_FALSE(parseOpenMeteo(nullptr, maxOver(12)).valid);
  CHECK_FALSE(parseOpenMeteo("not json at all", maxOver(12)).valid);
}

TEST(WindForecast, TruncatedResponseUsesTheSamplesThatArrived) {
  // A connection dropped mid-array. What did arrive is still usable.
  const char *json = R"({"hourly":{"wind_speed_10m":[11.0,42.0,17.)";
  const ForecastResult r = parseOpenMeteo(json, maxOver(12));
  CHECK_TRUE(r.valid);
  CHECK_NEAR(r.windKph, 42.0f, kTol);
}

TEST(WindForecast, NegativeWindSpeedsAreRejectedAsCorrupt) {
  const char *json = R"({"hourly":{"wind_speed_10m":[-5.0,-2.0]}})";
  CHECK_FALSE(parseOpenMeteo(json, maxOver(12)).valid);
}

TEST(WindForecast, ImplausibleWindSpeedsAreIgnored) {
  const char *json = R"({"hourly":{"wind_speed_10m":[14.0,99999.0]}})";
  const ForecastResult r = parseOpenMeteo(json, maxOver(12));
  CHECK_TRUE(r.valid);
  CHECK_NEAR(r.windKph, 14.0f, kTol);
  CHECK_EQ(r.sampleCount, 1);
}

TEST(WindForecast, ZeroOrNegativeHorizonIsRejected) {
  CHECK_FALSE(parseOpenMeteo(kTypicalResponse, maxOver(0)).valid);
  CHECK_FALSE(parseOpenMeteo(kTypicalResponse, maxOver(-3)).valid);
}

TEST(WindForecast, ScientificNotationAndIntegersParse) {
  const char *json = R"({"hourly":{"wind_speed_10m":[1.2e1,25,3.0E0]}})";
  const ForecastResult r = parseOpenMeteo(json, maxOver(12));
  CHECK_TRUE(r.valid);
  CHECK_NEAR(r.windKph, 25.0f, kTol);
  CHECK_EQ(r.sampleCount, 3);
}

TEST(WindForecast, WhitespaceAndNewlinesAreTolerated) {
  const char *json = "{\n \"hourly\" : {\n  \"wind_speed_10m\" : [\n"
                     "    11.0 ,\n    29.5 ,\n    8.0\n  ]\n }\n}";
  const ForecastResult r = parseOpenMeteo(json, maxOver(12));
  CHECK_TRUE(r.valid);
  CHECK_NEAR(r.windKph, 29.5f, kTol);
}

TEST(WindForecast, ConvertsMetresPerSecondToKph) {
  const char *json = R"({"hourly":{"wind_speed_10m":[10.0]}})";
  ForecastQuery q = maxOver(12);
  q.unit = WindUnit::Mps;
  const ForecastResult r = parseOpenMeteo(json, q);
  CHECK_TRUE(r.valid);
  CHECK_NEAR(r.windKph, 36.0f, kTol);
}

TEST(WindForecast, UnitConversionsAreCorrect) {
  CHECK_NEAR(toKph(1.0f, WindUnit::Kph), 1.0f, kTol);
  CHECK_NEAR(toKph(1.0f, WindUnit::Mps), 3.6f, kTol);
  CHECK_NEAR(toKph(1.0f, WindUnit::Mph), 1.609344f, kTol);
  CHECK_NEAR(toKph(1.0f, WindUnit::Knots), 1.852f, kTol);
  // A gale-force 40 knots is a little over 74 km/h.
  CHECK_NEAR(toKph(40.0f, WindUnit::Knots), 74.08f, kTol);
}

TEST(WindForecast, ReadsRawArraysDirectly) {
  float values[8] = {0};
  const int count =
      readNumberArray(kTypicalResponse, "wind_speed_10m", values, 8);
  CHECK_EQ(count, 4);
  CHECK_NEAR(values[0], 12.4f, kTol);
  CHECK_NEAR(values[3], 27.2f, kTol);
}

TEST(WindForecast, RawArrayReadReportsMissingKeys) {
  float values[4] = {0};
  CHECK_EQ(readNumberArray(kTypicalResponse, "nope_10m", values, 4), -1);
  CHECK_EQ(readNumberArray(kTypicalResponse, "wind_speed_10m", values, 0), -1);
  CHECK_EQ(readNumberArray(nullptr, "wind_speed_10m", values, 4), -1);
}

TEST(WindForecast, RawArrayReadStopsAtTheBufferLimit) {
  float values[2] = {0};
  CHECK_EQ(readNumberArray(kTypicalResponse, "wind_speed_10m", values, 2), 2);
  CHECK_NEAR(values[1], 18.9f, kTol);
}

TEST(WindForecast, RawArrayReadMarksNullsAsNotANumber) {
  const char *json = R"({"hourly":{"wind_speed_10m":[null,3.0]}})";
  float values[2] = {0};
  CHECK_EQ(readNumberArray(json, "wind_speed_10m", values, 2), 2);
  CHECK_TRUE(std::isnan(values[0]));
  CHECK_NEAR(values[1], 3.0f, kTol);
}
