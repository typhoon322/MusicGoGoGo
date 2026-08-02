#pragma once

// ESP32-S3 development board (R16N8 DevKit)
// MusicGoGoGo — INMP441 I2S mic + ST7789 3.2" TFT spectrum display

#define BOARD_NAME "MusicGoGoGo"
#define BOARD_ST7789_TFT

// I2S pins for INMP441 (I2S0)
#define PIN_I2S_BCK 4
#define PIN_I2S_WS 5
#define PIN_I2S_SD 6

// I2S sample config
#define I2S_PORT I2S_NUM_0
#define I2S_SAMPLE_RATE 16000
#define I2S_BITS_PER_SAMPLE 32
#define I2S_DMA_BUF_COUNT 4
#define I2S_DMA_BUF_LEN 256

// ST7789 TFT (SPI) — 3.2" 320×240 landscape
#define PIN_TFT_MOSI 11
#define PIN_TFT_SCK 12
#define PIN_TFT_CS 10
#define PIN_TFT_DC 13
#define PIN_TFT_RST 14
#define PIN_TFT_BL 3

// Native panel is 240×320; rotation 1 → 320×240 landscape
#define TFT_NATIVE_W 240
#define TFT_NATIVE_H 320
#define TFT_WIDTH 320
#define TFT_HEIGHT 240

// Some 3.2" ST7789 modules need Y offset; set non-zero if image is shifted
#define TFT_X_OFFSET 0
#define TFT_Y_OFFSET 0

// FFT / spectrum display defaults
#define FFT_SIZE 512
#define SPECTRUM_BARS 32
