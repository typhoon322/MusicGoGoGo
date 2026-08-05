#pragma once

// M5Stack Cardputer ADV — ES8311 built-in mic + built-in ST7789 (135×240)
// Validation on Cardputer internal LCD first; final product uses S3 DevKit + external ST7789

#define BOARD_NAME "MusicGoGoGo (Cardputer ADV)"
#define BOARD_ST7789_TFT

// Use Cardputer built-in display (M5GFX). Set 0 to revert to EXT 14P external TFT.
#define CARDPUTER_USE_BUILTIN_LCD 1

// ES8311 mic is managed by M5Cardputer library (not raw I2S pins)
#define I2S_SAMPLE_RATE 16000

#if CARDPUTER_USE_BUILTIN_LCD
// Landscape after setRotation(1): 240×135
#define TFT_NATIVE_W 135
#define TFT_NATIVE_H 240
#define TFT_WIDTH 240
#define TFT_HEIGHT 135
#define TFT_X_OFFSET 0
#define TFT_Y_OFFSET 0
#else
// External ST7789 2.4" 320×240 via EXT port (HSPI / SPI3)
#define PIN_TFT_MOSI 14
#define PIN_TFT_SCK 40
#define PIN_TFT_CS 5
#define PIN_TFT_DC 6
#define PIN_TFT_RST 3
#define PIN_TFT_BL 15
#define TFT_NATIVE_W 240
#define TFT_NATIVE_H 320
#define TFT_WIDTH 320
#define TFT_HEIGHT 240
#define TFT_X_OFFSET 0
#define TFT_Y_OFFSET 0
#endif

#define FFT_SIZE 512
#define SPECTRUM_BARS 32

// No OPI PSRAM on Cardputer ADV — keep waterfall buffer small enough for internal RAM
#define VFX_WATERFALL_HISTORY 80

// Align UI refresh with mic buffer (~512 samples @ 16 kHz ≈ 32 ms)
#define SPECTRUM_FRAME_MS 32

// Smoother bars on Cardputer (less SPI flicker)
#define SPECTRUM_DECAY_CARDPUTER 0.90f

// Compact header; mic debug overlay toggled via BtnA long-press
#define VFX_HEADER_H 24
#define VFX_AREA_TOP 26

// Default software gain for ES8311 mic validation
#define CARDPUTER_MIC_GAIN 2.0f

#define VFX_AUTO_CYCLE_MS 0
