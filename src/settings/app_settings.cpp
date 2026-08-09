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

float clampNoiseMargin(float m) {
  if (m < 1.00f) {
    return 1.00f;
  }
  if (m > 1.60f) {
    return 1.60f;
  }
  return m;
}

float clampDbRange(float db) {
  if (db < 24.0f) {
    return 24.0f;
  }
  if (db > 72.0f) {
    return 72.0f;
  }
  return db;
}

float clampAgcTarget(float t) {
  if (t < 0.40f) {
    return 0.40f;
  }
  if (t > 0.95f) {
    return 0.95f;
  }
  return t;
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
  data_.noiseMargin = prefs.getFloat("nMargin", data_.noiseMargin);
  data_.dbRange = prefs.getFloat("dbRange", data_.dbRange);
  data_.agcTarget = prefs.getFloat("agcTgt", data_.agcTarget);
  prefs.end();

  if (ver > 0 && ver < kVersion) {
    if (ver < 2) {
      data_.freqLabels = false;
    }
    if (ver < 3) {
      data_.bassGain = 1.0f;
      data_.midGain = 1.0f;
      data_.trebleGain = 1.0f;
    }
    if (ver < 4) {
      data_.frameMs = SPECTRUM_FRAME_MS;
    }
    if (ver < 5) {
      data_.frameMs = SPECTRUM_FRAME_MS;
    }
    if (ver < 6) {
      data_.attack = 0.50f;
      data_.decay = 0.10f;
      data_.peakDecay = 0.92f;
      data_.frameMs = SPECTRUM_FRAME_MS;
    }
    if (ver < 7) {
      data_.noiseMargin = 1.12f;
      data_.dbRange = 42.0f;
      data_.agcTarget = 0.72f;
    }
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
  data_.noiseMargin = clampNoiseMargin(data_.noiseMargin);
  data_.dbRange = clampDbRange(data_.dbRange);
  data_.agcTarget = clampAgcTarget(data_.agcTarget);
  if (data_.decay < 0.02f) {
    data_.decay = 0.02f;
  }
  if (data_.decay > 0.55f) {
    data_.decay = 0.10f;
  }
  if (data_.attack < 0.05f) {
    data_.attack = 0.05f;
  }
  if (data_.attack > 0.95f) {
    data_.attack = 0.95f;
  }

  loaded_ = true;
  Serial.printf(
      "[cfg] loaded mode=%u gain=%.2f nm=%.2f db=%.0f agc=%.2f eq=%.2f/%.2f/%.2f\n",
      data_.mode, data_.gain, data_.noiseMargin, data_.dbRange, data_.agcTarget, data_.bassGain,
      data_.midGain, data_.trebleGain);
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
  prefs.putFloat("nMargin", data_.noiseMargin);
  prefs.putFloat("dbRange", data_.dbRange);
  prefs.putFloat("agcTgt", data_.agcTarget);
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
