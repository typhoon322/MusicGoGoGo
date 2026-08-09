#include "dsp/beat_tracker.h"

#include <Arduino.h>
#include <math.h>
#include <string.h>

namespace {
// kick 3..9 = 40–200 Hz on kThird30EdgesHz
constexpr size_t kKickLo = 3;
constexpr size_t kKickHi = 9;    // inclusive
// snare 20..22 = 2–4 kHz on kThird30EdgesHz
constexpr size_t kSnareLo = 20;
constexpr size_t kSnareHi = 22;
constexpr uint32_t kKickRefractoryMs = 120;
constexpr uint32_t kSnareRefractoryMs = 80;
constexpr float kFluxMul = 1.8f;
constexpr float kEnvAttack = 0.35f;
constexpr float kEnvRelease = 0.08f;
constexpr float kPulseDecay = 0.72f;
constexpr float kBpmSmooth = 0.25f;
constexpr float kConfDecay = 0.97f;
constexpr uint32_t kIoiMinMs = 375;   // 160 BPM
constexpr uint32_t kIoiMaxMs = 857;   // 70 BPM
}  // namespace

void BeatTracker::reset() {
  state_ = BeatState{};
  prevKick_ = prevSnare_ = 0.0f;
  slowKick_ = slowSnare_ = 0.0f;
  lastKickMs_ = lastSnareMs_ = lastBpmOnsetMs_ = 0;
  ioiCount_ = ioiHead_ = 0;
  memset(ioiMs_, 0, sizeof(ioiMs_));
}

float BeatTracker::sumBands_(const float *levels, size_t count, size_t i0, size_t i1) const {
  float s = 0.0f;
  if (levels == nullptr || count == 0) {
    return 0.0f;
  }
  const size_t hi = (i1 < count) ? i1 : (count - 1);
  for (size_t i = i0; i <= hi; ++i) {
    const float v = levels[i];
    if (v > 0.0f) {
      s += v;
    }
  }
  return s;
}

void BeatTracker::detectOnset_(float energy, float &prevEnergy, float &slowEnv, uint32_t &lastOnsetMs,
                               uint32_t nowMs, uint32_t refractoryMs, float &pulseOut, bool isKick) {
  const float flux = energy > prevEnergy ? (energy - prevEnergy) : 0.0f;
  prevEnergy = energy;

  if (flux > slowEnv) {
    slowEnv += (flux - slowEnv) * kEnvAttack;
  } else {
    slowEnv += (flux - slowEnv) * kEnvRelease;
  }
  const float thr = slowEnv * kFluxMul + 1e-5f;
  const bool ready = (lastOnsetMs == 0) || (nowMs - lastOnsetMs >= refractoryMs);
  if (flux > thr && ready && energy > 0.02f) {
    lastOnsetMs = nowMs;
    pulseOut = 1.0f;
    if (isKick) {
      if (lastBpmOnsetMs_ != 0) {
        const uint32_t dt = nowMs - lastBpmOnsetMs_;
        if (dt >= kIoiMinMs && dt <= kIoiMaxMs) {
          ioiMs_[ioiHead_] = dt;
          ioiHead_ = (ioiHead_ + 1) % kIoiCap;
          if (ioiCount_ < kIoiCap) {
            ++ioiCount_;
          }
          updateBpm_(nowMs);
        } else {
          // Invalid IOI: one-step decay; silence path handles further fade.
          state_.confidence *= kConfDecay;
        }
      }
      lastBpmOnsetMs_ = nowMs;
    }
  }
}

void BeatTracker::updateBpm_(uint32_t /*nowMs*/) {
  if (ioiCount_ < 3) {
    state_.confidence *= kConfDecay;
    return;
  }
  uint32_t tmp[kIoiCap];
  for (size_t i = 0; i < ioiCount_; ++i) {
    const size_t idx = (ioiHead_ + kIoiCap - ioiCount_ + i) % kIoiCap;
    tmp[i] = ioiMs_[idx];
  }
  // insertion sort
  for (size_t i = 1; i < ioiCount_; ++i) {
    const uint32_t key = tmp[i];
    size_t j = i;
    while (j > 0 && tmp[j - 1] > key) {
      tmp[j] = tmp[j - 1];
      --j;
    }
    tmp[j] = key;
  }
  const uint32_t median = tmp[ioiCount_ / 2];
  float bpm = 60000.0f / static_cast<float>(median);
  if (bpm < 70.0f) {
    bpm = 70.0f;
  }
  if (bpm > 160.0f) {
    bpm = 160.0f;
  }

  float mean = 0.0f;
  for (size_t i = 0; i < ioiCount_; ++i) {
    mean += static_cast<float>(tmp[i]);
  }
  mean /= static_cast<float>(ioiCount_);
  float var = 0.0f;
  for (size_t i = 0; i < ioiCount_; ++i) {
    const float d = static_cast<float>(tmp[i]) - mean;
    var += d * d;
  }
  var /= static_cast<float>(ioiCount_);
  const float cv = (mean > 1.0f) ? (sqrtf(var) / mean) : 1.0f;
  float conf = 1.0f - cv * 2.0f;
  if (conf < 0.0f) {
    conf = 0.0f;
  }
  if (conf > 1.0f) {
    conf = 1.0f;
  }

  if (state_.bpm <= 1.0f) {
    state_.bpm = bpm;
  } else {
    state_.bpm += (bpm - state_.bpm) * kBpmSmooth;
  }
  state_.confidence = conf;
}

void BeatTracker::process(const float *levels, size_t count, uint32_t nowMs) {
  state_.kickPulse *= kPulseDecay;
  state_.snarePulse *= kPulseDecay;
  if (state_.kickPulse < 0.02f) {
    state_.kickPulse = 0.0f;
  }
  if (state_.snarePulse < 0.02f) {
    state_.snarePulse = 0.0f;
  }

  const float kick = sumBands_(levels, count, kKickLo, kKickHi);
  const float snare = sumBands_(levels, count, kSnareLo, kSnareHi);

  detectOnset_(kick, prevKick_, slowKick_, lastKickMs_, nowMs, kKickRefractoryMs, state_.kickPulse,
               true);
  detectOnset_(snare, prevSnare_, slowSnare_, lastSnareMs_, nowMs, kSnareRefractoryMs,
               state_.snarePulse, false);

  // 长时间无 kick → 置信度衰减
  if (lastKickMs_ != 0 && (nowMs - lastKickMs_) > 2000) {
    state_.confidence *= kConfDecay;
  }
}
