#pragma once

#include <stddef.h>
#include <stdint.h>

struct BeatState {
  float bpm = 0.0f;
  float confidence = 0.0f;
  float kickPulse = 0.0f;
  float snarePulse = 0.0f;
};

class BeatTracker {
 public:
  void reset();
  // levels: SPECTRUM_BARS raw linear32; nowMs = millis()
  void process(const float *levels, size_t count, uint32_t nowMs);
  const BeatState &state() const { return state_; }

 private:
  float sumBands_(const float *levels, size_t count, size_t i0, size_t i1) const;
  bool detectOnset_(float energy, float &prevEnergy, float &slowFlux, uint32_t &lastOnsetMs,
                    uint32_t nowMs, uint32_t refractoryMs, float energyMin);
  void onKickOnset_(uint32_t nowMs);
  void updateBpmFromIois_();
  uint32_t medianIoi_() const;
  bool acceptIoi_(uint32_t dt) const;
  void refreshPulses_(uint32_t nowMs);

  BeatState state_;
  float prevKick_ = 0.0f;
  float prevSnare_ = 0.0f;
  float slowKickFlux_ = 0.0f;
  float slowSnareFlux_ = 0.0f;
  uint32_t lastKickMs_ = 0;
  uint32_t lastSnareMs_ = 0;
  uint32_t lastBpmOnsetMs_ = 0;
  uint32_t lastGoodConfMs_ = 0;

  // Debounce / hold for UI + LED
  uint32_t lastAcceptedKickMs_ = 0;
  uint32_t lastAcceptedSnareMs_ = 0;
  uint32_t kickHoldUntil_ = 0;
  uint32_t snareHoldUntil_ = 0;
  // Pending kick must decay quickly (reject sustained guitar/bass notes).
  bool kickPending_ = false;
  float kickPendingPeak_ = 0.0f;
  uint32_t kickPendingMs_ = 0;
  float slowKickEnergy_ = 0.0f;

  static constexpr size_t kIoiCap = 12;
  uint32_t ioiMs_[kIoiCap] = {};
  size_t ioiCount_ = 0;
  size_t ioiHead_ = 0;
};
