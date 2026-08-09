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
constexpr int kMinBarPx = 3;  // idle floor so silence isn't a black void
constexpr uint16_t kBg = TFT_COL_BLACK;
constexpr uint16_t kText = TFT_COL_WHITE;
constexpr uint16_t kAccent = TFT_COL_CYAN;
constexpr uint16_t kGrid = 0x4208;
// Cat palette (RGB565) — fixed colors, no per-frame hue churn (avoids flicker)
constexpr uint16_t kCatOrange = 0xFBE0;  // warm orange
constexpr uint16_t kCatCream = 0xEF5D;
constexpr uint16_t kCatDark = 0x8410;
constexpr uint16_t kCatPink = 0xFCF0;
constexpr int kCatSpriteW = 26;
constexpr int kCatSpriteH = 22;

int floorBarH(int barH, int maxH) {
  if (maxH < kMinBarPx) {
    return barH < 0 ? 0 : (barH > maxH ? maxH : barH);
  }
  if (barH < kMinBarPx) {
    barH = kMinBarPx;
  }
  if (barH > maxH) {
    barH = maxH;
  }
  return barH;
}

struct SpectrumLayout {
  int startX = kMarginL;
  int areaW = 0;
  int gap = 0;
  int barW = 0;
  int extra = 0;
};

SpectrumLayout makeLayout(size_t count, int gap) {
  SpectrumLayout layout;
  layout.startX = kMarginL;
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

// 30-band / Log12: fill width; prefer 3px gaps (Cardputer keeps 0 for density).
SpectrumLayout makeBarLayout(size_t count) {
#if defined(BOARD_CARDPUTER_ADV)
  return makeLayout(count, 0);
#else
  return makeLayout(count, 3);
#endif
}

int barWidth(const SpectrumLayout &layout, size_t index) {
  return layout.barW + (static_cast<int>(index) < layout.extra ? 1 : 0);
}

int barX(const SpectrumLayout &layout, size_t index) {
  int x = layout.startX;
  for (size_t j = 0; j < index; ++j) {
    x += barWidth(layout, j) + layout.gap;
  }
  return x;
}

// Center frequency (Hz) for 30-band 1/3-octave or 12-band octave layouts.
float barCenterHz(size_t index, size_t count) {
  static const float kOctave12[] = {20.0f,   40.0f,    80.0f,    160.0f,   315.0f, 630.0f,
                                    1250.0f, 2500.0f,  5000.0f,  8000.0f, 12000.0f,
                                    16000.0f, 20000.0f};
  static const float kThird30[] = {
      20.0f,    25.0f,    31.0f,    40.0f,    50.0f,    63.0f,    80.0f,    100.0f,
      125.0f,   160.0f,   200.0f,   250.0f,   315.0f,   400.0f,   500.0f,   630.0f,
      800.0f,   1000.0f,  1250.0f,  1600.0f,  2000.0f,  2500.0f,  3150.0f,  4000.0f,
      5000.0f,  6300.0f,  8000.0f,  10000.0f, 12500.0f, 16000.0f, 20000.0f};
  if (count == SPECTRUM_BARS && index < SPECTRUM_BARS) {
    return sqrtf(kThird30[index] * kThird30[index + 1]);
  }
  if (count == static_cast<size_t>(VFX_LOG_BANDS) && index < VFX_LOG_BANDS) {
    return sqrtf(kOctave12[index] * kOctave12[index + 1]);
  }
  const float binW = static_cast<float>(I2S_SAMPLE_RATE) / static_cast<float>(FFT_SIZE);
  const float lo = 4.0f + static_cast<float>(index * (FFT_SIZE / 2)) / static_cast<float>(count);
  const float hi =
      4.0f + static_cast<float>((index + 1) * (FFT_SIZE / 2)) / static_cast<float>(count);
  return ((lo + hi) / 2.0f) * binW;
}

// Draw a small label vertically (one character under the next) at (x, y).
void drawVertLabel_(VfxTft *tft, int x, int y, const char *label) {
  int cy = y;
  for (const char *p = label; *p != '\0'; ++p) {
    tft->drawChar(x, cy, *p, kText, kBg, 1);
    cy += 8;
  }
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
  catHeaderInit_ = false;
  prevCatX_ = -1;
  prevCatY_ = -1;
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
    bounceH_[i] = 0.0f;
    bounceVel_[i] = 0.0f;
    prevDotY_[i] = -1;
    prevDotR_[i] = 0;
    prevSpokeLen_[i] = -1;
    if (i < 32) {
      prevVuSegLit_[i] = false;
    }
  }
  prevLineValid_ = false;
  vuInit_ = false;
  areaInit_ = false;
  ringInit_ = false;
  lastBarLabelCount_ = -1;
}

void VfxRenderer::setShowFreqLabels(bool on) {
  // Async WebUI may call this off the render task — defer clear/reset to draw().
  pendingFreqLabels_ = on;
  pendingFreqLabelsDirty_ = true;
}

void VfxRenderer::applyPendingFreqLabels_() {
  if (!pendingFreqLabelsDirty_) {
    return;
  }
  pendingFreqLabelsDirty_ = false;
  if (showFreqLabels_ == pendingFreqLabels_) {
    return;
  }
  showFreqLabels_ = pendingFreqLabels_;
  clearPlotArea();
  resetBarCache();
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
  float h = fmodf(hue, 1.0f);
  if (h < 0.0f) {
    h += 1.0f;
  }
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

uint16_t VfxRenderer::darken_(uint16_t color, float factor) const {
  if (factor < 0.0f) {
    factor = 0.0f;
  }
  if (factor > 1.0f) {
    factor = 1.0f;
  }
  const uint8_t r = static_cast<uint8_t>(((color >> 11) & 0x1F) * factor);
  const uint8_t g = static_cast<uint8_t>(((color >> 5) & 0x3F) * factor);
  const uint8_t b = static_cast<uint8_t>((color & 0x1F) * factor);
  return static_cast<uint16_t>((r << 11) | (g << 5) | b);
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
  lastHeaderMode_ = ctx.mode;
  tft_->fillRect(0, 0, TFT_WIDTH, kHeaderH, kBg);
  tft_->setTextSize(1);
  tft_->setTextColor(kAccent);
  tft_->setCursor(2, 3);
  tft_->print(vfxModeName(ctx.mode));
  drawBatteryBadge_(ctx);
#else
  (void)lastHeaderVu_;
  (void)lastHeaderPeak_;
  lastHeaderMode_ = ctx.mode;
  drawDancingCat_(ctx);
#endif
}

void VfxRenderer::drawDancingCat_(const VfxDrawContext &ctx) {
  if (tft_ == nullptr) {
    return;
  }

  float vu = ctx.vu;
  if (vu < 0.0f) {
    vu = 0.0f;
  }
  if (vu > 1.0f) {
    vu = 1.0f;
  }

  constexpr float kMusicOn = 0.035f;
  const bool dancing = vu >= kMusicOn;
  constexpr float kConfUse = 0.45f;
  const bool beatLock = dancing && ctx.beatConfidence >= kConfUse && ctx.beatBpm >= 70.0f;

  // Motion: beat-locked walk/hops when locked; VU fake hop when dancing;
  // slow stroll when quiet.
  float targetHop = 0.0f;
  int walkFrame = 0;
  if (beatLock) {
    const float bpm = ctx.beatBpm;
    const float beatPeriodMs = 60000.0f / bpm;
    walkFrame = static_cast<int>(fmodf(static_cast<float>(ctx.frameMs), beatPeriodMs) /
                                 (beatPeriodMs * 0.5f)) &
                1;
    // Walk speed: 70→~1.2px/frame, 160→~2.8px/frame (~30FPS coarse tune)
    const float t = (bpm - 70.0f) / 90.0f;
    const float speed = 1.2f + t * 1.6f;
    catX_ += static_cast<float>(catDir_) * speed;

    if (ctx.kickPulse > 0.15f) {
      targetHop = 10.0f + ctx.kickPulse * 16.0f;
    } else if (ctx.snarePulse > 0.15f) {
      targetHop = 3.0f + ctx.snarePulse * 6.0f;
    }
  } else if (dancing) {
    // Original VU fake-beat path
    const int wf = static_cast<int>((ctx.frameMs / 110) % 2);
    walkFrame = wf;
    const float tempo = 0.018f + vu * 0.045f;
    const float beat = sinf(static_cast<float>(ctx.frameMs) * tempo);
    targetHop = (beat > 0.0f ? beat : 0.0f) * (10.0f + vu * 16.0f);
    catX_ += static_cast<float>(catDir_) * (0.4f + vu * 1.2f);
  } else {
    walkFrame = static_cast<int>((ctx.frameMs / 110) % 2);
    targetHop = walkFrame ? 1.5f : 0.0f;
    catX_ += static_cast<float>(catDir_) * 2.2f;
  }
  catHopSmoothed_ += (targetHop - catHopSmoothed_) * 0.45f;

  const float minX = static_cast<float>(kCatSpriteW / 2 + 2);
  const float maxX = static_cast<float>(TFT_WIDTH - kCatSpriteW / 2 - 2);
  if (catX_ < minX) {
    catX_ = minX;
    catDir_ = 1;
  } else if (catX_ > maxX) {
    catX_ = maxX;
    catDir_ = -1;
  }

  const int cx = static_cast<int>(catX_ + 0.5f);
  const int ground = kHeaderH - 2;
  const int hop = static_cast<int>(catHopSmoothed_ + 0.5f);
  const int cy = ground - hop;  // feet y

  // Erase previous + current span so fast walks don't leave crumbs.
  if (!catHeaderInit_) {
    tft_->fillRect(0, 0, TFT_WIDTH, kHeaderH, kBg);
    catHeaderInit_ = true;
  } else if (prevCatX_ >= 0) {
    int left = (prevCatX_ < cx ? prevCatX_ : cx) - kCatSpriteW / 2 - 3;
    int right = (prevCatX_ > cx ? prevCatX_ : cx) + kCatSpriteW / 2 + 3;
    if (left < 0) {
      left = 0;
    }
    if (right > TFT_WIDTH) {
      right = TFT_WIDTH;
    }
    tft_->fillRect(left, 0, right - left, kHeaderH, kBg);
  }

  const int f = catDir_;  // +1 face right, -1 face left
  const int headX = cx + 7 * f;
  const int hipX = cx - 4 * f;
  const int bodyL = (f > 0) ? (cx - 9) : (cx - 5);

  // body + haunch
  tft_->fillRoundRect(bodyL, cy - 13, 14, 10, 3, kCatOrange);
  tft_->fillCircle(hipX, cy - 8, 6, kCatOrange);
  // head + muzzle
  tft_->fillCircle(headX, cy - 14, 7, kCatOrange);
  tft_->fillCircle(headX + 4 * f, cy - 12, 3, kCatCream);
  // ears (pink inside)
  tft_->fillTriangle(headX - 2 * f, cy - 19, headX - 1 * f, cy - 25, headX + 2 * f, cy - 18,
                     kCatOrange);
  tft_->fillTriangle(headX + 2 * f, cy - 19, headX + 4 * f, cy - 25, headX + 6 * f, cy - 18,
                     kCatOrange);
  tft_->fillTriangle(headX - 1 * f, cy - 19, headX - 1 * f, cy - 23, headX + 1 * f, cy - 19,
                     kCatPink);
  tft_->fillTriangle(headX + 3 * f, cy - 19, headX + 4 * f, cy - 23, headX + 5 * f, cy - 19,
                     kCatPink);
  // eye + nose
  tft_->fillCircle(headX + 2 * f, cy - 15, 1, kCatDark);
  tft_->fillCircle(headX + 5 * f, cy - 12, 1, kCatDark);

  // big wagging tail — snare accent when beat-locked
  const int tailSwing = beatLock
                            ? static_cast<int>(ctx.snarePulse * 10.0f +
                                              sinf(ctx.frameMs * 0.028f) * 3.0f)
                            : (dancing ? static_cast<int>(sinf(ctx.frameMs * 0.028f) * 6.0f)
                                       : (walkFrame ? 4 : -3));
  const int tailX = cx - 12 * f;
  const int tailY = cy - 18 - tailSwing;
  tft_->drawLine(hipX - 2 * f, cy - 10, tailX, tailY, kCatOrange);
  tft_->drawLine(hipX - 2 * f, cy - 9, tailX - f, tailY + 1, kCatOrange);
  tft_->fillCircle(tailX, tailY, 3, kCatOrange);

  // legs — long stride / tucked jump
  if (dancing) {
    const int tuck = hop > 8 ? 1 : 3;
    tft_->fillRect(cx - 5, cy - tuck, 3, 2 + tuck, kCatOrange);
    tft_->fillRect(cx - 1, cy - tuck, 3, 2 + tuck, kCatOrange);
    tft_->fillRect(cx + 3, cy - tuck, 3, 2 + tuck, kCatOrange);
  } else if (walkFrame == 0) {
    tft_->fillRect(cx - 8 * f, cy - 4, 3, 6, kCatOrange);
    tft_->fillRect(cx + 3 * f, cy - 4, 3, 6, kCatOrange);
    tft_->fillRect(cx - 3 * f, cy - 2, 3, 3, kCatOrange);
    tft_->fillRect(cx + 6 * f, cy - 2, 3, 3, kCatOrange);
  } else {
    tft_->fillRect(cx - 3 * f, cy - 4, 3, 6, kCatOrange);
    tft_->fillRect(cx + 6 * f, cy - 4, 3, 6, kCatOrange);
    tft_->fillRect(cx - 8 * f, cy - 2, 3, 3, kCatOrange);
    tft_->fillRect(cx + 3 * f, cy - 2, 3, 3, kCatOrange);
  }

  const int shadowW = dancing ? 12 : 18;
  tft_->drawFastHLine(cx - shadowW / 2, ground, shadowW, kCatDark);

  prevCatX_ = cx;
  prevCatY_ = cy;
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
  (void)areaH;
#if !defined(BOARD_CARDPUTER_ADV)
  constexpr int kBarLabelH = 26;
  const int barBottom = showFreqLabels_ ? (bottom - kBarLabelH) : bottom;
#endif
  const SpectrumLayout layout = makeBarLayout(count);
  if (layout.barW < 1) {
    return;
  }
  const int areaW = layout.areaW;

#if defined(BOARD_CARDPUTER_ADV)
    GFX->fillRect(layout.startX, top, areaW, areaH, kBg);
    GFX->drawFastHLine(layout.startX, bottom, areaW, kGrid);

  for (size_t i = 0; i < count; ++i) {
    const int x = barX(layout, i);
    const int bw = barWidth(layout, i);
    const float level = levels[i] > 1.0f ? 1.0f : levels[i];
    const float peak = peaks[i] > 1.0f ? 1.0f : peaks[i];
    int barH = floorBarH(static_cast<int>(level * static_cast<float>(areaH)), areaH);
    int peakH = static_cast<int>(peak * static_cast<float>(areaH));
    if (peakH < barH) {
      peakH = barH;
    }
    if (peakH > areaH) {
      peakH = areaH;
    }

    // Fixed per-index color (red low freq -> violet high freq); no color
    // band switching so bars do not flicker with level.
    const uint16_t color =
        gradient ? rainbowColor_(level, hueShift + static_cast<float>(i) * 0.04f)
                 : rainbowColor_(
                       0.85f, 0.80f * (static_cast<float>(i) /
                                       static_cast<float>(count > 1 ? count - 1 : 1)));

    if (barH > 0) {
      const int y = bottom - barH;
      GFX->fillRect(x, y, bw, barH, color);
    }

    if (peakH > barH + 1) {
      const int py = bottom - peakH;
      GFX->fillRect(x, py, bw, 2, TFT_COL_WHITE);
    }
  }
#else
  const int barAreaH = barBottom - top;
  GFX->drawFastHLine(layout.startX, barBottom, areaW, kGrid);

  // Vertical frequency labels, one per bar. Redraw only when the bar count
  // changes (i.e. mode switch); bars never cover the label strip below them.
  if (showFreqLabels_ && lastBarLabelCount_ != static_cast<int>(count)) {
    lastBarLabelCount_ = static_cast<int>(count);
    GFX->fillRect(layout.startX, barBottom + 1, areaW, bottom - barBottom - 1, kBg);
    char label[6];
    for (size_t i = 0; i < count; ++i) {
      const int x = barX(layout, i);
      const int bw = barWidth(layout, i);
      snprintf(label, sizeof(label), "%.1f", barCenterHz(i, count) / 1000.0f);
      drawVertLabel_(tft_, x + bw / 2 - 3, barBottom + 2, label);
    }
  }

  for (size_t i = 0; i < count; ++i) {
    const int x = barX(layout, i);
    const int bw = barWidth(layout, i);
    const float level = levels[i] > 1.0f ? 1.0f : levels[i];
    const float peak = peaks[i] > 1.0f ? 1.0f : peaks[i];
    int barH = floorBarH(static_cast<int>(level * static_cast<float>(barAreaH)), barAreaH);
    int peakH = static_cast<int>(peak * static_cast<float>(barAreaH));
    if (peakH < barH) {
      peakH = barH;
    }
    if (peakH > barAreaH) {
      peakH = barAreaH;
    }

    const bool first = prevBarH_[i] < 0;
    // Bar color is fixed per frequency index (low freq red -> high freq
    // violet) so there is no color-band switching that would cause the
    // bars to flicker between green/yellow/red on every level change.
    const uint16_t color =
        gradient ? rainbowColor_(level, hueShift + static_cast<float>(i) * 0.04f)
                 : rainbowColor_(
                       0.85f, 0.80f * (static_cast<float>(i) /
                                       static_cast<float>(count > 1 ? count - 1 : 1)));

    if (first) {
      GFX->fillRect(x, top, bw, barAreaH, kBg);
      if (barH > 0) {
        GFX->fillRect(x, barBottom - barH, bw, barH, color);
      }
    } else if (barH > prevBarH_[i]) {
      GFX->fillRect(x, barBottom - barH, bw, barH - prevBarH_[i], color);
    } else if (barH < prevBarH_[i]) {
      GFX->fillRect(x, barBottom - prevBarH_[i], bw, prevBarH_[i] - barH, kBg);
    }

    // Erase the old peak only when it still sits above the current bar.
    // If the bar grew over it, the new fill already repainted that area.
    const bool showOld = prevPeakH_[i] > barH + 1;
    if (showOld) {
      GFX->fillRect(x, barBottom - prevPeakH_[i], bw, 2, kBg);
    }
    if (peakH > barH + 1) {
      GFX->fillRect(x, barBottom - peakH, bw, 2, TFT_COL_WHITE);
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
  const SpectrumLayout layout = makeBarLayout(count);
  if (layout.barW < 1) {
    return;
  }

  for (size_t i = 0; i < count; ++i) {
    const int x = barX(layout, i);
    const int bw = barWidth(layout, i);
    const float level = levels[i] > 1.0f ? 1.0f : levels[i];
    const float peak = peaks[i] > 1.0f ? 1.0f : peaks[i];
    int barH = floorBarH(static_cast<int>(level * static_cast<float>(halfH)), halfH);
    int peakH = static_cast<int>(peak * static_cast<float>(halfH));
    if (peakH < barH) {
      peakH = barH;
    }
    if (peakH > halfH) {
      peakH = halfH;
    }

    const uint16_t color = rainbowColor_(level, static_cast<float>(i) * 0.03f);
    const int prevH = prevBarH_[i];
    const int prevPH = prevPeakH_[i];

    const bool showNew = peakH > barH + 1;
    const bool showOld = prevH >= 0 && prevPH > barH + 1;

    // Erase old peak lines first so bar growth can repaint over them if needed.
    if (showOld) {
      GFX->fillRect(x, midY - prevPH, bw, 1, kBg);
      GFX->fillRect(x, midY + prevPH, bw, 1, kBg);
    }

    if (prevH < 0) {
      GFX->fillRect(x, top, bw, bottom - top, kBg);
      if (barH > 0) {
        GFX->fillRect(x, midY - barH, bw, barH, color);
        GFX->fillRect(x, midY + 1, bw, barH, color);
      }
    } else if (barH > prevH) {
      GFX->fillRect(x, midY - barH, bw, barH - prevH, color);
      GFX->fillRect(x, midY + 1 + prevH, bw, barH - prevH, color);
    } else if (barH < prevH) {
      GFX->fillRect(x, midY - prevH, bw, prevH - barH, kBg);
      GFX->fillRect(x, midY + 1 + barH, bw, prevH - barH, kBg);
    }

    if (showNew) {
      GFX->fillRect(x, midY - peakH, bw, 1, TFT_COL_WHITE);
      GFX->fillRect(x, midY + peakH, bw, 1, TFT_COL_WHITE);
    }

    prevBarH_[i] = barH;
    prevPeakH_[i] = peakH;
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

  if (!vuInit_) {
    GFX->fillRect(kMarginL, vuY, areaW, vuH, 0x0400);
    vuInit_ = true;
  }

  for (int i = 0; i < segCount; ++i) {
    const int x = barX(segLayout, static_cast<size_t>(i));
    const int segW = barWidth(segLayout, static_cast<size_t>(i));
    const float threshold = static_cast<float>(i + 1) / static_cast<float>(segCount);
    const bool lit = threshold <= ctx.vu;
    if (lit == prevVuSegLit_[static_cast<size_t>(i)]) {
      continue;
    }

    uint16_t color = 0x0400;
    if (lit) {
      if (i < segCount * 6 / 10) {
        color = TFT_COL_GREEN;
      } else if (i < segCount * 85 / 100) {
        color = TFT_COL_YELLOW;
      } else {
        color = TFT_COL_RED;
      }
    }
    GFX->fillRect(x, vuY, segW, vuH, color);
    prevVuSegLit_[static_cast<size_t>(i)] = lit;
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
    case VfxMode::Rainbow: {
      const float hue = fmodf(static_cast<float>(ctx.frameMs) * 0.00008f, 1.0f);
      drawBars_(0, plotSpriteH_ - 1, levels, peaks, count, true, hue);
      break;
    }
    case VfxMode::LinePeaks:
      drawLinePeaks_(0, plotSpriteH_ - 1, levels, count);
      break;
    case VfxMode::Bounce:
      drawBounce_(0, plotSpriteH_ - 1, levels, peaks, count);
      break;
    case VfxMode::Dot:
      drawDot_(0, plotSpriteH_ - 1, levels, count);
      break;
    case VfxMode::Glow:
      drawGlow_(0, plotSpriteH_ - 1, levels, peaks, count);
      break;
    case VfxMode::Ring:
      drawRing_(0, plotSpriteH_ - 1, ctx, levels, count);
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
  if (areaH <= rows) {
    GFX->pushImage(0, top, TFT_WIDTH, rows, waterfallFb_);
  } else {
    for (int y = 0; y < areaH; ++y) {
      const int srcY = y * rows / areaH;
      GFX->pushImage(0, top + y, TFT_WIDTH, 1, waterfallFb_ + srcY * TFT_WIDTH);
    }
  }
#else
  if (areaH <= rows) {
    GFX->drawRGBBitmap(0, top, waterfallFb_, TFT_WIDTH, rows);
  } else {
    for (int y = 0; y < areaH; ++y) {
      const int srcY = y * rows / areaH;
      GFX->drawRGBBitmap(0, top + y, waterfallFb_ + srcY * TFT_WIDTH, TFT_WIDTH, 1);
    }
  }
#endif
}

void VfxRenderer::drawLinePeaks_(int top, int bottom, const float *levels, size_t count) {
  if (tft_ == nullptr || levels == nullptr || count < 2) {
    return;
  }

  const int areaW = TFT_WIDTH - kMarginL - kMarginR;
  const int areaH = bottom - top;

  GFX->drawFastHLine(kMarginL, bottom, areaW, kGrid);

  int xArr[64];
  int yArr[64];
  uint16_t colArr[64];
  for (size_t i = 0; i < count; ++i) {
    const float level = levels[i] > 1.0f ? 1.0f : levels[i];
    xArr[i] = kMarginL + static_cast<int>(i * areaW / (count - 1));
    yArr[i] = bottom - static_cast<int>(level * static_cast<float>(areaH));
    colArr[i] = rainbowColor_(level, static_cast<float>(i) * 0.02f);
  }

  if (prevLineValid_) {
    for (size_t i = 0; i < count; ++i) {
      GFX->fillCircle(prevLineX_[i], prevLineY_[i], 2, kBg);
    }
    for (size_t i = 1; i < count; ++i) {
      GFX->drawLine(prevLineX_[i - 1], prevLineY_[i - 1], prevLineX_[i], prevLineY_[i], kBg);
    }
  }

  for (size_t i = 0; i < count; ++i) {
    prevLineX_[i] = xArr[i];
    prevLineY_[i] = yArr[i];
    GFX->fillCircle(xArr[i], yArr[i], 2, colArr[i]);
  }
  for (size_t i = 1; i < count; ++i) {
    GFX->drawLine(xArr[i - 1], yArr[i - 1], xArr[i], yArr[i], colArr[i]);
  }
  prevLineValid_ = true;
}

void VfxRenderer::drawBounce_(int top, int bottom, const float *levels, const float *peaks,
                              size_t count) {
  if (tft_ == nullptr || levels == nullptr || peaks == nullptr || count == 0) {
    return;
  }
  const SpectrumLayout layout = makeBarLayout(count);
  if (layout.barW < 1) {
    return;
  }
  const int areaH = bottom - top;
  for (size_t i = 0; i < count; ++i) {
    const float level = levels[i] > 1.0f ? 1.0f : levels[i];
    const float target = level * static_cast<float>(areaH);
    bounceVel_[i] += (target - bounceH_[i]) * 0.35f;
    bounceVel_[i] *= 0.72f;
    bounceH_[i] += bounceVel_[i];
    if (bounceH_[i] < 0.0f) {
      bounceH_[i] = 0.0f;
      bounceVel_[i] = 0.0f;
    }
    if (bounceH_[i] > static_cast<float>(areaH)) {
      bounceH_[i] = static_cast<float>(areaH);
    }

    const int x = barX(layout, i);
    const int bw = barWidth(layout, i);
    int barH = floorBarH(static_cast<int>(bounceH_[i]), areaH);
    const float peak = peaks[i] > 1.0f ? 1.0f : peaks[i];
    int peakH = static_cast<int>(peak * static_cast<float>(areaH));
    if (peakH > areaH) {
      peakH = areaH;
    }
    if (peakH < barH) {
      peakH = barH;
    }

    const uint16_t color = rainbowColor_(
        0.85f, 0.80f * (static_cast<float>(i) / static_cast<float>(count > 1 ? count - 1 : 1)));
    const int prevH = prevBarH_[i];
    if (prevH < 0) {
      GFX->fillRect(x, top, bw, areaH, kBg);
      if (barH > 0) {
        GFX->fillRect(x, bottom - barH, bw, barH, color);
      }
    } else if (barH > prevH) {
      GFX->fillRect(x, bottom - barH, bw, barH - prevH, color);
    } else if (barH < prevH) {
      GFX->fillRect(x, bottom - prevH, bw, prevH - barH, kBg);
    }

    if (prevPeakH_[i] > barH + 1) {
      GFX->fillRect(x, bottom - prevPeakH_[i], bw, 2, kBg);
    }
    if (peakH > barH + 1) {
      GFX->fillRect(x, bottom - peakH, bw, 2, TFT_COL_WHITE);
    }
    prevBarH_[i] = barH;
    prevPeakH_[i] = peakH;
  }
}

void VfxRenderer::drawDot_(int top, int bottom, const float *levels, size_t count) {
  if (tft_ == nullptr || levels == nullptr || count == 0) {
    return;
  }
  const int areaW = TFT_WIDTH - kMarginL - kMarginR;
  const int areaH = bottom - top;
  for (size_t i = 0; i < count; ++i) {
    if (prevDotY_[i] >= 0 && prevDotR_[i] > 0) {
      GFX->fillCircle(prevLineX_[i], prevDotY_[i], prevDotR_[i] + 1, kBg);
    }
  }
  for (size_t i = 0; i < count; ++i) {
    const float level = levels[i] > 1.0f ? 1.0f : levels[i];
    const int x = kMarginL + static_cast<int>((i + 0.5f) * areaW / static_cast<float>(count));
    const int y = bottom - static_cast<int>(level * static_cast<float>(areaH - 4)) - 2;
    const int r = 1 + static_cast<int>(level * 6.0f);
    const uint16_t color =
        rainbowColor_(level, 0.80f * (static_cast<float>(i) /
                                      static_cast<float>(count > 1 ? count - 1 : 1)));
    GFX->fillCircle(x, y, r, color);
    prevLineX_[i] = x;
    prevDotY_[i] = y;
    prevDotR_[i] = r;
  }
}

void VfxRenderer::drawGlow_(int top, int bottom, const float *levels, const float *peaks,
                            size_t count) {
  if (tft_ == nullptr || levels == nullptr || peaks == nullptr || count == 0) {
    return;
  }
  const SpectrumLayout layout = makeBarLayout(count);
  if (layout.barW < 1) {
    return;
  }
  const int areaH = bottom - top;
  constexpr int kGlowExtra = 6;
  for (size_t i = 0; i < count; ++i) {
    const int x = barX(layout, i);
    const int bw = barWidth(layout, i);
    const float level = levels[i] > 1.0f ? 1.0f : levels[i];
    const float peak = peaks[i] > 1.0f ? 1.0f : peaks[i];
    int barH = floorBarH(static_cast<int>(level * static_cast<float>(areaH)), areaH);
    int peakH = static_cast<int>(peak * static_cast<float>(areaH));
    if (peakH < barH) {
      peakH = barH;
    }
    if (peakH > areaH) {
      peakH = areaH;
    }
    const uint16_t color = rainbowColor_(
        0.9f, 0.80f * (static_cast<float>(i) / static_cast<float>(count > 1 ? count - 1 : 1)));
    const int prevH = prevBarH_[i];

    if (prevH < 0) {
      GFX->fillRect(x, top, bw, areaH, kBg);
    } else if (barH > prevH) {
      GFX->fillRect(x, bottom - barH, bw, barH - prevH, color);
      const int clearY = bottom - barH - kGlowExtra;
      if (clearY >= top) {
        GFX->fillRect(x, clearY, bw, (bottom - prevH) - clearY, kBg);
      }
    } else if (barH < prevH) {
      const int eraseY = bottom - prevH - kGlowExtra;
      const int eraseH = prevH - barH + kGlowExtra;
      if (eraseY < top) {
        GFX->fillRect(x, top, bw, bottom - top - barH, kBg);
      } else {
        GFX->fillRect(x, eraseY, bw, eraseH, kBg);
      }
    }

    if (barH > 0) {
      GFX->fillRect(x, bottom - barH, bw, barH, color);
      for (int k = 1; k <= 3; ++k) {
        const int tipY = bottom - barH - k * 2;
        if (tipY < top) {
          break;
        }
        GFX->fillRect(x, tipY, bw, 2, darken_(color, 1.0f - 0.22f * static_cast<float>(k)));
      }
    }

    if (prevPeakH_[i] > barH + 4) {
      GFX->fillRect(x, bottom - prevPeakH_[i], bw, 1, kBg);
    }
    if (peakH > barH + 4) {
      GFX->fillRect(x, bottom - peakH, bw, 1, TFT_COL_WHITE);
    }
    prevBarH_[i] = barH;
    prevPeakH_[i] = peakH;
  }
}

void VfxRenderer::drawRing_(int top, int bottom, const VfxDrawContext &ctx, const float *levels,
                            size_t count) {
  if (tft_ == nullptr || levels == nullptr || count == 0) {
    return;
  }
  const int cx = TFT_WIDTH / 2;
  const int cy = (top + bottom) / 2;
  const int maxR = ((bottom - top) < TFT_WIDTH ? (bottom - top) : TFT_WIDTH) / 2 - 6;
  if (maxR < 8) {
    return;
  }
  if (!ringInit_) {
    GFX->fillRect(0, top, TFT_WIDTH, bottom - top, kBg);
    ringInit_ = true;
  }
  for (size_t i = 0; i < count; ++i) {
    const float angle = static_cast<float>(i) * 6.2831853f / static_cast<float>(count) - 1.5707963f;
    const float ca = cosf(angle);
    const float sa = sinf(angle);
    if (prevSpokeLen_[i] > 0) {
      const int ox = cx + static_cast<int>(ca * static_cast<float>(prevSpokeLen_[i]));
      const int oy = cy + static_cast<int>(sa * static_cast<float>(prevSpokeLen_[i]));
      GFX->drawLine(cx, cy, ox, oy, kBg);
    }
    const float level = levels[i] > 1.0f ? 1.0f : levels[i];
    const int len = 6 + static_cast<int>(level * static_cast<float>(maxR - 6));
    const int nx = cx + static_cast<int>(ca * static_cast<float>(len));
    const int ny = cy + static_cast<int>(sa * static_cast<float>(len));
    const uint16_t color = rainbowColor_(
        level, 0.80f * (static_cast<float>(i) / static_cast<float>(count > 1 ? count - 1 : 1)));
    GFX->drawLine(cx, cy, nx, ny, color);
    prevSpokeLen_[i] = len;
  }
  const int hub = 6 + static_cast<int>((ctx.vu > 1.0f ? 1.0f : ctx.vu) * 10.0f);
  GFX->fillCircle(cx, cy, hub + 2, kBg);
  GFX->fillCircle(cx, cy, hub, heatColor_(ctx.vu > 1.0f ? 1.0f : ctx.vu));
}

void VfxRenderer::draw(const VfxDrawContext &ctx, VfxMode mode, const float *levels,
                       const float *peaks, size_t count, float *waterfallHistory,
                       size_t &waterfallHead) {
  if (tft_ == nullptr) {
    return;
  }

  applyPendingFreqLabels_();
  drawHeader_(ctx);

#if defined(BOARD_CARDPUTER_ADV) && CARDPUTER_USE_BUILTIN_LCD
  if (plotSprite_ != nullptr && plotSprite_->width() > 0) {
    plotSprite_->fillScreen(kBg);
    resetBarCache();
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

  // On the first frame after entering a mode, clear the whole plot area so
  // column gaps (which incremental rendering never repaints) are not left
  // holding stale pixels (e.g. splash text).
  if (!areaInit_) {
    tft_->fillRect(0, kAreaTop, TFT_WIDTH, kAreaBottom - kAreaTop, kBg);
    areaInit_ = true;
  }

  // Adafruit primitives (fillRect/drawFastHLine/...) manage their own SPI
  // transactions, so do NOT wrap them in an outer startWrite/endWrite —
  // nesting would re-lock the non-recursive SPI mutex and deadlock.
  (void)waterfallHistory;
  (void)waterfallHead;
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
    case VfxMode::Rainbow: {
      const float hue = fmodf(static_cast<float>(ctx.frameMs) * 0.00002f, 1.0f);
      drawBars_(kAreaTop, kAreaBottom, levels, peaks, count, true, hue);
      break;
    }
    case VfxMode::LinePeaks:
      drawLinePeaks_(kAreaTop, kAreaBottom, levels, count);
      break;
    case VfxMode::Bounce:
      drawBounce_(kAreaTop, kAreaBottom, levels, peaks, count);
      break;
    case VfxMode::Dot:
      drawDot_(kAreaTop, kAreaBottom, levels, count);
      break;
    case VfxMode::Glow:
      drawGlow_(kAreaTop, kAreaBottom, levels, peaks, count);
      break;
    case VfxMode::Ring:
      drawRing_(kAreaTop, kAreaBottom, ctx, levels, count);
      break;
    default:
      drawBars_(kAreaTop, kAreaBottom, levels, peaks, count, false, 0.0f);
      break;
  }
}
