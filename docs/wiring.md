# 接线说明

## BOM

| 序号 | 元件 | 数量 | 备注 |
|------|------|------|------|
| 1 | ESP32-S3 开发板（8MB Flash + OPI PSRAM） | 1 | DevKitC-1，N16R8 / N8R8 |
| 2 | INMP441 I2S 麦克风模块 | 1 | L/R 接 GND = 左声道 |
| 3 | ST7789 TFT 3.2 寸 | 1 | 320×240，4-wire SPI |
| 4 | 杜邦线 | 若干 | 3.3V 逻辑 |
| 5 | USB 数据线 | 1 | 供电 + 烧录 |

完整采购清单与单价见 [bom.md](bom.md)。

## 引脚定义

引脚在 [`include/boards/board_s3.h`](../include/boards/board_s3.h) 中定义，编译 env 为 `esp32-s3-dev`。

| 功能 | GPIO | 说明 |
|------|------|------|
| I2S BCK | 4 | INMP441 SCK |
| I2S WS (LRCK) | 5 | INMP441 WS |
| I2S SD (DIN) | 6 | INMP441 SD → ESP32 输入 |
| SPI MOSI | 11 | ST7789 SDI |
| SPI SCK | 12 | ST7789 SCK |
| TFT CS | 10 | 片选 |
| TFT DC | 13 | 数据/命令 |
| TFT RST | 14 | 复位 |
| TFT BL | 3 | 背光（高电平亮） |

> I2S 与 SPI 引脚无冲突。本项目无 I2C 设备。

烧录：

```bash
pio run -e esp32-s3-dev -t upload
pio device monitor
```

## INMP441 麦克风（I2S）

| INMP441 | ESP32-S3 |
|---------|----------|
| VDD | 3.3V |
| GND | GND |
| SCK | GPIO 4 |
| WS | GPIO 5 |
| SD | GPIO 6 |
| L/R | GND（左声道） |

```
ESP32-S3                         INMP441
────────                         ───────
3.3V ─────────────────────────→ VDD
GND  ─────────────────────────→ GND
GPIO 4 (BCK) ─────────────────→ SCK
GPIO 5 (WS)  ─────────────────→ WS
GPIO 6 (DIN) ←───────────────── SD
GND  ─────────────────────────→ L/R
```

- VDD **必须 3.3V**，勿接 5V
- L/R 悬空会导致声道不确定；接 GND 固定为左声道单麦
- 模块丝印可能标为 BCK / LRCK / DOUT，与上表 SCK / WS / SD 对应

## ST7789 TFT（SPI）

| TFT | ESP32-S3 |
|-----|----------|
| VCC | 3.3V（**勿接 5V**） |
| GND | GND |
| CS | GPIO 10 |
| RESET | GPIO 14 |
| DC / RS | GPIO 13 |
| MOSI / SDI | GPIO 11 |
| SCK | GPIO 12 |
| LED / BL | GPIO 3 |
| MISO | 不接（4-wire SPI） |

```
ESP32-S3                         ST7789
────────                         ───────
3.3V ─────────────────────────→ VCC
GND  ─────────────────────────→ GND
GPIO 10 ──────────────────────→ CS
GPIO 14 ──────────────────────→ RESET
GPIO 13 ──────────────────────→ DC
GPIO 11 (MOSI) ───────────────→ SDI
GPIO 12 (SCK)  ───────────────→ SCK
GPIO 3 ───────────────────────→ BL
```

横屏显示：`setRotation(1)` → 320×240。若画面偏移，改 `board_s3.h` 中 `TFT_Y_OFFSET`。

## 电源

- MCU、麦克风、屏幕均使用 **3.3V**
- 所有 GND 共地
- USB 供电即可满足开发调试

## 改引脚

只需编辑 [`include/boards/board_s3.h`](../include/boards/board_s3.h)，重新编译烧录：

```bash
pio run -e esp32-s3-dev -t upload
```

## 上电检查

1. 先接 **ESP32 + TFT**，不接麦克风，USB 供电
2. 烧录固件，打开串口监视器（115200），确认启动日志中的 `Board: ...`
3. 确认 TFT 背光亮、测试图案正常
4. 再接 INMP441，确认 I2S 采样有数值（拍手 / 对音箱有反应）
5. 详细分阶段步骤见 [hardware-checklist.md](hardware-checklist.md)

**重要：**

- TFT VCC **不得**接 5V
- INMP441 VDD **不得**接 5V
- ESP32 GND 与所有模块 GND 必须共地
