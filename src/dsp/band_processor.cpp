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

  constexpr float kSilence = 0.028f;
  if (autoLevelEnabled_) {
    // Absolute silence / noise-floor gate: do NOT let AGC inflate HVAC
    // rumble into full-scale bars after the music stops.
    if (framePeak < kSilence) {
      levelTrack_ *= 0.82f;
      if (levelTrack_ < 0.04f) {
        levelTrack_ = 0.04f;
      }
      autoGain_ *= 0.80f;
      if (autoGain_ < 1.0f) {
        autoGain_ = 1.0f;
      }
    } else if (framePeak > levelTrack_) {
      levelTrack_ = framePeak;
      autoGain_ = 0.82f / levelTrack_;
    } else {
      levelTrack_ = levelTrack_ * 0.992f + framePeak * 0.008f;
      if (levelTrack_ < 0.04f) {
        levelTrack_ = 0.04f;
      }
      autoGain_ = 0.82f / levelTrack_;
    }
    if (autoGain_ < 0.35f) {
      autoGain_ = 0.35f;
    }
    if (autoGain_ > 6.0f) {
      autoGain_ = 6.0f;
    }
  } else {
    autoGain_ = 1.0f;
  }

  const bool silent = framePeak < kSilence;
  const float decayNow = silent ? 0.48f : decay_;
  const float peakDecayNow = silent ? 0.62f : peakDecay_;

  for (size_t i = 0; i < bandCount_; ++i) {
    float target = silent ? 0.0f : (input[i] * autoGain_);
    if (target > 1.0f) {
      target = 1.0f;
    }

    if (target > smoothed_[i]) {
      smoothed_[i] += (target - smoothed_[i]) * attack_;
    } else {
      smoothed_[i] = smoothed_[i] * decayNow + target * (1.0f - decayNow);
    }

    if (smoothed_[i] < 0.002f) {
      smoothed_[i] = 0.0f;
    }

    if (smoothed_[i] > peaks_[i]) {
      peaks_[i] = smoothed_[i];
    } else {
      peaks_[i] *= peakDecayNow;
      if (peaks_[i] < smoothed_[i]) {
        peaks_[i] = smoothed_[i];
      }
    }

    output[i] = smoothed_[i];
  }
}
