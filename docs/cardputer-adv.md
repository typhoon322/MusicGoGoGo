# Cardputer ADV 验证指南

在未焊接 S3 + INMP441 + 外接 ST7789 之前，可用 **M5Stack Cardputer ADV** 验证麦克风采集、FFT 与 7 种 VFX 显示效果。

当前默认配置：**内置 ES8311 麦克风 + 内置 ST7789（240×135 横屏）**。最终产品仍使用 [`board_s3.h`](../include/boards/board_s3.h) 上的 S3 DevKit 方案。

## 编译与烧录

```bash
pio run -e cardputer-adv -t upload
pio device monitor
```

| 项 | 说明 |
|----|------|
| PlatformIO env | `cardputer-adv` |
| 平台版本 | `espressif32@6.7.0`（锁定 IDF 5.1.x，避免 ES8311 在较新 IDF 上静音） |
| 串口 | USB CDC，115200 |
| 板型宏 | `BOARD_CARDPUTER_ADV` |

编译成功后，固件会自动复制到两处（由 [`scripts/copy_launcher_bin.py`](../scripts/copy_launcher_bin.py) 完成）：

| 路径 | 用途 |
|------|------|
| [`launcher/MusicGoGoGo.bin`](../launcher/) | 项目内，便于拷到 SD 卡 |
| `~/ESP32/M5StackBin/MusicGoGoGo.bin` | 本机 M5 固件汇总目录（绝对路径示例：`/Users/yanx/ESP32/M5StackBin`） |

SD 卡安装：将 `MusicGoGoGo.bin` 拷到 SD 卡根目录 → Cardputer **M5Launcher → SD → Install**。

## 显示模式切换

在 [`include/boards/board_cardputer_adv.h`](../include/boards/board_cardputer_adv.h) 中：

```c
#define CARDPUTER_USE_BUILTIN_LCD 1   // 1 = 机身内置屏（当前默认）
#define CARDPUTER_USE_BUILTIN_LCD 0   // 0 = EXT 14P 外接 ST7789 2.4"
```

外接屏接线见 [wiring.md — Cardputer EXT 14P](wiring.md#cardputer-adv-验证外接-st7789)。

## 界面说明

| 区域 | 内容 |
|------|------|
| 顶栏左侧 | 当前 VFX 模式名 |
| 顶栏右侧 | 电池图标 + 电量百分比（约 1 秒刷新） |
| 主区域 | 频谱 / 瀑布 / 镜面等 VFX |
| 调试浮层 | 默认隐藏；长按 BtnA 或按 `d` 调出 |

调试浮层内容（覆盖频谱区顶部）：

- RMS / Peak / 软件增益 G
- 原始采样 `raw[min..max] avg`
- 低频四段 B0–B3

## 操作

| 输入 | 功能 |
|------|------|
| **BtnA 短按** | 下一个 VFX |
| **BtnA 长按**（约 0.6 s） | 开关调试浮层 |
| `,` / `;` | 上一个 VFX |
| `.` / `:` | 下一个 VFX |
| `[` / `]` | 麦克风增益 −0.25 / +0.25 |
| `/` | 开关自动轮播（Cardputer 默认关闭） |
| `d` | 开关调试浮层 |
| 串口 | 行式命令：`help` 查看完整命令表 |

### 串口命令（115200）

用 Cardputer 或其他工具经串口发送，如 `m 3`、`g 2.5`：

| 命令 | 功能 |
|------|------|
| `n` / `next` | 下一个 VFX |
| `p` / `prev` | 上一个 VFX |
| `m` / `mode [n]` | 显示 / 跳转模式 0–6 |
| `g` / `gain [val]` | 显示 / 设置增益，`g+` `g-` 步进 |
| `a` / `auto [on\|off]` | 切换自动轮播 |
| `s` / `status` | 状态与耗时统计 |
| `d` / `debug` | 开关调试浮层 |
| `h` / `help` | 命令表 |

模式编号：0=Bars 1=Log 2=Mirror 3=VU 4=Waterfall 5=Rainbow 6=LinePeaks

Cardputer 验证固件 **默认关闭 VFX 自动轮播**（`VFX_AUTO_CYCLE_MS 0`）。

## 麦克风（ES8311）

- 由 **M5Cardputer** 库管理，不走原始 I2S 引脚
- `M5Cardputer.Mic.record()` 为 **异步**：必须在 `isRecording()` 结束后再读 buffer
- 默认软件增益：`CARDPUTER_MIC_GAIN 2.0f`（可在 `board_cardputer_adv.h` 修改）
- 低频段针对 Cardputer 做了 bass 权重与 bin 跳过，减轻 DC / 低频抬升

初始化顺序：`beginPlatform()` → 显示 `begin()` → `beginCapture()`（麦克风）。

## 显示与性能

内置屏使用 **M5GFX + 离屏 Sprite**：

1. 在内存中合成整帧频谱区域
2. 单次 `pushSprite` 推到屏幕（配合 `startWrite` / `endWrite`）

避免逐柱写屏造成的 **斜纹撕裂**。柱间保留 1 px 间隙，每柱带灰色边框。

| 宏 | Cardputer 值 | 说明 |
|----|--------------|------|
| `SPECTRUM_FRAME_MS` | 32 | 对齐 512 样本 @ 16 kHz |
| `VFX_WATERFALL_HISTORY` | 80 | 无 OPI PSRAM，缩小瀑布缓冲 |
| `SPECTRUM_DECAY_CARDPUTER` | 0.90 | 柱衰减，略慢于 S3 |
| `VFX_HEADER_H` | 24 | 紧凑顶栏 |
| `VFX_AREA_TOP` | 26 | 频谱区起始 Y |

## 电量

通过 `M5Cardputer.Power.getBatteryLevel()` 读取（0–100%）。Cardputer / Cardputer-Adv **无法可靠读取充电状态**（M5 官方说明）。

顶栏电池颜色：>50% 绿，20–50% 黄，≤20% 红。

## 常见问题

### 白屏 / 重启

- 确认使用 `espressif32@6.7.0` env，勿随意升级平台包
- 不要在 init 中调用 `Speaker.end()`（会导致 I2S 冲突）

### 频谱不动或呈固定阶梯

- 检查 ES8311 异步录音是否等待 `isRecording()` 结束
- 打开调试浮层，对着麦克风说话，观察 `raw[...]` 是否变化
- 用 `[` / `]` 调整增益

### 斜纹 / 撕裂

- 确认 `CARDPUTER_USE_BUILTIN_LCD 1` 且固件含 Sprite 整帧推送
- 勿改回逐柱增量刷新

### 外接 ST7789 画面偏移

修改 `board_cardputer_adv.h` 中 `TFT_Y_OFFSET`（常见 0 或 80）。

## 相关文件

| 文件 | 作用 |
|------|------|
| [`include/boards/board_cardputer_adv.h`](../include/boards/board_cardputer_adv.h) | 分辨率、帧率、增益、LCD 切换 |
| [`src/audio/cardputer_mic.cpp`](../src/audio/cardputer_mic.cpp) | ES8311 采集 |
| [`src/display/vfx_renderer.cpp`](../src/display/vfx_renderer.cpp) | VFX + Sprite 渲染 |
| [`src/main.cpp`](../src/main.cpp) | 输入、电量、主循环 |
| [`platformio.ini`](../platformio.ini) | `cardputer-adv` env |
