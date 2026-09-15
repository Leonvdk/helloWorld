# Building and flashing the wind clock

The mains-powered build, start to finish. It needs three wires, a USB-C
charger and no other electronics.

Running it on a battery instead? Everything up to step 5 is the same — then
see [Appendix A](#appendix-a-running-it-on-a-battery).

---

## 1. What you need

**Parts**

| | |
|---|---|
| ESP32 dev board | Any ESP32 with a USB port. On mains you don't need a low-sleep-current board, so a plain DevKitC is fine here. |
| Servo | Standard 180°, e.g. SG90 or MG90S. A 270° servo is more forgiving — see [HARDWARE.md](HARDWARE.md). |
| USB-C charger | **1 A or more.** The servo move and the WiFi transmit burst overlap badly on a 500 mA supply. |
| USB cable | Must carry data, not just power. This wastes more afternoons than any other item on this list. |
| 3 jumper wires | Female-to-male, to reach the servo's plug. |
| 470 µF electrolytic capacitor | Optional but recommended — steadies the 5 V rail when the servo starts moving. |
| Clock face and needle | Marked 0–100, with 0 at 9 o'clock and 100 at 3 o'clock. |

**Software**

- [PlatformIO](https://platformio.org) — the VS Code extension, or the CLI
  (`pip install platformio`).

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

You should see `122 passed, 0 failed`. This needs no hardware and no
PlatformIO — it's a good check that your toolchain is sane before a
microcontroller is involved.

Then print the dial you're about to build:

```sh
make table
```

Keep that output. You'll use it to mark the face and to check the needle in
step 4.

---

## 3. Wire it

That's the whole thing:

| Servo wire | Colour | Goes to |
|---|---|---|
| Signal | orange or white | **GPIO 18** |
| V+ | red | **5V** (sometimes labelled VUSB or VIN) |
| GND | brown or black | **GND** |

If you're fitting the capacitor, put it across the servo's V+ and GND as
close to the servo as you can. Mind the polarity — the stripe is negative.

**Never run the servo from the 3.3 V pin.** A moving servo will brown out
the board.

---

## 4. Bench test and fit the needle

Plug the board into your computer and flash the sweep:

```sh
pio run -e bench -t upload
pio device monitor
```

If upload fails with "failed to connect", hold the **BOOT** button while it
says *Connecting...*, then release. Some boards need this; some don't.

The servo should now cycle 0 → 25 → 50 → 75 → 100 → 50 km/h forever, two
seconds a stop, printing each position:

```
[bench] sweeping the dial -- no WiFi, no sleep.
[wind] needle -> 0.0 km/h (-90 deg, 500 us)
[wind] needle -> 25.0 km/h (-45 deg, 1000 us)
```

Servo power is cut between stops, so you can reposition the horn by hand:

1. Wait for the `0.0 km/h` line — the servo is now at one end of its travel.
2. Pull the horn off the spline and refit it with the needle pointing at
   **9 o'clock**. The splines are coarse, so you'll be within a few degrees.
3. Watch a full cycle. At `50.0 km/h` the needle should stand **straight up
   at 12**, and at `100.0 km/h` it should reach **3 o'clock**.

If it doesn't line up, check the troubleshooting table before changing
anything mechanical — most of it is one line in `src/config.h`.

---

## 5. Configure

Set your location in `src/config.h`:

```cpp
constexpr float kLatitude = 37.3167f;
constexpr float kLongitude = -8.8000f;
```

Right-click a spot in Google Maps and it gives you `latitude, longitude` in
that order. It ships pointing at Aljezur, Portugal.

The two settings you're most likely to want after that:

```cpp
constexpr ForecastQuery kForecast = {
    ForecastMode::MaxOverHorizon,  // NextHour for current conditions
    12,                            // hours to look ahead
    false,                         // true to show gusts
    WindUnit::Kph,
};
```

WiFi credentials are passed at build time so they stay out of the repo:

```sh
export WINDCLOCK_WIFI_SSID="your-network"
export WINDCLOCK_WIFI_PASSWORD="your-password"
```

On Windows PowerShell:

```powershell
$env:WINDCLOCK_WIFI_SSID="your-network"
$env:WINDCLOCK_WIFI_PASSWORD="your-password"
```

These must be set in the **same shell** you run `pio` from. If you'd rather
not retype them, put them in a `credentials.ini` — `.gitignore` already
covers that filename.

---

## 6. Flash it

```sh
pio run -e usb -t upload
pio device monitor
```

`-e usb` is the mains build: it skips the battery check and updates every
5 minutes instead of 30.

A healthy first boot:

```
[wind] wake #1
[wind] battery 4.00 V, mode 0
[wind] forecast 23.4 km/h (12 samples, index 5)
[wind] needle -> 23.4 km/h (-48 deg, 1000 us)
[wind] sleeping 300 s
```

The battery line always reads 4.00 V here — on the `usb` build it isn't
measuring anything, by design.

The board then deep sleeps and the serial port goes quiet for five minutes.
**Press reset** to trigger another cycle rather than waiting.

On the second cycle you'll usually see:

```
[wind] change within deadband; needle stays put
```

That's correct. The needle only moves when the wind changes by more than
1.5 km/h, which keeps it from twitching every five minutes.

---

## 7. Hang it up

Unplug from the computer, plug into the USB-C charger, and hang it
somewhere.

If it's dead from the charger but worked fine from your computer, see the
CC-resistor entry in the troubleshooting table — it's a known quirk of
cheap boards, not a fault.

Come back in a day and check the needle roughly tracks the forecast on your
phone. If it drifts out over a week, the likely cause is the servo horn
creeping on its spline rather than anything in software.

---

## Troubleshooting

| Symptom | Cause | Fix |
|---|---|---|
| Dead from a USB-C charger, fine from a computer | Board lacks the 5.1 kΩ CC pull-down resistors a USB-C source looks for | Use a **USB-A to USB-C cable** into an A-port charger. Nothing else is wrong |
| Board resets whenever the servo moves | Servo on the 3.3 V pin, or an underpowered charger | Servo to **5V**; use a 1 A+ charger; fit the 470 µF |
| Needle runs backwards — 100 km/h at 9 o'clock | Servo turns the other way | `kServo.reversed = true` |
| Needle won't quite reach 9 or 3 | Servo doesn't make a true 180° | Widen `minPulseUs`/`maxPulseUs` to 400/2600, or lower `travelDeg` to what it really turns |
| Needle slams into the end stop and buzzes | Same, in the other direction | Lower `travelDeg`, or narrow the pulse range |
| `FATAL: servo travel cannot cover the dial` | `travelDeg` smaller than the 180° sweep, or `trimDeg` pushes it past the end | See "About that missing trim margin" in HARDWARE.md |
| `WiFi failed` every cycle | 5 GHz network, or credentials not in the build | ESP32 is 2.4 GHz only; re-export the variables and rebuild |
| `HTTP -1` or `HTTP -11` | DNS or TLS handshake failed | Usually a weak signal or a captive portal; check the board is actually on the network |
| `forecast unusable` | Open-Meteo returned something unexpected | Check your coordinates are in range and in the right order |
| Upload fails with "failed to connect" | Board not in bootloader | Hold **BOOT** during *Connecting...*; check the cable carries data |
| Serial port shows nothing after the first cycle | It's in deep sleep, working correctly | Press reset |
| `battery 0.00 V, mode 2`, needle never moves | Battery build with the divider not wired | Set `kBatterySenseFitted = false`, or build with `-e usb` |

---

## Going back to the bench

Any time you want the sweep back — after changing the calibration, or to
refit the needle:

```sh
pio run -e bench -t upload
```

Bench mode skips the battery check and WiFi entirely, so it works with
nothing but the servo connected.

---

## Appendix A: Running it on a battery

Only needed if the clock can't reach an outlet. Read
"[Powering it](HARDWARE.md#powering-it)" first for which cell to buy — the
short version is a 1S LiPo or 3× AA NiMH, and **not** a USB power bank.

Build with `pio run -e esp32dev -t upload` instead of `-e usb`. Two things
change: the update interval goes to 30 minutes, and the board reads its
battery each cycle to decide when to slow down and when to stop driving the
servo entirely.

You'll also want a board that's actually built for battery use — a
FireBeetle, TinyPICO or LOLIN32 Lite sleeps at 80–100 µA where a generic
DevKitC sleeps at ~5 mA and flattens any pack inside a week. This single
number matters more than everything else here.

### Extra parts

- P-channel MOSFET (AO3401) and two N-channel (2N7002 or BSS138)
- 4× 100 kΩ, 1× 1 kΩ, 1× 100 nF
- The 470 µF becomes mandatory rather than optional

### Extra pins

| Signal | GPIO | Notes |
|---|---|---|
| Servo power enable | **25** | RTC-capable, so the level holds through deep sleep |
| Battery sense | **34** | ADC1 and input-only — ADC2 pins don't work while WiFi is on |
| Battery sense enable | **26** | Also RTC-capable |

Keep 34 on ADC1 and keep 25/26 RTC-capable (0, 2, 4, 12–15, 25–27, 32–39).

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
  its pins float — without it, the servo twitches on every boot.
- GPIO25 high → servo powered. That's `kServoPowerActiveHigh = true`.

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

### Then

- Battery to the board's **VBAT / JST** input, and to the P-FET source.
- **All grounds common** — battery, ESP32, servo.
- Match `kPower`'s thresholds to your cell — see "Battery thresholds" in
  HARDWARE.md. They ship set for a 1S LiPo.

A 1S cell gives the servo 3.0–4.2 V where an SG90 wants 4.8–6 V, so it'll
be sluggish. If that bothers you, put a small 5 V boost module **after** the
load switch: it's then unpowered during deep sleep, so its quiescent
current only flows while the needle is actually moving.

### Extra troubleshooting

| Symptom | Cause | Fix |
|---|---|---|
| `mode 1` or `mode 2` on a freshly charged pack | Thresholds still set for a different cell | Match `kPower` — see HARDWARE.md |
| Reboot loops once the battery is half used | Pack sagging under the WiFi current pulse | Alkaline cells or an undersized converter; see "Powering it" in HARDWARE.md |
| Flat in a week | Board's sleep current, not the code | Check it's a battery-oriented board, not a DevKitC |
| Servo twitches on every boot | Missing 100 kΩ pulldown on the N-FET gate | Add it |
