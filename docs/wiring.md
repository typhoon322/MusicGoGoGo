# 接线说明

## BOM

| 序号 | 元件 | 数量 | 备注 |
|------|------|------|------|
| 1 | ESP32-S3 开发板（8MB Flash + OPI PSRAM） | 1 | DevKitC-1，N16R8 / N8R8 |
| 2 | INMP441 I2S 麦克风模块 | 1 | L/R 接 GND = 左声道 |
| 3 | ST7789 TFT 3.2 寸 | 1 | 320×240，4-wire SPI |
| 4 | 杜邦线 | 若干 | 3.3V 逻辑 |
| 5 | USB 数据线 | 1 | 供电 + 烧录 |
| 6 | KY-040 旋转编码器 | 1 | 转=VFX 切换，按=自动轮播开关 |
| 7 | 电位器 10kΩ | 1 | 麦克风增益 0.4–8× |

完整采购清单与单价见 [bom.md](bom.md)。

## 引脚定义

引脚在 [`include/boards/board_s3.h`](../include/boards/board_s3.h) 中定义，编译 env 为 `esp32-s3-dev`。

| 功能 | GPIO | 说明 |
|------|------|------|
| I2S BCK | 4 | INMP441 SCK |
| I2S WS (LRCK) | 5 | INMP441 WS |
| I2S SD (DIN) | 6 | INMP441 SD → ESP32 输入 |
| SPI MOSI | 11 | ST7789 SDA（丝印 SDA） |
| SPI SCK | 12 | ST7789 SCL（丝印 SCL） |
| TFT CS | 10 | 片选（丝印 CS） |
| TFT DC | 13 | 数据/命令（丝印 DC） |
| TFT RST | 14 | 复位（丝印 RES） |
| TFT BL | 3 | 背光（丝印 BLK） |
| ENC CLK | 8 | KY-040 CLK |
| ENC DT | 9 | KY-040 DT |
| ENC SW | 15 | KY-040 按键（接 GND） |
| Gain POT | 1 | 电位器中间脚（ADC） |

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

| TFT 丝印 | ESP32-S3 |
|-----|----------|
| VCC | 3.3V（**勿接 5V**） |
| GND | GND |
| CS | GPIO 10 |
| RES | GPIO 14 |
| DC | GPIO 13 |
| SDA | GPIO 11 |
| SCL | GPIO 12 |
| BLK | GPIO 3 |
| MISO | 不接（4-wire SPI） |

```
ESP32-S3                         ST7789
────────                         ───────
3.3V ─────────────────────────→ VCC
GND  ─────────────────────────→ GND
GPIO 10 ──────────────────────→ CS
GPIO 14 ──────────────────────→ RES
GPIO 13 ──────────────────────→ DC
GPIO 11 (MOSI) ───────────────→ SDA
GPIO 12 (SCK)  ───────────────→ SCL
GPIO 3 ───────────────────────→ BLK
```

> 模块丝印为 **SCL / SDA / RES / DC / CS / BLK**：`SCL`=时钟、`SDA`=数据、`RES`=复位、`BLK`=背光，与 ESP32 的 SPI 命名 SCK / MOSI 对应。

横屏显示：`setRotation(1)` → 320×240。若画面偏移，改 `board_s3.h` 中 `TFT_Y_OFFSET`。

## KY-040 旋转编码器

| KY-040 | ESP32-S3 |
|--------|----------|
| CLK | GPIO 8 |
| DT | GPIO 9 |
| SW | GPIO 15 |
| + | 3.3V |
| GND | GND |

```
ESP32-S3                         KY-040
────────                         ──────
3.3V ─────────────────────────→ +
GND  ─────────────────────────→ GND
GPIO 8 ───────────────────────→ CLK
GPIO 9 ───────────────────────→ DT
GPIO 15 ──────────────────────→ SW
GND  ─────────────────────────→ SW（另一端，按下导通）
```

| 操作 | 功能 |
|------|------|
| **顺时针转** | 下一个 VFX |
| **逆时针转** | 上一个 VFX |
| **按下** | 开关自动轮播 |

> 若方向反了，对调 CLK 与 DT 接线。

## 增益电位器（10kΩ）

| 电位器 | ESP32-S3 |
|--------|----------|
| 一脚 | 3.3V |
| 中间脚 | GPIO 1 |
| 另一脚 | GND |

- 逆时针（电压低）→ 增益小，适合安静环境
- 顺时针（电压高）→ 增益大，适合远距离拾音
- 范围在 [`config.h`](../include/config.h) 的 `GAIN_POT_MIN` / `GAIN_POT_MAX` 可调（默认 0.4–8.0）

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

---

## Cardputer ADV 验证

在未焊接 S3 硬件前，可用 **M5Stack Cardputer ADV** 验证麦克风、FFT 与 VFX。

完整说明见 **[docs/cardputer-adv.md](cardputer-adv.md)**。

```bash
pio run -e cardputer-adv -t upload
pio device monitor
```

### 当前默认：内置屏 + 内置麦

[`board_cardputer_adv.h`](../include/boards/board_cardputer_adv.h) 中 `CARDPUTER_USE_BUILTIN_LCD 1`：

- 显示：机身 ST7789，横屏 **240×135**（M5GFX）
- 麦克风：ES8311（M5Cardputer 库）
- 渲染：离屏 Sprite 整帧推送，避免撕裂斜纹

设为 `0` 则改用 EXT 14P 外接 2.4" ST7789（见下文）。

### 操作（Cardputer）

| 输入 | 功能 |
|------|------|
| **BtnA 短按** | 下一个 VFX |
| **BtnA 长按**（约 0.6 s） | 开关 mic 调试浮层 |
| `d` | 开关调试浮层 |
| `,` / `;` | 上一个 VFX |
| `.` / `:` | 下一个 VFX |
| `[` / `]` | 降低 / 提高麦克风增益 |
| 顶栏右上 | 电池电量 % |
| 串口 `n` / `p` / `+` / `-` | 备用控制 |

Cardputer 固件 **默认关闭自动轮播**。

### SD 卡安装（可选）

编译 `cardputer-adv` 后自动复制 `MusicGoGoGo.bin` 到：

- `launcher/MusicGoGoGo.bin`（项目内）
- `/Users/yanx/ESP32/M5StackBin/MusicGoGoGo.bin`（本机固件汇总）

拷到 SD 卡根目录 → M5Launcher → SD → Install。

---

## Cardputer ADV — 外接 ST7789（EXT 14P）

当 `CARDPUTER_USE_BUILTIN_LCD 0` 时使用此外接方案。

### EXT 14P → ST7789

| TFT 丝印 | Cardputer GPIO | EXT 排针 | 说明 |
|----------|----------------|----------|------|
| VCC | **5V IN** | **pin 2** | EXT 标注为 5V IN；多数 2.4" 模块板载 LDO 可接此脚；**仅 3.3V 模块勿接** |
| GND | GND | pin 4 | 共地 |
| SCL | 40 | pin 7 | SPI 时钟 |
| SDA | 14 | pin 9 | SPI 数据 |
| CS | 5 | pin 13 | 片选 |
| DC | 6 | pin 5 | 数据/命令 |
| RES | 3 | pin 1 | 复位 |
| BLK | 15 | pin 14 | 背光（高电平亮） |
| MISO | — | — | 4-wire SPI 不接 |

### EXT 14P 按 pin 顺序（接 ST7789）

排针为 **2×7**（两排横针）。Cardputer ADV 丝印与 [M5 官方 PinMap](https://docs.m5stack.com/en/core/Cardputer-Adv) 一致：

- **上排** = 奇数 pin（1、3、5 … 13）→ 丝印 **G3、G4、G6 …**
- **下排** = 偶数 pin（2、4、6 … 14）→ 丝印 **5V IN、GND、5VOUT …**

> 以前写「左列 / 右列」容易和丝印对不上；以 **上排 / 下排** 为准。  
> **G3 = pin 1 = 上排最左侧**，不在下排。

面向 EXT 口（屏幕朝你、键盘在远端），从左到右：

| Pin | 排 | 丝印 / GPIO | 官方功能 | 接 ST7789 |
|-----|----|-------------|----------|-----------|
| 1 | **上** | G3 | RESET | **RES** |
| 2 | **下** | 5V IN | 5VIN | **VCC** |
| 3 | **上** | G4 | INT | 不接 |
| 4 | **下** | GND | GND | **GND** |
| 5 | **上** | G6 | BUSY | **DC** |
| 6 | **下** | 5VOUT | 5V 输出 | 不接 |
| 7 | **上** | G40 | SCK | **SCL**（屏上丝印 SCL） |
| 8 | **下** | G8 | I2C_SDA | 不接 |
| 9 | **上** | G14 | MOSI | **SDA**（屏上丝印 SDA） |
| 10 | **下** | G9 | I2C_SCL | 不接 |
| 11 | **上** | G39 | MISO | 不接（4-wire SPI） |
| 12 | **下** | G13 | UART_RX | 不接 |
| 13 | **上** | G5 | CS | **CS** |
| 14 | **下** | G15 | UART_TX | **BLK**（背光） |

```
EXT 14P → ST7789（上排 = 奇数 pin，下排 = 偶数 pin；● = 要接）

        ←──────────── 靠近屏幕侧 ────────────→

上排  G3      G4      G6      G40     G14     G39     G5
pin   1       3       5       7       9       11      13
      ●       ·       ●       ●       ●       ·       ●
      RES             DC      SCL     SDA             CS

下排  5V IN   GND     5VOUT   G8      G9      G13     G15
pin   2       4       6       8       10      12      14
      ●       ●       ·       ·       ·       ·       ●
      VCC     GND                                     BLK

共 8 线：上排 RES / DC / SCL / SDA / CS；下排 VCC / GND / BLK
```

> 屏模块丝印为 **SCL / SDA**：在 4-wire SPI 模式下分别接 **SCK / MOSI**（不是 EXT 下排的 I2C G8/G9）。

```
Cardputer EXT                     ST7789 2.4"
─────────────                     ────────────
pin 2  5V IN ───────────────────→ VCC
pin 4  GND  ───────────────────→ GND
pin 7  G40  ───────────────────→ SCL
pin 9  G14  ───────────────────→ SDA
pin 13 G5   ───────────────────→ CS
pin 5  G6   ───────────────────→ DC
pin 1  G3   ───────────────────→ RES
pin 14 G15  ───────────────────→ BLK
```

> EXT SPI 与 microSD 共用 40/14 总线；外接 CS 用 GPIO5，与 SD 片选 GPIO12 不冲突。  
> **pin 2 = 5V IN**（EXT 丝印），由 Cardputer 供电链路提供；SPI 信号仍为 3.3V 逻辑。

若使用内置屏验证，**无需接 EXT 屏**；见 [cardputer-adv.md](cardputer-adv.md)。
