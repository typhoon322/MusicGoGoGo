#pragma once

#include <stdint.h>

#if defined(BOARD_S3_DEV)
#include "boards/board_s3.h"
#else
#error "Define BOARD_S3_DEV in platformio build_flags"
#endif

// Target ~40 FPS UI refresh
#define SPECTRUM_FRAME_MS 25

// Legacy decay (band processor handles primary smoothing)
#define SPECTRUM_DECAY 0.82f

#define AUDIO_GAIN_DEFAULT 2.0f

// Potentiometer → mic gain (ADC on PIN_GAIN_POT)
#define GAIN_POT_MIN 0.4f
#define GAIN_POT_MAX 8.0f

#define STATUS_LOG_MS 2000

// VFX auto-rotate interval (0 = disable)
#define VFX_AUTO_CYCLE_MS 12000

#define VFX_LOG_BANDS 12
#define VFX_WATERFALL_BINS 160
#define VFX_WATERFALL_HISTORY 200
