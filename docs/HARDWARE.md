# Hardware

## The servo

The dial runs 0 km/h at 9 o'clock, up through 12 o'clock at half scale, to
100 km/h at 3 o'clock: **180 degrees of needle travel**, which is exactly
what a standard hobby servo turns. The servo horn goes straight on the
needle shaft — no gears, no belt.

| Build | `travelDeg` | `gearRatio` | `trimDeg` | Notes |
|---|---|---|---|---|
| **180-degree servo, direct** *(shipped default)* | `180` | `1.0` | `0` | An SG90 or MG90S does it. Uses the servo's entire travel, so there is no trim margin — see below. |
| 270-degree servo, direct | `270` | `1.0` | `45` | The comfortable option. The sweep sits in the middle of the travel with 45 degrees spare at each end, so assembly slop and a servo that over- or under-travels are both absorbed by trim. |
| Geared, any servo | your travel | ratio | to suit | Still supported. `gearRatio` is needle degrees per shaft degree, so a 2:1 step-up is `2.0`. |

`dialFitsCalibration()` checks the configured servo can actually reach both
ends of the scale, and the firmware refuses to run if it cannot, rather than
showing a needle silently stuck at 80 km/h.

### About that missing trim margin

A nominal "180 degree" servo rarely turns exactly 180, and 500–2500 µs may
drive it a little past or a little short. With the shipped config the sweep
consumes the whole travel, so there is nothing left over to trim with. If
the needle can't quite reach 9 or 3:

- widen `minPulseUs` / `maxPulseUs` (many servos accept 400–2600), or
- measure what the servo really turns, set `travelDeg` to that, and accept
  a scale compressed by a few degrees, or
- fit a 270-degree servo and use the row above.

If the needle runs backwards — 100 km/h at 9 o'clock — set `reversed = true`
rather than flipping the dial angles around.

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

1. Set `travelDeg` (and `gearRatio`, if you geared it) for your build in
   `src/config.h`.
2. Run `make table` to see where each wind speed lands and what pulse gets
   it there.
3. Command 0 km/h — 500 µs with the shipped config — and fit the needle
   pointing at 9 o'clock.
4. Command 100 km/h and check it reaches 3 o'clock. If it falls short or
   runs into the end stop, see "About that missing trim margin" above.
5. On a servo with travel to spare, `trimDeg` shifts the whole sweep round
   to line the zero up exactly.
6. If the needle runs anticlockwise, set `reversed = true`.
