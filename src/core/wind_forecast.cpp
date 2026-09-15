#include "wind_forecast.h"

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <limits>

namespace windclock {
namespace {

// Largest hourly array we will ever look at.
constexpr int kMaxSamples = 48;

const char *skipWhitespace(const char *p) {
  while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') ++p;
  return p;
}

// Finds `"key"` followed by a colon and an opening bracket, which is what
// tells the hourly arrays apart from the same names in "hourly_units",
// where the value is a unit string rather than an array.
const char *findArrayStart(const char *json, const char *key) {
  char quoted[64];
  const size_t keyLen = std::strlen(key);
  if (keyLen + 3 > sizeof(quoted)) return nullptr;
  quoted[0] = '"';
  std::memcpy(quoted + 1, key, keyLen);
  quoted[keyLen + 1] = '"';
  quoted[keyLen + 2] = '\0';

  const char *cursor = json;
  while ((cursor = std::strstr(cursor, quoted)) != nullptr) {
    const char *p = skipWhitespace(cursor + keyLen + 2);
    if (*p == ':') {
      p = skipWhitespace(p + 1);
      if (*p == '[') return p + 1;
    }
    cursor += keyLen + 2;
  }
  return nullptr;
}

} // namespace

float toKph(float value, WindUnit unit) {
  if (!std::isfinite(value)) return value;
  switch (unit) {
    case WindUnit::Mps: return value * 3.6f;
    case WindUnit::Mph: return value * 1.609344f;
    case WindUnit::Knots: return value * 1.852f;
    case WindUnit::Kph: break;
  }
  return value;
}

int readNumberArray(const char *json, const char *key, float *out,
                    int maxValues) {
  if (json == nullptr || key == nullptr || out == nullptr || maxValues <= 0) {
    return -1;
  }
  const char *p = findArrayStart(json, key);
  if (p == nullptr) return -1;

  int count = 0;
  p = skipWhitespace(p);
  if (*p == ']') return 0; // present but empty

  while (*p != '\0' && count < maxValues) {
    p = skipWhitespace(p);
    if (std::strncmp(p, "null", 4) == 0) {
      out[count++] = std::numeric_limits<float>::quiet_NaN();
      p += 4;
    } else {
      char *end = nullptr;
      const float value = std::strtof(p, &end);
      if (end == p) return count; // not a number: stop at whatever we have
      out[count++] = value;
      p = end;
    }
    p = skipWhitespace(p);
    if (*p == ',') {
      ++p;
      continue;
    }
    break; // ']' or a truncated response
  }
  return count;
}

ForecastResult parseOpenMeteo(const char *json, const ForecastQuery &query) {
  ForecastResult result;
  if (json == nullptr || query.horizonHours <= 0) return result;

  float values[kMaxSamples];
  int count = -1;

  if (query.useGusts) {
    count = readNumberArray(json, "wind_gusts_10m", values, kMaxSamples);
    if (count <= 0) {
      result.fellBackToSustained = true;
      count = -1;
    }
  }
  if (count < 0) {
    count = readNumberArray(json, "wind_speed_10m", values, kMaxSamples);
  }
  if (count <= 0) {
    result.fellBackToSustained = false;
    return result;
  }

  int limit = count;
  if (query.mode == ForecastMode::NextHour) {
    limit = 1;
  } else if (query.horizonHours < limit) {
    limit = query.horizonHours;
  }

  float best = 0.0f;
  int bestIndex = -1;
  int usable = 0;
  for (int i = 0; i < limit; ++i) {
    const float kph = toKph(values[i], query.unit);
    // A gap, a negative speed or a hurricane from outer space all mean the
    // sample is junk, not that the wind dropped to zero.
    if (!std::isfinite(kph) || kph < 0.0f || kph > kImplausibleWindKph) continue;
    ++usable;
    if (bestIndex < 0 || kph > best) {
      best = kph;
      bestIndex = i;
    }
  }

  if (usable == 0) {
    result.fellBackToSustained = false;
    return result;
  }

  result.valid = true;
  result.windKph = best;
  result.sampleCount = usable;
  result.chosenIndex = bestIndex;
  return result;
}

} // namespace windclock
