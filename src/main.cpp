#include <Arduino.h>
#include <math.h>
#include <string.h>

#include "config.h"
#include "display/display_driver.h"
#include "dsp/band_processor.h"
#include "dsp/spectrum_analyzer.h"
#include "settings/app_settings.h"
#include "vfx.h"

#if defined(BOARD_CARDPUTER_ADV)
#include <M5Cardputer.h>
#include "audio/cardputer_mic.h"
using AudioMic = CardputerMic;
#else
#include "audio/i2s_mic.h"
#include "input/gain_pot.h"
#include "input/rotary_encoder.h"
#include <WiFi.h>
#include "net/webui.h"
using AudioMic = I2sMic;
#endif

AudioMic audioMic;
SpectrumAnalyzer spectrum;
BandProcessor bandLinear;
BandProcessor bandLog;
BandProcessor bandMirror;
DisplayDriver display;
AppSettings settings;
#if !defined(BOARD_CARDPUTER_ADV)
RotaryEncoder encoder;
GainPot gainPot;
WebUi webUi;
#endif

bool autoCycleEnabled = false;

// Runtime-tunable timings (defaults from board headers)
uint32_t g_frameMs = SPECTRUM_FRAME_MS;
uint32_t g_cycleMs = VFX_AUTO_CYCLE_MS;
float g_instFps = 0.0f;
#if !defined(BOARD_CARDPUTER_ADV)
static volatile bool g_requestNoiseCal = false;
#endif


static void cfgDirty() {
  settings.markDirty();
}

static void cfgCapture() {
  AppSettingsData &d = settings.data();
  d.mode = static_cast<uint8_t>(display.mode());
  d.gain = audioMic.gain();
  d.brightness = display.backlightLevel();
  d.autoCycle = autoCycleEnabled;
  d.cycleMs = g_cycleMs;
  d.frameMs = g_frameMs;
  d.decay = bandLinear.decay();
  d.attack = bandLinear.attack();
  d.peakDecay = bandLinear.peakDecay();
  d.autoLevel = bandLinear.autoLevelEnabled();
  d.freqLabels = display.showFreqLabels();
  d.bassGain = spectrum.bassGain();
  d.midGain = spectrum.midGain();
  d.trebleGain = spectrum.trebleGain();
  d.noiseMargin = spectrum.noiseMargin();
  d.dbRange = spectrum.dbRange();
  d.agcTarget = bandLinear.agcTarget();
#if !defined(BOARD_CARDPUTER_ADV)
  d.potEnabled = gainPot.enabled();
#endif
}

static void cfgTouch() {
  cfgCapture();
  cfgDirty();
}

static void applyLoadedSettings() {
  const AppSettingsData &d = settings.data();
  if (d.mode < static_cast<uint8_t>(VfxMode::Count)) {
    display.setMode(static_cast<VfxMode>(d.mode));
  }
  g_frameMs = d.frameMs;
  g_cycleMs = d.cycleMs;
  autoCycleEnabled = d.autoCycle;
  bandLinear.setDecay(d.decay);
  bandLog.setDecay(d.decay);
  bandMirror.setDecay(d.decay);
  bandLinear.setAttack(d.attack);
  bandLog.setAttack(d.attack);
  bandMirror.setAttack(d.attack);
  bandLinear.setPeakDecay(d.peakDecay);
  bandLog.setPeakDecay(d.peakDecay);
  bandMirror.setPeakDecay(d.peakDecay);
  bandLinear.setAutoLevelEnabled(d.autoLevel);
  bandLog.setAutoLevelEnabled(d.autoLevel);
  bandMirror.setAutoLevelEnabled(d.autoLevel);
  display.setShowFreqLabels(d.freqLabels);
  spectrum.setEqGains(d.bassGain, d.midGain, d.trebleGain);
  spectrum.setNoiseMargin(d.noiseMargin);
  spectrum.setDbRange(d.dbRange);
  bandLinear.setAgcTarget(d.agcTarget);
  bandLog.setAgcTarget(d.agcTarget);
  bandMirror.setAgcTarget(d.agcTarget);
  display.setBacklight(d.brightness);
#if !defined(BOARD_CARDPUTER_ADV)
  gainPot.setEnabled(d.potEnabled);
  if (d.potEnabled) {
    audioMic.setGain(gainPot.gain());
  } else {
    audioMic.setGain(d.gain);
  }
#else
  audioMic.setGain(d.gain);
#endif
}

int16_t sampleBuffer[FFT_SIZE];
float smoothBars[SPECTRUM_BARS];
float peakBars[SPECTRUM_BARS];
float smoothLog12[VFX_LOG_BANDS];
float peakLog12[VFX_LOG_BANDS];
float smoothMirror[SPECTRUM_BARS];
float peakMirror[SPECTRUM_BARS];

uint32_t lastFrameMs = 0;
uint32_t lastStatusMs = 0;
uint32_t lastModeCycleMs = 0;
uint32_t frameCount = 0;
uint32_t profReadUs = 0;
uint32_t profFftUs = 0;
uint32_t profBandUs = 0;
uint32_t profRenderUs = 0;
uint32_t profFrames = 0;
#if defined(BOARD_CARDPUTER_ADV)
int8_t cardputerBatteryPct = -1;
uint32_t lastBatteryReadMs = 0;
#endif

#if !defined(BOARD_CARDPUTER_ADV)
// ---- Web UI callbacks -----------------------------------------------------
static uint8_t wbGetMode() {
  return static_cast<uint8_t>(display.mode());
}
static void wbSetMode(uint8_t m) {
  if (m < static_cast<uint8_t>(VfxMode::Count)) {
    display.setMode(static_cast<VfxMode>(m));
    lastModeCycleMs = millis();
    cfgTouch();
  }
}
static const char *wbGetModeName(uint8_t m) {
  return vfxModeName(static_cast<VfxMode>(m));
}
static uint8_t wbGetModeCount() {
  return static_cast<uint8_t>(VfxMode::Count);
}
static float wbGetGain() {
  return audioMic.gain();
}
static void wbSetGain(float g) {
  audioMic.setGain(g);
  cfgTouch();
}
static uint8_t wbGetBrightness() {
  return display.backlightLevel();
}
static void wbSetBrightness(uint8_t v) {
  display.setBacklight(v);
  cfgTouch();
}
static bool wbGetAutoCycle() {
  return autoCycleEnabled;
}
static void wbSetAutoCycle(bool b) {
  autoCycleEnabled = b;
  if (b) {
    lastModeCycleMs = millis();
  }
  Serial.printf("[web] auto-cycle %s\n", autoCycleEnabled ? "ON" : "OFF");
  cfgTouch();
}
static uint32_t wbGetCycleMs() {
  return g_cycleMs;
}
static void wbSetCycleMs(uint32_t v) {
  g_cycleMs = v;
  cfgTouch();
}
static uint32_t wbGetFrameMs() {
  return g_frameMs;
}
static void wbSetFrameMs(uint32_t v) {
  if (v >= 10 && v <= 500) {
    g_frameMs = v;
    cfgTouch();
  }
}
static float wbGetDecay() {
  return bandLinear.decay();
}
static void wbSetDecay(float v) {
  bandLinear.setDecay(v);
  bandLog.setDecay(v);
  bandMirror.setDecay(v);
  cfgTouch();
}
static float wbGetAttack() {
  return bandLinear.attack();
}
static void wbSetAttack(float v) {
  bandLinear.setAttack(v);
  bandLog.setAttack(v);
  bandMirror.setAttack(v);
  cfgTouch();
}
static float wbGetPeakDecay() {
  return bandLinear.peakDecay();
}
static void wbSetPeakDecay(float v) {
  bandLinear.setPeakDecay(v);
  bandLog.setPeakDecay(v);
  bandMirror.setPeakDecay(v);
  cfgTouch();
}
static bool wbGetAutoLevel() {
  return bandLinear.autoLevelEnabled();
}
static void wbSetAutoLevel(bool b) {
  bandLinear.setAutoLevelEnabled(b);
  bandLog.setAutoLevelEnabled(b);
  bandMirror.setAutoLevelEnabled(b);
  cfgTouch();
}
static bool wbGetFreqLabels() {
  return display.showFreqLabels();
}
static void wbSetFreqLabels(bool b) {
  display.setShowFreqLabels(b);
  cfgTouch();
}
static float wbGetBassGain() {
  return spectrum.bassGain();
}
static void wbSetBassGain(float g) {
  spectrum.setBassGain(g);
  cfgTouch();
}
static float wbGetMidGain() {
  return spectrum.midGain();
}
static void wbSetMidGain(float g) {
  spectrum.setMidGain(g);
  cfgTouch();
}
static float wbGetTrebleGain() {
  return spectrum.trebleGain();
}
static void wbSetTrebleGain(float g) {
  spectrum.setTrebleGain(g);
  cfgTouch();
}
static float wbGetNoiseMargin() {
  return spectrum.noiseMargin();
}
static void wbSetNoiseMargin(float v) {
  spectrum.setNoiseMargin(v);
  cfgTouch();
}
static float wbGetDbRange() {
  return spectrum.dbRange();
}
static void wbSetDbRange(float v) {
  spectrum.setDbRange(v);
  cfgTouch();
}
static float wbGetAgcTarget() {
  return bandLinear.agcTarget();
}
static void wbSetAgcTarget(float v) {
  bandLinear.setAgcTarget(v);
  bandLog.setAgcTarget(v);
  bandMirror.setAgcTarget(v);
  cfgTouch();
}
static void wbRequestNoiseCal() {
#if !defined(BOARD_CARDPUTER_ADV)
  g_requestNoiseCal = true;
  Serial.println(F("[web] noise recalibration requested"));
#endif
}
static void wbSaveSettings() {
  cfgCapture();
  settings.saveNow();
}
#if !defined(BOARD_CARDPUTER_ADV)
static void runNoiseCalibration() {
  Serial.println(F("[fft] noise cal start — keep quiet"));
  spectrum.beginNoiseCalibration();
  const uint32_t calMs = 2500;
  const uint32_t t0 = millis();
  while (millis() - t0 < calMs) {
    if (audioMic.readSamples(sampleBuffer, FFT_HOP)) {
      spectrum.analyze(sampleBuffer, FFT_HOP);
    }
    delay(1);
  }
  spectrum.finishNoiseCalibration();
}
#endif
static float wbGetFps() {
  return g_instFps;
}
static float wbGetVu() {
  return spectrum.frame().vuLevel;
}
static float wbGetRms() {
  return audioMic.lastRms();
}
static float wbGetPeak() {
  return audioMic.lastPeak();
}
static float wbGetAutoGain() {
  return bandLinear.autoGain();
}
static uint32_t wbGetUptimeMs() {
  return millis();
}

static void setupWebUi() {
  WebCallbacks cb;
  cb.getMode = wbGetMode;
  cb.setMode = wbSetMode;
  cb.getModeName = wbGetModeName;
  cb.getModeCount = wbGetModeCount;
  cb.getGain = wbGetGain;
  cb.setGain = wbSetGain;
  cb.getBrightness = wbGetBrightness;
  cb.setBrightness = wbSetBrightness;
  cb.getAutoCycle = wbGetAutoCycle;
  cb.setAutoCycle = wbSetAutoCycle;
  cb.getCycleMs = wbGetCycleMs;
  cb.setCycleMs = wbSetCycleMs;
  cb.getFrameMs = wbGetFrameMs;
  cb.setFrameMs = wbSetFrameMs;
  cb.getDecay = wbGetDecay;
  cb.setDecay = wbSetDecay;
  cb.getAttack = wbGetAttack;
  cb.setAttack = wbSetAttack;
  cb.getPeakDecay = wbGetPeakDecay;
  cb.setPeakDecay = wbSetPeakDecay;
  cb.getAutoLevel = wbGetAutoLevel;
  cb.setAutoLevel = wbSetAutoLevel;
  cb.getFreqLabels = wbGetFreqLabels;
  cb.setFreqLabels = wbSetFreqLabels;
  cb.getBassGain = wbGetBassGain;
  cb.setBassGain = wbSetBassGain;
  cb.getMidGain = wbGetMidGain;
  cb.setMidGain = wbSetMidGain;
  cb.getTrebleGain = wbGetTrebleGain;
  cb.setTrebleGain = wbSetTrebleGain;
  cb.getNoiseMargin = wbGetNoiseMargin;
  cb.setNoiseMargin = wbSetNoiseMargin;
  cb.getDbRange = wbGetDbRange;
  cb.setDbRange = wbSetDbRange;
  cb.getAgcTarget = wbGetAgcTarget;
  cb.setAgcTarget = wbSetAgcTarget;
  cb.requestNoiseCal = wbRequestNoiseCal;
  cb.saveSettings = wbSaveSettings;
  cb.getFps = wbGetFps;
  cb.getVu = wbGetVu;
  cb.getRms = wbGetRms;
  cb.getPeak = wbGetPeak;
  cb.getAutoGain = wbGetAutoGain;
  cb.getUptimeMs = wbGetUptimeMs;
  webUi.attach(cb);
  webUi.begin(WEB_AP_SSID, WEB_AP_PASS);
}
#endif

void printStatus() {
  Serial.printf("[status] mode=%s fps=%.1f vu=%.2f gain=%.1f auto=%.1f\n",
                vfxModeName(display.mode()), g_instFps, spectrum.frame().vuLevel, audioMic.gain(),
                bandLinear.autoGain());
  const uint32_t frames = frameCount > 0 ? frameCount : 1;
  Serial.printf("[prof] read=%lu fft=%lu band=%lu render=%lu us\n", profReadUs / frames,
                profFftUs / frames, profBandUs / frames, profRenderUs / frames);
  if (profFrames > 0) {
    Serial.printf("[prof] read=%lu fft=%lu band=%lu render=%lu us/frame\n",
                  profReadUs / profFrames, profFftUs / profFrames, profBandUs / profFrames,
                  profRenderUs / profFrames);
    profReadUs = 0;
    profFftUs = 0;
    profBandUs = 0;
    profRenderUs = 0;
    profFrames = 0;
  }

#if defined(BOARD_CARDPUTER_ADV)
  const SpectrumFrame &frame = spectrum.frame();
  Serial.printf("[bands] mic_rms=%.4f peak=%.4f raw=[%d..%d] mean=%ld\n",
                audioMic.lastRms(), audioMic.lastPeak(), audioMic.lastRawMin(),
                audioMic.lastRawMax(), static_cast<long>(audioMic.lastRawMean()));
  Serial.print(F("[bands] raw:"));
  for (size_t i = 0; i < SPECTRUM_BARS; ++i) {
    Serial.printf(" %.2f", frame.linear32[i]);
  }
  Serial.println();
  Serial.print(F("[bands] smooth:"));
  for (size_t i = 0; i < SPECTRUM_BARS; ++i) {
    Serial.printf(" %.2f", smoothBars[i]);
  }
  Serial.println();
#else
  webUi.printStatus();
#endif
}

#if defined(BOARD_CARDPUTER_ADV)
void handleCardputerInput() {
  static uint32_t btnAPressMs = 0;
  static bool btnAHandled = false;

  M5Cardputer.update();

  if (M5Cardputer.BtnA.isPressed()) {
    if (btnAPressMs == 0) {
      btnAPressMs = millis();
    }
    if (!btnAHandled && btnAPressMs > 0 && millis() - btnAPressMs >= 600) {
      display.toggleDebugOverlay();
      btnAHandled = true;
      Serial.println(F("[key] BtnA long -> debug"));
    }
  } else {
    if (btnAPressMs > 0 && !btnAHandled) {
      display.nextMode();
      lastModeCycleMs = millis();
      cfgTouch();
      Serial.println(F("[key] BtnA -> next mode"));
    }
    btnAPressMs = 0;
    btnAHandled = false;
  }

  if (!M5Cardputer.Keyboard.isChange()) {
    return;
  }

  if (M5Cardputer.Keyboard.isKeyPressed('d') || M5Cardputer.Keyboard.isKeyPressed('D')) {
    display.toggleDebugOverlay();
    Serial.println(F("[key] d -> debug"));
  }
  if (M5Cardputer.Keyboard.isKeyPressed(',') || M5Cardputer.Keyboard.isKeyPressed(';')) {
    display.prevMode();
    lastModeCycleMs = millis();
    cfgTouch();
    Serial.println(F("[key] ,/; -> prev mode"));
  }
  if (M5Cardputer.Keyboard.isKeyPressed('.') || M5Cardputer.Keyboard.isKeyPressed(':')) {
    display.nextMode();
    lastModeCycleMs = millis();
    cfgTouch();
    Serial.println(F("[key] ./: -> next mode"));
  }
  if (M5Cardputer.Keyboard.isKeyPressed('/')) {
    autoCycleEnabled = !autoCycleEnabled;
    cfgTouch();
    Serial.printf("[vfx] auto-cycle %s\n", autoCycleEnabled ? "ON" : "OFF");
  }
  if (M5Cardputer.Keyboard.isKeyPressed('[')) {
    audioMic.setGain(audioMic.gain() - 0.25f);
    cfgTouch();
    Serial.printf("[mic] gain=%.1f\n", audioMic.gain());
  }
  if (M5Cardputer.Keyboard.isKeyPressed(']')) {
    audioMic.setGain(audioMic.gain() + 0.25f);
    cfgTouch();
    Serial.printf("[mic] gain=%.1f\n", audioMic.gain());
  }
}
#else
void handleInput() {
  encoder.poll();
  gainPot.poll();

  const int8_t step = encoder.consumeStep();
  if (step > 0) {
    display.nextMode();
    lastModeCycleMs = millis();
    cfgTouch();
  } else if (step < 0) {
    display.prevMode();
    lastModeCycleMs = millis();
    cfgTouch();
  }

  if (encoder.consumePress()) {
    autoCycleEnabled = !autoCycleEnabled;
    cfgTouch();
    Serial.printf("[vfx] auto-cycle %s\n", autoCycleEnabled ? "ON" : "OFF");
  }

  if (gainPot.changed()) {
    audioMic.setGain(gainPot.gain());
    gainPot.clearChanged();
    cfgTouch();
  }
}
#endif

static void printSerialHelp() {
  Serial.println(F("=== MusicGoGoGo serial commands ==="));
  Serial.println(F("n / next          next effect"));
  Serial.println(F("p / prev          previous effect"));
  Serial.println(F("m / mode [n]      show effect, or jump: 0=Bars30 1=Log12 2=Mirror 3=VU 4=Rainbow 5=LinePeaks 6=Bounce 7=Dot 8=Glow 9=Ring"));
  Serial.println(F("g / gain [val]    show mic gain, or set: g 3.0 / g+ / g-"));
  Serial.println(F("+ / -             gain step (+/-0.25)"));
#if !defined(BOARD_CARDPUTER_ADV)
  Serial.println(F("pot [on|off]      gain pot control (set gain via serial disables it)"));
  Serial.println(F("fl / labels [on|off]  TFT frequency labels"));
  Serial.println(F("bl / bright [0-255]   backlight brightness"));
#endif
  Serial.println(F("a / auto [on|off] toggle auto-cycle"));
  Serial.println(F("al [on|off]       auto level (AGC) on/off"));
  Serial.println(F("eq [b m t]        show/set EQ gains 0..2 (bass mid treble)"));
  Serial.println(F("s / status        print status"));
  Serial.println(F("d / debug         toggle debug overlay (Cardputer)"));
  Serial.println(F("cfg / cfg save    show or force-save settings"));
  Serial.println(F("cfg reset         factory defaults + save"));
  Serial.println(F("h / help / ?      this help"));
  Serial.println(F("=================================="));
}

static void parseSerialCommand(char *line) {
  char *cmd = strtok(line, " \t");
  if (cmd == nullptr) {
    return;
  }
  char *arg = strtok(nullptr, " \t");

  if (strcmp(cmd, "help") == 0 || cmd[0] == 'h' || cmd[0] == '?') {
    printSerialHelp();
    return;
  }
  if (strcmp(cmd, "next") == 0 || cmd[0] == 'n') {
    display.nextMode();
    lastModeCycleMs = millis();
    cfgTouch();
    Serial.printf("[cmd] mode -> %s\n", vfxModeName(display.mode()));
    return;
  }
  if (strcmp(cmd, "prev") == 0 || cmd[0] == 'p') {
    display.prevMode();
    lastModeCycleMs = millis();
    cfgTouch();
    Serial.printf("[cmd] mode -> %s\n", vfxModeName(display.mode()));
    return;
  }
  if (strcmp(cmd, "mode") == 0 || strcmp(cmd, "m") == 0) {
    if (arg == nullptr) {
      Serial.printf("[cmd] mode = %s (%u)\n", vfxModeName(display.mode()),
                    static_cast<unsigned>(display.mode()));
      return;
    }
    if (strcmp(arg, "next") == 0) {
      display.nextMode();
    } else if (strcmp(arg, "prev") == 0) {
      display.prevMode();
    } else {
      const int m = atoi(arg);
      if (m >= 0 && m < static_cast<int>(VfxMode::Count)) {
        display.setMode(static_cast<VfxMode>(m));
      } else {
        Serial.printf("[cmd] bad mode '%s' (0..%u)\n", arg,
                      static_cast<unsigned>(VfxMode::Count) - 1);
        return;
      }
    }
    lastModeCycleMs = millis();
    cfgTouch();
    Serial.printf("[cmd] mode -> %s\n", vfxModeName(display.mode()));
    return;
  }
  if (strcmp(cmd, "gain") == 0 || strcmp(cmd, "g") == 0) {
    if (arg == nullptr) {
      Serial.printf("[cmd] gain = %.2f\n", audioMic.gain());
    } else {
      if (strcmp(arg, "+") == 0) {
        audioMic.setGain(audioMic.gain() + 0.25f);
      } else if (strcmp(arg, "-") == 0) {
        audioMic.setGain(audioMic.gain() - 0.25f);
      } else {
        audioMic.setGain(atof(arg));
      }
#if !defined(BOARD_CARDPUTER_ADV)
      gainPot.setEnabled(false);
      Serial.println(F("[cmd] gain pot control disabled (enable with 'pot on')"));
#endif
      cfgTouch();
    }
    Serial.printf("[cmd] gain = %.2f\n", audioMic.gain());
    return;
  }
  if (cmd[0] == '+' || cmd[0] == '=') {
    audioMic.setGain(audioMic.gain() + 0.25f);
#if !defined(BOARD_CARDPUTER_ADV)
    gainPot.setEnabled(false);
#endif
    cfgTouch();
    Serial.printf("[cmd] gain = %.2f\n", audioMic.gain());
    return;
  }
  if (cmd[0] == '-') {
    audioMic.setGain(audioMic.gain() - 0.25f);
#if !defined(BOARD_CARDPUTER_ADV)
    gainPot.setEnabled(false);
#endif
    cfgTouch();
    Serial.printf("[cmd] gain = %.2f\n", audioMic.gain());
    return;
  }
#if !defined(BOARD_CARDPUTER_ADV)
  if (strcmp(cmd, "pot") == 0) {
    if (arg != nullptr && strcmp(arg, "on") == 0) {
      gainPot.setEnabled(true);
      Serial.println(F("[cmd] gain pot control enabled"));
      cfgTouch();
    } else if (arg != nullptr && strcmp(arg, "off") == 0) {
      gainPot.setEnabled(false);
      Serial.println(F("[cmd] gain pot control disabled"));
      cfgTouch();
    } else {
      Serial.printf("[cmd] gain pot control %s\n",
                    gainPot.enabled() ? "enabled" : "disabled");
    }
    return;
  }
  if (strcmp(cmd, "labels") == 0 || strcmp(cmd, "fl") == 0) {
    if (arg != nullptr && strcmp(arg, "on") == 0) {
      display.setShowFreqLabels(true);
      cfgTouch();
    } else if (arg != nullptr && strcmp(arg, "off") == 0) {
      display.setShowFreqLabels(false);
      cfgTouch();
    }
    Serial.printf("[cmd] freq labels %s\n", display.showFreqLabels() ? "ON" : "OFF");
    return;
  }
  if (strcmp(cmd, "bright") == 0 || strcmp(cmd, "bl") == 0) {
    if (arg != nullptr) {
      display.setBacklight(static_cast<uint8_t>(atoi(arg)));
      cfgTouch();
    }
    Serial.printf("[cmd] backlight = %u\n", display.backlightLevel());
    return;
  }
#endif
  if (strcmp(cmd, "alevel") == 0 || strcmp(cmd, "al") == 0) {
    if (arg != nullptr && strcmp(arg, "on") == 0) {
      bandLinear.setAutoLevelEnabled(true);
      bandLog.setAutoLevelEnabled(true);
      bandMirror.setAutoLevelEnabled(true);
      Serial.println(F("[cmd] auto level ON"));
      cfgTouch();
    } else if (arg != nullptr && strcmp(arg, "off") == 0) {
      bandLinear.setAutoLevelEnabled(false);
      bandLog.setAutoLevelEnabled(false);
      bandMirror.setAutoLevelEnabled(false);
      Serial.println(F("[cmd] auto level OFF"));
      cfgTouch();
    } else {
      Serial.printf("[cmd] auto level %s\n",
                    bandLinear.autoLevelEnabled() ? "ON" : "OFF");
    }
    return;
  }
  if (strcmp(cmd, "auto") == 0 || cmd[0] == 'a') {
    if (arg != nullptr && strcmp(arg, "on") == 0) {
      autoCycleEnabled = true;
    } else if (arg != nullptr && strcmp(arg, "off") == 0) {
      autoCycleEnabled = false;
    } else {
      autoCycleEnabled = !autoCycleEnabled;
    }
    if (autoCycleEnabled) {
      lastModeCycleMs = millis();
    }
    cfgTouch();
    Serial.printf("[cmd] auto-cycle %s\n", autoCycleEnabled ? "ON" : "OFF");
    return;
  }
  if (strcmp(cmd, "eq") == 0) {
    if (arg == nullptr) {
      Serial.printf("[cmd] eq bass=%.2f mid=%.2f treble=%.2f\n", spectrum.bassGain(),
                    spectrum.midGain(), spectrum.trebleGain());
      return;
    }
    const float bass = atof(arg);
    char *arg2 = strtok(nullptr, " \t");
    char *arg3 = strtok(nullptr, " \t");
    const float mid = arg2 != nullptr ? static_cast<float>(atof(arg2)) : spectrum.midGain();
    const float treble =
        arg3 != nullptr ? static_cast<float>(atof(arg3)) : spectrum.trebleGain();
    spectrum.setEqGains(bass, mid, treble);
    cfgTouch();
    Serial.printf("[cmd] eq bass=%.2f mid=%.2f treble=%.2f\n", spectrum.bassGain(),
                  spectrum.midGain(), spectrum.trebleGain());
    return;
  }
  if (strcmp(cmd, "cfg") == 0) {
    if (arg != nullptr && strcmp(arg, "save") == 0) {
      cfgCapture();
      settings.saveNow();
    } else if (arg != nullptr && strcmp(arg, "reset") == 0) {
      settings.resetToDefaults();
      applyLoadedSettings();
    } else {
      cfgCapture();
      const AppSettingsData &d = settings.data();
      Serial.printf(
          "[cfg] mode=%u gain=%.2f bright=%u auto=%u cyc=%lu frm=%lu "
          "atk=%.2f dec=%.2f peak=%.3f al=%u labels=%u pot=%u "
          "eq=%.2f/%.2f/%.2f nm=%.2f db=%.0f agc=%.2f\n",
          d.mode, d.gain, d.brightness, d.autoCycle ? 1 : 0,
          static_cast<unsigned long>(d.cycleMs), static_cast<unsigned long>(d.frameMs),
          d.attack, d.decay, d.peakDecay, d.autoLevel ? 1 : 0, d.freqLabels ? 1 : 0,
#if !defined(BOARD_CARDPUTER_ADV)
          d.potEnabled ? 1 : 0,
#else
          0,
#endif
          d.bassGain, d.midGain, d.trebleGain, d.noiseMargin, d.dbRange, d.agcTarget);
    }
    return;
  }
  if (strcmp(cmd, "status") == 0 || cmd[0] == 's') {
    printStatus();
    return;
  }
  if (strcmp(cmd, "debug") == 0 || cmd[0] == 'd') {
#if defined(BOARD_CARDPUTER_ADV)
    display.toggleDebugOverlay();
#else
    Serial.println(F("[cmd] debug overlay not available on S3"));
#endif
    return;
  }
  Serial.printf("[cmd] unknown '%s' — try 'help'\n", cmd);
}

void handleSerial() {
  static char line[48];
  static size_t len = 0;
  while (Serial.available() > 0) {
    const char c = static_cast<char>(Serial.read());
    if (c == '\n' || c == '\r') {
      if (len > 0) {
        line[len] = '\0';
        parseSerialCommand(line);
        len = 0;
      }
    } else if (c >= 32 && c < 127 && len < sizeof(line) - 1) {
      line[len++] = c;
    }
  }
}

void setup() {
#if defined(BOARD_CARDPUTER_ADV)
  if (!audioMic.beginPlatform()) {
    while (true) {
      delay(1000);
    }
  }
#endif
  Serial.begin(115200);
#if defined(ARDUINO_USB_CDC_ON_BOOT)
  delay(500);
#endif
  Serial.println();
  Serial.printf("[boot] %s vfx modes=%u\n", BOARD_NAME,
                static_cast<unsigned>(VfxMode::Count));
#if defined(BOARD_S3_DEV)
  Serial.printf("[mem] flash=%u KB  psram=%u KB  free_psram=%u KB  free_heap=%u KB\n",
                static_cast<unsigned>(ESP.getFlashChipSize() / 1024),
                static_cast<unsigned>(ESP.getPsramSize() / 1024),
                static_cast<unsigned>(ESP.getFreePsram() / 1024),
                static_cast<unsigned>(ESP.getFreeHeap() / 1024));
#endif
  settings.begin();

#if defined(BOARD_CARDPUTER_ADV)
  if (!display.begin(settings.data().brightness)) {
    Serial.println(F("[boot] TFT init failed"));
    while (true) {
      delay(1000);
    }
  }
  if (!audioMic.beginCapture()) {
    Serial.println(F("[boot] mic init failed"));
    while (true) {
      delay(1000);
    }
  }
#else
  if (!display.begin(settings.data().brightness)) {
    Serial.println(F("[boot] display init failed"));
    while (true) {
      delay(1000);
    }
  }
  if (!audioMic.begin()) {
    display.showError("I2S mic init failed");
    while (true) {
      delay(1000);
    }
  }
#endif
#if !defined(BOARD_CARDPUTER_ADV)
  display.showSplash();
#endif

  if (!spectrum.begin()) {
    display.showError("FFT init failed");
    while (true) {
      delay(1000);
    }
  }

  bandLinear.begin(SPECTRUM_BARS);
  bandLog.begin(VFX_LOG_BANDS);
  bandMirror.begin(SPECTRUM_BARS);
  bandLinear.setAutoLevelEnabled(true);
  bandLog.setAutoLevelEnabled(true);
  bandMirror.setAutoLevelEnabled(true);
#if defined(BOARD_CARDPUTER_ADV)
  bandLinear.setAutoLevelEnabled(false);
  bandLog.setAutoLevelEnabled(false);
  bandMirror.setAutoLevelEnabled(false);
  bandLinear.setRelease(0.12f);
  bandLog.setRelease(0.12f);
  bandMirror.setRelease(0.12f);
  audioMic.setGain(CARDPUTER_MIC_GAIN);
  cardputerBatteryPct = static_cast<int8_t>(M5Cardputer.Power.getBatteryLevel());
  lastBatteryReadMs = millis();
#else
  audioMic.setGain(AUDIO_GAIN_DEFAULT);
#endif

#if !defined(BOARD_CARDPUTER_ADV)
  encoder.begin();
  gainPot.begin();
#endif

  if (settings.loaded()) {
    applyLoadedSettings();
  } else {
#if !defined(BOARD_CARDPUTER_ADV)
    audioMic.setGain(gainPot.gain());
#endif
    cfgCapture();
  }

#if !defined(BOARD_CARDPUTER_ADV)
  // Quiet-room noise-floor calibration (INMP441 hiss → gate)
  display.showSplash();
  runNoiseCalibration();
#endif
  delay(400);
  lastModeCycleMs = millis();
  memset(sampleBuffer, 0, sizeof(sampleBuffer));
  spectrum.analyze(sampleBuffer, FFT_SIZE);
  const SpectrumFrame &bootFrame = spectrum.frame();
  bandLinear.process(bootFrame.linear32, smoothBars);
  memcpy(peakBars, bandLinear.peaks(), sizeof(peakBars));
  display.render(bootFrame, smoothBars, peakBars, SPECTRUM_BARS, 0.0f, 0.0f
#if defined(BOARD_CARDPUTER_ADV)
                 , MicDebugInfo{}
#endif
  );
#if defined(BOARD_CARDPUTER_ADV)
  bandLinear.reset();
  bandLog.reset();
  bandMirror.reset();
  memset(smoothBars, 0, sizeof(smoothBars));
  memset(smoothLog12, 0, sizeof(smoothLog12));
  memset(smoothMirror, 0, sizeof(smoothMirror));
  memset(peakBars, 0, sizeof(peakBars));
  memset(peakLog12, 0, sizeof(peakLog12));
  memset(peakMirror, 0, sizeof(peakMirror));
  display.render(bootFrame, smoothBars, peakBars, SPECTRUM_BARS, 0.0f, 0.0f, MicDebugInfo{});
  Serial.println(F("[boot] ready — BtnA=mode | BtnA hold=debug | d=debug | ,/.=mode [ ]=gain | serial: 'help'"));
  Serial.flush();
#else
  Serial.println(F("[boot] ready — enc:rotate=mode press=auto | pot=gain | webui @ "));
  setupWebUi();
  Serial.println(WiFi.softAPIP().toString().c_str());
  Serial.println(F("[boot] serial: 'help' for commands"));
  Serial.flush();
#endif
}

void loop() {
  handleSerial();
  settings.poll();
#if defined(BOARD_CARDPUTER_ADV)
  handleCardputerInput();
#else
  handleInput();
#endif
#if !defined(BOARD_CARDPUTER_ADV)
  if (g_requestNoiseCal) {
    g_requestNoiseCal = false;
    runNoiseCalibration();
  }
#endif
  const uint32_t frameStartMs = millis();

  if (autoCycleEnabled && g_cycleMs > 0 && frameStartMs - lastModeCycleMs >= g_cycleMs) {
    display.nextMode();
    lastModeCycleMs = frameStartMs;
    cfgTouch();
  }

  uint32_t t0 = micros();
  if (!audioMic.readSamples(sampleBuffer, FFT_HOP)) {
    memset(sampleBuffer, 0, sizeof(sampleBuffer));
  }
  uint32_t t1 = micros();

  spectrum.analyze(sampleBuffer, FFT_HOP);
  const SpectrumFrame &frame = spectrum.frame();
  uint32_t t2 = micros();

  bandLinear.process(frame.linear32, smoothBars);
  bandLog.process(frame.log12, smoothLog12);
  bandMirror.process(frame.mirror32, smoothMirror);
  memcpy(peakBars, bandLinear.peaks(), sizeof(peakBars));
  memcpy(peakLog12, bandLog.peaks(), sizeof(peakLog12));
  memcpy(peakMirror, bandMirror.peaks(), sizeof(peakMirror));
  uint32_t t3 = micros();

  const float *levels = smoothBars;
  const float *peaks = peakBars;
  size_t count = SPECTRUM_BARS;

  switch (display.mode()) {
    case VfxMode::Log12:
      levels = smoothLog12;
      peaks = peakLog12;
      count = VFX_LOG_BANDS;
      break;
    case VfxMode::Mirror:
      levels = smoothMirror;
      peaks = peakMirror;
      count = SPECTRUM_BARS;
      break;
    case VfxMode::LinePeaks:
      levels = smoothBars;
      peaks = peakBars;
      count = SPECTRUM_BARS;
      break;
    default:
      break;
  }

#if defined(BOARD_CARDPUTER_ADV)
  if (frameStartMs - lastBatteryReadMs >= 1000) {
    cardputerBatteryPct =
        static_cast<int8_t>(M5Cardputer.Power.getBatteryLevel());
    lastBatteryReadMs = frameStartMs;
  }

  MicDebugInfo micDbg;
  micDbg.batteryPercent = cardputerBatteryPct;
  micDbg.rawMin = audioMic.lastRawMin();
  micDbg.rawMax = audioMic.lastRawMax();
  micDbg.rawMean = audioMic.lastRawMean();
  micDbg.gain = audioMic.gain();
  micDbg.bands[0] = frame.linear32[0];
  micDbg.bands[1] = frame.linear32[1];
  micDbg.bands[2] = frame.linear32[2];
  micDbg.bands[3] = frame.linear32[3];
  display.render(frame, levels, peaks, count, audioMic.lastRms(), audioMic.lastPeak(), micDbg);
#else
  display.render(frame, levels, peaks, count, audioMic.lastRms(), audioMic.lastPeak());
#endif

  const uint32_t t4 = micros();
  profReadUs += t1 - t0;
  profFftUs += t2 - t1;
  profBandUs += t3 - t2;
  profRenderUs += t4 - t3;
  ++profFrames;

  ++frameCount;

  // Pace from THIS frame's start so I2S wait isn't followed by another full delay.
  const uint32_t spentMs = millis() - frameStartMs;
  if (spentMs < g_frameMs) {
    delay(g_frameMs - spentMs);
  }
  const uint32_t framePeriod = millis() - frameStartMs;
  if (framePeriod > 0) {
    const float inst = 1000.0f / static_cast<float>(framePeriod);
    g_instFps = g_instFps <= 0.1f ? inst : (g_instFps * 0.8f + inst * 0.2f);
  }

  if (millis() - lastStatusMs >= STATUS_LOG_MS) {
    printStatus();
    lastStatusMs = millis();
  }
}
