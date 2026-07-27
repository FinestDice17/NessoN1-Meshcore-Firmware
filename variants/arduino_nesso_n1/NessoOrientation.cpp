#include "NessoOrientation.h"
#include <Wire.h>
#include <SparkFun_BMI270_Arduino_Library.h>
#include "target.h"

static BMI270 nessoImu;
static bool nessoImuOk = false;
static float accumRotation = 0;
static unsigned long lastPollMillis = 0;
static unsigned long cooldownUntilMillis = 0;
static bool isFlipped = false;

// Tuned against real on-device gyroZ traces of deliberate flips: incidental
// handling (picking the device up, minor jostling) tops out around 30 deg/s
// and only accumulates a few degrees total, while stationary/idle drift
// stays under ~1 deg/s. Genuine flips varied widely in speed across test
// attempts (peaks from ~100 to ~390 deg/s) -- a lower trigger threshold and
// gentler idle decay make moderate-paced flips register reliably too,
// while staying well clear of the ~30 deg/s incidental-motion ceiling.
static constexpr float GYRO_NOISE_THRESHOLD_DPS = 15.0f;
static constexpr float FLIP_TRIGGER_DEGREES = 90.0f;
static constexpr unsigned long POLL_INTERVAL_MS = 20;
static constexpr unsigned long POST_FLIP_COOLDOWN_MS = 400;

bool nessoOrientationBegin() {
  nessoImuOk = (nessoImu.beginI2C(BMI2_I2C_PRIM_ADDR) == BMI2_OK);
  return nessoImuOk;
}

void nessoOrientationLoop() {
  if (!nessoImuOk) return;

  unsigned long now = millis();
  if (lastPollMillis != 0 && now - lastPollMillis < POLL_INTERVAL_MS) return;
  float dt = (lastPollMillis == 0) ? 0 : (now - lastPollMillis) / 1000.0f;
  lastPollMillis = now;
  if (dt <= 0 || dt > 0.5f) return;  // skip first sample / long stalls

  if (now < cooldownUntilMillis) {
    // still settling from a flip that just triggered -- ignore the tail
    // end of that same physical motion instead of feeding it into the
    // next cycle's accumulator
    return;
  }

  nessoImu.getSensorData();
  float gz = nessoImu.data.gyroZ;

  if (fabsf(gz) > GYRO_NOISE_THRESHOLD_DPS) {
    accumRotation += gz * dt;
  } else {
    // idle: gently bleed off any small accumulated bias/noise rather than
    // letting it drift towards the trigger threshold over long periods,
    // without erasing real progress from a momentary pause mid-flip
    accumRotation *= 0.97f;
  }

  if (fabsf(accumRotation) >= FLIP_TRIGGER_DEGREES) {
    isFlipped = !isFlipped;
    display.setFlipped(isFlipped);
    accumRotation = 0;
    cooldownUntilMillis = now + POST_FLIP_COOLDOWN_MS;
  }
}
