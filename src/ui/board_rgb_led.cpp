#include "ui/board_rgb_led.h"

#if defined(BOARD_S3_DEV)

#include <Arduino.h>

#include "config.h"

namespace {
uint8_t lastR_ = 255;
uint8_t lastG_ = 255;
uint8_t lastB_ = 255;

void writeBoth_(uint8_t r, uint8_t g, uint8_t b) {
  // DevKitC-1 v1.1 = GPIO38, v1.0 = GPIO48 — drive both so either board flashes.
  neopixelWrite(38, r, g, b);
  neopixelWrite(48, r, g, b);
}
}  // namespace

void boardRgbBegin() {
  writeBoth_(0, 0, 0);
  delay(20);
  // Boot self-test: blue then green so wiring is obvious.
  writeBoth_(0, 0, 180);
  delay(180);
  writeBoth_(0, 180, 0);
  delay(180);
  writeBoth_(0, 0, 0);
  lastR_ = lastG_ = lastB_ = 0;
  Serial.println(F("[rgb] onboard NeoPixel: kick=blue snare=green (GPIO38+48)"));
}

void boardRgbUpdate(float kickPulse, float snarePulse) {
  if (kickPulse < 0.0f) {
    kickPulse = 0.0f;
  }
  if (kickPulse > 1.0f) {
    kickPulse = 1.0f;
  }
  if (snarePulse < 0.0f) {
    snarePulse = 0.0f;
  }
  if (snarePulse > 1.0f) {
    snarePulse = 1.0f;
  }

  uint8_t r = 0;
  uint8_t g = 0;
  uint8_t b = 0;
  if (kickPulse > 0.12f) {
    b = static_cast<uint8_t>(60.0f + kickPulse * 195.0f);
  }
  if (snarePulse > 0.12f) {
    g = static_cast<uint8_t>(50.0f + snarePulse * 205.0f);
  }
  if (kickPulse > 0.55f && snarePulse > 0.55f) {
    r = 60;
  }

  if (r == lastR_ && g == lastG_ && b == lastB_) {
    return;
  }
  lastR_ = r;
  lastG_ = g;
  lastB_ = b;
  writeBoth_(r, g, b);
}

#endif  // BOARD_S3_DEV
