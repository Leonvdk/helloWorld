// Pulls the wind figure the needle should show out of an Open-Meteo
// forecast response.
//
// Deliberately not a general JSON parser: it looks for a named array of
// numbers and reads it. That keeps the firmware free of a JSON dependency
// and keeps this file testable on a laptop. The firmware asks Open-Meteo
// for `forecast_hours=N`, so index 0 is always the current hour.
#ifndef WINDCLOCK_CORE_WIND_FORECAST_H
#define WINDCLOCK_CORE_WIND_FORECAST_H

#include <cstddef>

namespace windclock {

enum class WindUnit { Kph, Mps, Mph, Knots };

enum class ForecastMode {
  NextHour,       // what it is doing right now
  MaxOverHorizon, // the worst it will get over the next horizonHours
};

// Anything above this is a corrupt response, not weather. The strongest
// gust ever recorded at the surface is about 408 km/h.
constexpr float kImplausibleWindKph = 500.0f;

struct ForecastQuery {
  ForecastMode mode = ForecastMode::MaxOverHorizon;
  int horizonHours = 12;
  // Show gusts rather than the sustained wind. Falls back to sustained
  // wind if the response carries no gust array.
  bool useGusts = false;
  WindUnit unit = WindUnit::Kph;
};

struct ForecastResult {
  bool valid = false;
  float windKph = 0.0f;
  // How many usable samples were found in the horizon.
  int sampleCount = 0;
  // Index of the sample that won, -1 when invalid.
  int chosenIndex = -1;
  // True when gusts were asked for but the response only had sustained wind.
  bool fellBackToSustained = false;
};

float toKph(float value, WindUnit unit);

// Reads up to maxValues numbers from the array named `key`. Returns the
// number of entries read, or -1 if there is no such array. JSON nulls are
// written as NaN so the caller can tell a gap from a zero.
int readNumberArray(const char *json, const char *key, float *out,
                    int maxValues);

ForecastResult parseOpenMeteo(const char *json, const ForecastQuery &query);

} // namespace windclock

#endif // WINDCLOCK_CORE_WIND_FORECAST_H
