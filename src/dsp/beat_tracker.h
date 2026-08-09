#pragma once

#include <stddef.h>
#include <stdint.h>

struct BeatState {
  float bpm = 0.0f;
  float confidence = 0.0f;
  // Continuous band levels 0..1 for UI/LED (not onset pulses).
  float kickPulse = 0.0f;   // low / bass energy
  float snarePulse = 0.0f;  // high energy
};

class BeatTracker {
 public:
  void reset();
  void process(const float *levels, size_t count, uint32_t nowMs);
  const BeatState &state() const { return state_; }

 private:
  float sumBands_(const float *levels, size_t count, size_t i0, size_t i1) const;
  static float clamp01_(float v);
  void maybeUpdateBpm_(float bass, float prevBass, uint32_t nowMs);

  BeatState state_;
  float bassEma_ = 0.0f;
  float highEma_ = 0.0f;
  float prevBass_ = 0.0f;
  uint32_t lastBpmOnsetMs_ = 0;
  uint32_t lastGoodConfMs_ = 0;

  static constexpr size_t kIoiCap = 12;
  uint32_t ioiMs_[kIoiCap] = {};
  size_t ioiCount_ = 0;
  size_t ioiHead_ = 0;
};
