#include "forecast_url.h"

#include <cmath>
#include <cstdio>

namespace windclock {

int buildOpenMeteoUrl(char *out, size_t outSize, float latitude,
                      float longitude, const ForecastQuery &query) {
  if (out == nullptr || outSize == 0) return -1;
  if (!std::isfinite(latitude) || !std::isfinite(longitude)) return -1;
  if (latitude < -90.0f || latitude > 90.0f) return -1;
  if (longitude < -180.0f || longitude > 180.0f) return -1;
  if (query.horizonHours <= 0 || query.horizonHours > 48) return -1;

  // forecast_hours makes index 0 the current hour, which is what the
  // parser assumes. wind_speed_unit pins the units so the response cannot
  // quietly switch to m/s on us.
  const char *fields =
      query.useGusts ? "wind_speed_10m,wind_gusts_10m" : "wind_speed_10m";

  const int written = std::snprintf(
      out, outSize,
      "https://api.open-meteo.com/v1/forecast"
      "?latitude=%.4f&longitude=%.4f"
      "&hourly=%s"
      "&wind_speed_unit=kmh"
      "&forecast_hours=%d"
      "&timeformat=unixtime",
      static_cast<double>(latitude), static_cast<double>(longitude), fields,
      query.horizonHours);

  if (written < 0 || static_cast<size_t>(written) >= outSize) {
    out[0] = '\0';
    return -1;
  }
  return written;
}

} // namespace windclock
