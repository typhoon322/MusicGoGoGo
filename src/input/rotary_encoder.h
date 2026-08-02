#pragma once

#include <stdint.h>

// KY-040 rotary encoder — poll in loop()

class RotaryEncoder {
 public:
  void begin();
  void poll();

  // +1 CW, -1 CCW, 0 none (each step consumed once)
  int8_t consumeStep();

  // Encoder push (active LOW), edge once per press
  bool consumePress();

 private:
  uint8_t lastClk_ = 1;
  bool swStableDown_ = false;
  bool swFired_ = false;
  bool swLastRaw_ = true;
  uint32_t swLastChangeMs_ = 0;
  int8_t pendingSteps_ = 0;

  static constexpr uint8_t kSwDebounceMs = 35;
};
