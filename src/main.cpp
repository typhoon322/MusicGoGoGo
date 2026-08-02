#include <Arduino.h>

#include "audio/i2s_mic.h"
#include "config.h"
#include "display/display_driver.h"
#include "dsp/spectrum_analyzer.h"

I2sMic i2sMic;
SpectrumAnalyzer spectrum;
DisplayDriver display;

int16_t sampleBuffer[FFT_SIZE];

uint32_t lastFrameMs = 0;
uint32_t lastStatusMs = 0;
uint32_t frameCount = 0;

void printStatus() {
  const uint32_t elapsed = millis();
  const float fps =
      elapsed > 0 ? frameCount * 1000.0f / static_cast<float>(elapsed) : 0.0f;
  Serial.printf("[status] board=%s fps=%.1f rms=%.3f peak=%.3f gain=%.1f\n", BOARD_NAME, fps,
                i2sMic.lastRms(), i2sMic.lastPeak(), i2sMic.gain());
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.printf("[boot] %s fft=%u bars=%u\n", BOARD_NAME, FFT_SIZE, SPECTRUM_BARS);

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
  i2sMic.setGain(AUDIO_GAIN_DEFAULT);

  if (!spectrum.begin()) {
    display.showError("FFT init failed");
    while (true) {
      delay(1000);
    }
  }

  delay(400);
  Serial.println(F("[boot] ready — play music or clap near INMP441"));
}

void loop() {
  const uint32_t now = millis();

  if (!i2sMic.readSamples(sampleBuffer, FFT_SIZE)) {
    Serial.println(F("[i2s] read failed"));
    delay(50);
    return;
  }

  spectrum.analyze(sampleBuffer, FFT_SIZE);
  display.updateSpectrum(spectrum.bars(), spectrum.barCount(), i2sMic.lastRms(),
                         i2sMic.lastPeak());

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
