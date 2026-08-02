#include "dsp/spectrum_analyzer.h"

#include <Arduino.h>
#include <arduinoFFT.h>
#include <math.h>

namespace {
size_t binForFrequency(float hz) {
  const float binWidth = static_cast<float>(I2S_SAMPLE_RATE) / static_cast<float>(FFT_SIZE);
  if (hz <= 0.0f || binWidth <= 0.0f) {
    return 0;
  }
  size_t bin = static_cast<size_t>(hz / binWidth);
  const size_t maxBin = FFT_SIZE / 2;
  if (bin >= maxBin) {
    return maxBin - 1;
  }
  return bin;
}

float logBinEdgeHz(size_t barIndex, size_t barCount) {
  const float minHz = 80.0f;
  const float maxHz = static_cast<float>(I2S_SAMPLE_RATE) * 0.45f;
  const float t = static_cast<float>(barIndex) / static_cast<float>(barCount);
  return minHz * powf(maxHz / minHz, t);
}
}  // namespace

bool SpectrumAnalyzer::begin() {
  for (size_t i = 0; i < FFT_SIZE; ++i) {
    real_[i] = 0.0f;
    imag_[i] = 0.0f;
  }
  for (size_t i = 0; i < SPECTRUM_BARS; ++i) {
    bars_[i] = 0.0f;
  }
  Serial.printf("[fft] OK size=%u bars=%u rate=%u\n", FFT_SIZE, SPECTRUM_BARS,
                I2S_SAMPLE_RATE);
  return true;
}

void SpectrumAnalyzer::analyze(const int16_t *samples, size_t count) {
  if (samples == nullptr || count < FFT_SIZE) {
    return;
  }

  for (size_t i = 0; i < FFT_SIZE; ++i) {
    real_[i] = static_cast<float>(samples[i]);
    imag_[i] = 0.0f;
  }

  ArduinoFFT<float> fft(real_, imag_, FFT_SIZE, I2S_SAMPLE_RATE);
  fft.windowing(FFTWindow::Hamming, FFTDirection::Forward);
  fft.compute(FFTDirection::Forward);
  fft.complexToMagnitude();

  peakBinLevel_ = 0.0f;
  for (size_t b = 0; b < SPECTRUM_BARS; ++b) {
    const size_t startBin = binForFrequency(logBinEdgeHz(b, SPECTRUM_BARS));
    const size_t endBin = binForFrequency(logBinEdgeHz(b + 1, SPECTRUM_BARS));
    size_t lo = startBin;
    size_t hi = endBin;
    if (hi <= lo) {
      hi = lo + 1;
    }
    if (hi > FFT_SIZE / 2) {
      hi = FFT_SIZE / 2;
    }

    float sum = 0.0f;
    for (size_t k = lo; k < hi; ++k) {
      sum += real_[k];
    }
    const float avg = sum / static_cast<float>(hi - lo);
    // Normalize — tuned for typical INMP441 room levels
    const float level = log10f(1.0f + avg) / 4.0f;
    bars_[b] = level;
    if (level > peakBinLevel_) {
      peakBinLevel_ = level;
    }
  }
}
