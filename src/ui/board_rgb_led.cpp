#include "ui/board_rgb_led.h"

#if defined(BOARD_S3_DEV)

#include <Adafruit_NeoPixel.h>
#include <Arduino.h>

#include "config.h"

namespace {
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

float clamp01_(float v) {
  if (v < 0.0f) {
    return 0.0f;
  }
  if (v > 1.0f) {
    return 1.0f;
  }
  return v;
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
  Serial.println(F("[rgb] NeoPixel GPIO38+48 (flash on beat only)"));
}

void boardRgbUpdate(float beatPulse, float /*bassLevel*/, float /*highLevel*/) {
  beatPulse = clamp01_(beatPulse);
  // Beat-only flash — continuous wash looked like chaotic blinking.
  uint8_t r = 0;
  uint8_t g = 0;
  uint8_t b = 0;
  if (beatPulse > 0.15f) {
    const uint8_t flash = static_cast<uint8_t>(40.0f + beatPulse * 215.0f);
    r = g = b = flash;
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
