#pragma once

// ESP32-S3 DevKit — N16R8 (16MB Flash + 8MB OPI PSRAM)
// MusicGoGoGo — INMP441 I2S mic + ST7789 3.2" TFT spectrum display

#define BOARD_NAME "MusicGoGoGo"
#define BOARD_ST7789_TFT

// I2S pins for INMP441 (I2S0)
#define PIN_I2S_BCK 4
#define PIN_I2S_WS 5
#define PIN_I2S_SD 6

// I2S sample config — 44.1kHz so bands reach ~20kHz (Nyquist)
#define I2S_PORT I2S_NUM_0
#define I2S_SAMPLE_RATE 44100
#define I2S_BITS_PER_SAMPLE 32
// Deeper DMA ring so WiFi/TFT load doesn't underrun captures
#define I2S_DMA_BUF_COUNT 16
#define I2S_DMA_BUF_LEN 512

// ST7789 TFT (SPI) — 3.2" 320×240 landscape
#define PIN_TFT_MOSI 11
#define PIN_TFT_SCK 12
#define PIN_TFT_CS 10
#define PIN_TFT_DC 13
#define PIN_TFT_RST 14
#define PIN_TFT_BL 3

// KY-040 rotary encoder (active LOW, internal pull-up)
#define PIN_ENC_CLK 8
#define PIN_ENC_DT 9
#define PIN_ENC_SW 15

// Gain potentiometer — wiper to ADC, ends to 3.3V / GND
#define PIN_GAIN_POT 1

// Native panel is 240×320; rotation 1 → 320×240 landscape
#define TFT_NATIVE_W 240
#define TFT_NATIVE_H 320
#define TFT_WIDTH 320
#define TFT_HEIGHT 240

// Some 3.2" ST7789 modules need Y offset; set non-zero if image is shifted
#define TFT_X_OFFSET 0
#define TFT_Y_OFFSET 0

// FFT 2048 @ 44.1kHz ≈ 21.5 Hz/bin; hop advances window for 25–30 FPS
#define FFT_SIZE 2048
#define FFT_HOP 1470  // ≈30 FPS audio cadence (44100/30)
#define SPECTRUM_BARS 30

// Auto-cycle disabled by default on S3; enable anytime via Web UI / serial 'a'
#define VFX_AUTO_CYCLE_MS 0

// Display budget; hop already paces ~30 FPS when DMA is fed continuously
#ifndef SPECTRUM_FRAME_MS
#define SPECTRUM_FRAME_MS 33
#endif

// Waterfall ring (PSRAM-backed on S3). Matches full plot height.
#define VFX_WATERFALL_HISTORY 240

// Taller header strip for the walking/jumping cat
#define VFX_HEADER_H 36
#define VFX_AREA_TOP 38
