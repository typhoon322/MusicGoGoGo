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
| MCU | ESP32-S3 N16R8 DevKitC-1（16MB Flash + 8MB OPI PSRAM） |
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

上电进入 Bars 模式；可用串口命令或编码器切换效果（自动轮播默认关闭，输入 `a` 或按下编码器开启）。

### Cardputer ADV 验证固件

```bash
pio run -e cardputer-adv -t upload
pio device monitor
```

详见 [docs/cardputer-adv.md](docs/cardputer-adv.md)。编译后 bin 自动复制到 `launcher/` 与 `~/ESP32/M5StackBin/`，可拷到 SD 卡经 M5Launcher 安装。

## 操作

### S3（编码器 + 电位器）

| 输入 | 功能 |
|------|------|
| 编码器旋转 | 上 / 下一个 VFX |
| 编码器按下 | 开关自动轮播 |
| 电位器 | 麦克风增益（0.4–8×） |
| 串口 | 完整命令见下方「串口命令」 |

### Cardputer ADV

| 输入 | 功能 |
|------|------|
| BtnA 短按 | 下一个 VFX |
| BtnA 长按 / `d` | 开关 mic 调试浮层 |
| `,` / `.` | 上 / 下一个 VFX |
| `[` / `]` | 增益 ±0.25 |
| 顶栏右上 | 电池电量 |

## 串口命令

两套目标均支持串口（115200）行式命令，用于外接串口调试工具（如 Cardputer）：

| 命令 | 功能 |
|------|------|
| `n` / `next` | 下一个 VFX |
| `p` / `prev` | 上一个 VFX |
| `m` / `mode` | 显示当前模式 |
| `m 2` / `mode next` / `mode prev` | 跳转到指定模式 0–10，或上/下一个 |
| `g` / `gain` | 显示当前增益 |
| `g 3.0` | 直接设置增益（自动禁用增益电位器） |
| `g+` / `g-` / `+` / `-` | 增益 ±0.25（自动禁用增益电位器） |
| `pot [on\|off]` | 恢复/关闭增益电位器控制（仅 S3） |
| `fl` / `labels [on\|off]` | TFT 频率标注（仅 S3） |
| `al [on\|off]` | 自动电平 AGC |
| `a` / `auto [on\|off]` | 切换/设置自动轮播 |
| `s` / `status` | 打印状态与耗时统计 |
| `d` / `debug` | 开关 mic 调试浮层（仅 Cardputer） |
| `h` / `help` / `?` | 打印命令表 |

模式编号：0=Bars 1=Log 2=Mirror 3=VU 4=Waterfall 5=Rainbow 6=LinePeaks 7=Bounce 8=Dot 9=Glow 10=Ring

## VFX 模式

| 模式 | 效果 |
|------|------|
| Bars 32 | 32 柱 + peak 落点（S3 默认 3px 间隙） |
| Log 12 | 12 段对数宽柱 |
| Mirror | 中心对称镜面 + 彩虹 |
| VU Meter | 24 段电平表 + 迷你频谱 |
| Waterfall | 热力瀑布图（纵向铺满绘图区） |
| Rainbow | 彩虹柱 + 色相滚动 |
| Line Peaks | 折线峰值 |
| Bounce | 弹性柱 + 弹簧阻尼 |
| Dot | 星点频谱 |
| Glow | 辉光柱 |
| Ring | 环形放射 + 中心 VU |

中文更新说明见 [`docs/vfx-effects-2026-08-09.zh.md`](docs/vfx-effects-2026-08-09.zh.md)。

## 工程结构

```
MusicGoGoGo/
├── platformio.ini          # esp32-s3-dev / cardputer-adv
├── scripts/copy_launcher_bin.py
├── launcher/               # cardputer-adv → MusicGoGoGo.bin（及 ~/ESP32/M5StackBin/）
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
| `VFX_AUTO_CYCLE_MS` | `board_*.h` / `config.h` | 0（关，board_s3.h 覆盖） | 0（关） |
| `SPECTRUM_FRAME_MS` | `config.h` / board | 25 | 32 |

## 文档索引

| 文档 | 内容 |
|------|------|
| [docs/bom.md](docs/bom.md) | 采购清单 |
| [docs/wiring.md](docs/wiring.md) | S3 接线 + Cardputer EXT 外接屏 |
| [docs/hardware-checklist.md](docs/hardware-checklist.md) | S3 / Cardputer 联调清单 |
| [docs/cardputer-adv.md](docs/cardputer-adv.md) | Cardputer 验证、操作、排错 |
