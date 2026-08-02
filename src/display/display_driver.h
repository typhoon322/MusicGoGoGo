#pragma once

#include <stddef.h>
#include <stdint.h>

#include "config.h"
#include "dsp/spectrum_analyzer.h"
#include "vfx.h"

class DisplayDriver {
 public:
  bool begin(uint8_t backlightLevel = 255);

  void showSplash();
  void showError(const char *message);
  void setBacklight(uint8_t level);

  void setMode(VfxMode mode);
  VfxMode mode() const { return mode_; }
  void nextMode();
  void prevMode();

  void render(const SpectrumFrame &spec, const float *smoothLevels, const float *peakLevels,
              size_t bandCount, float rms, float peak);

 private:
  bool initialized_ = false;
  uint8_t backlightLevel_ = 255;
  VfxMode mode_ = VfxMode::Bars32;
  size_t waterfallHead_ = 0;
  float waterfallHistory_[VFX_WATERFALL_HISTORY * VFX_WATERFALL_BINS] = {};
};
