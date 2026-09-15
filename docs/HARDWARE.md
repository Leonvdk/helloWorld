# Hardware

## The 330-degree problem

The dial asks for 0 km/h at 12 o'clock and 100 km/h at 11 o'clock, going
clockwise. That is **330 degrees of needle travel**, and an ordinary hobby
servo only turns 180. Reaching the whole scale needs one of these:

| Build | `travelDeg` | `gearRatio` | Notes |
|---|---|---|---|
| **180-degree servo, geared 2:1** *(shipped default)* | `180` | `2.0` | 165 degrees of shaft becomes 330 of needle. Cheapest, and an SG90/MG90S is plenty. Costs you half the angular resolution and adds backlash. |
| 270-degree servo, geared 1.25:1 | `270` | `1.25` | 264 shaft degrees. Less gearing, less backlash. |
| 360-degree **positional** servo, direct | `360` | `1.0` | Simplest mechanically, no gear train. Must be a *positional* 360 servo (e.g. a sail-winch servo or a DS3218-360), **not** a continuous-rotation servo — those take a speed, not an angle, and cannot hold a position. |

`dialFitsCalibration()` checks the configured servo can actually reach both
ends of the scale, and the firmware refuses to run if it cannot, rather than
showing a needle silently stuck at 80 km/h.

### Gearing it 2:1

Two GT2 pulleys, 20 tooth on the servo and 40 tooth on the needle shaft,
with a short belt. Anything with a 2:1 ratio and little backlash works —
spur gears are fine too, but reverse the needle direction if you use a
single pair (set `reversed = true`), since one meshing pair reverses the
rotation.

## Parts

- ESP32 dev board — a low-quiescent-current one. A plain DevKitC wastes
  most of the battery on its regulator and USB-serial chip; a board meant
  for battery use (FireBeetle, TinyPICO, LOLIN32 Lite) sleeps at tens of
  microamps instead of milliamps.
- Servo — see the table above.
- 1S LiPo, 2000 mAh or more, or 3x AA with a boost converter.
- P-channel MOSFET or a load switch for the servo rail (e.g. AO3401 plus a
  small N-channel to drive the gate from 3.3 V logic).
- Two resistors for the battery divider (100k/100k) and an N-channel MOSFET
  to ground the bottom of it, so the divider is not a permanent 16 uA load.
- A clock face marked 0 to 100, and a needle.

## Wiring

| Signal | GPIO | Notes |
|---|---|---|
| Servo PWM | 18 | Straight to the servo's signal wire. |
| Servo power enable | 25 | Gate of the load switch feeding the servo's V+. Must be an RTC-capable GPIO — `gpio_hold_en` only holds the level through deep sleep on those. |
| Battery sense | 34 | Midpoint of the divider. Must be an ADC1 pin — ADC2 does not work while WiFi is on. |
| Battery sense enable | 26 | Grounds the bottom of the divider only while measuring. Also RTC-capable, for the same reason. |

Give the load switch's gate a pull resistor to the off state, so the servo
rail stays dead while the ESP32 is in reset and its pins are floating.

The servo runs from the battery, **not** from the ESP32's 3.3 V regulator —
a stalled servo pulls an amp or more and will brown out the board mid-WiFi.
Put a 470 uF capacitor across the servo's supply, close to the servo.

Grounds must be common between the battery, the ESP32 and the servo.

## Power budget

Roughly, for a 2000 mAh cell and the shipped 30-minute update interval:

| State | Current | Time per day |
|---|---|---|
| Deep sleep | ~20 uA (a good board) | ~23.9 h |
| WiFi + fetch | ~120 mA | ~4 min (48 wakes x ~5 s) |
| Servo moving | ~300 mA average | well under a minute |

That comes to about 9 mAh a day, so a couple of months per charge. The
things that actually decide it are the board's sleep current and how often
the needle is allowed to move — which is what the deadband in
`NeedlePolicy` is for.

Stretch `normalSleepSeconds` to an hour and it roughly doubles. Wind
forecasts are hourly anyway, so half-hourly updates are already generous.

## Calibration

1. Set `travelDeg` and `gearRatio` for your build in `src/config.h`.
2. Run `make table` to see where each wind speed lands and what pulse gets
   it there.
3. Fit the needle at 12 o'clock with the servo commanded to 0 km/h.
4. Adjust `trimDeg` to take up the slack. The shipped 7.5 degrees centres
   the 165-degree sweep inside a 180-degree servo's travel, leaving margin
   at both end stops.
5. If the needle runs anticlockwise, set `reversed = true`.
