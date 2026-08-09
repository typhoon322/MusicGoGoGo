#include "dsp/spectrum_analyzer.h"

#include <Arduino.h>
#include <arduinoFFT.h>
#include <math.h>
#include <string.h>

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

// Defaults; runtime-tunable via WebUI / NVS.
constexpr float kNoiseMarginDefault = 1.12f;
constexpr float kDbRangeDefault = 42.0f;

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

void fillRawMags(float *out, size_t count, const float *edges, size_t edgeBands,
                 const float *mags, size_t maxBin) {
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
    out[b] = avgMagnitude(mags, lo, hi);
  }
  for (size_t b = n; b < count; ++b) {
    out[b] = 0.0f;
  }
}
}  // namespace

bool SpectrumAnalyzer::begin() {
  for (size_t i = 0; i < FFT_SIZE; ++i) {
    real_[i] = 0.0f;
    imag_[i] = 0.0f;
    ring_[i] = 0;
  }
  ringPos_ = 0;
  ringFilled_ = 0;
  noiseReady_ = false;
  noiseCalibrating_ = false;
  Serial.printf("[fft] OK size=%u hop=%u rate=%u bars=%u (dB+noise gate)\n", FFT_SIZE,
                static_cast<unsigned>(FFT_HOP), I2S_SAMPLE_RATE,
                static_cast<unsigned>(SPECTRUM_BARS));
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

void SpectrumAnalyzer::setNoiseMargin(float m) {
  if (m < 1.00f) {
    m = 1.00f;
  }
  if (m > 1.60f) {
    m = 1.60f;
  }
  noiseMargin_ = m;
}

void SpectrumAnalyzer::setDbRange(float db) {
  if (db < 24.0f) {
    db = 24.0f;
  }
  if (db > 72.0f) {
    db = 72.0f;
  }
  dbRange_ = db;
}

float SpectrumAnalyzer::bandEqGain_(size_t band, size_t bandCount) const {
  if (bandCount <= 1) {
    return midGain_;
  }
  const float t = static_cast<float>(band) / static_cast<float>(bandCount - 1);
  if (t <= 0.5f) {
    const float u = t * 2.0f;
    return bassGain_ + (midGain_ - bassGain_) * u;
  }
  const float u = (t - 0.5f) * 2.0f;
  return midGain_ + (trebleGain_ - midGain_) * u;
}

float SpectrumAnalyzer::magnitudeToLevel_(float mag, float noiseFloor) const {
  const float floor = noiseFloor > 1.0f ? noiseFloor : 1.0f;
  const float threshold = floor * noiseMargin_;
  if (mag < threshold) {
    return 0.0f;
  }
  float energy = mag - floor;
  if (energy <= 0.0f) {
    return 0.0f;
  }
  const float ref = floor * 0.25f;
  float db = 20.0f * log10f(energy / (ref > 1.0f ? ref : 1.0f) + 1e-9f);
  if (db < 0.0f) {
    db = 0.0f;
  }
  const float range = dbRange_ > 1.0f ? dbRange_ : kDbRangeDefault;
  float level = db / range;
  if (level > 1.0f) {
    level = 1.0f;
  }
  return level;
}

float *SpectrumAnalyzer::noiseFloorForCount_(size_t count) {
  if (count == VFX_LOG_BANDS || count == 12) {
    return noiseFloor12_;
  }
  return noiseFloor30_;
}

void SpectrumAnalyzer::beginNoiseCalibration() {
  noiseCalibrating_ = true;
  noiseReady_ = false;
  noiseFrames_ = 0;
  memset(noiseAcc30_, 0, sizeof(noiseAcc30_));
  memset(noiseAcc12_, 0, sizeof(noiseAcc12_));
  memset(noiseFloor30_, 0, sizeof(noiseFloor30_));
  memset(noiseFloor12_, 0, sizeof(noiseFloor12_));
  Serial.println(F("[fft] noise cal: keep quiet 2.5s…"));
}

void SpectrumAnalyzer::accumulateNoiseSample_() {
  if (!noiseCalibrating_) {
    return;
  }
  // Average floor (peak was too harsh — one spike deafened the band).
  float raw30[SPECTRUM_BARS];
  float raw12[VFX_LOG_BANDS];
  const size_t maxBin = FFT_SIZE / 2;
  fillRawMags(raw30, SPECTRUM_BARS, kThird30EdgesHz, 30, real_, maxBin);
  fillRawMags(raw12, VFX_LOG_BANDS, kOctave12EdgesHz, 12, real_, maxBin);
  for (size_t i = 0; i < SPECTRUM_BARS; ++i) {
    noiseAcc30_[i] += raw30[i];
  }
  for (size_t i = 0; i < VFX_LOG_BANDS; ++i) {
    noiseAcc12_[i] += raw12[i];
  }
  ++noiseFrames_;
}

void SpectrumAnalyzer::finishNoiseCalibration() {
  const float n = noiseFrames_ > 0 ? static_cast<float>(noiseFrames_) : 1.0f;
  for (size_t i = 0; i < SPECTRUM_BARS; ++i) {
    noiseFloor30_[i] = (noiseAcc30_[i] / n) * 1.05f;
    if (noiseFloor30_[i] < 1.0f) {
      noiseFloor30_[i] = 1.0f;
    }
  }
  for (size_t i = 0; i < VFX_LOG_BANDS; ++i) {
    noiseFloor12_[i] = (noiseAcc12_[i] / n) * 1.05f;
    if (noiseFloor12_[i] < 1.0f) {
      noiseFloor12_[i] = 1.0f;
    }
  }
  noiseCalibrating_ = false;
  noiseReady_ = true;
  Serial.printf("[fft] noise cal done frames=%lu (avg floor)\n",
                static_cast<unsigned long>(noiseFrames_));
}

void SpectrumAnalyzer::pushSamples_(const int16_t *samples, size_t count) {
  if (samples == nullptr || count == 0) {
    return;
  }
  if (count >= FFT_SIZE) {
    memcpy(ring_, samples + (count - FFT_SIZE), FFT_SIZE * sizeof(int16_t));
    ringPos_ = 0;
    ringFilled_ = FFT_SIZE;
    return;
  }
  for (size_t i = 0; i < count; ++i) {
    ring_[ringPos_] = samples[i];
    ringPos_ = (ringPos_ + 1) % FFT_SIZE;
    if (ringFilled_ < FFT_SIZE) {
      ++ringFilled_;
    }
  }
}

void SpectrumAnalyzer::copyRingToFft_() {
  // Oldest → newest into real_[], subtract DC mean.
  double mean = 0.0;
  if (ringFilled_ < FFT_SIZE) {
    for (size_t i = 0; i < FFT_SIZE; ++i) {
      const float s = static_cast<float>(ring_[i]);
      real_[i] = s;
      imag_[i] = 0.0f;
      mean += s;
    }
  } else {
    size_t idx = ringPos_;  // next write = oldest
    for (size_t i = 0; i < FFT_SIZE; ++i) {
      const float s = static_cast<float>(ring_[idx]);
      real_[i] = s;
      imag_[i] = 0.0f;
      mean += s;
      idx = (idx + 1) % FFT_SIZE;
    }
  }
  mean /= static_cast<double>(FFT_SIZE);
  for (size_t i = 0; i < FFT_SIZE; ++i) {
    real_[i] = static_cast<float>(static_cast<double>(real_[i]) - mean);
  }
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

  const float *floors = noiseFloorForCount_(count);
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
    const float floor = noiseReady_ ? floors[b] : 1.0f;
    float level = magnitudeToLevel_(mag, floor) * bandEqGain_(b, n);
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
  if (samples == nullptr || count == 0) {
    return;
  }

  pushSamples_(samples, count);
  if (ringFilled_ < FFT_SIZE / 4) {
    return;
  }

  copyRingToFft_();

  ArduinoFFT<float> fft(real_, imag_, FFT_SIZE, I2S_SAMPLE_RATE);
  fft.windowing(FFTWindow::Hamming, FFTDirection::Forward);
  fft.compute(FFTDirection::Forward);
  fft.complexToMagnitude();

  if (noiseCalibrating_) {
    accumulateNoiseSample_();
  }

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

  // VU from latest hop / window RMS (DC-removed ring copy already in real_ before FFT —
  // use PCM ring mean-square instead).
  double sumSq = 0.0;
  if (ringFilled_ < FFT_SIZE) {
    for (size_t i = 0; i < ringFilled_; ++i) {
      const float s = static_cast<float>(ring_[i]) / 32768.0f;
      sumSq += static_cast<double>(s) * static_cast<double>(s);
    }
    frame_.vuLevel = static_cast<float>(sqrt(sumSq / static_cast<double>(ringFilled_)));
  } else {
    for (size_t i = 0; i < FFT_SIZE; ++i) {
      const float s = static_cast<float>(ring_[i]) / 32768.0f;
      sumSq += static_cast<double>(s) * static_cast<double>(s);
    }
    frame_.vuLevel = static_cast<float>(sqrt(sumSq / static_cast<double>(FFT_SIZE)));
  }
  if (frame_.vuLevel > 1.0f) {
    frame_.vuLevel = 1.0f;
  }
}
