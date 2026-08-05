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

float logEdgeHz(size_t index, size_t count) {
#if defined(BOARD_CARDPUTER_ADV)
  const float minHz = 90.0f;
#else
  const float minHz = 60.0f;
#endif
  const float maxHz = static_cast<float>(I2S_SAMPLE_RATE) * 0.48f;
  const float t = static_cast<float>(index) / static_cast<float>(count);
  return minHz * powf(maxHz / minHz, t);
}

#if defined(BOARD_CARDPUTER_ADV)
float bassWeight_(size_t band) {
  static const float kWeights[] = {0.82f, 0.88f, 0.92f, 0.96f};
  if (band >= 4) {
    return 1.0f;
  }
  return kWeights[band];
}

size_t lowBinSkip_(size_t band) {
  static const size_t kSkip[] = {5, 3, 2, 1};
  if (band >= 4) {
    return 0;
  }
  return kSkip[band];
}
#endif

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
  Serial.printf("[fft] OK size=%u rate=%u\n", FFT_SIZE, I2S_SAMPLE_RATE);
  return true;
}

float SpectrumAnalyzer::magnitudeToLevel_(float mag) const {
  return log10f(1.0f + mag * 0.08f) / 3.8f;
}

void SpectrumAnalyzer::fillLogBands_(float *out, size_t count) {
  for (size_t b = 0; b < count; ++b) {
    size_t lo = binForFrequency(logEdgeHz(b, count));
    size_t hi = binForFrequency(logEdgeHz(b + 1, count));
    if (lo == 0) {
      lo = 1;
    }
    if (hi <= lo) {
      hi = lo + 1;
    }
    const float mag = avgMagnitude(real_, lo, hi);
    out[b] = magnitudeToLevel_(mag);
  }
}

void SpectrumAnalyzer::fillLinearBands_(float *out, size_t count, bool linearSpacing) {
  const size_t maxBin = FFT_SIZE / 2;
  for (size_t b = 0; b < count; ++b) {
    size_t lo = 0;
    size_t hi = 0;
    if (linearSpacing) {
      lo = (b * maxBin) / count;
      hi = ((b + 1) * maxBin) / count;
#if defined(BOARD_CARDPUTER_ADV)
      const size_t skip = lowBinSkip_(b);
      if (lo < skip) {
        lo = skip;
      }
#else
      // Skip DC + very low bins (1/f noise)
      if (b == 0 && lo < 4) {
        lo = 4;
      }
#endif
    } else {
      lo = binForFrequency(logEdgeHz(b, count));
      hi = binForFrequency(logEdgeHz(b + 1, count));
    }
    if (hi <= lo) {
      hi = lo + 1;
    }
    if (hi > maxBin) {
      hi = maxBin;
    }
    if (lo == 0) {
      lo = 1;
    }
    const float mag = avgMagnitude(real_, lo, hi);
    out[b] = magnitudeToLevel_(mag);
#if defined(BOARD_CARDPUTER_ADV)
    out[b] *= bassWeight_(b);
#endif
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
