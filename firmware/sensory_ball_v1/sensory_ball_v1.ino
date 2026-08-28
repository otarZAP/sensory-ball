/*
 * Sensory Ball v1
 * XIAO ESP32S3 + MPU-6050 + single RGB LED
 *
 * Logic:
 *   - Read accelerometer magnitude
 *   - Subtract gravity baseline (~9.8 m/s²) to isolate "activity"
 *   - Exponential smooth it so the LED fades naturally as it comes to rest
 *   - Map activity level to color: blue (calm) → green → red (big smack)
 *
 * Wiring:
 *   MPU-6050  → XIAO ESP32S3
 *     VCC     → 3V3
 *     GND     → GND
 *     SDA     → D4 (GPIO5)
 *     SCL     → D5 (GPIO6)
 *     AD0     → GND  (sets I2C address to 0x68)
 *     INT     → not connected (not used here)
 *
 *   RGB LED (common cathode assumed — see COMMON_ANODE flag below)
 *     R pin   → 220Ω resistor → D0
 *     G pin   → 220Ω resistor → D1
 *     B pin   → 220Ω resistor → D2
 *     GND pin → GND
 *
 * Libraries required (install via Arduino Library Manager):
 *   - Adafruit MPU6050
 *   - Adafruit Unified Sensor
 *   - Adafruit BusIO (dependency, usually auto-installed)
 *
 * Board: "XIAO_ESP32S3" via Seeed Studio XIAO ESP32S3 board package
 */

#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <math.h>

// ── Pin definitions ───────────────────────────────────────────────────────────
#define PIN_RED   D0
#define PIN_GREEN D1
#define PIN_BLUE  D2

// ── LED polarity ──────────────────────────────────────────────────────────────
// Common cathode = false (GND shared, PWM drives HIGH to brighten)
// Common anode   = true  (VCC shared, PWM drives LOW to brighten → logic inverts)
// Not sure? Probe with a multimeter: the longest leg is usually common.
// Common cathode is more typical for 4-pin diffused RGB LEDs.
#define COMMON_ANODE false

// ── Tuning constants ──────────────────────────────────────────────────────────
// Gravity baseline in m/s² — calibrate if your IMU reads slightly off
#define GRAVITY_MS2     9.81f

// Any activity below this (m/s²) is treated as "at rest" — light off
#define REST_THRESHOLD  1.2f

// Activity level that maps to full brightness (clamp ceiling)
// A hard smack might hit 30–50 m/s² peak; 25 gives a good dynamic range
#define MAX_ACTIVITY    25.0f

// Exponential smoothing factor (0.0–1.0)
// Lower = slower fade (more inertia), Higher = snappier response
// 0.15 feels natural for a ball on a spring; try 0.05–0.3
#define ALPHA           0.15f

// Loop period in ms — 20ms = 50Hz, fast enough for smooth fading
#define LOOP_MS         20

// ── Globals ───────────────────────────────────────────────────────────────────
Adafruit_MPU6050 mpu;
float smoothed = 0.0f;

// ── Helpers ───────────────────────────────────────────────────────────────────

// Write RGB values 0–255, respecting common anode/cathode polarity
void setRGB(uint8_t r, uint8_t g, uint8_t b) {
  if (COMMON_ANODE) {
    analogWrite(PIN_RED,   255 - r);
    analogWrite(PIN_GREEN, 255 - g);
    analogWrite(PIN_BLUE,  255 - b);
  } else {
    analogWrite(PIN_RED,   r);
    analogWrite(PIN_GREEN, g);
    analogWrite(PIN_BLUE,  b);
  }
}

/*
 * Map a normalized activity value t (0.0 = rest, 1.0 = max smack)
 * to an RGB color with brightness scaling.
 *
 * Color arc: black → blue → cyan → green → yellow → red
 * The LED is essentially off at rest and peaks to full saturation
 * color at max activity.
 *
 * Teaching note: this is a simple two-segment HSV-like interpolation.
 * A cleaner version would do a proper HSV→RGB conversion, but this
 * avoids the math overhead and is easy to tweak per segment.
 */
void activityToRGB(float t) {
  // Dead zone — don't flicker near rest
  if (t < (REST_THRESHOLD / MAX_ACTIVITY)) {
    setRGB(0, 0, 0);
    return;
  }

  // Remap t past the dead zone to 0–1 range
  float tScaled = constrain((t - (REST_THRESHOLD / MAX_ACTIVITY))
                            / (1.0f - (REST_THRESHOLD / MAX_ACTIVITY)),
                            0.0f, 1.0f);

  uint8_t r = 0, g = 0, b = 0;

  if (tScaled < 0.33f) {
    // blue → cyan
    float f = tScaled / 0.33f;
    b = 255;
    g = (uint8_t)(255 * f);
    r = 0;
  } else if (tScaled < 0.66f) {
    // cyan → green → yellow
    float f = (tScaled - 0.33f) / 0.33f;
    g = 255;
    b = (uint8_t)(255 * (1.0f - f));
    r = (uint8_t)(255 * f);
  } else {
    // yellow → red
    float f = (tScaled - 0.66f) / 0.34f;
    r = 255;
    g = (uint8_t)(255 * (1.0f - f));
    b = 0;
  }

  // Scale brightness proportional to activity — gentle at low motion
  float brightness = 0.15f + 0.85f * tScaled; // min 15% brightness when first active
  setRGB((uint8_t)(r * brightness),
         (uint8_t)(g * brightness),
         (uint8_t)(b * brightness));
}

// ── Setup ─────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);

  pinMode(PIN_RED,   OUTPUT);
  pinMode(PIN_GREEN, OUTPUT);
  pinMode(PIN_BLUE,  OUTPUT);
  setRGB(0, 0, 0);

  // Startup sweep so you can visually confirm all 3 channels work at boot
  setRGB(255, 0, 0); delay(300);
  setRGB(0, 255, 0); delay(300);
  setRGB(0, 0, 255); delay(300);
  setRGB(0, 0, 0);

  if (!mpu.begin()) {
    Serial.println("ERROR: MPU-6050 not found. Check wiring and I2C address.");
    // Rapid red flash = wiring fault indicator
    while (true) {
      setRGB(255, 0, 0); delay(100);
      setRGB(0,   0, 0); delay(100);
    }
  }

  // ±8G range — gives headroom for a hard smack without saturating
  // ±2G would clip on impact; ±16G has lower resolution at rest
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);

  // Gyro not used for LED logic here but set it anyway for future use
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);

  // 21Hz low-pass filter on the MPU itself — kills high-freq vibration noise
  // without introducing noticeable lag at spring-bounce speeds
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  Serial.println("MPU-6050 ready. Waiting for activity...");
}

// ── Main loop ─────────────────────────────────────────────────────────────────
void loop() {
  sensors_event_t accelEvent, gyroEvent, tempEvent;
  mpu.getEvent(&accelEvent, &gyroEvent, &tempEvent);

  // Vector magnitude of acceleration
  float ax = accelEvent.acceleration.x;
  float ay = accelEvent.acceleration.y;
  float az = accelEvent.acceleration.z;
  float mag = sqrtf(ax*ax + ay*ay + az*az);

  // Subtract gravity — now "activity" is 0 at rest, positive when moving
  // Teaching note: this works in any orientation because we're using magnitude,
  // not a single axis. The ball can be tilted or flipped; doesn't matter.
  float activity = fmaxf(0.0f, mag - GRAVITY_MS2);

  // Exponential moving average — this is what gives the natural fade-to-rest
  // Teaching note: new_value = α * raw + (1-α) * old_value
  // At α=0.15: hitting 30 m/s² will fade back to near-zero in ~2-3 seconds
  smoothed = ALPHA * activity + (1.0f - ALPHA) * smoothed;

  // Normalize to 0–1 and drive the LED
  float t = constrain(smoothed / MAX_ACTIVITY, 0.0f, 1.0f);
  activityToRGB(t);

  // Debug output — open Serial Monitor at 115200 to watch values
  // Comment this out once tuned; it adds a tiny overhead
  Serial.print("mag: "); Serial.print(mag, 2);
  Serial.print("  activity: "); Serial.print(activity, 2);
  Serial.print("  smoothed: "); Serial.print(smoothed, 2);
  Serial.print("  t: "); Serial.println(t, 3);

  delay(LOOP_MS);
}
