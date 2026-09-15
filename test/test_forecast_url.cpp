// The request URL, checked here rather than on the device.
#include <cstring>
#include <limits>
#include <string>

#include "core/forecast_url.h"
#include "test_framework.h"

using namespace windclock;

namespace {

std::string build(float lat, float lon, const ForecastQuery &q,
                  size_t bufferSize = 256) {
  std::string buffer(bufferSize, '\0');
  const int written = buildOpenMeteoUrl(&buffer[0], bufferSize, lat, lon, q);
  if (written < 0) return std::string("<error>");
  buffer.resize(static_cast<size_t>(written));
  return buffer;
}

bool contains(const std::string &haystack, const char *needle) {
  return haystack.find(needle) != std::string::npos;
}

} // namespace

TEST(ForecastUrl, BuildsTheExpectedRequest) {
  ForecastQuery q;
  q.horizonHours = 12;
  const std::string url = build(37.3167f, -8.8f, q);
  CHECK_STREQ(url,
              "https://api.open-meteo.com/v1/forecast"
              "?latitude=37.3167&longitude=-8.8000"
              "&hourly=wind_speed_10m"
              "&wind_speed_unit=kmh"
              "&forecast_hours=12"
              "&timeformat=unixtime");
}

TEST(ForecastUrl, PinsTheUnitsSoTheParserCanTrustThem) {
  ForecastQuery q;
  CHECK_TRUE(contains(build(52.0f, 4.0f, q), "wind_speed_unit=kmh"));
}

TEST(ForecastUrl, AsksForTheCurrentHourOnwards) {
  // forecast_hours is what makes index 0 the current hour.
  ForecastQuery q;
  q.horizonHours = 6;
  CHECK_TRUE(contains(build(52.0f, 4.0f, q), "forecast_hours=6"));
}

TEST(ForecastUrl, RequestsGustsOnlyWhenTheyAreWanted) {
  ForecastQuery sustained;
  CHECK_FALSE(contains(build(52.0f, 4.0f, sustained), "wind_gusts_10m"));

  ForecastQuery gusty;
  gusty.useGusts = true;
  CHECK_TRUE(contains(build(52.0f, 4.0f, gusty),
                      "hourly=wind_speed_10m,wind_gusts_10m"));
}

TEST(ForecastUrl, HandlesSouthernAndWesternCoordinates) {
  ForecastQuery q;
  const std::string url = build(-33.8688f, 151.2093f, q);
  CHECK_TRUE(contains(url, "latitude=-33.8688"));
  CHECK_TRUE(contains(url, "longitude=151.2093"));
}

TEST(ForecastUrl, RejectsOutOfRangeCoordinates) {
  ForecastQuery q;
  char buffer[256];
  CHECK_EQ(buildOpenMeteoUrl(buffer, sizeof(buffer), 91.0f, 0.0f, q), -1);
  CHECK_EQ(buildOpenMeteoUrl(buffer, sizeof(buffer), 0.0f, 181.0f, q), -1);
  CHECK_EQ(buildOpenMeteoUrl(buffer, sizeof(buffer), -90.1f, 0.0f, q), -1);
}

TEST(ForecastUrl, RejectsNonFiniteCoordinates) {
  ForecastQuery q;
  char buffer[256];
  const float nan = std::numeric_limits<float>::quiet_NaN();
  CHECK_EQ(buildOpenMeteoUrl(buffer, sizeof(buffer), nan, 0.0f, q), -1);
}

TEST(ForecastUrl, RejectsAnUnusableHorizon) {
  char buffer[256];
  ForecastQuery zero;
  zero.horizonHours = 0;
  CHECK_EQ(buildOpenMeteoUrl(buffer, sizeof(buffer), 52.0f, 4.0f, zero), -1);

  ForecastQuery tooFar;
  tooFar.horizonHours = 100;
  CHECK_EQ(buildOpenMeteoUrl(buffer, sizeof(buffer), 52.0f, 4.0f, tooFar), -1);
}

TEST(ForecastUrl, RefusesToTruncateIntoASmallBuffer) {
  ForecastQuery q;
  char buffer[32];
  CHECK_EQ(buildOpenMeteoUrl(buffer, sizeof(buffer), 52.0f, 4.0f, q), -1);
  // Nothing half-written is left behind for the caller to fetch.
  CHECK_EQ(std::strlen(buffer), 0u);
}

TEST(ForecastUrl, RejectsANullBuffer) {
  ForecastQuery q;
  CHECK_EQ(buildOpenMeteoUrl(nullptr, 256, 52.0f, 4.0f, q), -1);
}

TEST(ForecastUrl, ReturnsTheWrittenLength) {
  ForecastQuery q;
  char buffer[256];
  const int written = buildOpenMeteoUrl(buffer, sizeof(buffer), 52.0f, 4.0f, q);
  CHECK_TRUE(written > 0);
  CHECK_EQ(static_cast<size_t>(written), std::strlen(buffer));
}
