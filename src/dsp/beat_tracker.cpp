#include "dsp/beat_tracker.h"

#include <Arduino.h>
#include <math.h>
#include <string.h>

namespace {
// True kick fundamental: 40–100 Hz only (exclude 100–200 where snare body lives).
// Edges: 40–50,50–63,63–80,80–100 → bands 3..6
constexpr size_t kKickLo = 3;
constexpr size_t kKickHi = 6;
// Snare crack + upper body
constexpr size_t kSnareLo = 20;
constexpr size_t kSnareHi = 23;  // 2–5 kHz
// Brush / hat air — used to veto false kicks
constexpr size_t kAirLo = 24;
constexpr size_t kAirHi = 27;  // ~5–12.5 kHz

constexpr uint32_t kKickRefractoryMs = 300;
constexpr uint32_t kSnareRefractoryMs = 90;
constexpr float kFluxMul = 1.55f;
constexpr float kFluxFloor = 0.014f;
constexpr float kKickEnergyMin = 0.055f;
constexpr float kSnareEnergyMin = 0.030f;
constexpr float kEnvAttack = 0.22f;
constexpr float kEnvRelease = 0.06f;
constexpr float kPulseDecay = 0.88f;
constexpr float kBpmSmooth = 0.08f;
constexpr float kConfSmooth = 0.18f;
constexpr float kConfDecay = 0.992f;
constexpr uint32_t kIoiMinMs = 375;
constexpr uint32_t kIoiMaxMs = 857;
constexpr float kIoiOutlier = 0.18f;

// Kick must dominate crack/air or we treat the transient as snare/brush bleed.
constexpr float kKickVsSnare = 1.25f;
constexpr float kKickVsAir = 1.05f;
}  // namespace

void BeatTracker::reset() {
  state_ = BeatState{};
  prevKick_ = prevSnare_ = 0.0f;
  slowKickFlux_ = slowSnareFlux_ = 0.0f;
  lastKickMs_ = lastSnareMs_ = lastBpmOnsetMs_ = lastGoodConfMs_ = 0;
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

uint32_t BeatTracker::medianIoi_() const {
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

bool BeatTracker::acceptIoi_(uint32_t dt) const {
  if (dt < kIoiMinMs || dt > kIoiMaxMs) {
    return false;
  }
  if (ioiCount_ < 3) {
    return true;
  }
  const uint32_t med = medianIoi_();
  if (med == 0) {
    return true;
  }
  const float rel = fabsf(static_cast<float>(dt) - static_cast<float>(med)) / static_cast<float>(med);
  return rel <= kIoiOutlier;
}

bool BeatTracker::detectOnset_(float energy, float &prevEnergy, float &slowFlux, uint32_t &lastOnsetMs,
                               uint32_t nowMs, uint32_t refractoryMs, float energyMin) {
  const float flux = energy > prevEnergy ? (energy - prevEnergy) : 0.0f;
  const float thr = slowFlux * kFluxMul + kFluxFloor;
  const bool ready = (lastOnsetMs == 0) || (nowMs - lastOnsetMs >= refractoryMs);
  const bool onset = flux > thr && energy >= energyMin && ready;

  if (flux > slowFlux) {
    slowFlux += (flux - slowFlux) * kEnvAttack;
  } else {
    slowFlux += (flux - slowFlux) * kEnvRelease;
  }
  prevEnergy = energy;

  if (onset) {
    lastOnsetMs = nowMs;
    return true;
  }
  return false;
}

void BeatTracker::onKickOnset_(uint32_t nowMs) {
  if (lastBpmOnsetMs_ != 0) {
    uint32_t dt = nowMs - lastBpmOnsetMs_;

    if (state_.bpm >= 70.0f && state_.confidence >= 0.35f) {
      const float expected = 60000.0f / state_.bpm;
      const float d1 = fabsf(static_cast<float>(dt) - expected);
      const float dHalf = fabsf(static_cast<float>(dt) - expected * 0.5f);
      const float dDouble = fabsf(static_cast<float>(dt) - expected * 2.0f);
      if (dDouble < d1 && dDouble < dHalf) {
        const uint32_t half = dt / 2;
        if (half >= kIoiMinMs && half <= kIoiMaxMs) {
          dt = half;
        }
      } else if (dHalf < d1 && dt * 2u <= kIoiMaxMs) {
        dt = dt * 2u;
      }
    }

    if (acceptIoi_(dt)) {
      ioiMs_[ioiHead_] = dt;
      ioiHead_ = (ioiHead_ + 1) % kIoiCap;
      if (ioiCount_ < kIoiCap) {
        ++ioiCount_;
      }
      updateBpmFromIois_();
    }
  }
  lastBpmOnsetMs_ = nowMs;
  lastKickMs_ = nowMs;
}

void BeatTracker::updateBpmFromIois_() {
  if (ioiCount_ < 4) {
    return;
  }
  const uint32_t median = medianIoi_();
  if (median == 0) {
    return;
  }
  float bpm = 60000.0f / static_cast<float>(median);
  if (bpm < 70.0f) {
    bpm = 70.0f;
  }
  if (bpm > 160.0f) {
    bpm = 160.0f;
  }

  uint32_t tmp[kIoiCap];
  for (size_t i = 0; i < ioiCount_; ++i) {
    const size_t idx = (ioiHead_ + kIoiCap - ioiCount_ + i) % kIoiCap;
    tmp[i] = ioiMs_[idx];
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
  float confTarget = 1.0f - cv * 1.35f;
  if (confTarget < 0.0f) {
    confTarget = 0.0f;
  }
  if (confTarget > 1.0f) {
    confTarget = 1.0f;
  }

  if (state_.bpm < 1.0f) {
    state_.bpm = bpm;
  } else {
    float a = kBpmSmooth;
    if (state_.confidence > 0.55f) {
      a *= 0.55f;
    }
    state_.bpm += (bpm - state_.bpm) * a;
  }
  state_.confidence += (confTarget - state_.confidence) * kConfSmooth;
  if (state_.confidence > 0.4f) {
    lastGoodConfMs_ = millis();
  }
}

void BeatTracker::process(const float *levels, size_t count, uint32_t nowMs) {
  state_.kickPulse *= kPulseDecay;
  state_.snarePulse *= kPulseDecay;
  if (state_.kickPulse < 0.015f) {
    state_.kickPulse = 0.0f;
  }
  if (state_.snarePulse < 0.015f) {
    state_.snarePulse = 0.0f;
  }

  const float kick = sumBands_(levels, count, kKickLo, kKickHi);
  const float snare = sumBands_(levels, count, kSnareLo, kSnareHi);
  const float air = sumBands_(levels, count, kAirLo, kAirHi);

  const bool snareOn =
      detectOnset_(snare, prevSnare_, slowSnareFlux_, lastSnareMs_, nowMs, kSnareRefractoryMs,
                   kSnareEnergyMin);
  if (snareOn) {
    state_.snarePulse = 1.0f;
    lastSnareMs_ = nowMs;
  }

  const bool kickCand =
      detectOnset_(kick, prevKick_, slowKickFlux_, lastKickMs_, nowMs, kKickRefractoryMs,
                   kKickEnergyMin);
  if (kickCand) {
    // Spectral veto: brushes/snares light crack+air without a real sub kick.
    const bool bassDominant = (kick >= snare * kKickVsSnare) && (kick >= air * kKickVsAir);
    // If snare/air just fired, require even stronger bass dominance.
    const bool recentSnare = (lastSnareMs_ != 0) && (nowMs - lastSnareMs_ < 60);
    const bool ok = bassDominant && (!recentSnare || kick >= snare * 1.6f);
    if (ok) {
      state_.kickPulse = 1.0f;
      onKickOnset_(nowMs);
    }
    // else: low-band bleed from brush — do not flash / do not feed BPM
  }

  if (lastKickMs_ != 0 && (nowMs - lastKickMs_) > 1800) {
    state_.confidence *= kConfDecay;
  }
  if (lastGoodConfMs_ != 0 && (nowMs - lastGoodConfMs_) > 3500 && state_.confidence < 0.2f) {
    if (state_.confidence < 0.08f) {
      state_.bpm = 0.0f;
      ioiCount_ = 0;
      ioiHead_ = 0;
      lastBpmOnsetMs_ = 0;
    }
  }
}
