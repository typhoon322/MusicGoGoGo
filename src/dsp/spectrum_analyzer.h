#pragma once

#include <stddef.h>

#include "config.h"

class SpectrumAnalyzer {
 public:
  bool begin();

  // Feed time-domain samples (length == FFT_SIZE), compute bar levels 0.0–1.0
  void analyze(const int16_t *samples, size_t count);

  const float *bars() const { return bars_; }
  size_t barCount() const { return SPECTRUM_BARS; }

  float peakBinLevel() const { return peakBinLevel_; }

 private:
  float real_[FFT_SIZE] = {};
  float imag_[FFT_SIZE] = {};
  float bars_[SPECTRUM_BARS] = {};
  float peakBinLevel_ = 0.0f;
};
