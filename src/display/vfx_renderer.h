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
  float beatBpm = 0.0f;
  float beatConfidence = 0.0f;
  float kickPulse = 0.0f;
  float snarePulse = 0.0f;
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

  void setShowFreqLabels(bool on);
  bool showFreqLabels() const {
    return pendingFreqLabelsDirty_ ? pendingFreqLabels_ : showFreqLabels_;
  }

 private:
  void applyPendingFreqLabels_();
  uint16_t heatColor_(float level) const;
  uint16_t rainbowColor_(float level, float hue) const;
  uint16_t darken_(uint16_t color, float factor) const;
  void drawHeader_(const VfxDrawContext &ctx);
  void drawDancingCat_(const VfxDrawContext &ctx);
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
  void drawBounce_(int top, int bottom, const float *levels, const float *peaks, size_t count);
  void drawDot_(int top, int bottom, const float *levels, size_t count);
  void drawGlow_(int top, int bottom, const float *levels, const float *peaks, size_t count);
  void drawRing_(int top, int bottom, const VfxDrawContext &ctx, const float *levels, size_t count);
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
  // S3 header cat state
  float catX_ = 40.0f;
  int catDir_ = 1;
  int prevCatX_ = -1;
  int prevCatY_ = -1;
  float catHopSmoothed_ = 0.0f;
  bool catHeaderInit_ = false;
  int prevBarH_[64] = {};
  int prevPeakH_[64] = {};
  int lastBarLabelCount_ = -1;
  bool showFreqLabels_ = false;
  bool pendingFreqLabels_ = false;
  bool pendingFreqLabelsDirty_ = false;
  bool prevVuSegLit_[32] = {};
  int prevLineX_[64] = {};
  int prevLineY_[64] = {};
  bool prevLineValid_ = false;
  bool vuInit_ = false;
  bool areaInit_ = false;
  float bounceH_[64] = {};
  float bounceVel_[64] = {};
  int prevDotY_[64] = {};
  int prevDotR_[64] = {};
  int prevSpokeLen_[64] = {};
  bool ringInit_ = false;
};
