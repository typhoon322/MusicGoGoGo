#include "display/display_driver.h"

#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>
#include <math.h>

#include "config.h"

namespace {
constexpr int kHeaderH = 28;
constexpr int kSpectrumTop = 32;
constexpr int kSpectrumBottom = TFT_HEIGHT - 4;
constexpr int kMarginL = 4;
constexpr int kMarginR = 4;

Adafruit_ST7789 tft(PIN_TFT_CS, PIN_TFT_DC, PIN_TFT_RST);

const uint16_t kBg = ST77XX_BLACK;
const uint16_t kText = ST77XX_WHITE;
const uint16_t kAccent = ST77XX_CYAN;
const uint16_t kGrid = 0x4208;  // dark grey RGB565
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
  tft.fillScreen(kBg);
  tft.setTextWrap(false);

  initialized_ = true;
  Serial.printf("[tft] OK %dx%d st7789 mosi=%d sck=%d cs=%d\n", TFT_WIDTH, TFT_HEIGHT,
                PIN_TFT_MOSI, PIN_TFT_SCK, PIN_TFT_CS);
  return true;
}

void DisplayDriver::setBacklight(uint8_t level) {
  backlightLevel_ = level;
  analogWrite(PIN_TFT_BL, level);
}

void DisplayDriver::showSplash() {
  if (!initialized_) {
    return;
  }
  tft.fillScreen(kBg);
  tft.setTextColor(kAccent);
  tft.setTextSize(3);
  tft.setCursor(24, 90);
  tft.print(F("MusicGoGoGo"));
  tft.setTextSize(2);
  tft.setTextColor(kText);
  tft.setCursor(72, 130);
  tft.print(F("Starting..."));
}

void DisplayDriver::showError(const char *message) {
  if (!initialized_) {
    return;
  }
  tft.fillScreen(kBg);
  tft.setTextColor(ST77XX_RED);
  tft.setTextSize(2);
  tft.setCursor(8, 100);
  tft.print(F("Error"));
  tft.setTextColor(kText);
  tft.setTextSize(1);
  tft.setCursor(8, 130);
  tft.print(message != nullptr ? message : "unknown");
}

uint16_t DisplayDriver::barColor_(float level) const {
  if (level < 0.33f) {
    return ST77XX_GREEN;
  }
  if (level < 0.66f) {
    return ST77XX_YELLOW;
  }
  return ST77XX_RED;
}

void DisplayDriver::drawHeader_(float rms, float peak) {
  tft.fillRect(0, 0, TFT_WIDTH, kHeaderH, kBg);
  tft.setTextColor(kAccent);
  tft.setTextSize(1);
  tft.setCursor(4, 4);
  tft.print(F("MusicGoGoGo"));
  tft.setTextColor(kText);
  tft.setCursor(4, 16);
  tft.print(F("RMS "));
  tft.print(rms, 3);
  tft.print(F("  Peak "));
  tft.print(peak, 3);
}

void DisplayDriver::updateSpectrum(const float *levels, size_t count, float rms, float peak) {
  if (!initialized_ || levels == nullptr || count == 0) {
    return;
  }
  if (count > 64) {
    count = 64;
  }

  for (size_t i = 0; i < count; ++i) {
    const float target = levels[i];
    if (target > displayLevels_[i]) {
      displayLevels_[i] = target;
    } else {
      displayLevels_[i] *= SPECTRUM_DECAY;
    }
    if (displayLevels_[i] < 0.01f) {
      displayLevels_[i] = 0.0f;
    }
  }
  displayCount_ = count;

  drawHeader_(rms, peak);

  const int areaW = TFT_WIDTH - kMarginL - kMarginR;
  const int areaH = kSpectrumBottom - kSpectrumTop;
  tft.drawFastHLine(kMarginL, kSpectrumBottom, areaW, kGrid);

  const int gap = 1;
  const int barW = (areaW - static_cast<int>(count - 1) * gap) / static_cast<int>(count);
  if (barW < 1) {
    return;
  }

  for (size_t i = 0; i < count; ++i) {
    const int x = kMarginL + static_cast<int>(i) * (barW + gap);
    const float norm = displayLevels_[i];
    if (norm > 1.0f) {
      displayLevels_[i] = 1.0f;
    }
    int barH = static_cast<int>(norm * static_cast<float>(areaH));
    if (barH > areaH) {
      barH = areaH;
    }

    tft.fillRect(x, kSpectrumTop, barW, areaH, kBg);
    if (barH > 0) {
      const int y = kSpectrumBottom - barH;
      tft.fillRect(x, y, barW, barH, barColor_(norm));
    }
  }
}
