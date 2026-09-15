// Prints the whole dial as a table: wind speed, needle angle, the position
// on the clock face and the servo pulse that gets it there.
//
// Useful when marking up the face and when checking a calibration change
// without flashing anything. Build and run it with `make table`.
#include <cstdio>

#include "config.h"
#include "core/servo_map.h"
#include "core/wind_dial.h"

using namespace windclock;

namespace {

const char *clockFaceLabel(float hour) {
  static char label[16];
  int whole = static_cast<int>(hour);
  const int minutes = static_cast<int>((hour - static_cast<float>(whole)) * 60.0f + 0.5f);
  if (whole == 0) whole = 12;
  std::snprintf(label, sizeof(label), "%2d:%02d", whole, minutes);
  return label;
}

} // namespace

int main() {
  std::printf("Wind clock dial\n");
  std::printf("  scale      %.0f - %.0f km/h\n", config::kDial.minWindKph,
              config::kDial.maxWindKph);
  std::printf("  sweep      %.0f deg (%.0f o'clock to %.0f o'clock)\n",
              dialSweepDeg(config::kDial),
              dialDegToClockHour(config::kDial.minDialDeg),
              dialDegToClockHour(config::kDial.maxDialDeg));
  std::printf("  servo      %.0f deg travel, %.1f:1 gearing, %.1f deg trim\n",
              config::kServo.travelDeg, config::kServo.gearRatio,
              config::kServo.trimDeg);
  std::printf("  reachable  %s\n\n",
              dialFitsCalibration(config::kServo, config::kDial) ? "yes"
                                                                 : "NO -- fix config.h");

  std::printf("  km/h    needle     face    shaft     pulse\n");
  std::printf("  ----    ------    -----    -----    ------\n");
  for (float kph = 0.0f; kph <= config::kDial.maxWindKph + 0.01f; kph += 5.0f) {
    const float deg = windToDialDeg(config::kDial, kph);
    std::printf("  %4.0f    %5.1f\u00b0    %s    %5.1f    %4u us\n", kph, deg,
                clockFaceLabel(dialDegToClockHour(deg)),
                dialDegToShaftDeg(config::kServo, deg),
                windKphToPulseUs(config::kServo, config::kDial, kph));
  }
  return 0;
}
