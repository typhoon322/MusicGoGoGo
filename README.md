# MusicGoGoGo 音乐动态频谱

基于 ESP32-S3 的实时音频频谱分析仪：INMP441 I2S 麦克风环境拾音，ST7789 3.2" TFT 显示动态柱状频谱。

## 硬件

| 组件 | 说明 |
|------|------|
| MCU | **ESP32-S3** DevKitC-1（8MB Flash + OPI PSRAM） |
| 音频输入 | INMP441 I2S 数字麦克风（环境拾音） |
| 显示 | ST7789 TFT 3.2"（320×240，SPI） |

详细接线与引脚见 [docs/wiring.md](docs/wiring.md)。采购清单见 [docs/bom.md](docs/bom.md)。硬件联调步骤见 [docs/hardware-checklist.md](docs/hardware-checklist.md)。

## 引脚概览（S3）

| 功能 | GPIO |
|------|------|
| I2S BCK / WS / SD | 4 / 5 / 6 |
| TFT MOSI / SCK / CS / DC / RST / BL | 11 / 12 / 10 / 13 / 14 / 3 |

完整定义：[`include/boards/board_s3.h`](include/boards/board_s3.h)

## 快速开始

```bash
pio run -e esp32-s3-dev -t upload
pio device monitor
```

上电后 **7 种 VFX 自动轮播**（12 秒/模式）。

## 操作

| 输入 | 功能 |
|------|------|
| **编码器旋转** | 上 / 下一个 VFX |
| **编码器按下** | 开关自动轮播 |
| **电位器** | 麦克风增益（0.4–8×） |
| 串口 `n`/`p`/`a` | 同上的备用控制 |

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
├── platformio.ini
├── src/
│   ├── main.cpp
│   ├── audio/i2s_mic.*
│   ├── dsp/band_processor.*      # 平滑 / 自动电平 / peak
│   ├── dsp/spectrum_analyzer.*   # FFT 多频段输出
│   └── display/
│       ├── display_driver.*
│       └── vfx_renderer.*        # 7 种视觉效果
├── include/
│   ├── config.h
│   ├── vfx.h
│   └── boards/board_s3.h
└── docs/
```

## 调参

在 [`include/config.h`](include/config.h) 中：

| 宏 | 默认 | 说明 |
|----|------|------|
| `AUDIO_GAIN_DEFAULT` | 2.0 | 麦克风软件增益 |
| `FFT_SIZE` | 512 | FFT 点数（`board_s3.h`） |
| `SPECTRUM_BARS` | 32 | 柱状数量 |
| `VFX_AUTO_CYCLE_MS` | 12000 | 自动轮播间隔（0=关闭） |
| `SPECTRUM_FRAME_MS` | 25 | 目标帧间隔 (~40 FPS) |
| `TFT_Y_OFFSET` | 0 | 屏偏移（图像错位时调整） |

## 文档索引

| 文档 | 内容 |
|------|------|
| [docs/bom.md](docs/bom.md) | 采购清单、选型说明、预算 |
| [docs/wiring.md](docs/wiring.md) | BOM 简表、引脚、接线图 |
| [docs/hardware-checklist.md](docs/hardware-checklist.md) | TFT → I2S → FFT → 显示 联调清单 |
