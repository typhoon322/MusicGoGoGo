#include "display/vfx_renderer.h"

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <math.h>
#include <string.h>

#include "config.h"
#include "display/tft_colors.h"

#if defined(BOARD_CARDPUTER_ADV) && CARDPUTER_USE_BUILTIN_LCD
#define GFX (gfxOverride_ != nullptr ? gfxOverride_ : static_cast<lgfx::LovyanGFX *>(tft_))
#else
#define GFX tft_
#endif

namespace {
#ifndef VFX_HEADER_H
constexpr int kHeaderH = 24;
#else
constexpr int kHeaderH = VFX_HEADER_H;
#endif
#ifndef VFX_AREA_TOP
constexpr int kAreaTop = 26;
#else
constexpr int kAreaTop = VFX_AREA_TOP;
#endif
constexpr int kAreaBottom = TFT_HEIGHT - 2;
constexpr int kMarginL = 2;
constexpr int kMarginR = 2;
constexpr uint16_t kBg = TFT_COL_BLACK;
constexpr uint16_t kText = TFT_COL_WHITE;
constexpr uint16_t kAccent = TFT_COL_CYAN;
constexpr uint16_t kGrid = 0x4208;

struct SpectrumLayout {
  int areaW = 0;
  int gap = 0;
  int barW = 0;
  int extra = 0;
};

SpectrumLayout makeLayout(size_t count, int gap) {
  SpectrumLayout layout;
  layout.gap = gap;
  layout.areaW = TFT_WIDTH - kMarginL - kMarginR;
  if (count == 0) {
    return layout;
  }
  layout.barW =
      (layout.areaW - static_cast<int>(count - 1) * gap) / static_cast<int>(count);
  const int used =
      static_cast<int>(count) * layout.barW + static_cast<int>(count - 1) * gap;
  layout.extra = layout.areaW - used;
  return layout;
}

int barWidth(const SpectrumLayout &layout, size_t index) {
  return layout.barW + (static_cast<int>(index) < layout.extra ? 1 : 0);
}

int barX(const SpectrumLayout &layout, size_t index) {
  int x = kMarginL;
  for (size_t j = 0; j < index; ++j) {
    x += barWidth(layout, j) + layout.gap;
  }
  return x;
}
}  // namespace

void VfxRenderer::attach(VfxTft *tft) {
  tft_ = tft;
  if (waterfallFb_ == nullptr) {
    const size_t bytes = TFT_WIDTH * VFX_WATERFALL_HISTORY * sizeof(uint16_t);
    waterfallFb_ = static_cast<uint16_t *>(heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM));
    if (waterfallFb_ == nullptr) {
      waterfallFb_ = static_cast<uint16_t *>(malloc(bytes));
    }
    resetWaterfall();
  }
#if defined(BOARD_CARDPUTER_ADV) && CARDPUTER_USE_BUILTIN_LCD
  plotSpriteH_ = kAreaBottom - kAreaTop + 1;
  if (plotSprite_ == nullptr) {
    plotSprite_ = new lgfx::LGFX_Sprite(tft_);
  }
  if (plotSprite_ != nullptr) {
    if (plotSprite_->width() != TFT_WIDTH || plotSprite_->height() != plotSpriteH_) {
      plotSprite_->deleteSprite();
      plotSprite_->createSprite(TFT_WIDTH, plotSpriteH_);
    }
  }
#endif
}

void VfxRenderer::resetWaterfall() {
  if (waterfallFb_ != nullptr) {
    memset(waterfallFb_, 0, TFT_WIDTH * VFX_WATERFALL_HISTORY * sizeof(uint16_t));
  }
}

void VfxRenderer::resetHeaderCache() {
  lastHeaderMode_ = VfxMode::Count;
  lastHeaderVu_ = -1.0f;
  lastHeaderPeak_ = -1.0f;
#if defined(BOARD_CARDPUTER_ADV) && CARDPUTER_USE_BUILTIN_LCD
  lastHeaderDrawMs_ = 0;
  lastBatteryPercent_ = -1;
  lastShowMicDebug_ = false;
#endif
}

void VfxRenderer::resetBarCache() {
  for (size_t i = 0; i < 64; ++i) {
    prevBarH_[i] = -1;
    prevPeakH_[i] = -1;
  }
}

void VfxRenderer::clearPlotArea() {
  if (tft_ == nullptr) {
    return;
  }
  tft_->fillRect(0, kAreaTop, TFT_WIDTH, TFT_HEIGHT - kAreaTop, kBg);
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
    return TFT_COL_GREEN;
  }
  if (level < 0.75f) {
    return TFT_COL_YELLOW;
  }
  return TFT_COL_RED;
}

void VfxRenderer::drawHeader_(const VfxDrawContext &ctx) {
  if (tft_ == nullptr) {
    return;
  }
#if defined(BOARD_CARDPUTER_ADV)
  if (ctx.mode == lastHeaderMode_ && ctx.batteryPercent == lastBatteryPercent_ &&
      ctx.showMicDebug == lastShowMicDebug_ && ctx.frameMs - lastHeaderDrawMs_ < 1000) {
    return;
  }
  lastHeaderDrawMs_ = ctx.frameMs;
  lastBatteryPercent_ = ctx.batteryPercent;
  lastShowMicDebug_ = ctx.showMicDebug;
#else
  if (ctx.mode == lastHeaderMode_ && fabsf(ctx.vu - lastHeaderVu_) < 0.02f &&
      fabsf(ctx.peak - lastHeaderPeak_) < 0.02f) {
    return;
  }
  lastHeaderVu_ = ctx.vu;
  lastHeaderPeak_ = ctx.peak;
#endif
  lastHeaderMode_ = ctx.mode;

  tft_->fillRect(0, 0, TFT_WIDTH, kHeaderH, kBg);
  tft_->setTextSize(1);
#if defined(BOARD_CARDPUTER_ADV)
  tft_->setTextColor(kAccent);
  tft_->setCursor(2, 3);
  tft_->print(vfxModeName(ctx.mode));
  drawBatteryBadge_(ctx);
#else
  tft_->setTextColor(kAccent);
  tft_->setCursor(4, 3);
  tft_->print(vfxModeName(ctx.mode));
  tft_->setTextColor(kText);
  tft_->setCursor(4, 13);
  tft_->print(F("VU "));
  tft_->print(ctx.vu, 2);
  tft_->print(F("  P "));
  tft_->print(ctx.peak, 2);
#endif
}

#if defined(BOARD_CARDPUTER_ADV)
void VfxRenderer::drawBatteryBadge_(const VfxDrawContext &ctx) {
  if (tft_ == nullptr) {
    return;
  }

  const int pct = ctx.batteryPercent < 0 ? 0 : ctx.batteryPercent;
  const int pctClamped = pct > 100 ? 100 : pct;
  const uint16_t fillColor =
      pctClamped > 50 ? TFT_COL_GREEN : (pctClamped > 20 ? TFT_COL_YELLOW : TFT_COL_RED);

  char label[8];
  snprintf(label, sizeof(label), "%d%%", pctClamped);
  const int labelW = static_cast<int>(strlen(label)) * 6;
  const int badgeW = 22 + labelW;
  const int x = TFT_WIDTH - badgeW - 2;
  const int y = 4;

  tft_->drawRect(x, y, 18, 10, kText);
  tft_->fillRect(x + 18, y + 3, 2, 4, kText);
  const int fillW = (pctClamped * 14) / 100;
  if (fillW > 0) {
    tft_->fillRect(x + 2, y + 2, fillW, 6, fillColor);
  }

  tft_->setTextColor(kText);
  tft_->setCursor(x + 22, 3);
  tft_->print(label);
}

void VfxRenderer::drawMicDebugOverlay_(const VfxDrawContext &ctx) {
  constexpr int kOverlayH = 36;
  constexpr uint16_t kPanel = 0x1082;

  GFX->fillRect(0, 0, TFT_WIDTH, kOverlayH, kPanel);
  GFX->drawFastHLine(0, kOverlayH - 1, TFT_WIDTH, kAccent);
  GFX->setTextSize(1);
  GFX->setTextColor(kText);
  GFX->setCursor(2, 2);
  GFX->printf("RMS %.3f  P %.3f  G %.1f", ctx.rms, ctx.peak, ctx.micGain);
  GFX->setCursor(2, 12);
  GFX->printf("raw[%d..%d] avg%ld", ctx.micRawMin, ctx.micRawMax,
              static_cast<long>(ctx.micRawMean));
  GFX->setCursor(2, 22);
  GFX->printf("B0-3:%.2f %.2f %.2f %.2f", ctx.band0, ctx.band1, ctx.band2, ctx.band3);
}
#endif

void VfxRenderer::drawBars_(int top, int bottom, const float *levels, const float *peaks,
                            size_t count, bool gradient, float hueShift) {
  if (tft_ == nullptr || levels == nullptr || peaks == nullptr || count == 0) {
    return;
  }

  const int areaH = bottom - top;
#if defined(BOARD_CARDPUTER_ADV)
  const int gap = 1;
#else
  const int gap = count > 24 ? 1 : 2;
#endif
  const SpectrumLayout layout = makeLayout(count, gap);
  if (layout.barW < 1) {
    return;
  }
  const int areaW = layout.areaW;

#if defined(BOARD_CARDPUTER_ADV)
  GFX->fillRect(kMarginL, top, areaW, areaH, kBg);
  GFX->drawFastHLine(kMarginL, bottom, areaW, kGrid);

  for (size_t i = 0; i < count; ++i) {
    const int x = barX(layout, i);
    const int bw = barWidth(layout, i);
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

    GFX->drawRect(x, top, bw, areaH, kGrid);
    if (barH > 0) {
      const int y = bottom - barH;
      const uint16_t color =
          gradient ? rainbowColor_(level, hueShift + static_cast<float>(i) * 0.04f)
                   : gradientBarColor_(level);
      GFX->fillRect(x, y, bw, barH, color);
    }

    if (peakH > barH + 1) {
      const int py = bottom - peakH;
      GFX->fillRect(x, py, bw, 2, TFT_COL_WHITE);
    }
  }
#else
  bool fullRedraw = false;
  for (size_t i = 0; i < count; ++i) {
    if (prevBarH_[i] < 0) {
      fullRedraw = true;
      break;
    }
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
    if (abs(barH - prevBarH_[i]) > 1 || abs(peakH - prevPeakH_[i]) > 1) {
      fullRedraw = true;
      break;
    }
  }

  if (fullRedraw) {
    GFX->fillRect(kMarginL, top, areaW, areaH, kBg);
    GFX->drawFastHLine(kMarginL, bottom, areaW, kGrid);
  }

  for (size_t i = 0; i < count; ++i) {
    const int x = barX(layout, i);
    const int bw = barWidth(layout, i);
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

    if (!fullRedraw && barH == prevBarH_[i] && peakH == prevPeakH_[i]) {
      continue;
    }

    GFX->fillRect(x, top, bw, areaH, kBg);

    if (barH > 0) {
      const int y = bottom - barH;
      const uint16_t color =
          gradient ? rainbowColor_(level, hueShift + static_cast<float>(i) * 0.04f)
                   : gradientBarColor_(level);
      GFX->fillRect(x, y, bw, barH, color);
    }

    if (peakH > barH + 1) {
      const int py = bottom - peakH;
      GFX->fillRect(x, py, bw, 2, TFT_COL_WHITE);
    }

    prevBarH_[i] = barH;
    prevPeakH_[i] = peakH;
  }
#endif
}

void VfxRenderer::drawMirror_(int top, int bottom, const float *levels, const float *peaks,
                              size_t count) {
  if (tft_ == nullptr || levels == nullptr || peaks == nullptr || count == 0) {
    return;
  }

  const int midY = (top + bottom) / 2;
  const int halfH = (bottom - top) / 2 - 2;
  const SpectrumLayout layout = makeLayout(count, 1);
  const int areaW = layout.areaW;

  GFX->fillRect(kMarginL, top, areaW, bottom - top, kBg);
  GFX->drawFastHLine(kMarginL, midY, areaW, kAccent);

  for (size_t i = 0; i < count; ++i) {
    const int x = barX(layout, i);
    const int bw = barWidth(layout, i);
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
    GFX->drawRect(x, top, bw, bottom - top, kGrid);
    if (barH > 0) {
      GFX->fillRect(x, midY - barH, bw, barH, color);
      GFX->fillRect(x, midY + 1, bw, barH, color);
    }
    if (peakH > barH + 1) {
      GFX->fillRect(x, midY - peakH, bw, 1, TFT_COL_WHITE);
      GFX->fillRect(x, midY + peakH, bw, 1, TFT_COL_WHITE);
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
  const SpectrumLayout segLayout = makeLayout(static_cast<size_t>(segCount), segGap);

  GFX->fillRect(kMarginL, top, areaW, bottom - top, kBg);

  for (int i = 0; i < segCount; ++i) {
    const int x = barX(segLayout, static_cast<size_t>(i));
    const int segW = barWidth(segLayout, static_cast<size_t>(i));
    const float threshold = static_cast<float>(i + 1) / static_cast<float>(segCount);
    uint16_t color = 0x0400;
    if (threshold <= ctx.vu) {
      if (i < segCount * 6 / 10) {
        color = TFT_COL_GREEN;
      } else if (i < segCount * 85 / 100) {
        color = TFT_COL_YELLOW;
      } else {
        color = TFT_COL_RED;
      }
    }
    GFX->fillRect(x, vuY, segW, vuH, color);
  }

  drawBars_(vuY + vuH + 10, bottom, levels, levels, count > 16 ? 16 : count, true, 0.0f);
}

#if defined(BOARD_CARDPUTER_ADV) && CARDPUTER_USE_BUILTIN_LCD
void VfxRenderer::drawPlotMode_(const VfxDrawContext &ctx, VfxMode mode, const float *levels,
                                const float *peaks, size_t count, float *waterfallHistory,
                                size_t &waterfallHead) {
  switch (mode) {
    case VfxMode::Bars32:
      drawBars_(0, plotSpriteH_ - 1, levels, peaks, count, false, 0.0f);
      break;
    case VfxMode::Log12:
      drawBars_(0, plotSpriteH_ - 1, levels, peaks, count, false, 0.0f);
      break;
    case VfxMode::Mirror:
      drawMirror_(0, plotSpriteH_ - 1, levels, peaks, count);
      break;
    case VfxMode::VuMeter:
      drawVu_(0, plotSpriteH_ - 1, ctx, levels, count);
      break;
    case VfxMode::Waterfall:
      drawWaterfall_(0, plotSpriteH_ - 1, levels, waterfallHistory, waterfallHead);
      break;
    case VfxMode::Rainbow: {
      const float hue = fmodf(static_cast<float>(ctx.frameMs) * 0.00008f, 1.0f);
      drawBars_(0, plotSpriteH_ - 1, levels, peaks, count, true, hue);
      break;
    }
    case VfxMode::LinePeaks:
      drawLinePeaks_(0, plotSpriteH_ - 1, levels, count);
      break;
    default:
      drawBars_(0, plotSpriteH_ - 1, levels, peaks, count, false, 0.0f);
      break;
  }
}
#endif

void VfxRenderer::drawWaterfall_(int top, int bottom, const float *row, float *history,
                               size_t &head) {
  if (tft_ == nullptr || row == nullptr || history == nullptr || waterfallFb_ == nullptr) {
    return;
  }

  const int areaH = bottom - top;
  const int rows = areaH < VFX_WATERFALL_HISTORY ? areaH : VFX_WATERFALL_HISTORY;
  const int areaW = TFT_WIDTH - kMarginL - kMarginR;
  if (areaW < 1 || rows < 1) {
    return;
  }

  for (size_t i = 0; i < VFX_WATERFALL_BINS; ++i) {
    history[head * VFX_WATERFALL_BINS + i] = row[i];
  }
  head = (head + 1) % static_cast<size_t>(rows);

  for (int y = 0; y < rows; ++y) {
    const size_t srcRow = (head + static_cast<size_t>(y)) % static_cast<size_t>(rows);
    for (int px = 0; px < areaW; ++px) {
      size_t bin = static_cast<size_t>(px) * VFX_WATERFALL_BINS / static_cast<size_t>(areaW);
      if (bin >= VFX_WATERFALL_BINS) {
        bin = VFX_WATERFALL_BINS - 1;
      }
      const float level = history[srcRow * VFX_WATERFALL_BINS + bin];
      waterfallFb_[static_cast<size_t>(y) * TFT_WIDTH + kMarginL + px] = heatColor_(level);
    }
    for (int px = 0; px < kMarginL; ++px) {
      waterfallFb_[static_cast<size_t>(y) * TFT_WIDTH + px] = kBg;
    }
    for (int px = kMarginL + areaW; px < TFT_WIDTH; ++px) {
      waterfallFb_[static_cast<size_t>(y) * TFT_WIDTH + px] = kBg;
    }
  }

#if defined(BOARD_CARDPUTER_ADV) && CARDPUTER_USE_BUILTIN_LCD
  GFX->pushImage(0, top, TFT_WIDTH, rows, waterfallFb_);
#else
  GFX->drawRGBBitmap(0, top, waterfallFb_, TFT_WIDTH, rows);
#endif
}

void VfxRenderer::drawLinePeaks_(int top, int bottom, const float *levels, size_t count) {
  if (tft_ == nullptr || levels == nullptr || count < 2) {
    return;
  }

  const int areaW = TFT_WIDTH - kMarginL - kMarginR;
  const int areaH = bottom - top;
  GFX->fillRect(kMarginL, top, areaW, areaH, kBg);
  GFX->drawFastHLine(kMarginL, bottom, areaW, kGrid);

  int prevX = -1;
  int prevY = -1;
  for (size_t i = 0; i < count; ++i) {
    const float level = levels[i] > 1.0f ? 1.0f : levels[i];
    const int x = kMarginL + static_cast<int>(i * areaW / (count - 1));
    const int y = bottom - static_cast<int>(level * static_cast<float>(areaH));
    const uint16_t color = rainbowColor_(level, static_cast<float>(i) * 0.02f);
    GFX->fillCircle(x, y, 2, color);
    if (prevX >= 0) {
      GFX->drawLine(prevX, prevY, x, y, color);
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

#if defined(BOARD_CARDPUTER_ADV) && CARDPUTER_USE_BUILTIN_LCD
  if (plotSprite_ != nullptr && plotSprite_->width() > 0) {
    plotSprite_->fillScreen(kBg);
    gfxOverride_ = plotSprite_;
    drawPlotMode_(ctx, mode, levels, peaks, count, waterfallHistory, waterfallHead);
    if (ctx.showMicDebug) {
      drawMicDebugOverlay_(ctx);
    }
    gfxOverride_ = nullptr;
    tft_->startWrite();
    plotSprite_->pushSprite(0, kAreaTop);
    tft_->endWrite();
    return;
  }
#endif

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
