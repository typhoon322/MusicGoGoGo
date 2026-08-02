#include <Arduino.h>
#include <math.h>
#include <string.h>

#include "audio/i2s_mic.h"
#include "config.h"
#include "display/display_driver.h"
#include "dsp/band_processor.h"
#include "dsp/spectrum_analyzer.h"
#include "input/gain_pot.h"
#include "input/rotary_encoder.h"
#include "vfx.h"

I2sMic i2sMic;
SpectrumAnalyzer spectrum;
BandProcessor bandLinear;
BandProcessor bandLog;
BandProcessor bandMirror;
DisplayDriver display;
RotaryEncoder encoder;
GainPot gainPot;

bool autoCycleEnabled = VFX_AUTO_CYCLE_MS > 0;

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

void printStatus() {
  const uint32_t elapsed = millis();
  const float fps =
      elapsed > 0 ? frameCount * 1000.0f / static_cast<float>(elapsed) : 0.0f;
  Serial.printf("[status] mode=%s fps=%.1f vu=%.2f gain=%.1f auto=%.1f\n",
                vfxModeName(display.mode()), fps, spectrum.frame().vuLevel, i2sMic.gain(),
                bandLinear.autoGain());
}

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
    i2sMic.setGain(gainPot.gain());
    gainPot.clearChanged();
  }
}

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
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.printf("[boot] %s vfx modes=%u\n", BOARD_NAME,
                static_cast<unsigned>(VfxMode::Count));

  if (!display.begin(255)) {
    Serial.println(F("[boot] display init failed"));
    while (true) {
      delay(1000);
    }
  }
  display.showSplash();

  if (!i2sMic.begin()) {
    display.showError("I2S mic init failed");
    while (true) {
      delay(1000);
    }
  }

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

  encoder.begin();
  gainPot.begin();
  i2sMic.setGain(gainPot.gain());

  delay(400);
  lastModeCycleMs = millis();
  Serial.println(F("[boot] ready — enc:rotate=mode press=auto | pot=gain"));
}

void loop() {
  handleSerial();
  handleInput();
  const uint32_t now = millis();

#if VFX_AUTO_CYCLE_MS > 0
  if (autoCycleEnabled && now - lastModeCycleMs >= VFX_AUTO_CYCLE_MS) {
    display.nextMode();
    lastModeCycleMs = now;
  }
#endif

  if (!i2sMic.readSamples(sampleBuffer, FFT_SIZE)) {
    Serial.println(F("[i2s] read failed"));
    delay(50);
    return;
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

  display.render(frame, levels, peaks, count, i2sMic.lastRms(), i2sMic.lastPeak());

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
