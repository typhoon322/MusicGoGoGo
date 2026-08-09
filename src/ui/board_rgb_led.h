#pragma once

#include <stdint.h>

#if defined(BOARD_S3_DEV)
void boardRgbBegin();
// Drive on-board NeoPixel from kick (blue) / snare (green) pulses 0..1.
void boardRgbUpdate(float kickPulse, float snarePulse);
#else
inline void boardRgbBegin() {}
inline void boardRgbUpdate(float, float) {}
#endif
