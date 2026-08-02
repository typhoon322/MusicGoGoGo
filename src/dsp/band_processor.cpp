#include <Arduino.h>

#include "dsp/band_processor.h"

#include <math.h>

void BandProcessor::begin(size_t bandCount) {
  bandCount_ = bandCount > kMaxBands ? kMaxBands : bandCount;
  reset();
}

void BandProcessor::reset() {
  for (size_t i = 0; i < kMaxBands; ++i) {
    smoothed_[i] = 0.0f;
    peaks_[i] = 0.0f;
  }
  autoGain_ = 1.0f;
  levelTrack_ = 0.08f;
}

void BandProcessor::process(const float *input, float *output) {
  if (input == nullptr || output == nullptr || bandCount_ == 0) {
    return;
  }

  float framePeak = 0.0f;
  for (size_t i = 0; i < bandCount_; ++i) {
    if (input[i] > framePeak) {
      framePeak = input[i];
    }
  }

  if (autoLevelEnabled_) {
    if (framePeak > levelTrack_) {
      levelTrack_ = framePeak;
    } else {
      levelTrack_ = levelTrack_ * 0.992f + framePeak * 0.008f;
    }
    if (levelTrack_ < 0.04f) {
      levelTrack_ = 0.04f;
    }
    autoGain_ = 0.82f / levelTrack_;
    if (autoGain_ < 0.35f) {
      autoGain_ = 0.35f;
    }
    if (autoGain_ > 6.0f) {
      autoGain_ = 6.0f;
    }
  } else {
    autoGain_ = 1.0f;
  }

  for (size_t i = 0; i < bandCount_; ++i) {
    float target = input[i] * autoGain_;
    if (target > 1.0f) {
      target = 1.0f;
    }

    if (target > smoothed_[i]) {
      smoothed_[i] += (target - smoothed_[i]) * attack_;
    } else {
      smoothed_[i] = smoothed_[i] * decay_ + target * (1.0f - decay_);
    }

    if (smoothed_[i] < 0.002f) {
      smoothed_[i] = 0.0f;
    }

    if (smoothed_[i] > peaks_[i]) {
      peaks_[i] = smoothed_[i];
    } else {
      peaks_[i] *= peakDecay_;
      if (peaks_[i] < smoothed_[i]) {
        peaks_[i] = smoothed_[i];
      }
    }

    output[i] = smoothed_[i];
  }
}
