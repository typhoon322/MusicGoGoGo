#include "input/gain_pot.h"

#if defined(BOARD_S3_DEV)

#include <Arduino.h>
#include <math.h>

#include "config.h"

void GainPot::begin() {
  pinMode(PIN_GAIN_POT, INPUT);
  analogSetPinAttenuation(PIN_GAIN_POT, ADC_11db);
  analogRead(PIN_GAIN_POT);
  delay(10);
  filtered_ = static_cast<float>(analogRead(PIN_GAIN_POT));
  gain_ = GAIN_POT_MIN;
  Serial.printf("[pot] OK pin=%d range=%.1f-%.1f\n", PIN_GAIN_POT, GAIN_POT_MIN,
                GAIN_POT_MAX);
}

void GainPot::poll() {
  if (!enabled_) {
    return;
  }
  const int raw = analogRead(PIN_GAIN_POT);
  filtered_ = filtered_ * 0.82f + static_cast<float>(raw) * 0.18f;

  const float norm = filtered_ / 4095.0f;
  const float newGain = GAIN_POT_MIN + norm * (GAIN_POT_MAX - GAIN_POT_MIN);

  if (lastRaw_ < 0 || abs(raw - lastRaw_) >= 8) {
    if (fabsf(newGain - gain_) >= 0.05f) {
      gain_ = newGain;
      changed_ = true;
    }
    lastRaw_ = raw;
  }
}

#endif  // BOARD_S3_DEV
