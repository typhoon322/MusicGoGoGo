#pragma once

#include <stddef.h>
#include <stdint.h>

#include "config.h"
#include "dsp/spectrum_analyzer.h"
#include "vfx.h"

#if defined(BOARD_CARDPUTER_ADV)
struct MicDebugInfo {
  int8_t batteryPercent = -1;
  int16_t rawMin = 0;
  int16_t rawMax = 0;
  int32_t rawMean = 0;
  float gain = 1.0f;
  float bands[4] = {};
};
#endif

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

  uint8_t backlightLevel() const { return backlightLevel_; }

  void setShowFreqLabels(bool on);
  bool showFreqLabels() const;

#if defined(BOARD_CARDPUTER_ADV)
  void toggleDebugOverlay();
  bool debugOverlay() const { return debugOverlay_; }
#endif

  void render(const SpectrumFrame &spec, const float *smoothLevels, const float *peakLevels,
              size_t bandCount, float rms, float peak
#if defined(BOARD_CARDPUTER_ADV)
              ,
              const MicDebugInfo &micDebug
#endif
  );

 private:
  bool initialized_ = false;
  uint8_t backlightLevel_ = 255;
#if defined(BOARD_CARDPUTER_ADV)
  bool debugOverlay_ = false;
#endif
  VfxMode mode_ = VfxMode::Bars32;
  size_t waterfallHead_ = 0;
#if defined(BOARD_S3_DEV)
  // Allocated in PSRAM at begin() — keeps ~150KB+ off internal heap.
  float *waterfallHistory_ = nullptr;
#else
  float waterfallHistory_[VFX_WATERFALL_HISTORY * VFX_WATERFALL_BINS] = {};
#endif
};
