#pragma once

#include <stddef.h>
#include <stdint.h>

#include "config.h"

struct SpectrumFrame {
  float linear32[SPECTRUM_BARS];
  float log12[VFX_LOG_BANDS];
  float mirror32[SPECTRUM_BARS];
  float waterfallRow[VFX_WATERFALL_BINS];
  float line32[SPECTRUM_BARS];
  float peakLevel;
  float vuLevel;
};

class SpectrumAnalyzer {
 public:
  bool begin();
  void analyze(const int16_t *samples, size_t count);

  const SpectrumFrame &frame() const { return frame_; }

 private:
  void fillLogBands_(float *out, size_t count);
  void fillLinearBands_(float *out, size_t count, bool linearSpacing);
  void fillWaterfallRow_();
  float magnitudeToLevel_(float mag) const;

  float real_[FFT_SIZE] = {};
  float imag_[FFT_SIZE] = {};
  SpectrumFrame frame_ = {};
};
