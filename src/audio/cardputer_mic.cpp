#include "audio/cardputer_mic.h"

#if defined(BOARD_CARDPUTER_ADV)

#include <Arduino.h>
#include <M5Cardputer.h>
#include <math.h>

#include "config.h"

namespace {
int16_t lastRawMin_ = 0;
int16_t lastRawMax_ = 0;
int32_t lastRawMean_ = 0;
}  // namespace

bool CardputerMic::beginPlatform() {
  if (platformReady_) {
    return true;
  }

  auto cfg = M5.config();
  cfg.fallback_board = m5::board_t::board_M5CardputerADV;
  cfg.internal_mic = true;
  cfg.serial_baudrate = 115200;
  M5Cardputer.begin(cfg);
  Serial.printf("[mic] board=%d\n", static_cast<int>(M5.getBoard()));
  platformReady_ = true;
  return true;
}

bool CardputerMic::beginCapture() {
  if (ready_) {
    return true;
  }
  if (!platformReady_) {
    return false;
  }

  auto micCfg = M5Cardputer.Mic.config();
  micCfg.sample_rate = I2S_SAMPLE_RATE;
  micCfg.magnification = 48;
  micCfg.noise_filter_level = 0;
  M5Cardputer.Mic.config(micCfg);

  if (!M5Cardputer.Mic.begin()) {
    Serial.println(F("[mic] M5Cardputer.Mic.begin failed"));
    return false;
  }

  ready_ = true;
  Serial.printf("[mic] OK ES8311 rate=%u hw_mag=%u gain=%.1f\n", I2S_SAMPLE_RATE,
                micCfg.magnification, gain_);
  return true;
}

bool CardputerMic::begin() {
  return beginPlatform() && beginCapture();
}

void CardputerMic::end() {
  if (!ready_) {
    return;
  }
  M5Cardputer.Mic.end();
  ready_ = false;
}

void CardputerMic::setGain(float gain) {
  if (gain < 0.1f) {
    gain = 0.1f;
  }
  if (gain > 32.0f) {
    gain = 32.0f;
  }
  gain_ = gain;
}

bool CardputerMic::readSamples(int16_t *out, size_t count) {
  if (!ready_ || out == nullptr || count == 0) {
    return false;
  }

  M5Cardputer.update();

  if (!M5Cardputer.Mic.record(out, count, I2S_SAMPLE_RATE)) {
    return false;
  }

  // M5 Mic.record() queues async capture — wait before reading the buffer
  uint32_t waitStart = millis();
  while (M5Cardputer.Mic.isRecording()) {
    delay(1);
    if (millis() - waitStart > 500) {
      Serial.println(F("[mic] record timeout"));
      return false;
    }
  }

  int32_t sum = 0;
  int16_t rmin = 32767;
  int16_t rmax = -32768;
  for (size_t i = 0; i < count; ++i) {
    const int16_t raw = out[i];
    if (raw < rmin) {
      rmin = raw;
    }
    if (raw > rmax) {
      rmax = raw;
    }
    sum += raw;
  }
  lastRawMin_ = rmin;
  lastRawMax_ = rmax;
  lastRawMean_ = static_cast<int32_t>(sum / static_cast<int32_t>(count));

  double sumSq = 0.0;
  float peak = 0.0f;

  for (size_t i = 0; i < count; ++i) {
    float sample = static_cast<float>(out[i]) * gain_;
    if (sample > 32767.0f) {
      sample = 32767.0f;
    }
    if (sample < -32768.0f) {
      sample = -32768.0f;
    }
    out[i] = static_cast<int16_t>(sample);

    const float absSample = fabsf(sample);
    if (absSample > peak) {
      peak = absSample;
    }
    sumSq += static_cast<double>(sample) * static_cast<double>(sample);
  }

  lastPeak_ = peak / 32768.0f;
  lastRms_ = static_cast<float>(sqrt(sumSq / static_cast<double>(count))) / 32768.0f;
  return true;
}

int16_t CardputerMic::lastRawMin() const {
  return lastRawMin_;
}

int16_t CardputerMic::lastRawMax() const {
  return lastRawMax_;
}

int32_t CardputerMic::lastRawMean() const {
  return lastRawMean_;
}

#endif  // BOARD_CARDPUTER_ADV
