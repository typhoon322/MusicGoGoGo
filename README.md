# MusicGoGoGo 音乐动态频谱

基于 ESP32-S3 的实时音频频谱分析仪：环境拾音 + ST7789 TFT 显示 7 种动态 VFX。

支持两套硬件目标：

| 目标 | 用途 | PlatformIO env |
|------|------|----------------|
| **ESP32-S3 DevKit** | 最终产品：INMP441 + 外接 ST7789 + 编码器 + 电位器 | `esp32-s3-dev` |
| **M5 Cardputer ADV** | 开发验证：内置 ES8311 麦 + 内置/外接屏 | `cardputer-adv` |

## 硬件（S3 产品）

| 组件 | 说明 |
|------|------|
| MCU | ESP32-S3 DevKitC-1（8MB Flash + OPI PSRAM） |
| 音频输入 | INMP441 I2S 数字麦克风 |
| 显示 | ST7789 TFT 3.2"（320×240，SPI） |
| 输入 | KY-040 旋转编码器、10kΩ 增益电位器 |

详细接线见 [docs/wiring.md](docs/wiring.md)。采购见 [docs/bom.md](docs/bom.md)。联调见 [docs/hardware-checklist.md](docs/hardware-checklist.md)。

## 快速开始

### S3 产品固件

```bash
pio run -e esp32-s3-dev -t upload
pio device monitor
```

上电后 **7 种 VFX 自动轮播**（12 秒/模式，可在 `config.h` 关闭）。

### Cardputer ADV 验证固件

```bash
pio run -e cardputer-adv -t upload
pio device monitor
```

详见 [docs/cardputer-adv.md](docs/cardputer-adv.md)。编译产物可复制到 SD 卡经 M5Launcher 安装。

## 操作

### S3（编码器 + 电位器）

| 输入 | 功能 |
|------|------|
| 编码器旋转 | 上 / 下一个 VFX |
| 编码器按下 | 开关自动轮播 |
| 电位器 | 麦克风增益（0.4–8×） |
| 串口 `n`/`p`/`a` | 备用控制 |

### Cardputer ADV

| 输入 | 功能 |
|------|------|
| BtnA 短按 | 下一个 VFX |
| BtnA 长按 / `d` | 开关 mic 调试浮层 |
| `,` / `.` | 上 / 下一个 VFX |
| `[` / `]` | 增益 ±0.25 |
| 顶栏右上 | 电池电量 |

## VFX 模式

| 模式 | 效果 |
|------|------|
| Bars 32 | 32 柱 + 渐变 + peak 落点 |
| Log 12 | 12 段对数宽柱 |
| Mirror | 中心对称镜面 + 彩虹 |
| VU Meter | 24 段电平表 + 迷你频谱 |
| Waterfall | 热力瀑布图 |
| Rainbow | 彩虹柱 + 色相滚动 |
| Line Peaks | 折线峰值 |

## 工程结构

```
MusicGoGoGo/
├── platformio.ini          # esp32-s3-dev / cardputer-adv
├── scripts/copy_launcher_bin.py
├── launcher/               # cardputer-adv 编译产物（.bin 已 gitignore）
├── src/
│   ├── main.cpp
│   ├── audio/
│   │   ├── i2s_mic.*           # S3 INMP441
│   │   └── cardputer_mic.*     # Cardputer ES8311
│   ├── dsp/
│   │   ├── band_processor.*
│   │   └── spectrum_analyzer.*
│   ├── display/
│   │   ├── display_driver.*
│   │   └── vfx_renderer.*
│   └── input/                  # S3 编码器 / 电位器
├── include/
│   ├── config.h
│   ├── vfx.h
│   ├── boards/
│   │   ├── board_s3.h
│   │   └── board_cardputer_adv.h
│   └── display/tft_colors.h
└── docs/
```

## 调参

| 宏 | 位置 | S3 默认 | Cardputer |
|----|------|---------|-----------|
| `AUDIO_GAIN_DEFAULT` | `config.h` | 2.0 | — |
| `CARDPUTER_MIC_GAIN` | `board_cardputer_adv.h` | — | 2.0 |
| `FFT_SIZE` | `board_*.h` | 512 | 512 |
| `SPECTRUM_BARS` | `board_*.h` | 32 | 32 |
| `VFX_AUTO_CYCLE_MS` | `board_*.h` / `config.h` | 12000 | 0（关） |
| `SPECTRUM_FRAME_MS` | `config.h` / board | 25 | 32 |

## 文档索引

| 文档 | 内容 |
|------|------|
| [docs/bom.md](docs/bom.md) | 采购清单 |
| [docs/wiring.md](docs/wiring.md) | S3 接线 + Cardputer EXT 外接屏 |
| [docs/hardware-checklist.md](docs/hardware-checklist.md) | S3 / Cardputer 联调清单 |
| [docs/cardputer-adv.md](docs/cardputer-adv.md) | Cardputer 验证、操作、排错 |
