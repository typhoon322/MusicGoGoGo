#include "ui/board_rgb_led.h"

#if defined(BOARD_S3_DEV)

#include <Arduino.h>

#include "config.h"

namespace {
uint8_t lastR_ = 255;
uint8_t lastG_ = 255;
uint8_t lastB_ = 255;
}  // namespace

void boardRgbBegin() {
  // Clear both common DevKitC-1 RGB pins once (v1.1=38, v1.0=48).
  neopixelWrite(PIN_RGB_LED, 0, 0, 0);
  if (PIN_RGB_LED != 48) {
    neopixelWrite(48, 0, 0, 0);
  }
  lastR_ = lastG_ = lastB_ = 0;
  Serial.printf("[rgb] onboard NeoPixel pin=%u (kick=blue snare=green)\n",
                static_cast<unsigned>(PIN_RGB_LED));
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
    b = static_cast<uint8_t>(40.0f + kickPulse * 180.0f);
  }
  if (snarePulse > 0.12f) {
    g = static_cast<uint8_t>(30.0f + snarePulse * 190.0f);
  }
  if (kickPulse > 0.55f && snarePulse > 0.55f) {
    r = 50;
  }

  if (r == lastR_ && g == lastG_ && b == lastB_) {
    return;
  }
  lastR_ = r;
  lastG_ = g;
  lastB_ = b;
  neopixelWrite(PIN_RGB_LED, r, g, b);
}

#endif  // BOARD_S3_DEV
