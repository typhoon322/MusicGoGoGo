#pragma once

#include <stddef.h>
#include <stdint.h>

class I2sMic {
 public:
  bool begin();
  void end();
  bool ready() const { return ready_; }

  // Read count int16 samples blocking. Returns false on driver error.
  bool readSamples(int16_t *out, size_t count);

  void setGain(float gain);
  float gain() const { return gain_; }

  float lastRms() const { return lastRms_; }
  float lastPeak() const { return lastPeak_; }

 private:
  bool ready_ = false;
  float gain_ = 1.0f;
  float lastRms_ = 0.0f;
  float lastPeak_ = 0.0f;
};
