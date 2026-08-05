#pragma once

#include <stdint.h>

#if defined(BOARD_S3_DEV)
#include "boards/board_s3.h"
#elif defined(BOARD_CARDPUTER_ADV)
#include "boards/board_cardputer_adv.h"
#else
#error "Define BOARD_S3_DEV or BOARD_CARDPUTER_ADV in platformio build_flags"
#endif

// Target ~40 FPS UI refresh
#ifndef SPECTRUM_FRAME_MS
#define SPECTRUM_FRAME_MS 25
#endif

// Legacy decay (band processor handles primary smoothing)
#define SPECTRUM_DECAY 0.82f

#define AUDIO_GAIN_DEFAULT 2.0f

// Potentiometer → mic gain (ADC on PIN_GAIN_POT)
#define GAIN_POT_MIN 0.4f
#define GAIN_POT_MAX 8.0f

#define STATUS_LOG_MS 2000

// VFX auto-rotate interval (0 = disable)
#ifndef VFX_AUTO_CYCLE_MS
#define VFX_AUTO_CYCLE_MS 12000
#endif

#define VFX_LOG_BANDS 12
#define VFX_WATERFALL_BINS 160
#ifndef VFX_WATERFALL_HISTORY
#define VFX_WATERFALL_HISTORY 200
#endif
