#include "audio/i2s_mic.h"

#if defined(BOARD_S3_DEV)

#include <Arduino.h>
#include <driver/i2s.h>
#include <math.h>

#include "config.h"

namespace {
int32_t rawToPcm(int32_t raw) {
  // INMP441: 24-bit MSB-aligned in 32-bit slot
  return raw >> 14;
}
}  // namespace

bool I2sMic::begin() {
  if (ready_) {
    return true;
  }

  i2s_config_t cfg = {};
  cfg.mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_RX);
  cfg.sample_rate = I2S_SAMPLE_RATE;
  cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT;
  cfg.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
  cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  cfg.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  cfg.dma_buf_count = I2S_DMA_BUF_COUNT;
  cfg.dma_buf_len = I2S_DMA_BUF_LEN;
  cfg.use_apll = false;
  cfg.tx_desc_auto_clear = false;
  cfg.fixed_mclk = 0;

  i2s_pin_config_t pins = {};
  pins.bck_io_num = PIN_I2S_BCK;
  pins.ws_io_num = PIN_I2S_WS;
  pins.data_out_num = I2S_PIN_NO_CHANGE;
  pins.data_in_num = PIN_I2S_SD;

  if (i2s_driver_install(I2S_PORT, &cfg, 0, nullptr) != ESP_OK) {
    Serial.println(F("[i2s] driver_install failed"));
    return false;
  }
  if (i2s_set_pin(I2S_PORT, &pins) != ESP_OK) {
    Serial.println(F("[i2s] set_pin failed"));
    i2s_driver_uninstall(I2S_PORT);
    return false;
  }
  i2s_zero_dma_buffer(I2S_PORT);

  ready_ = true;
  Serial.printf("[i2s] OK rate=%u bck=%d ws=%d sd=%d\n", I2S_SAMPLE_RATE, PIN_I2S_BCK,
                PIN_I2S_WS, PIN_I2S_SD);
  return true;
}

void I2sMic::end() {
  if (!ready_) {
    return;
  }
  i2s_driver_uninstall(I2S_PORT);
  ready_ = false;
}

void I2sMic::setGain(float gain) {
  if (gain < 0.1f) {
    gain = 0.1f;
  }
  if (gain > 32.0f) {
    gain = 32.0f;
  }
  gain_ = gain;
}

bool I2sMic::readSamples(int16_t *out, size_t count) {
  if (!ready_ || out == nullptr || count == 0) {
    return false;
  }

  double sumSq = 0.0;
  float peak = 0.0f;

  for (size_t i = 0; i < count; ++i) {
    int32_t raw = 0;
    size_t bytesRead = 0;
    const esp_err_t err =
        i2s_read(I2S_PORT, &raw, sizeof(raw), &bytesRead, portMAX_DELAY);
    if (err != ESP_OK || bytesRead != sizeof(raw)) {
      return false;
    }

    float sample = static_cast<float>(rawToPcm(raw)) * gain_;
    if (sample > 32767.0f) {
      sample = 32767.0f;
    }
    if (sample < -32768.0f) {
      sample = -32768.0f;
    }
    out[i] = static_cast<int16_t>(sample);

    const float absSample = fabsf(sample);
    if (absSample > peak) {
      peak = absSample;
    }
    sumSq += static_cast<double>(sample) * static_cast<double>(sample);
  }

  lastPeak_ = peak / 32768.0f;
  lastRms_ = static_cast<float>(sqrt(sumSq / static_cast<double>(count))) / 32768.0f;
  return true;
}

#endif  // BOARD_S3_DEV
