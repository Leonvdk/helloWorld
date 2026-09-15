// Prints the whole dial as a table: wind speed, needle angle, the position
// on the clock face and the servo pulse that gets it there.
//
// Useful when marking up the face and when checking a calibration change
// without flashing anything. Build and run it with `make table`.
#include <cstdio>
#include <string>

#include "config.h"
#include "core/servo_map.h"
#include "core/wind_dial.h"

using namespace windclock;

namespace {

// Returns by value: two labels are printed on the same line, so a shared
// static buffer would show the same time twice.
std::string clockFaceLabel(float hour) {
  int whole = static_cast<int>(hour);
  int minutes =
      static_cast<int>((hour - static_cast<float>(whole)) * 60.0f + 0.5f);
  if (minutes == 60) {
    minutes = 0;
    ++whole;
  }
  if (whole == 0) whole = 12;
  char label[16];
  std::snprintf(label, sizeof(label), "%2d:%02d", whole, minutes);
  return label;
}

} // namespace

int main() {
  std::printf("Wind clock dial\n");
  std::printf("  scale      %.0f - %.0f km/h\n", config::kDial.minWindKph,
              config::kDial.maxWindKph);
  std::printf("  sweep      %.0f deg (%s to %s on the face)\n",
              dialSweepDeg(config::kDial),
              clockFaceLabel(dialDegToClockHour(config::kDial.minDialDeg)).c_str(),
              clockFaceLabel(dialDegToClockHour(config::kDial.maxDialDeg)).c_str());
  std::printf("  servo      %.0f deg travel, %.2f:1 gearing, %.1f deg trim\n",
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
                clockFaceLabel(dialDegToClockHour(deg)).c_str(),
                dialDegToShaftDeg(config::kServo, config::kDial, deg),
                windKphToPulseUs(config::kServo, config::kDial, kph));
  }
  return 0;
}
