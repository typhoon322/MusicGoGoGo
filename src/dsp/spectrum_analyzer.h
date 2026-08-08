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

  // 3-band EQ gains (0..2). Default 1.0 = flat (no cut/boost).
  void setBassGain(float g);
  void setMidGain(float g);
  void setTrebleGain(float g);
  void setEqGains(float bass, float mid, float treble);
  float bassGain() const { return bassGain_; }
  float midGain() const { return midGain_; }
  float trebleGain() const { return trebleGain_; }

 private:
  void fillLogBands_(float *out, size_t count);
  void fillLinearBands_(float *out, size_t count, bool linearSpacing);
  void fillWaterfallRow_();
  float magnitudeToLevel_(float mag) const;
  float bandEqGain_(size_t band, size_t bandCount) const;
  static float clampEqGain_(float g);

  float real_[FFT_SIZE] = {};
  float imag_[FFT_SIZE] = {};
  SpectrumFrame frame_ = {};
  float bassGain_ = 1.0f;
  float midGain_ = 1.0f;
  float trebleGain_ = 1.0f;
};
