#include "dsp/spectrum_analyzer.h"

#include <Arduino.h>
#include <arduinoFFT.h>
#include <math.h>

namespace {
// 12-band octave edges (Log12 only).
constexpr float kOctave12EdgesHz[] = {20.0f,   40.0f,    80.0f,    160.0f,   315.0f,
                                      630.0f,  1250.0f,  2500.0f,  5000.0f,  8000.0f,
                                      12000.0f, 16000.0f, 20000.0f};

// 30-band ~1/3-octave edges (Bars / Mirror / Rainbow / …).
constexpr float kThird30EdgesHz[] = {
    20.0f,    25.0f,    31.0f,    40.0f,    50.0f,    63.0f,    80.0f,    100.0f,
    125.0f,   160.0f,   200.0f,   250.0f,   315.0f,   400.0f,   500.0f,   630.0f,
    800.0f,   1000.0f,  1250.0f,  1600.0f,  2000.0f,  2500.0f,  3150.0f,  4000.0f,
    5000.0f,  6300.0f,  8000.0f,  10000.0f, 12500.0f, 16000.0f, 20000.0f};

size_t binForFrequency(float hz) {
  const float binWidth = static_cast<float>(I2S_SAMPLE_RATE) / static_cast<float>(FFT_SIZE);
  if (hz <= 0.0f || binWidth <= 0.0f) {
    return 0;
  }
  size_t bin = static_cast<size_t>(hz / binWidth + 0.5f);
  const size_t maxBin = FFT_SIZE / 2;
  if (bin >= maxBin) {
    return maxBin - 1;
  }
  return bin;
}

float avgMagnitude(const float *mags, size_t lo, size_t hi) {
  if (hi <= lo) {
    return mags[lo];
  }
  float sum = 0.0f;
  for (size_t k = lo; k < hi; ++k) {
    sum += mags[k];
  }
  return sum / static_cast<float>(hi - lo);
}
}  // namespace

bool SpectrumAnalyzer::begin() {
  for (size_t i = 0; i < FFT_SIZE; ++i) {
    real_[i] = 0.0f;
    imag_[i] = 0.0f;
  }
  Serial.printf("[fft] OK size=%u rate=%u bars=%u log=%u (20Hz-20kHz)\n", FFT_SIZE,
                I2S_SAMPLE_RATE, static_cast<unsigned>(SPECTRUM_BARS),
                static_cast<unsigned>(VFX_LOG_BANDS));
  return true;
}

float SpectrumAnalyzer::clampEqGain_(float g) {
  if (g < 0.0f) {
    return 0.0f;
  }
  if (g > 2.0f) {
    return 2.0f;
  }
  return g;
}

void SpectrumAnalyzer::setBassGain(float g) {
  bassGain_ = clampEqGain_(g);
}

void SpectrumAnalyzer::setMidGain(float g) {
  midGain_ = clampEqGain_(g);
}

void SpectrumAnalyzer::setTrebleGain(float g) {
  trebleGain_ = clampEqGain_(g);
}

void SpectrumAnalyzer::setEqGains(float bass, float mid, float treble) {
  bassGain_ = clampEqGain_(bass);
  midGain_ = clampEqGain_(mid);
  trebleGain_ = clampEqGain_(treble);
}

float SpectrumAnalyzer::bandEqGain_(size_t band, size_t bandCount) const {
  // Thirds: 30→10/10/10, 12→4/4/4.
  size_t third = bandCount / 3;
  if (third < 1) {
    third = 1;
  }
  if (band < third) {
    return bassGain_;
  }
  if (band < third * 2) {
    return midGain_;
  }
  return trebleGain_;
}

float SpectrumAnalyzer::magnitudeToLevel_(float mag) const {
  return log10f(1.0f + mag * 0.08f) / 3.8f;
}

void SpectrumAnalyzer::fillLogBands_(float *out, size_t count) {
  fillLinearBands_(out, count, false);
}

void SpectrumAnalyzer::fillLinearBands_(float *out, size_t count, bool /*linearSpacing*/) {
  const size_t maxBin = FFT_SIZE / 2;
  const float *edges = kThird30EdgesHz;
  size_t edgeBands = 30;
  if (count == VFX_LOG_BANDS || count == 12) {
    edges = kOctave12EdgesHz;
    edgeBands = 12;
  } else if (count >= 30) {
    edges = kThird30EdgesHz;
    edgeBands = 30;
  }

  const size_t n = count < edgeBands ? count : edgeBands;
  for (size_t b = 0; b < n; ++b) {
    size_t lo = binForFrequency(edges[b]);
    size_t hi = binForFrequency(edges[b + 1]);
    if (lo < 1) {
      lo = 1;
    }
    if (hi <= lo) {
      hi = lo + 1;
    }
    if (hi > maxBin) {
      hi = maxBin;
    }
    const float mag = avgMagnitude(real_, lo, hi);
    float level = magnitudeToLevel_(mag) * bandEqGain_(b, n);
    if (level > 1.0f) {
      level = 1.0f;
    }
    out[b] = level;
  }
  for (size_t b = n; b < count; ++b) {
    out[b] = 0.0f;
  }
}

void SpectrumAnalyzer::fillWaterfallRow_() {
  fillLogBands_(frame_.waterfallRow, VFX_WATERFALL_BINS);
}

void SpectrumAnalyzer::analyze(const int16_t *samples, size_t count) {
  if (samples == nullptr || count < FFT_SIZE) {
    return;
  }

  double mean = 0.0;
  for (size_t i = 0; i < FFT_SIZE; ++i) {
    mean += static_cast<double>(samples[i]);
  }
  mean /= static_cast<double>(FFT_SIZE);

  for (size_t i = 0; i < FFT_SIZE; ++i) {
    real_[i] = static_cast<float>(static_cast<double>(samples[i]) - mean);
    imag_[i] = 0.0f;
  }

  ArduinoFFT<float> fft(real_, imag_, FFT_SIZE, I2S_SAMPLE_RATE);
  fft.windowing(FFTWindow::Hamming, FFTDirection::Forward);
  fft.compute(FFTDirection::Forward);
  fft.complexToMagnitude();

  fillLinearBands_(frame_.linear32, SPECTRUM_BARS, true);
  fillLogBands_(frame_.log12, VFX_LOG_BANDS);
  fillLinearBands_(frame_.mirror32, SPECTRUM_BARS, false);
  fillWaterfallRow_();

  for (size_t i = 0; i < SPECTRUM_BARS; ++i) {
    frame_.line32[i] = frame_.linear32[i];
  }

  frame_.peakLevel = 0.0f;
  for (size_t i = 0; i < SPECTRUM_BARS; ++i) {
    if (frame_.linear32[i] > frame_.peakLevel) {
      frame_.peakLevel = frame_.linear32[i];
    }
  }

  double sumSq = 0.0;
  for (size_t i = 0; i < FFT_SIZE; ++i) {
    const float s = static_cast<float>(samples[i]) / 32768.0f;
    sumSq += static_cast<double>(s) * static_cast<double>(s);
  }
  frame_.vuLevel = static_cast<float>(sqrt(sumSq / static_cast<double>(FFT_SIZE)));
  if (frame_.vuLevel > 1.0f) {
    frame_.vuLevel = 1.0f;
  }
}
