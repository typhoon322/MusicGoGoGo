#pragma once

#include <stdint.h>

#if defined(BOARD_S3_DEV)
void boardRgbBegin();
// Flash on-board NeoPixel from beatPulse (0..1). Optional bass/high wash.
void boardRgbUpdate(float beatPulse, float bassLevel = 0.0f, float highLevel = 0.0f);
#else
inline void boardRgbBegin() {}
inline void boardRgbUpdate(float, float = 0.0f, float = 0.0f) {}
#endif
