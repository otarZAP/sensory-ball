# Assembly Guide

Step-by-step build of the Sensory Ball v1. Plan on ~2 hours for the electronics if you've soldered before, plus print time for the shell.

---

## 1. Bill of materials

| Qty | Part | Notes |
|----:|------|-------|
| 1 | **XIAO ESP32S3** (plain — not Sense) | Has onboard LiPo charging on `BAT+/BAT−` pads. |
| 1 | **MPU-6050** on GY-521 breakout | 3-axis accelerometer + gyro, I²C. |
| 1 | **4-pin diffused RGB LED**, common cathode | Common anode also works — flip one flag in the sketch. |
| 3 | **220 Ω resistors** (1/4 W) | One per RGB channel. |
| 1 | **1S LiPo, ~450 mAh** | Anything 300–600 mAh in roughly 35 × 20 × 5 mm fits. |
| 1 | **SPST slide switch** (optional) | Inline on the `BAT+` lead for a hard off-switch. |
| 1 | **Compression spring**, ~60–80 mm free length, ~0.4 N/mm | Bobblehead feel. Hardware store or McMaster. |
| 1 | **Printed sphere shell**, two halves | PETG recommended over PLA for impact resistance. |
| 1 | **Printed base** | Houses the lower end of the spring. |
| — | Thin silicone wire (28–30 AWG) | Easier to route inside a tight sphere than solid-core. |
| — | Heat-shrink, small zip ties or hot glue | Strain relief and stack mounting. |

The 110ft / 200ft addressable RGB strips in the parts inventory are out of scope for v1. A single 4-pin LED is simpler to wire and validate; swap to a WS2812B ring in v2 once the rest of the system is proven.

---

## 2. Print the shell

**Recommended slicer settings (Bambu P1S, PETG):**

| Setting | Value |
|--------|-------|
| Wall thickness | **1.2–1.6 mm** (3–4 walls @ 0.4 mm nozzle) |
| Infill | 15% gyroid (the electronics sit in a pocket, infill is just for shell rigidity) |
| Layer height | 0.2 mm |
| Orientation | Each hemisphere flat-side down — **no supports needed** |
| Top/bottom layers | 4 |

Split the sphere at the equator. The recommended diameter is **~80 mm** outside, which leaves comfortable room for the electronics stack (~50 mm tall by ~25 mm wide) plus wall thickness on all sides.

**Diffusion test before printing the full shell:** slice a 40 × 40 mm flat tile at your chosen wall thickness and shine a phone flashlight through it. PETG can read slightly translucent-gray at 1.2 mm; if hotspots are visible, bump to 1.6 mm or add a fourth wall.

The two halves should join with a quarter-turn bayonet or a coarse-pitch thread so it's tool-free to open for charging but won't fly apart when smacked.

---

## 3. Solder the electronics

### Step 3.1 — Stack layout

The internal stack from top to bottom inside the sphere:

```
    LED (top — closest to the user)
     ↓
   resistors (3× 220Ω, can be flush against MCU board)
     ↓
   XIAO ESP32S3
     ↓
   MPU-6050 breakout (mounted as low and central as possible —
                       the accelerometer benefits from being near
                       the ball's center of mass, not on top of it)
     ↓
   LiPo cell (taped to bottom hemisphere, near spring boss)
```

You don't need a perfboard. Hot glue and short silicone-wire jumpers are sufficient and lighter than a board sandwich.

### Step 3.2 — Wire the LED first

Solder a 220 Ω resistor to each of the R, G, B leads on the LED (positions 1, 3, 4 — leave the long common cathode leg straight).

- Position 1 (R) → 220 Ω → wire → **D0**
- Position 3 (G) → 220 Ω → wire → **D1**
- Position 4 (B) → 220 Ω → wire → **D2**
- Position 2 (common −, longest leg) → wire → **GND**

**Verify polarity:** common cathode LEDs light when the long center leg is the most negative pin. If you're not sure, briefly touch the long leg to GND and any other leg to 3.3 V through a 220 Ω resistor — if it lights, it's common cathode. If you have to reverse that to get light, it's common anode → set `#define COMMON_ANODE true` in the sketch.

### Step 3.3 — Wire the MPU-6050

Five wires:

| MPU-6050 | XIAO ESP32S3 |
|----------|--------------|
| VCC | 3V3 |
| GND | GND |
| SDA | D4 |
| SCL | D5 |
| AD0 | GND |

Tying AD0 to GND locks the I²C address to `0x68`, which is what the Adafruit MPU6050 library probes for by default.

### Step 3.4 — Wire the LiPo

Solder the LiPo's red lead to the `BAT+` pad on the **underside** of the XIAO, and the black lead to `BAT−`. These pads are small — use a fine tip and tin both surfaces first.

If you want an external power switch on the base, cut the red lead and put an SPST switch inline.

> **LiPo safety:** never solder with the cell connected — solder both pad ends first, then bring the cell in and join with a 5–10 mm pigtail. Cover the connection in heat-shrink. Don't pinch the cell under the MCU; route it alongside.

---

## 4. Flash the firmware

### Step 4.1 — Arduino IDE setup

1. **Install the Seeed XIAO ESP32S3 board package.** In Arduino IDE, *File → Preferences → Additional Board Manager URLs*, add:
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
   Then *Tools → Board → Boards Manager* → search "esp32" → install **esp32 by Espressif Systems**.

2. **Select the board.** *Tools → Board → ESP32 Arduino → XIAO_ESP32S3*.

3. **Install libraries.** *Tools → Manage Libraries…* and install:
   - `Adafruit MPU6050`
   - `Adafruit Unified Sensor`
   - `Adafruit BusIO` (usually auto-installed as a dependency)

### Step 4.2 — Upload

1. Plug the XIAO into your PC over USB-C.
2. Select the correct COM port under *Tools → Port*.
3. Open `firmware/sensory_ball_v1/sensory_ball_v1.ino`.
4. Click **Upload**.

If upload fails with "no DTR/RTS" or similar, hold the XIAO's **BOOT** button, tap **RESET**, release **BOOT**, then retry upload.

### Step 4.3 — Smoke test

The sketch does a red → green → blue sweep on startup. If you see all three colors at boot, your LED, resistors, and pin assignments are correct.

After the sweep, open *Tools → Serial Monitor* at **115200 baud**. You should see:

```
MPU-6050 ready. Waiting for activity...
mag: 9.81  activity: 0.00  smoothed: 0.00  t: 0.000
mag: 9.83  activity: 0.02  smoothed: 0.00  t: 0.000
```

Pick the breadboard up and shake it. `mag` should jump well above 10, `activity` should climb, `smoothed` should follow with a lag, and the LED should fade blue → green → red proportional to how hard you shake it.

**Failure modes:**
- **Red rapid-flash forever:** MPU-6050 not detected. Check SDA/SCL aren't swapped, VCC is on 3V3, GND is shared, AD0 is grounded.
- **No light at all:** check `COMMON_ANODE` flag, verify resistor solder joints, check that the cathode leg actually reaches GND.
- **One color always on:** that channel's pin is shorted to its supply rail — usually a solder bridge.
- **`mag` stuck near 0:** I²C is talking but the sensor isn't streaming. Re-check the wiring of all five MPU pins.

---

## 5. Tune

Three constants in the sketch you may want to adjust after the first session with the child using it:

| Constant | Effect | Good starting range |
|---------|--------|---------------------|
| `MAX_ACTIVITY` | Acceleration (m/s²) that maps to full brightness red. Lower = LED redlines on small motion. | 15 (sensitive) → 40 (need a hard smack) |
| `ALPHA` | Exponential smoothing. Lower = longer fade after motion stops. | 0.05 (long dreamy fade) → 0.3 (snappy) |
| `REST_THRESHOLD` | Activity (m/s²) below which the LED is fully off. Higher = more dead zone, less flicker from passive motion. | 0.8 → 2.0 |

Watch the Serial Monitor while tuning — `t` is the normalized 0–1 input to the color mapping. If `t` never gets above ~0.4 even on a hard smack, lower `MAX_ACTIVITY`. If it's pinned at 1.0 the whole time, raise it.

---

## 6. Mechanical assembly

1. Press-fit or epoxy the spring's top coil into a printed boss on the **inside** of the bottom hemisphere. Don't go through the wall.
2. Press-fit the spring's bottom coil into a matching boss on the base.
3. Tape the LiPo to the inside floor of the bottom hemisphere near the spring boss (low center of mass = better bobble dynamics).
4. Hot-glue the MPU-6050 flat against the inside floor, centered as best you can — accelerometer readings improve when the IMU is near the ball's geometric center.
5. Glue or zip-tie the XIAO above the MPU, with the LED poking up toward the top hemisphere.
6. Snake all wires gently. Avoid kinking the LiPo leads.
7. Close the sphere. The bayonet/thread closure should hold against expected impact.

A small access port in the base or the bottom hemisphere for the USB-C cable lets you charge without opening the ball.

---

## 7. v2 ideas (deferred)

- **NeoPixel ring or 8× WS2812B cluster** for much brighter, even glow at the same diameter. Single-pin control means fewer wires inside the sphere.
- **Sleep mode:** put the ESP32 into light sleep when `smoothed < REST_THRESHOLD` for >30 s, wake on motion via the MPU-6050's INT pin. Could 10× the battery life.
- **Sound:** small piezo or DFPlayer Mini playing chimes correlated with motion magnitude.
- **Haptic feedback:** vibration motor for tactile response in addition to light.
- **Configurable color themes:** several palettes (calm pastels, high-contrast primaries, single-color "lava") cycleable by a button on the base.
