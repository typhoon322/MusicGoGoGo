#pragma once

#include <Adafruit_ST7789.h>

#include "config.h"
#include "vfx.h"

struct VfxDrawContext {
  float rms;
  float peak;
  float vu;
  uint32_t frameMs;
  VfxMode mode;
};

class VfxRenderer {
 public:
  void attach(Adafruit_ST7789 *tft);

  void draw(const VfxDrawContext &ctx, VfxMode mode, const float *levels, const float *peaks,
            size_t count, float *waterfallHistory, size_t &waterfallHead);

  void resetWaterfall();

 private:
  uint16_t heatColor_(float level) const;
  uint16_t rainbowColor_(float level, float hue) const;
  uint16_t gradientBarColor_(float level) const;
  void drawHeader_(const VfxDrawContext &ctx);
  void drawBars_(int top, int bottom, const float *levels, const float *peaks, size_t count,
                 bool gradient, float hueShift);
  void drawMirror_(int top, int bottom, const float *levels, const float *peaks, size_t count);
  void drawVu_(int top, int bottom, const VfxDrawContext &ctx, const float *levels, size_t count);
  void drawWaterfall_(int top, int bottom, const float *row, float *history, size_t &head);
  void drawLinePeaks_(int top, int bottom, const float *levels, size_t count);

  Adafruit_ST7789 *tft_ = nullptr;
  uint16_t *waterfallFb_ = nullptr;
};
