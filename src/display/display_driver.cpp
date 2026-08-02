#include "display/display_driver.h"

#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>

#include "config.h"
#include "display/vfx_renderer.h"

namespace {
Adafruit_ST7789 tft(PIN_TFT_CS, PIN_TFT_DC, PIN_TFT_RST);
VfxRenderer vfx;
}  // namespace

bool DisplayDriver::begin(uint8_t backlightLevel) {
  pinMode(PIN_TFT_BL, OUTPUT);
  setBacklight(backlightLevel);

  SPI.begin(PIN_TFT_SCK, -1, PIN_TFT_MOSI, PIN_TFT_CS);
  tft.init(TFT_NATIVE_W, TFT_NATIVE_H);
  if (TFT_X_OFFSET != 0 || TFT_Y_OFFSET != 0) {
    tft.setAddrWindow(TFT_X_OFFSET, TFT_Y_OFFSET, TFT_WIDTH, TFT_HEIGHT);
  }
  tft.setRotation(1);
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextWrap(false);

  vfx.attach(&tft);
  initialized_ = true;
  Serial.printf("[tft] OK %dx%d st7789\n", TFT_WIDTH, TFT_HEIGHT);
  return true;
}

void DisplayDriver::setBacklight(uint8_t level) {
  backlightLevel_ = level;
  analogWrite(PIN_TFT_BL, level);
}

void DisplayDriver::nextMode() {
  const auto next = static_cast<VfxMode>((static_cast<uint8_t>(mode_) + 1) %
                                         static_cast<uint8_t>(VfxMode::Count));
  setMode(next);
}

void DisplayDriver::prevMode() {
  const uint8_t cur = static_cast<uint8_t>(mode_);
  const uint8_t count = static_cast<uint8_t>(VfxMode::Count);
  const auto prev = static_cast<VfxMode>((cur + count - 1) % count);
  setMode(prev);
}

void DisplayDriver::setMode(VfxMode mode) {
  if (mode_ != mode) {
    mode_ = mode;
    waterfallHead_ = 0;
    vfx.resetWaterfall();
    Serial.printf("[vfx] mode -> %s\n", vfxModeName(mode_));
  }
}

void DisplayDriver::showSplash() {
  if (!initialized_) {
    return;
  }
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_CYAN);
  tft.setTextSize(3);
  tft.setCursor(24, 80);
  tft.print(F("MusicGoGoGo"));
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(56, 120);
  tft.print(F("VFX Loading..."));
}

void DisplayDriver::showError(const char *message) {
  if (!initialized_) {
    return;
  }
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_RED);
  tft.setTextSize(2);
  tft.setCursor(8, 100);
  tft.print(F("Error"));
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(1);
  tft.setCursor(8, 130);
  tft.print(message != nullptr ? message : "unknown");
}

void DisplayDriver::render(const SpectrumFrame &spec, const float *levels, const float *peaks,
                           size_t count, float rms, float peak) {
  if (!initialized_ || levels == nullptr || peaks == nullptr || count == 0) {
    return;
  }

  VfxDrawContext ctx;
  ctx.rms = rms;
  ctx.peak = peak;
  ctx.vu = spec.vuLevel;
  ctx.frameMs = millis();
  ctx.mode = mode_;

  vfx.draw(ctx, mode_, levels, peaks, count, waterfallHistory_, waterfallHead_);
}
