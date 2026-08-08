#pragma once

#include <stddef.h>

// Attack / peak smoothing inspired by ESP32-AudioInI2S falloff models.

class BandProcessor {
 public:
  static constexpr size_t kMaxBands = 64;

  void begin(size_t bandCount);
  void reset();

  void setAutoLevelEnabled(bool enabled) { autoLevelEnabled_ = enabled; }
  void setAttack(float attack) { attack_ = attack; }
  void setDecay(float decay) { decay_ = decay; }
  void setPeakDecay(float decay) { peakDecay_ = decay; }

  bool autoLevelEnabled() const { return autoLevelEnabled_; }
  float attack() const { return attack_; }
  float decay() const { return decay_; }
  float peakDecay() const { return peakDecay_; }

  // input/output length == bandCount, values 0..1
  void process(const float *input, float *output);
  const float *peaks() const { return peaks_; }
  float autoGain() const { return autoGain_; }

 private:
  size_t bandCount_ = 0;
  bool autoLevelEnabled_ = true;
  float attack_ = 0.62f;
  float decay_ = 0.76f;
  float peakDecay_ = 0.90f;
  float autoGain_ = 1.0f;
  float levelTrack_ = 0.08f;
  float smoothed_[kMaxBands] = {};
  float peaks_[kMaxBands] = {};
};
