#include "dsp/beat_tracker.h"

#include <Arduino.h>
#include <math.h>
#include <string.h>

namespace {
// 50–150 Hz → 1/3-octave bands 4..8
constexpr size_t kKickLo = 4;
constexpr size_t kKickHi = 8;

constexpr float kEnvAttack = 0.60f;
constexpr float kEnvRelease = 0.14f;

// Serial (clear drums, no flash): peak gate env>avg*1.55 blocked real kicks
// while flux was already 0.6–1.4; valley never re-armed under sustained bass.
constexpr float kAvgAttack = 0.05f;
constexpr float kFluxAvgAttack = 0.08f;
constexpr float kFluxThrFactor = 2.0f;
constexpr float kFluxAbsMin = 0.35f;
constexpr float kFluxRelMin = 0.18f;  // vs avgEnergy
constexpr float kSoftPeak = 1.08f;    // mild: env above baseline a bit

constexpr uint32_t kCooldownMinMs = 280;
constexpr uint32_t kWarmupMs = 800;
constexpr uint32_t kIoiMinMs = 375;
constexpr uint32_t kIoiMaxMs = 1000;
constexpr float kPulseDecay = 0.88f;  // stay visible ~0.5s
constexpr float kBpmSmooth = 0.22f;
constexpr float kMinAvg = 0.20f;
}  // namespace

void BeatTracker::reset() {
  state_ = BeatState{};
  env_ = 0.0f;
  prevEnv_ = 0.0f;
  avgEnergy_ = 0.0f;
  avgFlux_ = 0.0f;
  startMs_ = 0;
  lastBeatMs_ = 0;
  armed_ = true;
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
    if (levels[i] > 0.0f) {
      s += levels[i];
    }
  }
  return s;
}

uint32_t BeatTracker::medianInterval_() const {
  if (ioiCount_ == 0) {
    return 0;
  }
  uint32_t tmp[kIoiCap];
  for (size_t i = 0; i < ioiCount_; ++i) {
    const size_t idx = (ioiHead_ + kIoiCap - ioiCount_ + i) % kIoiCap;
    tmp[i] = ioiMs_[idx];
  }
  for (size_t i = 1; i < ioiCount_; ++i) {
    const uint32_t key = tmp[i];
    size_t j = i;
    while (j > 0 && tmp[j - 1] > key) {
      tmp[j] = tmp[j - 1];
      --j;
    }
    tmp[j] = key;
  }
  return tmp[ioiCount_ / 2];
}

uint32_t BeatTracker::cooldownMs_() const {
  if (state_.bpm >= 60.0f && state_.confidence >= 0.35f) {
    const float period = 60000.0f / state_.bpm;
    const uint32_t adaptive = static_cast<uint32_t>(period * 0.55f);
    return adaptive > kCooldownMinMs ? adaptive : kCooldownMinMs;
  }
  return kCooldownMinMs;
}

void BeatTracker::onBeat_(uint32_t nowMs, float /*flux*/) {
  state_.beatPulse = 1.0f;  // always flash on accepted onset
  state_.dbgFired = true;
  ++state_.dbgBeatCount;
  armed_ = false;

  if (lastBeatMs_ != 0) {
    const uint32_t dt = nowMs - lastBeatMs_;
    if (dt >= kIoiMinMs && dt <= kIoiMaxMs) {
      ioiMs_[ioiHead_] = dt;
      ioiHead_ = (ioiHead_ + 1) % kIoiCap;
      if (ioiCount_ < kIoiCap) {
        ++ioiCount_;
      }
      if (ioiCount_ >= 4) {
        const uint32_t med = medianInterval_();
        float bpm = 60000.0f / static_cast<float>(med);
        if (bpm < 60.0f) {
          bpm = 60.0f;
        }
        if (bpm > 160.0f) {
          bpm = 160.0f;
        }
        if (state_.bpm < 1.0f) {
          state_.bpm = bpm;
        } else {
          state_.bpm += (bpm - state_.bpm) * kBpmSmooth;
        }
        float mean = 0.0f;
        for (size_t i = 0; i < ioiCount_; ++i) {
          const size_t idx = (ioiHead_ + kIoiCap - ioiCount_ + i) % kIoiCap;
          mean += static_cast<float>(ioiMs_[idx]);
        }
        mean /= static_cast<float>(ioiCount_);
        float var = 0.0f;
        for (size_t i = 0; i < ioiCount_; ++i) {
          const size_t idx = (ioiHead_ + kIoiCap - ioiCount_ + i) % kIoiCap;
          const float d = static_cast<float>(ioiMs_[idx]) - mean;
          var += d * d;
        }
        var /= static_cast<float>(ioiCount_);
        const float cv = mean > 1.0f ? (sqrtf(var) / mean) : 1.0f;
        float conf = 1.0f - cv * 1.5f;
        if (conf < 0.0f) {
          conf = 0.0f;
        }
        if (conf > 1.0f) {
          conf = 1.0f;
        }
        state_.confidence += (conf - state_.confidence) * 0.30f;
      }
    }
  }
  lastBeatMs_ = nowMs;
}

void BeatTracker::process(const float *levels, size_t count, uint32_t nowMs) {
  state_.dbgFired = false;
  if (startMs_ == 0) {
    startMs_ = nowMs;
  }

  state_.beatPulse *= kPulseDecay;
  if (state_.beatPulse < 0.02f) {
    state_.beatPulse = 0.0f;
  }

  const float low = sumBands_(levels, count, kKickLo, kKickHi);
  if (low > env_) {
    env_ += (low - env_) * kEnvAttack;
  } else {
    env_ += (low - env_) * kEnvRelease;
  }

  state_.bassLevel = env_ / static_cast<float>(kKickHi - kKickLo + 1);
  if (state_.bassLevel > 1.0f) {
    state_.bassLevel = 1.0f;
  }
  state_.highLevel = 0.0f;

  // Baseline prefers non-peak frames.
  if (env_ < avgEnergy_ * 1.25f) {
    avgEnergy_ += (env_ - avgEnergy_) * kAvgAttack;
  } else {
    avgEnergy_ += (env_ - avgEnergy_) * (kAvgAttack * 0.25f);
  }

  const float flux = env_ > prevEnv_ ? (env_ - prevEnv_) : 0.0f;
  if (flux < avgFlux_ * 2.2f || avgFlux_ < 1e-4f) {
    avgFlux_ += (flux - avgFlux_) * kFluxAvgAttack;
  } else {
    avgFlux_ += (flux - avgFlux_) * (kFluxAvgAttack * 0.2f);
  }

  const float thr =
      fmaxf(kFluxAbsMin, fmaxf(avgFlux_ * kFluxThrFactor, avgEnergy_ * kFluxRelMin));

  // Re-arm: valley OR (cooldown done and flux quiet) — sustained bass OK.
  if (!armed_) {
    const bool cooled = (nowMs - lastBeatMs_) >= cooldownMs_();
    if (env_ <= avgEnergy_ * 0.95f || (cooled && flux < 0.08f)) {
      armed_ = true;
    }
  }

  state_.dbgLow = env_;
  state_.dbgLowFlux = flux;
  state_.dbgSpecFlux = low;
  state_.dbgThr = thr;
  state_.dbgScore = thr;
  state_.dbgAvg = avgEnergy_;

  const bool warmed = (nowMs - startMs_) >= kWarmupMs;
  const bool hasSignal = avgEnergy_ >= kMinAvg || env_ >= kMinAvg;
  const bool cooled = (lastBeatMs_ == 0) || (nowMs - lastBeatMs_ >= cooldownMs_());
  const bool softPeak = env_ >= avgEnergy_ * kSoftPeak;
  const bool onset = warmed && hasSignal && armed_ && cooled && softPeak && flux >= thr;

  if (onset) {
    onBeat_(nowMs, flux);
  }
  prevEnv_ = env_;

  if (lastBeatMs_ != 0 && (nowMs - lastBeatMs_) > 4000) {
    state_.confidence *= 0.96f;
    if (state_.confidence < 0.12f) {
      state_.bpm = 0.0f;
      ioiCount_ = 0;
      ioiHead_ = 0;
      lastBeatMs_ = 0;
      armed_ = true;
    }
  }
}
