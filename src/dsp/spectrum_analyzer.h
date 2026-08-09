#pragma once

#include <stddef.h>
#include <stdint.h>

#include "config.h"

#ifndef FFT_HOP
#define FFT_HOP FFT_SIZE
#endif

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

  // Push PCM (full window or hop) then run FFT + band fill.
  void analyze(const int16_t *samples, size_t count);

  // Quiet-room noise-floor capture (call analyze() each hop during window).
  void beginNoiseCalibration();
  void finishNoiseCalibration();
  bool noiseCalibrated() const { return noiseReady_; }

  const SpectrumFrame &frame() const { return frame_; }

  void setBassGain(float g);
  void setMidGain(float g);
  void setTrebleGain(float g);
  void setEqGains(float bass, float mid, float treble);
  float bassGain() const { return bassGain_; }
  float midGain() const { return midGain_; }
  float trebleGain() const { return trebleGain_; }

  void setNoiseMargin(float m);
  void setDbRange(float db);
  float noiseMargin() const { return noiseMargin_; }
  float dbRange() const { return dbRange_; }

 private:
  void fillLogBands_(float *out, size_t count);
  void fillLinearBands_(float *out, size_t count, bool linearSpacing);
  void fillWaterfallRow_();
  void pushSamples_(const int16_t *samples, size_t count);
  void copyRingToFft_();
  void accumulateNoiseSample_();
  float magnitudeToLevel_(float mag, float noiseFloor) const;
  float bandEqGain_(size_t band, size_t bandCount) const;
  float *noiseFloorForCount_(size_t count);
  static float clampEqGain_(float g);

  float real_[FFT_SIZE] = {};
  float imag_[FFT_SIZE] = {};
  int16_t ring_[FFT_SIZE] = {};
  size_t ringPos_ = 0;
  size_t ringFilled_ = 0;
  SpectrumFrame frame_ = {};
  float bassGain_ = 1.0f;
  float midGain_ = 1.0f;
  float trebleGain_ = 1.0f;
  float noiseMargin_ = 1.12f;
  float dbRange_ = 42.0f;

  float noiseFloor30_[SPECTRUM_BARS] = {};
  float noiseFloor12_[VFX_LOG_BANDS] = {};
  float noiseAcc30_[SPECTRUM_BARS] = {};
  float noiseAcc12_[VFX_LOG_BANDS] = {};
  uint32_t noiseFrames_ = 0;
  bool noiseCalibrating_ = false;
  bool noiseReady_ = false;
};
