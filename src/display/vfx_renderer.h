#pragma once

#include "config.h"
#include "display/tft_colors.h"
#include "vfx.h"

#if defined(BOARD_CARDPUTER_ADV) && CARDPUTER_USE_BUILTIN_LCD
#include <M5GFX.h>
using VfxTft = M5GFX;
#else
#include <Adafruit_ST7789.h>
using VfxTft = Adafruit_ST7789;
#endif

struct VfxDrawContext {
  float rms;
  float peak;
  float vu;
  uint32_t frameMs;
  VfxMode mode;
#if defined(BOARD_CARDPUTER_ADV)
  bool showMicDebug = false;
  int8_t batteryPercent = -1;
  int16_t micRawMin;
  int16_t micRawMax;
  int32_t micRawMean;
  float micGain;
  float band0;
  float band1;
  float band2;
  float band3;
#endif
};

class VfxRenderer {
 public:
  void attach(VfxTft *tft);

  void draw(const VfxDrawContext &ctx, VfxMode mode, const float *levels, const float *peaks,
            size_t count, float *waterfallHistory, size_t &waterfallHead);

  void resetWaterfall();
  void resetHeaderCache();
  void resetBarCache();
  void clearPlotArea();

 private:
  uint16_t heatColor_(float level) const;
  uint16_t rainbowColor_(float level, float hue) const;
  uint16_t gradientBarColor_(float level) const;
  void drawHeader_(const VfxDrawContext &ctx);
#if defined(BOARD_CARDPUTER_ADV)
  void drawBatteryBadge_(const VfxDrawContext &ctx);
  void drawMicDebugOverlay_(const VfxDrawContext &ctx);
#endif
  void drawBars_(int top, int bottom, const float *levels, const float *peaks, size_t count,
                 bool gradient, float hueShift);
  void drawMirror_(int top, int bottom, const float *levels, const float *peaks, size_t count);
  void drawVu_(int top, int bottom, const VfxDrawContext &ctx, const float *levels, size_t count);
  void drawWaterfall_(int top, int bottom, const float *row, float *history, size_t &head);
  void drawLinePeaks_(int top, int bottom, const float *levels, size_t count);
  void drawPlotMode_(const VfxDrawContext &ctx, VfxMode mode, const float *levels,
                     const float *peaks, size_t count, float *waterfallHistory,
                     size_t &waterfallHead);

  VfxTft *tft_ = nullptr;
  uint16_t *waterfallFb_ = nullptr;
#if defined(BOARD_CARDPUTER_ADV) && CARDPUTER_USE_BUILTIN_LCD
  lgfx::LGFX_Sprite *plotSprite_ = nullptr;
  lgfx::LovyanGFX *gfxOverride_ = nullptr;
  int plotSpriteH_ = 0;
  uint32_t lastHeaderDrawMs_ = 0;
  int8_t lastBatteryPercent_ = -1;
  bool lastShowMicDebug_ = false;
#endif
  VfxMode lastHeaderMode_ = VfxMode::Count;
  float lastHeaderVu_ = -1.0f;
  float lastHeaderPeak_ = -1.0f;
  int prevBarH_[64] = {};
  int prevPeakH_[64] = {};
};
