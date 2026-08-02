#pragma once

#include <stddef.h>
#include <stdint.h>

class DisplayDriver {
 public:
  bool begin(uint8_t backlightLevel = 255);

  void showSplash();
  void showError(const char *message);

  // Draw spectrum bars (levels 0.0–1.0) with peak-hold decay
  void updateSpectrum(const float *levels, size_t count, float rms, float peak);

  void setBacklight(uint8_t level);

 private:
  void drawHeader_(float rms, float peak);
  uint16_t barColor_(float level) const;

  bool initialized_ = false;
  uint8_t backlightLevel_ = 255;
  float displayLevels_[64] = {};
  size_t displayCount_ = 0;
};
