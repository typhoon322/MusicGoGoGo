#include "ui/board_rgb_led.h"

#if defined(BOARD_S3_DEV)

#include <Adafruit_NeoPixel.h>
#include <Arduino.h>

#include "config.h"

namespace {
// DevKitC-1: v1.1 → GPIO38, v1.0 → GPIO48. Drive both with separate drivers
// (Arduino neopixelWrite() only binds RMT to the first pin forever).
Adafruit_NeoPixel px38(1, 38, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel px48(1, 48, NEO_GRB + NEO_KHZ800);
uint8_t lastR_ = 255;
uint8_t lastG_ = 255;
uint8_t lastB_ = 255;

void writeBoth_(uint8_t r, uint8_t g, uint8_t b) {
  const uint32_t c = px38.Color(r, g, b);
  px38.setPixelColor(0, c);
  px48.setPixelColor(0, c);
  px38.show();
  px48.show();
}
}  // namespace

void boardRgbBegin() {
  px38.begin();
  px48.begin();
  px38.setBrightness(96);
  px48.setBrightness(96);
  writeBoth_(0, 0, 0);
  delay(30);
  writeBoth_(0, 0, 200);
  delay(200);
  writeBoth_(0, 200, 0);
  delay(200);
  writeBoth_(0, 0, 0);
  lastR_ = lastG_ = lastB_ = 0;
  Serial.println(F("[rgb] NeoPixel drivers on GPIO38+48 (kick=blue snare=green)"));
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
    b = static_cast<uint8_t>(70.0f + kickPulse * 185.0f);
  }
  if (snarePulse > 0.12f) {
    g = static_cast<uint8_t>(60.0f + snarePulse * 195.0f);
  }
  if (kickPulse > 0.55f && snarePulse > 0.55f) {
    r = 70;
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
