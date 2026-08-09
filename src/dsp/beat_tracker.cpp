#include "dsp/beat_tracker.h"

#include <Arduino.h>
#include <math.h>
#include <string.h>

namespace {
// Low meter: ~40–160 Hz (bars that move with bass / kick body)
constexpr size_t kBassLo = 3;
constexpr size_t kBassHi = 8;
// High meter: ~2–12.5 kHz
constexpr size_t kHighLo = 20;
constexpr size_t kHighHi = 27;

constexpr float kLevelAttack = 0.45f;
constexpr float kLevelRelease = 0.18f;
constexpr float kLevelGain = 1.35f;  // headroom so typical music fills the dots

// Lightweight BPM from bass jumps only (does not drive the dots/LED).
constexpr uint32_t kBpmRefractoryMs = 320;
constexpr uint32_t kIoiMinMs = 375;
constexpr uint32_t kIoiMaxMs = 857;
constexpr float kBpmFluxMin = 0.12f;
constexpr float kBpmSmooth = 0.10f;
}  // namespace

void BeatTracker::reset() {
  state_ = BeatState{};
  bassEma_ = highEma_ = prevBass_ = 0.0f;
  lastBpmOnsetMs_ = lastGoodConfMs_ = 0;
  ioiCount_ = ioiHead_ = 0;
  memset(ioiMs_, 0, sizeof(ioiMs_));
}

float BeatTracker::clamp01_(float v) {
  if (v < 0.0f) {
    return 0.0f;
  }
  if (v > 1.0f) {
    return 1.0f;
  }
  return v;
}

float BeatTracker::sumBands_(const float *levels, size_t count, size_t i0, size_t i1) const {
  float s = 0.0f;
  if (levels == nullptr || count == 0) {
    return 0.0f;
  }
  const size_t hi = (i1 < count) ? i1 : (count - 1);
  const size_t n = (hi >= i0) ? (hi - i0 + 1) : 1;
  for (size_t i = i0; i <= hi; ++i) {
    const float v = levels[i];
    if (v > 0.0f) {
      s += v;
    }
  }
  // Average so wider ranges don't dominate unfairly.
  return s / static_cast<float>(n);
}

void BeatTracker::maybeUpdateBpm_(float bass, float prevBass, uint32_t nowMs) {
  const float flux = bass > prevBass ? (bass - prevBass) : 0.0f;
  const bool ready = (lastBpmOnsetMs_ == 0) || (nowMs - lastBpmOnsetMs_ >= kBpmRefractoryMs);
  if (!(flux >= kBpmFluxMin && bass >= 0.15f && ready)) {
    if (lastBpmOnsetMs_ != 0 && (nowMs - lastBpmOnsetMs_) > 2000) {
      state_.confidence *= 0.99f;
    }
    return;
  }

  if (lastBpmOnsetMs_ != 0) {
    const uint32_t dt = nowMs - lastBpmOnsetMs_;
    if (dt >= kIoiMinMs && dt <= kIoiMaxMs) {
      ioiMs_[ioiHead_] = dt;
      ioiHead_ = (ioiHead_ + 1) % kIoiCap;
      if (ioiCount_ < kIoiCap) {
        ++ioiCount_;
      }
      if (ioiCount_ >= 4) {
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
        const float bpm = 60000.0f / static_cast<float>(tmp[ioiCount_ / 2]);
        if (state_.bpm < 1.0f) {
          state_.bpm = bpm;
        } else {
          state_.bpm += (bpm - state_.bpm) * kBpmSmooth;
        }
        if (state_.bpm < 70.0f) {
          state_.bpm = 70.0f;
        }
        if (state_.bpm > 160.0f) {
          state_.bpm = 160.0f;
        }
        state_.confidence += (0.55f - state_.confidence) * 0.2f;
        lastGoodConfMs_ = nowMs;
      }
    }
  }
  lastBpmOnsetMs_ = nowMs;
}

void BeatTracker::process(const float *levels, size_t count, uint32_t nowMs) {
  const float bass = sumBands_(levels, count, kBassLo, kBassHi);
  const float high = sumBands_(levels, count, kHighLo, kHighHi);

  // Attack/release envelope → smooth “dB-ish” brightness, no onset fireworks.
  auto follow = [](float &ema, float x) {
    if (x > ema) {
      ema += (x - ema) * kLevelAttack;
    } else {
      ema += (x - ema) * kLevelRelease;
    }
  };
  follow(bassEma_, bass);
  follow(highEma_, high);

  // Soft curve so quiet rooms stay dim and loud fills the dots.
  state_.kickPulse = clamp01_(powf(bassEma_ * kLevelGain, 0.85f));
  state_.snarePulse = clamp01_(powf(highEma_ * kLevelGain, 0.85f));

  maybeUpdateBpm_(bass, prevBass_, nowMs);
  prevBass_ = bass;

  if (lastGoodConfMs_ != 0 && (nowMs - lastGoodConfMs_) > 4000 && state_.confidence < 0.25f) {
    state_.bpm = 0.0f;
    ioiCount_ = 0;
    ioiHead_ = 0;
    lastBpmOnsetMs_ = 0;
  }
}
