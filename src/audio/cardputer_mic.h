#pragma once

#include <stddef.h>
#include <stdint.h>

class CardputerMic {
 public:
  bool begin();
  bool beginPlatform();
  bool beginCapture();
  void end();
  bool ready() const { return ready_; }

  bool readSamples(int16_t *out, size_t count);

  void setGain(float gain);
  float gain() const { return gain_; }

  float lastRms() const { return lastRms_; }
  float lastPeak() const { return lastPeak_; }

  int16_t lastRawMin() const;
  int16_t lastRawMax() const;
  int32_t lastRawMean() const;

 private:
  bool platformReady_ = false;
  bool ready_ = false;
  float gain_ = 1.0f;
  float lastRms_ = 0.0f;
  float lastPeak_ = 0.0f;
};
