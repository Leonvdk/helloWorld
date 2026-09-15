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
- A battery — see "Powering it" below. 1S LiPo or 3x AA NiMH, either
  straight onto the board's battery input.
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

## Powering it

The ESP32 wants **3.0 to 3.6 V**. Anything that keeps the rail in that band
without a converter is a good battery here, because a converter's idle
current is the thing that decides how long the clock runs.

| Pack | Fresh | Flat | Verdict |
|---|---|---|---|
| **1S LiPo / 18650** | 4.2 V | 3.0 V | The default. Connects straight to a battery-input board, handles the WiFi current pulses, and the shipped thresholds are already set for it. |
| **3x AA NiMH** | ~4.0 V | ~3.0 V | The good AA answer. 3.6 V nominal sits right in the ESP32's band, so it needs **no converter at all** — same wiring as a LiPo, just different thresholds. Rechargeable, and low internal resistance handles the current pulses. |
| **4x AA NiMH** | ~5.3 V | ~4.0 V | Best servo performance, since the servo gets its rated 4.8 V. Needs a low-quiescent 3.3 V regulator for the ESP32; the servo runs straight off the pack. |
| 2x AA alkaline | 3.1 V | 2.0 V | **Avoid.** See below. |
| 2x AA lithium primary | 3.6 V | 2.0 V | Workable with a low-Iq boost. Much lower internal resistance than alkaline and a flatter discharge, but the boost's idle current still applies. |
| 3x AA alkaline | 4.5 V | 3.0 V | Works via a low-quiescent LDO or buck. Alkaline sags under the WiFi pulses as it depletes, so NiMH is the better cell. |
| 9 V PP3 block | 9 V | 6 V | **Worst of the lot.** Least energy of anything here and the most conversion needed. See below. |

### Why not 2x AA

Two cells give 3.1 V fresh and 2.0 V flat, so most of the pack's energy sits
*below* the ESP32's 3.0 V minimum. Getting at it needs a boost converter,
and that is where the idea comes apart:

- **Boost idle current.** This clock sleeps at roughly 20 µA. A common boost
  module (MT3608, XL6009 and friends) idles at 1–2 mA — fifty to a hundred
  times the entire rest of the design. Months of battery life becomes about
  a week. You would need a low-Iq part in power-save mode — TPS61200,
  TPS61098 class, under 10 µA — not a generic module.
- **Current pulses.** WiFi transmit pulls ~300 mA. Through a boost from a
  half-depleted 2.2 V pack, that is 500 mA-plus out of the cells. Alkaline
  AAs have high internal resistance and sag under it; the ESP32's brownout
  detector trips around 2.8 V and you get reboot loops that look like
  firmware bugs.
- **The servo.** An SG90 is specified for 4.8–6 V. On a 3.3 V boosted rail
  it is slow and weak. A light needle will probably still move, but it is
  outside spec.

Energy-wise two AA cells are actually in the same ballpark as a 2000 mAh
LiPo. The problem is entirely delivery, not capacity.

### Why not a 9 V block

Voltage is not energy, and a PP3 is six very small cells in series. Stored
energy, roughly:

| Pack | Energy |
|---|---|
| 3x AA alkaline | ~11 Wh |
| 1S LiPo 2000 mAh | ~7.4 Wh |
| 3x AA NiMH 2000 mAh | ~7.2 Wh |
| **9 V PP3 alkaline** | **~5 Wh** |

So it starts with the least to give, and then has the furthest to fall:

- **A linear regulator throws most of it away.** Dropping 9 V to 3.3 V in an
  AMS1117 burns 63% of every joule as heat, and the regulator's own
  quiescent current is around 5 mA — 250 times this design's sleep current.
  That combination gets you days, not months.
- **A buck converter is better but still needs care.** 85–90% efficient, but
  a generic module idles in the milliamps. As with the boost, you need a
  genuine low-Iq part.
- **The servo needs its own rail.** 9 V straight to an SG90 destroys it, so
  that is a second regulator.
- **High internal resistance.** A PP3 is 1.5–2 Ω fresh and worse as it
  depletes, so the WiFi current pulse pulls it down hard and the usable
  capacity is well below the rated 550 mAh.

The one thing a 9 V block has going for it is a tidy snap connector. If that
is the appeal, a 3x AA holder with a switch is barely larger, holds more
energy, costs less to refill and needs no converter at all.

### Battery thresholds

`kPower` in `src/config.h` ships with 1S LiPo numbers. Change them to match
your pack, or the clock will either never warn or refuse to run:

```cpp
// 1S LiPo / 18650 (shipped)
/*lowBatteryVolts=*/3.50f, /*criticalBatteryVolts=*/3.20f, /*recoverVolts=*/3.65f,

// 3x AA NiMH
/*lowBatteryVolts=*/3.30f, /*criticalBatteryVolts=*/3.10f, /*recoverVolts=*/3.45f,
```

Two rules whatever you use:

- **Measure the raw pack, not a regulated rail.** After a converter the
  sense pin reads a steady 3.3 V right up until the moment it collapses,
  which tells you nothing. Wire the divider across the cells.
- **Keep the divider output under ~2.4 V at full charge.** With the shipped
  100k/100k pair and `kBatteryDividerRatio = 2.0`, that covers packs up to
  4.8 V. A 4x AA pack at 5.3 V needs a different ratio — 100k/47k gives
  3.13, so set `kBatteryDividerRatio = 3.13f`.

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
