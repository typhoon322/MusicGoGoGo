#include "settings/app_settings.h"

#include <Arduino.h>
#include <Preferences.h>

#include "config.h"

namespace {
float clampEq(float g) {
  if (g < 0.0f) {
    return 0.0f;
  }
  if (g > 2.0f) {
    return 2.0f;
  }
  return g;
}
}  // namespace

void AppSettings::begin() {
  load();
}

void AppSettings::load() {
  Preferences prefs;
  if (!prefs.begin(kNs, true)) {
    Serial.println(F("[cfg] NVS open (ro) failed — using defaults"));
    loaded_ = false;
    return;
  }

  const uint16_t ver = prefs.getUShort("ver", 0);
  if (ver == 0) {
    prefs.end();
    Serial.println(F("[cfg] no saved settings — defaults"));
    loaded_ = false;
    return;
  }

  data_.mode = prefs.getUChar("mode", data_.mode);
  data_.gain = prefs.getFloat("gain", data_.gain);
  data_.brightness = prefs.getUChar("bright", data_.brightness);
  data_.autoCycle = prefs.getBool("autoCyc", data_.autoCycle);
  data_.cycleMs = prefs.getUInt("cycMs", data_.cycleMs);
  data_.frameMs = prefs.getUInt("frmMs", data_.frameMs);
  data_.decay = prefs.getFloat("decay", data_.decay);
  data_.attack = prefs.getFloat("attack", data_.attack);
  data_.peakDecay = prefs.getFloat("peakDc", data_.peakDecay);
  data_.autoLevel = prefs.getBool("autoLvl", data_.autoLevel);
  data_.freqLabels = prefs.getBool("freqLbl", data_.freqLabels);
  data_.potEnabled = prefs.getBool("potEn", data_.potEnabled);
  data_.bassGain = prefs.getFloat("eqBass", data_.bassGain);
  data_.midGain = prefs.getFloat("eqMid", data_.midGain);
  data_.trebleGain = prefs.getFloat("eqTreble", data_.trebleGain);
  prefs.end();

  // v1/v2 -> v3: labels default off; EQ flat (no built-in bass cut)
  if (ver > 0 && ver < kVersion) {
    if (ver < 2) {
      data_.freqLabels = false;
    }
    data_.bassGain = 1.0f;
    data_.midGain = 1.0f;
    data_.trebleGain = 1.0f;
    dirty_ = true;
    dirtyAtMs_ = 0;
  }
  if (data_.frameMs < 10 || data_.frameMs > 500) {
    data_.frameMs = SPECTRUM_FRAME_MS;
  }
  if (data_.gain < 0.05f) {
    data_.gain = 0.05f;
  }
  if (data_.gain > 32.0f) {
    data_.gain = 32.0f;
  }
  data_.bassGain = clampEq(data_.bassGain);
  data_.midGain = clampEq(data_.midGain);
  data_.trebleGain = clampEq(data_.trebleGain);

  loaded_ = true;
  Serial.printf(
      "[cfg] loaded mode=%u gain=%.2f bright=%u auto=%u labels=%u pot=%u "
      "eq=%.2f/%.2f/%.2f\n",
      data_.mode, data_.gain, data_.brightness, data_.autoCycle ? 1 : 0,
      data_.freqLabels ? 1 : 0, data_.potEnabled ? 1 : 0, data_.bassGain, data_.midGain,
      data_.trebleGain);
}

void AppSettings::saveNow() {
  Preferences prefs;
  if (!prefs.begin(kNs, false)) {
    Serial.println(F("[cfg] NVS open (rw) failed"));
    return;
  }
  prefs.putUShort("ver", kVersion);
  prefs.putUChar("mode", data_.mode);
  prefs.putFloat("gain", data_.gain);
  prefs.putUChar("bright", data_.brightness);
  prefs.putBool("autoCyc", data_.autoCycle);
  prefs.putUInt("cycMs", data_.cycleMs);
  prefs.putUInt("frmMs", data_.frameMs);
  prefs.putFloat("decay", data_.decay);
  prefs.putFloat("attack", data_.attack);
  prefs.putFloat("peakDc", data_.peakDecay);
  prefs.putBool("autoLvl", data_.autoLevel);
  prefs.putBool("freqLbl", data_.freqLabels);
  prefs.putBool("potEn", data_.potEnabled);
  prefs.putFloat("eqBass", data_.bassGain);
  prefs.putFloat("eqMid", data_.midGain);
  prefs.putFloat("eqTreble", data_.trebleGain);
  prefs.end();
  dirty_ = false;
  Serial.println(F("[cfg] saved"));
}

void AppSettings::markDirty() {
  dirty_ = true;
  dirtyAtMs_ = millis();
}

void AppSettings::poll() {
  if (!dirty_) {
    return;
  }
  if (millis() - dirtyAtMs_ < kSaveDelayMs) {
    return;
  }
  saveNow();
}

void AppSettings::resetToDefaults() {
  data_ = AppSettingsData{};
  data_.frameMs = SPECTRUM_FRAME_MS;
  data_.cycleMs = VFX_AUTO_CYCLE_MS;
  data_.gain = AUDIO_GAIN_DEFAULT;
  markDirty();
  saveNow();
  Serial.println(F("[cfg] reset to defaults"));
}
