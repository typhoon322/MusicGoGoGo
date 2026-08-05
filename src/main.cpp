#include <Arduino.h>
#include <math.h>
#include <string.h>

#include "config.h"
#include "display/display_driver.h"
#include "dsp/band_processor.h"
#include "dsp/spectrum_analyzer.h"
#include "vfx.h"

#if defined(BOARD_CARDPUTER_ADV)
#include <M5Cardputer.h>
#include "audio/cardputer_mic.h"
using AudioMic = CardputerMic;
#else
#include "audio/i2s_mic.h"
#include "input/gain_pot.h"
#include "input/rotary_encoder.h"
using AudioMic = I2sMic;
#endif

AudioMic audioMic;
SpectrumAnalyzer spectrum;
BandProcessor bandLinear;
BandProcessor bandLog;
BandProcessor bandMirror;
DisplayDriver display;
#if !defined(BOARD_CARDPUTER_ADV)
RotaryEncoder encoder;
GainPot gainPot;
#endif

bool autoCycleEnabled =
#if defined(BOARD_CARDPUTER_ADV)
    false;
#else
    VFX_AUTO_CYCLE_MS > 0;
#endif

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
#if defined(BOARD_CARDPUTER_ADV)
int8_t cardputerBatteryPct = -1;
uint32_t lastBatteryReadMs = 0;
#endif

void printStatus() {
  const uint32_t elapsed = millis();
  const float fps =
      elapsed > 0 ? frameCount * 1000.0f / static_cast<float>(elapsed) : 0.0f;
  Serial.printf("[status] mode=%s fps=%.1f vu=%.2f gain=%.1f auto=%.1f\n",
                vfxModeName(display.mode()), fps, spectrum.frame().vuLevel, audioMic.gain(),
                bandLinear.autoGain());

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
    Serial.println(F("[key] ,/; -> prev mode"));
  }
  if (M5Cardputer.Keyboard.isKeyPressed('.') || M5Cardputer.Keyboard.isKeyPressed(':')) {
    display.nextMode();
    lastModeCycleMs = millis();
    Serial.println(F("[key] ./: -> next mode"));
  }
  if (M5Cardputer.Keyboard.isKeyPressed('/')) {
    autoCycleEnabled = !autoCycleEnabled;
    Serial.printf("[vfx] auto-cycle %s\n", autoCycleEnabled ? "ON" : "OFF");
  }
  if (M5Cardputer.Keyboard.isKeyPressed('[')) {
    audioMic.setGain(audioMic.gain() - 0.25f);
    Serial.printf("[mic] gain=%.1f\n", audioMic.gain());
  }
  if (M5Cardputer.Keyboard.isKeyPressed(']')) {
    audioMic.setGain(audioMic.gain() + 0.25f);
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
  } else if (step < 0) {
    display.prevMode();
    lastModeCycleMs = millis();
  }

  if (encoder.consumePress()) {
    autoCycleEnabled = !autoCycleEnabled;
    Serial.printf("[vfx] auto-cycle %s\n", autoCycleEnabled ? "ON" : "OFF");
  }

  if (gainPot.changed()) {
    audioMic.setGain(gainPot.gain());
    gainPot.clearChanged();
  }
}
#endif

void handleSerial() {
  while (Serial.available() > 0) {
    const char c = static_cast<char>(Serial.read());
    if (c == 'n' || c == 'N') {
      display.nextMode();
      lastModeCycleMs = millis();
    } else if (c == 'p' || c == 'P') {
      display.prevMode();
      lastModeCycleMs = millis();
    } else if (c == 'a' || c == 'A') {
      autoCycleEnabled = !autoCycleEnabled;
      Serial.printf("[vfx] auto-cycle %s\n", autoCycleEnabled ? "ON" : "OFF");
    } else if (c == '+' || c == '=') {
      audioMic.setGain(audioMic.gain() + 0.25f);
    } else if (c == '-') {
      audioMic.setGain(audioMic.gain() - 0.25f);
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

#if defined(BOARD_CARDPUTER_ADV)
  if (!display.begin(255)) {
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
  if (!display.begin(255)) {
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
  bandLinear.setDecay(SPECTRUM_DECAY_CARDPUTER);
  bandLog.setDecay(SPECTRUM_DECAY_CARDPUTER);
  bandMirror.setDecay(SPECTRUM_DECAY_CARDPUTER);
  audioMic.setGain(CARDPUTER_MIC_GAIN);
  cardputerBatteryPct = static_cast<int8_t>(M5Cardputer.Power.getBatteryLevel());
  lastBatteryReadMs = millis();
#else
  audioMic.setGain(AUDIO_GAIN_DEFAULT);
#endif

#if !defined(BOARD_CARDPUTER_ADV)
  encoder.begin();
  gainPot.begin();
  audioMic.setGain(gainPot.gain());
#endif

  delay(400);
  lastModeCycleMs = millis();
  memset(sampleBuffer, 0, sizeof(sampleBuffer));
  spectrum.analyze(sampleBuffer, FFT_SIZE);
  const SpectrumFrame &bootFrame = spectrum.frame();
  bandLinear.process(bootFrame.linear32, smoothBars);
  memcpy(peakBars, bandLinear.peaks(), sizeof(peakBars));
  display.render(bootFrame, smoothBars, peakBars, SPECTRUM_BARS, 0.0f, 0.0f,
#if defined(BOARD_CARDPUTER_ADV)
                 MicDebugInfo{}
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
  Serial.println(F("[boot] ready — BtnA=mode | BtnA hold=debug | d=debug | ,/.=mode [ ]=gain"));
  Serial.flush();
#else
  Serial.println(F("[boot] ready — enc:rotate=mode press=auto | pot=gain"));
#endif
}

void loop() {
  handleSerial();
#if defined(BOARD_CARDPUTER_ADV)
  handleCardputerInput();
#else
  handleInput();
#endif
  const uint32_t now = millis();

#if VFX_AUTO_CYCLE_MS > 0
  if (autoCycleEnabled && now - lastModeCycleMs >= VFX_AUTO_CYCLE_MS) {
    display.nextMode();
    lastModeCycleMs = now;
  }
#endif

  if (!audioMic.readSamples(sampleBuffer, FFT_SIZE)) {
    memset(sampleBuffer, 0, sizeof(sampleBuffer));
  }

  spectrum.analyze(sampleBuffer, FFT_SIZE);
  const SpectrumFrame &frame = spectrum.frame();

  bandLinear.process(frame.linear32, smoothBars);
  bandLog.process(frame.log12, smoothLog12);
  bandMirror.process(frame.mirror32, smoothMirror);
  memcpy(peakBars, bandLinear.peaks(), sizeof(peakBars));
  memcpy(peakLog12, bandLog.peaks(), sizeof(peakLog12));
  memcpy(peakMirror, bandMirror.peaks(), sizeof(peakMirror));

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
    case VfxMode::Waterfall:
      levels = frame.waterfallRow;
      peaks = frame.waterfallRow;
      count = VFX_WATERFALL_BINS;
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
  if (now - lastBatteryReadMs >= 1000) {
    cardputerBatteryPct =
        static_cast<int8_t>(M5Cardputer.Power.getBatteryLevel());
    lastBatteryReadMs = now;
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

  ++frameCount;

  if (now - lastStatusMs >= STATUS_LOG_MS) {
    printStatus();
    lastStatusMs = now;
  }

  const uint32_t elapsed = now - lastFrameMs;
  if (elapsed < SPECTRUM_FRAME_MS) {
    delay(SPECTRUM_FRAME_MS - elapsed);
  }
  lastFrameMs = millis();
}
