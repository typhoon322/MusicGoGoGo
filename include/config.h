#pragma once

#include <stdint.h>

#if defined(BOARD_S3_DEV)
#include "boards/board_s3.h"
#else
#error "Define BOARD_S3_DEV in platformio build_flags"
#endif

// Display refresh target (ms between spectrum frames)
#define SPECTRUM_FRAME_MS 33

// Bar fall-off per frame (0.0–1.0, higher = slower decay)
#define SPECTRUM_DECAY 0.82f

// Software gain applied to I2S samples (tune via serial if needed)
#define AUDIO_GAIN_DEFAULT 2.0f

// Status log interval
#define STATUS_LOG_MS 2000
