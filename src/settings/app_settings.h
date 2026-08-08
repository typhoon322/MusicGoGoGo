#pragma once

#include <stdint.h>

// Runtime settings persisted in NVS (Preferences). Survives reboot/reflash
// of the app partition; wiped only by full flash erase / NVS erase.

struct AppSettingsData {
  uint8_t mode = 0;
  float gain = 2.0f;
  uint8_t brightness = 255;
  bool autoCycle = false;
  uint32_t cycleMs = 0;
  uint32_t frameMs = 25;
  float decay = 0.76f;
  float attack = 0.62f;
  float peakDecay = 0.90f;
  bool autoLevel = true;
  bool freqLabels = false;
  bool potEnabled = true;
  float bassGain = 1.0f;
  float midGain = 1.0f;
  float trebleGain = 1.0f;
};

class AppSettings {
 public:
  void begin();
  void load();
  void saveNow();
  void markDirty();
  void poll();  // debounce-flush to NVS
  void resetToDefaults();

  AppSettingsData &data() { return data_; }
  const AppSettingsData &data() const { return data_; }
  bool loaded() const { return loaded_; }

 private:
  static constexpr const char *kNs = "mgg";
  static constexpr uint16_t kVersion = 3;
  static constexpr uint32_t kSaveDelayMs = 500;

  AppSettingsData data_;
  bool loaded_ = false;
  bool dirty_ = false;
  uint32_t dirtyAtMs_ = 0;
};
