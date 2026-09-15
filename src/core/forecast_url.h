// Builds the Open-Meteo request URL.
//
// Kept apart from the networking so the exact query string can be checked
// in a test rather than by watching a device fail to parse a response.
#ifndef WINDCLOCK_CORE_FORECAST_URL_H
#define WINDCLOCK_CORE_FORECAST_URL_H

#include <cstddef>

#include "wind_forecast.h"

namespace windclock {

// Writes a NUL-terminated URL into `out`. Returns the number of characters
// written, or -1 if the arguments are unusable or the buffer is too small.
// Nothing partial is ever left in the buffer on failure.
int buildOpenMeteoUrl(char *out, size_t outSize, float latitude,
                      float longitude, const ForecastQuery &query);

} // namespace windclock

#endif // WINDCLOCK_CORE_FORECAST_URL_H
