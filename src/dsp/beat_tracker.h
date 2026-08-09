#pragma once

#include <stddef.h>
#include <stdint.h>

struct BeatState {
  float bpm = 0.0f;
  float confidence = 0.0f;
  float beatPulse = 0.0f;
  float bassLevel = 0.0f;
  float highLevel = 0.0f;
  float dbgLow = 0.0f;
  float dbgLowFlux = 0.0f;
  float dbgSpecFlux = 0.0f;
  float dbgThr = 0.0f;
  float dbgScore = 0.0f;
  float dbgAvg = 0.0f;
  bool dbgFired = false;
  uint32_t dbgBeatCount = 0;
};

class BeatTracker {
 public:
  void reset();
  void process(const float *levels, size_t count, uint32_t nowMs);
  const BeatState &state() const { return state_; }

 private:
  float sumBands_(const float *levels, size_t count, size_t i0, size_t i1) const;
  void onBeat_(uint32_t nowMs, float flux);
  uint32_t medianInterval_() const;
  uint32_t cooldownMs_() const;

  BeatState state_;
  float env_ = 0.0f;
  float prevEnv_ = 0.0f;
  float avgEnergy_ = 0.0f;
  float avgFlux_ = 0.0f;
  uint32_t startMs_ = 0;
  uint32_t lastBeatMs_ = 0;
  bool armed_ = true;

  static constexpr size_t kIoiCap = 20;
  uint32_t ioiMs_[kIoiCap] = {};
  size_t ioiCount_ = 0;
  size_t ioiHead_ = 0;
};
