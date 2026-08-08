#pragma once

class GainPot {
 public:
  void begin();
  void poll();

  float gain() const { return gain_; }
  bool changed() const { return changed_; }
  void clearChanged() { changed_ = false; }
  void setEnabled(bool enabled) { enabled_ = enabled; }
  bool enabled() const { return enabled_; }

 private:
  float gain_ = 1.0f;
  float filtered_ = 0.0f;
  bool changed_ = false;
  bool enabled_ = true;
  int lastRaw_ = -1;
};
