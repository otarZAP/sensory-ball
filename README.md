# Sensory Ball

A bobblehead-style sensory toy: a hollow printed sphere on a long spring, with an IMU and an RGB LED inside. Smack it, the spring oscillates, and the LED color tracks the motion intensity — calm blue when nearly at rest, climbing through green and yellow to red on a hard impact, then fading naturally as the motion damps out.

Built as an assistive sensory toy for a child with a visual impairment, who is drawn to slow, rhythmic motion — pendulums especially — and to high-contrast diffused light. The combination of tactile smack + spring oscillation + correlated glow is designed to engage three sensory channels at once.

![status: prototype](https://img.shields.io/badge/status-v1%20prototype-orange) ![license](https://img.shields.io/badge/license-MIT-blue)

---

## How it works

1. The **MPU-6050** streams 3-axis acceleration over I²C at ~50 Hz.
2. Firmware computes the **vector magnitude** of the acceleration. At rest this always equals gravity (~9.81 m/s²) regardless of orientation, so subtracting that constant yields a clean "activity" signal that is zero when still.
3. An **exponential moving average** (α = 0.15) smooths the signal — that's what gives the LED its natural decay as the spring settles, instead of switching off the instant motion stops.
4. The smoothed value is mapped through a piecewise color arc — **blue → cyan → green → yellow → red** — with brightness scaled by activity.
5. PWM on three GPIO pins (D0/D1/D2) drives the RGB LED through current-limiting resistors.

The gravity-magnitude trick is the part I'm proudest of: because we use `‖a‖ - g` rather than picking an axis, the ball can be flipped, tilted, or mounted at any angle and behaves identically. No calibration required after assembly.

---

## Repository layout

```
Sensory_Ball/
├── README.md                       this file
├── docs/
│   ├── V2_DESIGN.md                v2 design document (pendulum-centric, multi-output)
│   ├── wiring-diagram.html         v1 wiring diagram (single RGB LED)
│   ├── wiring-diagram-v2.html      v2 wiring diagram (NeoPixel + audio + haptics)
│   └── ASSEMBLY.md                 step-by-step build guide for v1
└── firmware/
    └── sensory_ball_v1/
        └── sensory_ball_v1.ino     v1 Arduino sketch (shipped)
```

> **v1 wiring diagram:** [view rendered](https://otarzap.github.io/sensory-ball/docs/wiring-diagram.html) · [source](docs/wiring-diagram.html) — the build that currently runs.
> **v2 design + wiring:** [`V2_DESIGN.md`](docs/V2_DESIGN.md) and v2 wiring diagram — [view rendered](https://otarzap.github.io/sensory-ball/docs/wiring-diagram-v2.html) · [source](docs/wiring-diagram-v2.html) — the next revision, not yet built.

---

## Hardware

| Component | Choice | Why |
|-----------|--------|-----|
| MCU | **Seeed XIAO ESP32S3** | 21 × 17.5 mm with onboard LiPo charging — no separate TP4056 module needed. Dual-core ESP32-S3 has all the headroom needed for the IMU math and smooth PWM fading. |
| IMU | **MPU-6050** (GY-521 breakout) | 3-axis accel + gyro over I²C, well-supported by Adafruit's library. Gyro is unused in v1 but available for future motion classification. |
| Light | **Single 4-pin diffused RGB LED** + 3× 220 Ω | Simplest possible v1. Easy to upgrade to a WS2812B ring later without rewiring anything else. |
| Power | **1S LiPo, ~450 mAh** | ~12–15 h runtime at typical mixed activity. Charges via USB-C on the XIAO. |
| Shell | **80 mm hollow sphere, two halves, PETG** | Split at the equator, bayonet/thread close. PETG over PLA for impact resistance. Wall 1.2–1.6 mm for diffusion. |
| Mount | **Compression spring** ~60–80 mm free length, ~0.4 N/mm | Bobblehead feel — slow oscillation, satisfying smack response. |

See [`docs/ASSEMBLY.md`](docs/ASSEMBLY.md) for the full bill of materials and printer settings.

---

## Firmware

Single file: [`firmware/sensory_ball_v1/sensory_ball_v1.ino`](firmware/sensory_ball_v1/sensory_ball_v1.ino).

**Dependencies** (install via Arduino Library Manager):
- `Adafruit MPU6050`
- `Adafruit Unified Sensor`
- `Adafruit BusIO`

**Board package:** Espressif ESP32, target **XIAO_ESP32S3**.

Three tuning constants near the top of the sketch:

| Constant | Default | Effect |
|----------|---------|--------|
| `MAX_ACTIVITY` | 25.0 m/s² | Smack intensity that lights the LED full red. Lower for more sensitivity. |
| `ALPHA`        | 0.15     | Fade speed. Lower = longer decay; higher = snappier. |
| `REST_THRESHOLD` | 1.2 m/s² | Dead zone — below this, LED is off. |

Open the Serial Monitor at 115200 baud after upload to watch live `mag`, `activity`, `smoothed`, and normalized `t` values while tuning. The startup red→green→blue sweep confirms the LED wiring before anything else runs.

---

## Design rationale

A short tour of the non-obvious decisions:

- **Why magnitude of acceleration, not Z-axis?** Magnitude is invariant under orientation. A bobbling ball rotates as it bounces, so any single-axis approach would give wildly different readings for the same impact depending on how the ball happens to be tilted.
- **Why exponential smoothing instead of just averaging?** Exponential is one multiply + one add per sample and has no buffer to manage. The α parameter is also more intuitive than a window length: "fraction of the new value to trust per tick."
- **Why a single RGB LED for v1 and not the addressable strip I have?** The 110 ft / 200 ft strips on hand are designed to be glued to walls in long runs, not coiled into a fist-sized sphere. A single LED is enough to validate the firmware and tuning loop. Adding a NeoPixel ring later is a one-pin firmware change.
- **Why PETG, not PLA?** This toy gets smacked. PLA is stiff and embrittles slightly with handling; PETG is more impact-forgiving with only a small cost in print finish and diffusion uniformity.
- **Why split the sphere instead of printing it closed?** A closed sphere needs internal supports that are difficult to clean out, and it would have to be cut open anyway to install electronics. Two flat-side-down hemispheres print cleanly with no supports.

---

## Status & next steps

**v1 (shipped)** — single RGB LED, manual sphere assembly, smacks-to-light works end-to-end. Tuning constants exposed for per-child adjustment.

**v2 (designed, not built)** — pendulum-centric sensory platform. Observation from v1 use: the real engagement is with *pendulum oscillation at specific frequencies*, deliberately modulated by the user, rather than with impact events. v2 reframes the ball as a pendulum first, smack-detector second:

- Real-time **pendulum frequency / amplitude / phase estimator** from the IMU stream.
- **12-pixel WS2812B NeoPixel ring** for smooth phase-synchronized animations.
- **DFPlayer Mini + 8 Ω speaker** for an audio channel that mirrors the rhythm.
- **Xbox-controller rumble motor** (via 2N7000 MOSFET) for tactile feedback through the spring transmission.
- Selectable **programs** (pendulum-sync, smack-burst, gentle-roll, calm-breathe) with NVS persistence.
- **Companion PWA over BLE** (Web Bluetooth) for live tuning and program selection.
- *(later)* motion-wake light sleep, session logging, engagement view.

See [`docs/V2_DESIGN.md`](docs/V2_DESIGN.md) for the full spec, pin map, power topology, and phase plan, and the [v2 wiring schematic](https://otarzap.github.io/sensory-ball/docs/wiring-diagram-v2.html).

---

## License

MIT. Build one for your kid.
