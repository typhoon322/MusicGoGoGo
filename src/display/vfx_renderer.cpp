#include "display/vfx_renderer.h"

#include <Arduino.h>
#include <math.h>
#include <string.h>

#include "config.h"

namespace {
constexpr int kHeaderH = 24;
constexpr int kAreaTop = 26;
constexpr int kAreaBottom = TFT_HEIGHT - 2;
constexpr int kMarginL = 2;
constexpr int kMarginR = 2;
constexpr uint16_t kBg = ST77XX_BLACK;
constexpr uint16_t kText = ST77XX_WHITE;
constexpr uint16_t kAccent = ST77XX_CYAN;
constexpr uint16_t kGrid = 0x4208;
}  // namespace

void VfxRenderer::attach(Adafruit_ST7789 *tft) {
  tft_ = tft;
  if (waterfallFb_ == nullptr) {
    waterfallFb_ =
        static_cast<uint16_t *>(ps_malloc(TFT_WIDTH * VFX_WATERFALL_HISTORY * sizeof(uint16_t)));
    resetWaterfall();
  }
}

void VfxRenderer::resetWaterfall() {
  if (waterfallFb_ != nullptr) {
    memset(waterfallFb_, 0, TFT_WIDTH * VFX_WATERFALL_HISTORY * sizeof(uint16_t));
  }
}

uint16_t VfxRenderer::heatColor_(float level) const {
  if (level < 0.15f) {
    return 0x0008;
  }
  if (level < 0.35f) {
    return 0x001F;
  }
  if (level < 0.55f) {
    return 0x07FF;
  }
  if (level < 0.75f) {
    return 0x07E0;
  }
  if (level < 0.90f) {
    return 0xFFE0;
  }
  return 0xF800;
}

uint16_t VfxRenderer::rainbowColor_(float level, float hue) const {
  const float h = fmodf(hue + level * 0.25f, 1.0f);
  const float s = 0.85f;
  const float v = 0.25f + level * 0.75f;
  const int i = static_cast<int>(h * 6.0f);
  const float f = h * 6.0f - static_cast<float>(i);
  const float p = v * (1.0f - s);
  const float q = v * (1.0f - s * f);
  const float t = v * (1.0f - s * (1.0f - f));
  float r = 0, g = 0, b = 0;
  switch (i % 6) {
    case 0:
      r = v;
      g = t;
      b = p;
      break;
    case 1:
      r = q;
      g = v;
      b = p;
      break;
    case 2:
      r = p;
      g = v;
      b = t;
      break;
    case 3:
      r = p;
      g = q;
      b = v;
      break;
    case 4:
      r = t;
      g = p;
      b = v;
      break;
    default:
      r = v;
      g = p;
      b = q;
      break;
  }
  const uint8_t ri = static_cast<uint8_t>(r * 255.0f);
  const uint8_t gi = static_cast<uint8_t>(g * 255.0f);
  const uint8_t bi = static_cast<uint8_t>(b * 255.0f);
  return static_cast<uint16_t>(((ri & 0xF8) << 8) | ((gi & 0xFC) << 3) | (bi >> 3));
}

uint16_t VfxRenderer::gradientBarColor_(float level) const {
  if (level < 0.45f) {
    return ST77XX_GREEN;
  }
  if (level < 0.75f) {
    return ST77XX_YELLOW;
  }
  return ST77XX_RED;
}

void VfxRenderer::drawHeader_(const VfxDrawContext &ctx) {
  if (tft_ == nullptr) {
    return;
  }
  tft_->fillRect(0, 0, TFT_WIDTH, kHeaderH, kBg);
  tft_->setTextColor(kAccent);
  tft_->setTextSize(1);
  tft_->setCursor(4, 3);
  tft_->print(vfxModeName(ctx.mode));
  tft_->setTextColor(kText);
  tft_->setCursor(4, 13);
  tft_->print(F("VU "));
  tft_->print(ctx.vu, 2);
  tft_->print(F("  P "));
  tft_->print(ctx.peak, 2);
}

void VfxRenderer::drawBars_(int top, int bottom, const float *levels, const float *peaks,
                            size_t count, bool gradient, float hueShift) {
  if (tft_ == nullptr || levels == nullptr || peaks == nullptr || count == 0) {
    return;
  }

  const int areaW = TFT_WIDTH - kMarginL - kMarginR;
  const int areaH = bottom - top;
  const int gap = count > 24 ? 1 : 2;
  const int barW = (areaW - static_cast<int>(count - 1) * gap) / static_cast<int>(count);
  if (barW < 1) {
    return;
  }

  tft_->drawFastHLine(kMarginL, bottom, areaW, kGrid);

  for (size_t i = 0; i < count; ++i) {
    const int x = kMarginL + static_cast<int>(i) * (barW + gap);
    const float level = levels[i] > 1.0f ? 1.0f : levels[i];
    const float peak = peaks[i] > 1.0f ? 1.0f : peaks[i];
    int barH = static_cast<int>(level * static_cast<float>(areaH));
    int peakH = static_cast<int>(peak * static_cast<float>(areaH));
    if (barH > areaH) {
      barH = areaH;
    }
    if (peakH > areaH) {
      peakH = areaH;
    }

    tft_->fillRect(x, top, barW, areaH, kBg);

    if (barH > 0) {
      const int y = bottom - barH;
      const uint16_t color =
          gradient ? rainbowColor_(level, hueShift + static_cast<float>(i) * 0.04f)
                   : gradientBarColor_(level);
      tft_->fillRect(x, y, barW, barH, color);
    }

    if (peakH > barH + 1) {
      const int py = bottom - peakH;
      tft_->fillRect(x, py, barW, 2, ST77XX_WHITE);
    }
  }
}

void VfxRenderer::drawMirror_(int top, int bottom, const float *levels, const float *peaks,
                              size_t count) {
  if (tft_ == nullptr || levels == nullptr || count == 0) {
    return;
  }

  const int midY = (top + bottom) / 2;
  const int halfH = (bottom - top) / 2 - 2;
  const int areaW = TFT_WIDTH - kMarginL - kMarginR;
  const int gap = 1;
  const int barW = (areaW - static_cast<int>(count - 1) * gap) / static_cast<int>(count);

  tft_->fillRect(kMarginL, top, areaW, bottom - top, kBg);
  tft_->drawFastHLine(kMarginL, midY, areaW, kAccent);

  for (size_t i = 0; i < count; ++i) {
    const int x = kMarginL + static_cast<int>(i) * (barW + gap);
    const float level = levels[i] > 1.0f ? 1.0f : levels[i];
    const float peak = peaks[i] > 1.0f ? 1.0f : peaks[i];
    int barH = static_cast<int>(level * static_cast<float>(halfH));
    int peakH = static_cast<int>(peak * static_cast<float>(halfH));
    if (barH > halfH) {
      barH = halfH;
    }
    if (peakH > halfH) {
      peakH = halfH;
    }

    const uint16_t color = rainbowColor_(level, static_cast<float>(i) * 0.03f);
    if (barH > 0) {
      tft_->fillRect(x, midY - barH, barW, barH, color);
      tft_->fillRect(x, midY + 1, barW, barH, color);
    }
    if (peakH > barH + 1) {
      tft_->fillRect(x, midY - peakH, barW, 1, ST77XX_WHITE);
      tft_->fillRect(x, midY + peakH, barW, 1, ST77XX_WHITE);
    }
  }
}

void VfxRenderer::drawVu_(int top, int bottom, const VfxDrawContext &ctx, const float *levels,
                          size_t count) {
  if (tft_ == nullptr) {
    return;
  }

  const int areaW = TFT_WIDTH - kMarginL - kMarginR;
  const int vuH = 36;
  const int vuY = top + 8;
  const int segGap = 2;
  const int segCount = 24;
  const int segW = (areaW - (segCount - 1) * segGap) / segCount;

  tft_->fillRect(kMarginL, top, areaW, bottom - top, kBg);

  for (int i = 0; i < segCount; ++i) {
    const int x = kMarginL + i * (segW + segGap);
    const float threshold = static_cast<float>(i + 1) / static_cast<float>(segCount);
    uint16_t color = 0x0400;
    if (threshold <= ctx.vu) {
      if (i < segCount * 6 / 10) {
        color = ST77XX_GREEN;
      } else if (i < segCount * 85 / 100) {
        color = ST77XX_YELLOW;
      } else {
        color = ST77XX_RED;
      }
    }
    tft_->fillRect(x, vuY, segW, vuH, color);
  }

  drawBars_(vuY + vuH + 10, bottom, levels, levels, count > 16 ? 16 : count, true, 0.0f);
}

void VfxRenderer::drawWaterfall_(int top, int bottom, const float *row, float *history,
                               size_t &head) {
  if (tft_ == nullptr || row == nullptr || history == nullptr || waterfallFb_ == nullptr) {
    return;
  }

  const int areaH = bottom - top;
  const int rows = areaH < VFX_WATERFALL_HISTORY ? areaH : VFX_WATERFALL_HISTORY;
  const int binW = TFT_WIDTH / VFX_WATERFALL_BINS;
  if (binW < 1) {
    return;
  }

  for (size_t i = 0; i < VFX_WATERFALL_BINS; ++i) {
    history[head * VFX_WATERFALL_BINS + i] = row[i];
  }
  head = (head + 1) % static_cast<size_t>(rows);

  for (int y = 0; y < rows; ++y) {
    const size_t srcRow = (head + static_cast<size_t>(y)) % static_cast<size_t>(rows);
    for (size_t x = 0; x < VFX_WATERFALL_BINS; ++x) {
      const float level = history[srcRow * VFX_WATERFALL_BINS + x];
      const uint16_t color = heatColor_(level);
      for (int dx = 0; dx < binW; ++dx) {
        waterfallFb_[static_cast<size_t>(y) * TFT_WIDTH + x * binW + dx] = color;
      }
    }
  }

  tft_->drawRGBBitmap(0, top, waterfallFb_, TFT_WIDTH, rows);
}

void VfxRenderer::drawLinePeaks_(int top, int bottom, const float *levels, size_t count) {
  if (tft_ == nullptr || levels == nullptr || count < 2) {
    return;
  }

  const int areaW = TFT_WIDTH - kMarginL - kMarginR;
  const int areaH = bottom - top;
  tft_->fillRect(kMarginL, top, areaW, areaH, kBg);
  tft_->drawFastHLine(kMarginL, bottom, areaW, kGrid);

  int prevX = -1;
  int prevY = -1;
  for (size_t i = 0; i < count; ++i) {
    const float level = levels[i] > 1.0f ? 1.0f : levels[i];
    const int x = kMarginL + static_cast<int>(i * areaW / (count - 1));
    const int y = bottom - static_cast<int>(level * static_cast<float>(areaH));
    const uint16_t color = rainbowColor_(level, static_cast<float>(i) * 0.02f);
    tft_->fillCircle(x, y, 2, color);
    if (prevX >= 0) {
      tft_->drawLine(prevX, prevY, x, y, color);
    }
    prevX = x;
    prevY = y;
  }
}

void VfxRenderer::draw(const VfxDrawContext &ctx, VfxMode mode, const float *levels,
                       const float *peaks, size_t count, float *waterfallHistory,
                       size_t &waterfallHead) {
  if (tft_ == nullptr) {
    return;
  }

  drawHeader_(ctx);

  switch (mode) {
    case VfxMode::Bars32:
      drawBars_(kAreaTop, kAreaBottom, levels, peaks, count, false, 0.0f);
      break;
    case VfxMode::Log12:
      drawBars_(kAreaTop, kAreaBottom, levels, peaks, count, false, 0.0f);
      break;
    case VfxMode::Mirror:
      drawMirror_(kAreaTop, kAreaBottom, levels, peaks, count);
      break;
    case VfxMode::VuMeter:
      drawVu_(kAreaTop, kAreaBottom, ctx, levels, count);
      break;
    case VfxMode::Waterfall:
      drawWaterfall_(kAreaTop, kAreaBottom, levels, waterfallHistory, waterfallHead);
      break;
    case VfxMode::Rainbow: {
      const float hue = fmodf(static_cast<float>(ctx.frameMs) * 0.00008f, 1.0f);
      drawBars_(kAreaTop, kAreaBottom, levels, peaks, count, true, hue);
      break;
    }
    case VfxMode::LinePeaks:
      drawLinePeaks_(kAreaTop, kAreaBottom, levels, count);
      break;
    default:
      drawBars_(kAreaTop, kAreaBottom, levels, peaks, count, false, 0.0f);
      break;
  }
}
