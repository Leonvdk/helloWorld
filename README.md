# Wind clock

A clock face with one arm, driven by a servo on an ESP32, showing how hard
the wind is going to blow. **0 km/h parks the arm at 12 o'clock and it
sweeps clockwise as the wind picks up, reaching 11 o'clock at 100 km/h.**

Battery powered: it wakes every half hour, fetches the hourly wind forecast
from [Open-Meteo](https://open-meteo.com), moves the needle if the wind has
meaningfully changed, and goes back to deep sleep.

```
  km/h    needle     face       km/h    needle     face
     0      0.0°    12:00         60    198.0°     6:36
    10     33.0°     1:06         70    231.0°     7:42
    25     82.5°     2:45         85    280.5°     9:21
    50    165.0°     5:30        100    330.0°    11:00
```

## Heads up: 330 degrees is more than a servo turns

12 o'clock round to 11 o'clock is 330 degrees of needle travel, and a plain
hobby servo only manages 180. The default configuration assumes a **180
degree servo geared 2:1**, so 165 degrees of shaft swings the needle the
full 330. A 360-degree *positional* servo driving the needle directly works
too — one line of config. [docs/HARDWARE.md](docs/HARDWARE.md) lays out the
options, the wiring and the power budget.

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
test/                   115 tests, run natively with g++
tools/dial_table.cpp    prints the dial for marking up the face
```

Everything worth testing lives in `src/core` and has no Arduino dependency,
so the whole suite builds and runs on a laptop in about a second.

## Tests

```sh
make test     # build and run all 115 tests
make table    # print the dial as a calibration table
```

Needs nothing but `g++` and `make`. What's covered:

- **Dial** — 0 at 12 o'clock and 100 at 11 o'clock, monotonic clockwise
  travel, known speeds landing on the expected hours, clamping above and
  below the scale, NaN and infinity, round-tripping, custom scales.
- **Servo** — pulse endpoints, gear ratios, trim, reversed servos, narrow
  pulse ranges, and rejecting a calibration that cannot reach 11 o'clock.
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

## Design notes

**Why a forecast and not an anemometer.** The clock answers "how windy will
it be", so it reads the hourly forecast rather than measuring. By default it
shows the strongest hour in the next 12 — switch `kForecast.mode` to
`NextHour` for what it's doing right now, or set `useGusts` to show gusts
instead of sustained wind.

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
