#include "display/display_driver.h"

#include "config.h"
#include "display/tft_colors.h"
#include "display/vfx_renderer.h"

#if defined(BOARD_S3_DEV)
#include <esp_heap_caps.h>
#include <string.h>
#endif

#if defined(BOARD_CARDPUTER_ADV) && CARDPUTER_USE_BUILTIN_LCD
#include <M5Cardputer.h>
#else
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>
#endif

namespace {
#if defined(BOARD_CARDPUTER_ADV) && !CARDPUTER_USE_BUILTIN_LCD
SPIClass tftSpi(HSPI);
Adafruit_ST7789 tft(&tftSpi, PIN_TFT_CS, PIN_TFT_DC, PIN_TFT_RST);
#elif !defined(BOARD_CARDPUTER_ADV)
Adafruit_ST7789 tft(PIN_TFT_CS, PIN_TFT_DC, PIN_TFT_RST);
#endif
VfxRenderer vfx;

#if defined(BOARD_CARDPUTER_ADV) && CARDPUTER_USE_BUILTIN_LCD
VfxTft &activeTft() {
  return M5Cardputer.Display;
}
#else
VfxTft &activeTft() {
  return tft;
}
#endif
}  // namespace

bool DisplayDriver::begin(uint8_t backlightLevel) {
#if defined(BOARD_CARDPUTER_ADV) && CARDPUTER_USE_BUILTIN_LCD
  VfxTft &tft = activeTft();
  tft.setRotation(1);
  tft.setBrightness(backlightLevel);
  tft.fillScreen(TFT_COL_BLACK);
  tft.setTextWrap(false);
  vfx.attach(&tft);
  vfx.resetBarCache();
  initialized_ = true;
  Serial.printf("[tft] OK builtin %dx%d\n", TFT_WIDTH, TFT_HEIGHT);
  return true;
#else
  pinMode(PIN_TFT_RST, OUTPUT);
  digitalWrite(PIN_TFT_RST, HIGH);
  delay(10);
  digitalWrite(PIN_TFT_RST, LOW);
  delay(20);
  digitalWrite(PIN_TFT_RST, HIGH);
  delay(20);

  // ESP32-S3 backlight via LEDC PWM (analogWrite alone is a no-op until a
  // channel is attached). 8-bit resolution, ~5 kHz, channel 0.
  ledcSetup(0, 5000, 8);
  ledcAttachPin(PIN_TFT_BL, 0);
  setBacklight(backlightLevel);

#if defined(BOARD_CARDPUTER_ADV)
  Serial.println(F("[tft] init HSPI..."));
  tftSpi.end();
  tftSpi.begin(PIN_TFT_SCK, -1, PIN_TFT_MOSI, PIN_TFT_CS);
#else
  SPI.begin(PIN_TFT_SCK, -1, PIN_TFT_MOSI, PIN_TFT_CS);
#endif
  Serial.println(F("[tft] st7789 init..."));
  tft.init(TFT_NATIVE_W, TFT_NATIVE_H);
  if (TFT_X_OFFSET != 0 || TFT_Y_OFFSET != 0) {
    tft.setAddrWindow(TFT_X_OFFSET, TFT_Y_OFFSET, TFT_WIDTH, TFT_HEIGHT);
  }
  tft.setRotation(1);
  // This 3.2" ST7789 panel does not want the inversion the library forces
  // (INVON); without INVOFF the whole frame is color-inverted (black bg ->
  // white, green bars -> purple).  Send INVOFF so black stays black.
  tft.invertDisplay(false);
  tft.fillScreen(TFT_COL_BLACK);
  tft.setTextWrap(false);
  tft.setSPISpeed(60000000);
  vfx.attach(&tft);
  vfx.resetBarCache();
#if defined(BOARD_S3_DEV)
  if (waterfallHistory_ == nullptr) {
    const size_t n = static_cast<size_t>(VFX_WATERFALL_HISTORY) * VFX_WATERFALL_BINS;
    const size_t bytes = n * sizeof(float);
    waterfallHistory_ = static_cast<float *>(
        heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (waterfallHistory_ == nullptr) {
      waterfallHistory_ = static_cast<float *>(malloc(bytes));
    }
    if (waterfallHistory_ != nullptr) {
      memset(waterfallHistory_, 0, bytes);
      Serial.printf("[mem] waterfall history %u KB @ %s\n",
                    static_cast<unsigned>(bytes / 1024),
                    esp_ptr_external_ram(waterfallHistory_) ? "PSRAM" : "DRAM");
    } else {
      Serial.println(F("[mem] waterfall history alloc FAILED"));
    }
  }
#endif
  initialized_ = true;
  Serial.printf("[tft] OK %dx%d st7789 spi=60MHz\n", TFT_WIDTH, TFT_HEIGHT);
  return true;
#endif
}

void DisplayDriver::setBacklight(uint8_t level) {
  backlightLevel_ = level;
#if defined(BOARD_CARDPUTER_ADV) && CARDPUTER_USE_BUILTIN_LCD
  activeTft().setBrightness(level);
#elif defined(BOARD_CARDPUTER_ADV)
  digitalWrite(PIN_TFT_BL, level > 0 ? HIGH : LOW);
#else
  ledcWrite(0, level);
#endif
}

void DisplayDriver::setShowFreqLabels(bool on) {
  vfx.setShowFreqLabels(on);
}

bool DisplayDriver::showFreqLabels() const {
  return vfx.showFreqLabels();
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
    vfx.resetHeaderCache();
    vfx.resetBarCache();
    if (initialized_) {
      vfx.clearPlotArea();
    }
    Serial.printf("[vfx] mode -> %s\n", vfxModeName(mode_));
  }
}

#if defined(BOARD_CARDPUTER_ADV)
void DisplayDriver::toggleDebugOverlay() {
  debugOverlay_ = !debugOverlay_;
  vfx.resetHeaderCache();
  Serial.printf("[ui] debug overlay %s\n", debugOverlay_ ? "ON" : "OFF");
}
#endif

void DisplayDriver::showSplash() {
  if (!initialized_) {
    return;
  }
  VfxTft &tft = activeTft();
  tft.fillScreen(TFT_COL_BLACK);
  tft.setTextColor(TFT_COL_CYAN);
#if defined(BOARD_CARDPUTER_ADV) && CARDPUTER_USE_BUILTIN_LCD
  tft.setTextSize(2);
  tft.setCursor(8, 40);
#else
  tft.setTextSize(3);
  tft.setCursor(24, 80);
#endif
  tft.print(F("MusicGoGoGo"));
  tft.setTextSize(1);
  tft.setTextColor(TFT_COL_WHITE);
#if defined(BOARD_CARDPUTER_ADV) && CARDPUTER_USE_BUILTIN_LCD
  tft.setCursor(24, 70);
  tft.print(F("Loading..."));
#else
  tft.setTextSize(2);
  tft.setCursor(56, 120);
  tft.print(F("VFX Loading..."));
#endif
}

void DisplayDriver::showError(const char *message) {
  if (!initialized_) {
    return;
  }
  VfxTft &tft = activeTft();
  tft.fillScreen(TFT_COL_BLACK);
  tft.setTextColor(TFT_COL_RED);
  tft.setTextSize(2);
#if defined(BOARD_CARDPUTER_ADV) && CARDPUTER_USE_BUILTIN_LCD
  tft.setCursor(8, 48);
#else
  tft.setCursor(8, 100);
#endif
  tft.print(F("Error"));
  tft.setTextColor(TFT_COL_WHITE);
  tft.setTextSize(1);
#if defined(BOARD_CARDPUTER_ADV) && CARDPUTER_USE_BUILTIN_LCD
  tft.setCursor(8, 72);
#else
  tft.setCursor(8, 130);
#endif
  tft.print(message != nullptr ? message : "unknown");
}

void DisplayDriver::render(const SpectrumFrame &spec, const float *levels, const float *peaks,
                           size_t count, float rms, float peak
#if defined(BOARD_CARDPUTER_ADV)
                           ,
                           const MicDebugInfo &micDebug
#endif
) {
  if (!initialized_ || levels == nullptr || peaks == nullptr || count == 0) {
    return;
  }

  VfxDrawContext ctx;
  ctx.rms = rms;
  ctx.peak = peak;
  ctx.vu = spec.vuLevel;
  ctx.frameMs = millis();
  ctx.mode = mode_;
#if defined(BOARD_CARDPUTER_ADV)
  ctx.showMicDebug = debugOverlay_;
  ctx.batteryPercent = micDebug.batteryPercent;
  ctx.micRawMin = micDebug.rawMin;
  ctx.micRawMax = micDebug.rawMax;
  ctx.micRawMean = micDebug.rawMean;
  ctx.micGain = micDebug.gain;
  ctx.band0 = micDebug.bands[0];
  ctx.band1 = micDebug.bands[1];
  ctx.band2 = micDebug.bands[2];
  ctx.band3 = micDebug.bands[3];
#endif

  vfx.draw(ctx, mode_, levels, peaks, count, waterfallHistory_, waterfallHead_);
}
