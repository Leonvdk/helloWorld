# Wind clock

A clock face with one arm, driven by a servo on an ESP32, showing how hard
the wind is going to blow. **0 km/h parks the arm at 9 o'clock, half scale
stands it straight up at 12, and it reaches 3 o'clock at 100 km/h** — a
180 degree sweep clockwise across the top of the face.

Battery powered: it wakes every half hour, fetches the hourly wind forecast
from [Open-Meteo](https://open-meteo.com), moves the needle if the wind has
meaningfully changed, and goes back to deep sleep.

```
  km/h    needle     face     pulse       km/h    needle     face     pulse
     0    -90.0°     9:00     500 us        60     18.0°    12:36    1700 us
    10    -72.0°     9:36     700 us        75     45.0°     1:30    2000 us
    25    -45.0°    10:30    1000 us        90     72.0°     2:24    2300 us
    50      0.0°    12:00    1500 us       100     90.0°     3:00    2500 us
```

180 degrees is exactly what a standard hobby servo turns, so the horn goes
straight onto the needle shaft — no gears, no belt, and a clean 20 µs of
pulse width per km/h. The one catch is that the sweep uses the servo's
entire travel, leaving no margin for a servo that doesn't quite make its
nominal 180; [docs/HARDWARE.md](docs/HARDWARE.md) covers that, the wiring
and the power budget. Gearing and a 270-degree servo are both still a
config line away.

The firmware checks at startup that the configured servo can actually reach
both ends of the dial, and refuses to run if it cannot.

## Layout

```
src/
  main.cpp              wake cycle: battery, WiFi, fetch, move, sleep
  config.h              every tunable in one place
  core/                 portable, no Arduino headers, fully unit-tested
    wind_dial.*         wind speed  -> angle on the clock face
    servo_map.*         angle       -> servo pulse width (gearing, trim)
    wind_forecast.*     Open-Meteo response -> a wind speed
    forecast_url.*      builds the API request
    needle_filter.*     deadband and slew limit, so the servo stays still
    power_policy.*      battery thresholds, sleep intervals, backoff
test/                   120 tests, run natively with g++
tools/dial_table.cpp    prints the dial for marking up the face
```

Everything worth testing lives in `src/core` and has no Arduino dependency,
so the whole suite builds and runs on a laptop in about a second.

## Tests

```sh
make test     # build and run all 120 tests
make table    # print the dial as a calibration table
```

Needs nothing but `g++` and `make`. What's covered:

- **Dial** — 0 at 9 o'clock, half scale at 12 and 100 at 3, monotonic
  clockwise travel, symmetry about 12, known speeds landing on the expected
  hours, clamping above and below the scale, NaN and infinity,
  round-tripping, narrower and rescaled dials.
- **Servo** — pulse endpoints, shaft angle measured from the calm end of
  the dial, trim, gear ratios, reversed servos, narrow pulse ranges, and
  rejecting a servo whose travel falls short of the sweep.
- **Forecast** — real Open-Meteo payloads, gusts vs sustained wind, the
  horizon window, nulls, empty and missing arrays, truncated responses,
  API error payloads, unit conversion, and not mistaking the unit strings
  in `hourly_units` for data.
- **Needle filter** — the deadband that stops the servo twitching, the slew
  limit on a sudden gale, convergence, and a steady wind never waking the
  servo at all.
- **Power** — battery classification with hysteresis, sleep intervals,
  parking the needle on a flat cell, exponential backoff, staleness.
- **Integration** — a canned HTTP response straight through to a pulse
  width, using the shipped configuration.

## Firmware

```sh
export WINDCLOCK_WIFI_SSID="your-network"
export WINDCLOCK_WIFI_PASSWORD="your-password"
pio run -e esp32dev -t upload
pio device monitor
```

Set your location in `src/config.h` (`kLatitude` / `kLongitude`); it ships
pointing at Aljezur, Portugal.

Two other build targets:

- `pio run -e usb -t upload` — **mains-powered build.** No battery, no sense
  divider, no MOSFETs, and a 5-minute update interval. About a third of the
  parts, and the servo gets a proper 5 V.
- `pio run -e bench -t upload` — sweeps the needle across the whole scale on
  a loop, with no WiFi and no sleep. Use it to fit the needle before wiring
  anything else.

**[docs/SETUP.md](docs/SETUP.md) is the step-by-step build guide.** It
leads with the mains-powered build — three wires and a USB-C charger — and
keeps the battery wiring as an appendix.

## Design notes

**Why a forecast and not an anemometer.** The clock answers "how windy will
it be", so it reads the hourly forecast rather than measuring. By default it
shows the strongest hour in the next 12 — switch `kForecast.mode` to
`NextHour` for what it's doing right now, or set `useGusts` to show gusts
instead of sustained wind.

**Why the scale starts at 9 and not at 12.** A sweep that begins at 12 and
runs right round the face needs more rotation than a servo has, and needs
gearing to get it. Starting at 9 and finishing at 3 keeps the whole scale
across the top of the face, in the 180 degrees a servo turns natively, and
puts half scale bolt upright where it is easy to read at a glance.

**Why the needle mostly doesn't move.** Powering the servo is by far the
most expensive thing in a wake cycle, so a change smaller than 1.5 km/h is
ignored entirely and a big jump is slew-limited to 25 km/h per cycle. A
steady wind costs nothing but the WiFi fetch.

**Why the servo gets its power cut.** A servo idles at a few milliamps,
which over weeks is more than everything else put together. It sits behind
a load switch that is held off through deep sleep.

**What happens when things break.** A failed fetch leaves the needle where
it is and backs off exponentially. If no good reading arrives for six
hours the needle parks at 12 rather than confidently showing yesterday's
gale. Below 3.2 V the servo stops being driven at all, so a flat cell can't
stall it against an end stop.
