#include "input/rotary_encoder.h"

#include <Arduino.h>

#include "config.h"

void RotaryEncoder::begin() {
  pinMode(PIN_ENC_CLK, INPUT_PULLUP);
  pinMode(PIN_ENC_DT, INPUT_PULLUP);
  pinMode(PIN_ENC_SW, INPUT_PULLUP);

  lastClk_ = digitalRead(PIN_ENC_CLK);
  swLastRaw_ = digitalRead(PIN_ENC_SW) == LOW;
  swStableDown_ = swLastRaw_;
  swLastChangeMs_ = millis();

  Serial.printf("[enc] OK clk=%d dt=%d sw=%d\n", PIN_ENC_CLK, PIN_ENC_DT, PIN_ENC_SW);
}

void RotaryEncoder::poll() {
  const uint8_t clk = digitalRead(PIN_ENC_CLK);
  if (lastClk_ == HIGH && clk == LOW) {
    if (digitalRead(PIN_ENC_DT) == HIGH) {
      ++pendingSteps_;
    } else {
      --pendingSteps_;
    }
  }
  lastClk_ = clk;

  const uint32_t now = millis();
  const bool swRaw = digitalRead(PIN_ENC_SW) == LOW;
  if (swRaw != swLastRaw_) {
    swLastChangeMs_ = now;
    swLastRaw_ = swRaw;
  }
  if ((now - swLastChangeMs_) >= kSwDebounceMs) {
    swStableDown_ = swRaw;
  }
}

int8_t RotaryEncoder::consumeStep() {
  if (pendingSteps_ > 0) {
    --pendingSteps_;
    return 1;
  }
  if (pendingSteps_ < 0) {
    ++pendingSteps_;
    return -1;
  }
  return 0;
}

bool RotaryEncoder::consumePress() {
  if (swStableDown_ && !swFired_) {
    swFired_ = true;
    return true;
  }
  if (!swStableDown_) {
    swFired_ = false;
  }
  return false;
}
