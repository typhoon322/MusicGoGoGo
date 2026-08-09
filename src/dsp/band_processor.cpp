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
  levelTrack_ = 0.12f;
}

void BandProcessor::setAgcTarget(float t) {
  if (t < 0.40f) {
    t = 0.40f;
  }
  if (t > 0.95f) {
    t = 0.95f;
  }
  agcTarget_ = t;
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

  // Soft AGC: don't crush quiet passages; don't zero the whole frame on mild dips.
  constexpr float kQuiet = 0.008f;
  if (autoLevelEnabled_) {
    if (framePeak < kQuiet) {
      levelTrack_ *= 0.97f;
      if (levelTrack_ < 0.06f) {
        levelTrack_ = 0.06f;
      }
      autoGain_ *= 0.97f;
      if (autoGain_ < 1.0f) {
        autoGain_ = 1.0f;
      }
    } else if (framePeak > levelTrack_) {
      // Rise slowly so sudden peaks don't instantly squash sensitivity.
      levelTrack_ = levelTrack_ * 0.85f + framePeak * 0.15f;
      autoGain_ = agcTarget_ / levelTrack_;
    } else {
      levelTrack_ = levelTrack_ * 0.995f + framePeak * 0.005f;
      if (levelTrack_ < 0.06f) {
        levelTrack_ = 0.06f;
      }
      autoGain_ = agcTarget_ / levelTrack_;
    }
    if (autoGain_ < 0.55f) {
      autoGain_ = 0.55f;
    }
    if (autoGain_ > 4.5f) {
      autoGain_ = 4.5f;
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
      smoothed_[i] += (target - smoothed_[i]) * release_;
    }

    if (smoothed_[i] < 0.0015f) {
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
