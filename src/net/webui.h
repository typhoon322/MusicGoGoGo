#pragma once

#include <stdint.h>

// Callback table the firmware fills in so the web UI can read/change live params.
struct WebCallbacks {
  uint8_t (*getMode)() = nullptr;
  void (*setMode)(uint8_t) = nullptr;
  const char *(*getModeName)(uint8_t) = nullptr;
  uint8_t (*getModeCount)() = nullptr;

  float (*getGain)() = nullptr;
  void (*setGain)(float) = nullptr;
  uint8_t (*getBrightness)() = nullptr;
  void (*setBrightness)(uint8_t) = nullptr;

  bool (*getAutoCycle)() = nullptr;
  void (*setAutoCycle)(bool) = nullptr;
  uint32_t (*getCycleMs)() = nullptr;
  void (*setCycleMs)(uint32_t) = nullptr;
  uint32_t (*getFrameMs)() = nullptr;
  void (*setFrameMs)(uint32_t) = nullptr;

  float (*getDecay)() = nullptr;
  void (*setDecay)(float) = nullptr;
  float (*getAttack)() = nullptr;
  void (*setAttack)(float) = nullptr;
  float (*getPeakDecay)() = nullptr;
  void (*setPeakDecay)(float) = nullptr;
  bool (*getAutoLevel)() = nullptr;
  void (*setAutoLevel)(bool) = nullptr;

  bool (*getFreqLabels)() = nullptr;
  void (*setFreqLabels)(bool) = nullptr;

  float (*getBassGain)() = nullptr;
  void (*setBassGain)(float) = nullptr;
  float (*getMidGain)() = nullptr;
  void (*setMidGain)(float) = nullptr;
  float (*getTrebleGain)() = nullptr;
  void (*setTrebleGain)(float) = nullptr;

  float (*getNoiseMargin)() = nullptr;
  void (*setNoiseMargin)(float) = nullptr;
  float (*getDbRange)() = nullptr;
  void (*setDbRange)(float) = nullptr;
  float (*getAgcTarget)() = nullptr;
  void (*setAgcTarget)(float) = nullptr;
  void (*requestNoiseCal)() = nullptr;
  void (*saveSettings)() = nullptr;

  float (*getFps)() = nullptr;
  float (*getVu)() = nullptr;
  float (*getRms)() = nullptr;
  float (*getPeak)() = nullptr;
  float (*getAutoGain)() = nullptr;
  float (*getBpm)() = nullptr;
  float (*getBeatConf)() = nullptr;
  uint32_t (*getUptimeMs)() = nullptr;
};

class WebUi {
 public:
  void attach(const WebCallbacks &cb);
  bool begin(const char *ssid, const char *pass);
  void printStatus();

 private:
  void startServer_();

  WebCallbacks cb_;
  bool wifiOk_ = false;
};
