# Building and flashing the wind clock

Start to finish: toolchain, a bench test with nothing but a servo, then the
real wiring. Each stage works on its own, so if something goes wrong you
know which stage broke it.

Read [HARDWARE.md](HARDWARE.md) first if you haven't picked a servo yet.

---

## 1. What you need

**Parts**

| | |
|---|---|
| ESP32 dev board | Any ESP32 with a USB port. For battery life pick a low-sleep-current board — FireBeetle ESP32, TinyPICO, LOLIN32 Lite. A plain DevKitC works for the bench test but will flatten a battery in days. |
| Servo | Standard 180°, e.g. SG90 or MG90S. A 270° servo is more forgiving — see HARDWARE.md. |
| 1S LiPo, 2000 mAh+ | Ideally with a JST-PH connector matching your board's battery input. |
| P-channel MOSFET | AO3401 or similar, for the servo power switch. |
| N-channel MOSFETs ×2 | 2N7002 or BSS138, to drive the P-FET gate and to gate the divider. |
| Resistors | 2× 100 kΩ (divider), 2× 100 kΩ (gate pulls), 1× 1 kΩ. |
| Capacitors | 470 µF electrolytic, 100 nF ceramic. |
| Clock face and needle | Marked 0–100, with 0 at 9 o'clock and 100 at 3 o'clock. |

**Software**

- [PlatformIO](https://platformio.org) — either the VS Code extension or the
  CLI (`pip install platformio`).
- A USB cable that carries data, not just power. This wastes more afternoons
  than any other item on this list.

**A 2.4 GHz WiFi network.** The ESP32 has no 5 GHz radio. If your router
publishes one SSID for both bands you may need to split them or use a guest
network.

---

## 2. Get the code and run the tests

```sh
git clone https://github.com/Leonvdk/helloWorld.git wind-clock
cd wind-clock
git checkout claude/wind-clock-esp32-servo-bt61lc
make test
```

You should see `120 passed, 0 failed`. This needs no hardware and no
PlatformIO — it's a good check that your toolchain is sane before you
involve a microcontroller.

Then print the dial you're about to build:

```sh
make table
```

Keep that output. You'll use it to mark the face and to check the needle in
step 4.

---

## 3. Bench test: servo only

Don't wire the battery, the MOSFETs or the divider yet. Just this:

| Servo wire | Goes to |
|---|---|
| Signal (orange/white) | **GPIO 18** |
| V+ (red) | **5V** / **VUSB** pin on the board |
| GND (brown/black) | **GND** |

Powering the servo from the board's 5V pin is fine *for the bench test* on
USB. Never do it from the 3.3V regulator — a moving servo will brown out
the board.

Flash the bench firmware:

```sh
pio run -e bench -t upload
pio device monitor
```

If upload fails with "failed to connect", hold the **BOOT** button while it
says *Connecting...*, then release. Some boards need this; some don't.

The needle should now cycle 0 → 25 → 50 → 75 → 100 → 50 → 0 km/h forever,
two seconds a stop, printing each position:

```
[bench] sweeping the dial -- no WiFi, no sleep.
[wind] needle -> 0.0 km/h (-90 deg, 500 us)
[wind] needle -> 25.0 km/h (-45 deg, 1000 us)
```

---

## 4. Fit the needle

Servo power is cut between stops, so you can reposition the horn by hand.

1. Wait for the `0.0 km/h` line — the servo is now at one end of its travel.
2. Pull the horn off the spline and refit it with the needle pointing at
   **9 o'clock**. The splines are coarse, so you'll be within a few degrees;
   `trimDeg` takes up the rest if your servo has travel to spare.
3. Watch a full cycle. At `50.0 km/h` the needle should stand **straight up
   at 12**, and at `100.0 km/h` it should reach **3 o'clock**.

If it doesn't line up, see the troubleshooting table at the bottom before
changing anything mechanical — most of it is one line in `src/config.h`.

---

## 5. Wire it for real

Power down and disconnect USB first.

### Pin assignments

| Signal | GPIO | Notes |
|---|---|---|
| Servo signal | **18** | Straight to the servo's signal wire. |
| Servo power enable | **25** | Gate of the power switch below. RTC-capable, so the level holds through deep sleep. |
| Battery sense | **34** | Divider midpoint. ADC1 and input-only — ADC2 pins don't work while WiFi is on. |
| Battery sense enable | **26** | Gates the divider. Also RTC-capable. |

Change any of these in `src/config.h` if your board doesn't break them out,
but keep 34 on ADC1 and keep 25/26 RTC-capable (0, 2, 4, 12–15, 25–27,
32–39).

### Servo power switch

The servo idles at a few milliamps, which over weeks costs more than
everything else combined, so it sits behind a high-side switch:

```
  VBAT ────┬──────────────[S]  P-FET  [D]────┬──── SERVO V+
           │                   (AO3401)      │
           └──[100k]──┬── [G]                ├──[470uF]── GND
                      │                      │
                   [D] N-FET (2N7002)        └──── (servo red wire)
                       [S] ── GND
        GPIO25 ──[1k]──[G]
                        └──[100k]── GND
```

- The 100 kΩ from the P-FET gate to VBAT holds the servo rail **off** by
  default.
- The 100 kΩ on the N-FET gate holds it off while the ESP32 is in reset and
  its pins float — without it, the servo can twitch on every boot.
- GPIO25 high → servo powered. That's `kServoPowerActiveHigh = true`.
- Put the 470 µF as close to the servo as you can.

### Battery sense divider

```
  VBAT ──[100k]──┬──[100k]──[D] N-FET (2N7002) [S]── GND
                 │                  [G]
              GPIO34            GPIO26 ──[100k]── GND
                 │
             [100nF]
                 │
                GND
```

GPIO26 goes high only during the measurement, so the divider isn't a
permanent 21 µA drain. The 100 nF steadies the ADC.

### Everything else

- Battery to the board's **VBAT / JST** input, and to the P-FET source.
- **All grounds common** — battery, ESP32, servo.
- Servo signal stays on GPIO 18; its red wire now comes from the switched
  rail, not from 5V.

---

## 6. Configure

Edit `src/config.h`:

```cpp
// Where you want the forecast for. Ships pointing at Aljezur, Portugal.
constexpr float kLatitude = 37.3167f;
constexpr float kLongitude = -8.8000f;
```

Get your coordinates from any map — right-click a spot in Google Maps and
it gives you `latitude, longitude` in that order.

While you're in there, the two you're most likely to want:

```cpp
constexpr ForecastQuery kForecast = {
    ForecastMode::MaxOverHorizon,  // NextHour for current conditions
    12,                            // hours to look ahead
    false,                         // true to show gusts
    WindUnit::Kph,
};

constexpr PowerPolicy kPower = {
    30 * 60,   // seconds between updates
    ...
};
```

WiFi credentials are passed at build time, so they stay out of the repo:

```sh
export WINDCLOCK_WIFI_SSID="your-network"
export WINDCLOCK_WIFI_PASSWORD="your-password"
```

On Windows PowerShell:

```powershell
$env:WINDCLOCK_WIFI_SSID="your-network"
$env:WINDCLOCK_WIFI_PASSWORD="your-password"
```

These must be set in the same shell you run `pio` from. If you'd rather not
retype them, put them in a `credentials.ini` — `.gitignore` already covers
that filename.

---

## 7. Flash the real firmware

```sh
pio run -e esp32dev -t upload
pio device monitor
```

A healthy first boot looks like this:

```
[wind] wake #1
[wind] battery 4.02 V, mode 0
[wind] forecast 23.4 km/h (12 samples, index 5)
[wind] needle -> 23.4 km/h (-48 deg, 1000 us)
[wind] sleeping 1800 s
```

`mode 0` is Normal, `1` is low battery, `2` is critical.

After that the board is in deep sleep and the serial port goes quiet for 30
minutes. **Press reset** to trigger another cycle rather than waiting.

On the second cycle you'll usually see:

```
[wind] change within deadband; needle stays put
```

That's correct — the needle only moves when the wind changes by more than
1.5 km/h. It's the single biggest reason the battery lasts.

---

## 8. Check it over a day

Leave it running and come back. Things worth confirming:

- The needle tracks the forecast on your phone's weather app, roughly.
- It survives overnight — if it's dead in the morning, the board's sleep
  current is the first suspect, not the code.
- `[wind] battery` drops slowly and sensibly. A reading that jumps around
  by half a volt usually means a missing 100 nF or a shared ground that
  isn't.

---

## Troubleshooting

| Symptom | Cause | Fix |
|---|---|---|
| `battery 0.00 V, mode 2` and the needle never moves | Divider not fitted or not wired | Set `kBatterySenseFitted = false` in `config.h` until you wire it |
| `FATAL: servo travel cannot cover the dial` | `travelDeg` smaller than the 180° sweep, or `trimDeg` pushes it past the end | See "About that missing trim margin" in HARDWARE.md |
| Needle runs backwards — 100 km/h at 9 o'clock | Servo turns the other way | `kServo.reversed = true` |
| Needle won't quite reach 9 or 3 | Servo doesn't make a true 180° | Widen `minPulseUs`/`maxPulseUs` to 400/2600, or lower `travelDeg` to what it really turns |
| Needle slams into the end stop and buzzes | Same, in the other direction | Lower `travelDeg`, or narrow the pulse range |
| Board resets whenever the servo moves | Servo on the 3.3 V rail, or no bulk capacitor | Servo from the switched battery rail; add the 470 µF |
| `WiFi failed` every cycle | 5 GHz network, or credentials not in the build | ESP32 is 2.4 GHz only; re-export the variables and rebuild |
| `HTTP -1` or `HTTP -11` | DNS or TLS handshake failed | Usually a weak signal or a captive portal; check the board is actually on the network |
| `forecast unusable` | Open-Meteo returned something unexpected | Check your coordinates are in range and in the right order |
| Upload fails with "failed to connect" | Board not in bootloader | Hold **BOOT** during *Connecting...*; check the cable carries data |
| Serial port shows nothing after the first cycle | It's in deep sleep, working correctly | Press reset |
| Servo twitches on every boot | Missing 100 kΩ pulldown on the N-FET gate | Add it |

## Going back to the bench

Any time you want the sweep back — after changing the calibration, or to
refit the needle:

```sh
pio run -e bench -t upload
```

Bench mode skips the battery check and WiFi entirely, so it works with
nothing but the servo connected.
