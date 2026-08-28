# Sensory Ball v2 — Design Document

This document captures the v2 design before any firmware or hardware work begins. It is the single source of truth that the v2 wiring diagram, ASSEMBLY guide, and firmware reference.

---

## Design center

**Pendulum oscillation is the primary input.** v1 treated the ball as a smack-detector; v2 treats it as a pendulum and synchronizes feedback (light, sound, vibration) to the swing frequency, amplitude, and phase. This change is motivated by the observed engagement pattern in v1 use: sustained interest in specific pendulum frequencies, deliberately modulated, rather than in impact events.

Every output channel should *amplify the rhythm the user is already creating*, not compete with it.

---

## System overview

```
          ┌─────────────────────────────┐
          │  Phone PWA (companion app)  │
          │  Web Bluetooth              │
          └─────────────┬───────────────┘
                        │ BLE GATT
                        ▼
          ┌─────────────────────────────┐
          │  XIAO ESP32S3               │
          │  ─────────────────          │
          │  MPU-6050 (I²C)             │
          │     │                       │
          │  Motion estimator           │
          │  ├── pendulum f, A, phase   │
          │  ├── smack detector         │
          │  ├── hold-still detector    │
          │  └── shake detector         │
          │     │                       │
          │  Program engine             │
          │     │                       │
          │     ├──→ NeoPixel ring (12) │
          │     ├──→ DFPlayer (UART)    │
          │     └──→ Vibration motor    │
          │                             │
          │  NVS: program, palette,     │
          │       tuning constants      │
          └─────────────────────────────┘
```

---

## Bill of materials — additions over v1

| Qty | Part | Notes |
|----:|------|-------|
| 1 | **WS2812B NeoPixel ring, 12 pixels** (~37 mm OD) | Replaces v1's single 4-pin LED + 3 resistors. Adafruit part 1643 or equivalent. |
| 1 | **DFPlayer Mini** | UART-controlled MP3/WAV player with onboard microSD slot and integrated 3W amp. |
| 1 | **microSD card, 4–8 GB** | Holds audio samples. FAT32, files named `0001.mp3`, `0002.mp3`, etc. |
| 1 | **Small 8Ω 0.5W speaker** (~28 mm) | Drives off DFPlayer's SPK1/SPK2 pins. Anything in the 25–35 mm range fits behind the NeoPixel ring. |
| 1 | **Vibration motor** | Salvaged from an Xbox controller rumble pack. Standard 3 V coin/cylinder motor. |
| 1 | **2N7000 MOSFET** (TO-92) | Logic-level N-channel, switches the vibration motor from a GPIO. |
| 1 | **1N4148 diode** | Flyback diode across the vibration motor (cathode to +, anode to motor side of the MOSFET drain). |
| 1 | **100 kΩ resistor** | Pulldown on the MOSFET gate so the motor stays off at boot before the GPIO is configured. |
| 1 | **1 kΩ resistor** | Series on the DFPlayer's RX line to drop the ESP32's 3.3 V logic to a level the DFPlayer cleanly latches. Optional but recommended in DFPlayer's datasheet. |
| 1 | **330 Ω resistor** *(optional)* | Series on the NeoPixel DIN line, recommended by Adafruit to clamp transients. |
| 1 | **1000 µF electrolytic capacitor** *(optional)* | Across the NeoPixel VCC/GND to absorb inrush current when the ring lights up. |

Existing v1 parts retained: XIAO ESP32S3, MPU-6050 breakout, 1S LiPo (~450 mAh), spring, printed shell. The 4-pin RGB LED and its three 220 Ω resistors are no longer used.

---

## Pin map

| Function | Sketch constant | XIAO pin | ESP32-S3 GPIO | Notes |
|----------|-----------------|----------|----------------|-------|
| I²C SDA | `Wire` default | D4 | GPIO5 | MPU-6050 SDA |
| I²C SCL | `Wire` default | D5 | GPIO6 | MPU-6050 SCL |
| NeoPixel DIN | `PIN_NEOPIXEL` | D0 | GPIO1 | RMT-driven, single data line for all 12 pixels |
| DFPlayer RX | `PIN_DFP_RX` | D8 → DFP `RX` | GPIO7 | XIAO transmits to DFPlayer (series 1 kΩ on this line) |
| DFPlayer TX | `PIN_DFP_TX` | D9 ← DFP `TX` | GPIO8 | DFPlayer transmits to XIAO |
| Vibration MOSFET gate | `PIN_VIB` | D3 | GPIO4 | PWM out, drives 2N7000 gate via 100 kΩ pulldown to GND |
| USB serial (debug) | reserved | D6/D7 | GPIO43/44 | Keep free for USB-CDC debugging |
| Reserved | — | D1, D2, D10 | GPIO2/3/9 | Future expansion: mode-switch button, secondary haptic, etc. |

**Why this assignment:**
- D4/D5 stay on I²C — already wired and working in v1.
- D0 picks up the NeoPixel because the v1 sketch had it on the LED's red channel; it's a clean conceptual carry-over (still "the light pin").
- D8/D9 picked for DFPlayer UART because they're on UART1 in the ESP32-S3 default mapping and stay clear of USB-CDC (UART0 on D6/D7).
- D3 for vibration because PWM channels are universally available on ESP32-S3 GPIOs; nothing pin-specific here.
- Three pins kept free (D1, D2, D10) so adding a button or a second sensor in v3 doesn't force a redesign.

---

## Power topology

Everything runs off the 1S LiPo (3.7 V nominal, 4.2 V fully charged).

| Rail | Source | Loads |
|------|--------|-------|
| **3.7 V** (LiPo direct) | `BAT+` pad on XIAO | NeoPixel ring VCC, DFPlayer VCC, vibration motor + |
| **3.3 V** (XIAO regulated) | XIAO `3V3` pin | MPU-6050 VCC, logic levels for all data lines |
| **GND** | XIAO `GND` and `BAT−` | All grounds shared |

The XIAO's onboard LiPo charger handles charging whenever USB-C is connected.

**NeoPixel on 3.7 V**, not 5 V: the WS2812B's nominal range is 3.5 V to 5.3 V. At 3.7 V the LEDs run slightly dim but well within spec, and the ESP32-S3's 3.3 V logic comfortably exceeds the data-pin threshold (~70 % of VDD = 2.6 V). Skipping a boost converter saves board space and ~5 % battery drain. If a future v3 wants full brightness, drop in an MT3608 boost to 5 V on the LED rail only.

**DFPlayer on 3.7 V**: the datasheet says 3.2–5.0 V, so 3.7 V is in spec. Audio output level scales with supply voltage, so a 3.7 V supply gives slightly quieter audio than 5 V — acceptable for this application.

---

## Runtime estimate

Rough mixed-use draw on the 450 mAh LiPo:

| State | Current | Time per cycle |
|-------|---------|----------------|
| Idle (LED off, audio idle, motor off) | ~30 mA | ~70 % |
| Pendulum-sync active (ring at 30 % avg, occasional vibration) | ~120 mA | ~25 % |
| Smack burst (ring full brightness 200 ms + vibration + sound) | ~600 mA | ~5 % |

Weighted average ≈ 75 mA → **~6 hours active runtime**. About half of v1 because of the added peripherals. Acceptable for typical session use; the ball charges over USB-C in ~1.5 hours.

Light-sleep / motion-wake (deferred to a later phase) would push this back above v1's runtime.

---

## Firmware architecture sketch

```
sensory_ball_v2/
  sensory_ball_v2.ino       — top level: setup(), loop(), program selection
  motion.h / motion.cpp     — pendulum/smack/shake/hold-still estimators
  programs.h / programs.cpp — selectable feedback programs (state machines)
  outputs.h / outputs.cpp   — NeoPixel + DFPlayer + vibration drivers
  ble.h / ble.cpp           — BLE GATT service for companion app
  settings.h / settings.cpp — NVS persistence
```

The single .ino of v1 splits into modules because the v2 surface area no longer fits comfortably in one file (and never will, once BLE and the program engine land).

**Loop frequency: 50 Hz** — same as v1. Motion estimator runs every tick; LED/audio/vibration update at the same rate.

---

## What's deliberately out of scope for v2.0

- **Sphere v2 redesign.** v1's printed sphere accommodates v2's parts with no changes — the NeoPixel ring is smaller than the cluster of LED + resistors it replaces, the DFPlayer Mini is smaller than the MPU-6050 breakout, and the vibration motor + MOSFET fit anywhere. Mechanical changes deferred to v3 if needed.
- **Battery management beyond what the XIAO provides.** The XIAO's onboard charger is sufficient for the size of cell we're using. No fuel gauge / low-voltage cutoff / state-of-charge reporting in v2.
- **Production-quality enclosure.** This is a one-off prototype. A version meant for multiple builds would need a proper potted assembly and certified components.
- **Light/deep sleep with motion-wake.** Listed as v2 candidate in v1's README; deferred to v2.1 because the BLE companion app is the higher portfolio value-add and sleep makes BLE harder.

---

## Phase plan

| Phase | Scope | Definition of done |
|------:|-------|---|
| **v2.0-α** | Firmware refactor + pendulum estimator, **driving v1 hardware (single LED)**. | Pendulum frequency and amplitude readable over serial; LED pulses synchronously with a hand-swung breadboard. |
| **v2.0-β** | Swap to v2 hardware: NeoPixel ring, DFPlayer + speaker, vibration motor. Wire per this document. | All three outputs respond. Smoke test passes (boot animation + audio chime + brief vibration). |
| **v2.0** | Program engine: 4 selectable programs (pendulum-sync, smack-burst, gentle-roll, calm-breathe) hard-coded with one default. NVS persistence of tuning constants. | Default program runs out of the box. Behaviors are tuned with the end user. |
| **v2.1** | BLE GATT service + PWA companion app for live program selection and tuning. | App connects, picks programs, edits tuning constants, persists across reboots. |
| **v2.2** | Session logging (impact count, swing durations, dominant frequencies). Engagement view in the PWA. | Logs survive a power cycle, viewable as a graph in the PWA. |
| **v2.3** | Motion-wake light sleep. | Idle current drops to <1 mA; ball wakes within 200 ms of motion. |

Each phase is shippable on its own — the ball is usable as a toy at every stage.
