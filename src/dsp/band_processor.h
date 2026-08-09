#pragma once

#include <stddef.h>

// Attack / release smoothing (player-like spectrum falloff).

class BandProcessor {
 public:
  static constexpr size_t kMaxBands = 64;

  void begin(size_t bandCount);
  void reset();

  void setAutoLevelEnabled(bool enabled) { autoLevelEnabled_ = enabled; }
  void setAttack(float attack) { attack_ = attack; }
  // Legacy name: WebUI "decay" maps to release blend (0.05..0.5 typical).
  void setDecay(float release) { release_ = release; }
  void setRelease(float release) { release_ = release; }
  void setPeakDecay(float decay) { peakDecay_ = decay; }
  void setAgcTarget(float t);
  float agcTarget() const { return agcTarget_; }

  bool autoLevelEnabled() const { return autoLevelEnabled_; }
  float attack() const { return attack_; }
  float decay() const { return release_; }
  float release() const { return release_; }
  float peakDecay() const { return peakDecay_; }

  // input/output length == bandCount, values 0..1
  void process(const float *input, float *output);
  const float *peaks() const { return peaks_; }
  float autoGain() const { return autoGain_; }

 private:
  size_t bandCount_ = 0;
  bool autoLevelEnabled_ = true;
  float attack_ = 0.50f;   // rise fast
  float release_ = 0.10f;  // fall slow
  float peakDecay_ = 0.92f;
  float agcTarget_ = 0.72f;
  float autoGain_ = 1.0f;
  float levelTrack_ = 0.12f;
  float smoothed_[kMaxBands] = {};
  float peaks_[kMaxBands] = {};
};
