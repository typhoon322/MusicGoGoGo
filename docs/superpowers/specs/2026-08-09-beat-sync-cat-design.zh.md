# 节拍同步跳舞猫（底鼓 + 军鼓）

日期：2026-08-09  
状态：已实现  
目标：以 ESP32-S3 为主（顶栏猫）；Cardputer ADV 可编译 tracker，若无顶栏猫路径可不绘制

## 目标

用房间麦克风音频，从底鼓频带 onset 估计节拍速度（BPM），驱动顶栏猫的走路速度；底鼓触发大跳，军鼓触发小幅舞蹈动作。节拍置信度低或环境安静时，回退到当前的 VU 慢走 + 假跳。

## 已锁定决策

| 主题 | 选择 |
|------|------|
| 运动模型 | **C**：BPM 定走路速度；底鼓 onset → 大跳 |
| 低置信 / 安静 | **A**：回退现有 VU 慢走 + 假跳 |
| 检测方案 | **2**：底鼓 + 军鼓双轨（BPM 只跟底鼓） |
| FFT | 复用现有频谱，不开第二次 FFT |
| BPM 写入 NVS | 否（仅实时估计） |
| WebUI | `/api/state` 只读 `bpm`、`beatConf`；本轮不做门限滑条 |
| 屏上 BPM 数字 | 不做 |
| 踩镲轨 | 不做 |

## 架构

```
SpectrumAnalyzer（现有 FFT + 30 线性频带）
        │
        ▼
BeatTracker::process(linear32[30])
   ├─ 底鼓能量  ~40–200 Hz  → 包络 / 自适应门限 → kick onset
   │                                ├─ IOI 中位数 → bpm（平滑）
   │                                └─ kickPulse（衰减）
   └─ 军鼓能量  ~2–4 kHz    → 包络 / 自适应门限 → snare onset
                                    └─ snarePulse（衰减）
        │
        ▼
main → display.render(..., BeatState)
        │
        ▼
VfxRenderer::drawDancingCat_
   高置信 → 步速 ∝ bpm；kickPulse → 大跳；snarePulse → 小跳 + 甩尾
   低置信 / 安静 → 现有 VU 路径
```

## 算法

### 频带求和（1/3 倍频程 `linear32`）

固定频带下标范围，与 `spectrum_analyzer.cpp` 中 `kThirdOctaveEdgesHz` 对齐：

- **底鼓（Kick）**：中心落在 **40–200 Hz** 的频带能量求和。使用 `SpectrumFrame::linear32` 的**原始帧电平**（不要用柱状图 EMA 平滑后的值），以免门限变钝。
- **军鼓（Snare）**：**2–4 kHz**（只用脆头；故意不用 150–300 Hz 鼓体，减少人声串扰）。

具体下标常量写在 `beat_tracker.cpp`，旁注与边沿表对应关系。

### Onset 检测（每个频带组）

每帧（约 30 Hz hop）：

1. `flux = max(0, energy - prevEnergy)`。
2. 用近期 flux 维护慢自适应门限（均值 × 倍率，或包络跟随）。
3. 当 `flux > threshold` 且已过不应期时触发 onset：
   - 底鼓不应期 ≈ **120 ms**
   - 军鼓不应期 ≈ **80 ms**

### BPM（仅底鼓）

- 保留最近约 **8** 个合法底鼓 onset 间隔（IOI）。
- 只接受对应约 **70–160 BPM** 的间隔（`interval_ms ∈ [375, 857]`）。
- `bpm = 60000 / median(IOI)`，再一阶平滑。
- **置信度**：近期 ≥3 个 IOI 且变异系数低则高；否则向 0 衰减。
- 使用/展示的 BPM 钳位在 **70–160**。

### 输出（`BeatState`）

| 字段 | 含义 |
|------|------|
| `bpm` | 平滑估计（未知时可为上次有效值或 0） |
| `confidence` | 0…1 |
| `kickPulse` | 0…1，底鼓 onset 置 1，每帧指数衰减 |
| `snarePulse` | 军鼓同理 |

## 猫动画映射

当 `confidence >= kConfUse`（例如 0.45）且 VU 判定有音乐时，替换硬编码的 `sin(frameMs * tempo)` 假节拍路径：

| 输入 | 猫行为 |
|------|--------|
| `bpm` | 水平步速按约 70（慢）～160（快）缩放；腿部相位周期 = `60000/bpm`；默认 **一拍一个走路周期** |
| `kickPulse` | 大幅垂直起跳（优先于军鼓跳） |
| `snarePulse` | 小幅颠一下 + 更用力甩尾 |
| 底鼓与军鼓同时 | 大跳 + 甩尾；垂直目标不叠加两次 |

当 `confidence < kConfUse` 或 VU 低于现有安静门限：行为与现在相同（慢走 + VU 假跳）。

## 模块边界 / 文件

| 部分 | 改动 |
|------|------|
| `src/dsp/beat_tracker.h/.cpp` | **新建** — 纯 DSP，不依赖显示/WiFi |
| `src/main.cpp` | 构造 tracker；`spectrum.analyze` 之后 `process`；把状态传入 render |
| `src/display/display_driver.*` | 渲染上下文 / 签名增加 `BeatState`（或薄 POD） |
| `src/display/vfx_renderer.*` | `drawDancingCat_` 消费 beat 字段 |
| `src/net/webui.*` | JSON 增加 `bpm`、`beatConf`；可选状态行文字 |
| Settings / NVS | beat 相关**不改** |

## 错误处理 / 边界情况

- 静音或 onset 稀疏 → 置信度下降 → 行为 A（VU 回退）。
- 越界 IOI 丢弃，不更新 BPM。
- 麦底噪 / 持续低音：靠自适应门限 + 不应期压制连发；置信度仍低时由回退保护猫不狂跳。
- CPU：每帧少量浮点求和 + 小环缓冲；不额外分配 FFT。

## 本轮非目标

- 踩镲 / 完整鼓分离
- WebUI 可调 onset 门限
- TFT 上显示 BPM / 置信度数字
- 机器学习或离线分轨

## 验收标准

1. 中等音量、底鼓清晰的音乐下，数秒内估计 BPM 与已知曲目速度大致相差 ±10 以内，猫的步频肉眼跟得上。
2. 底鼓跳明显大于军鼓点缀；军鼓有可辨认的小动作/甩尾。
3. 说话、静音、环境嘶声：猫回到当前 VU 慢走/假跳，不乱跳。
4. S3 帧率可接受（相对当前约 25–30 FPS 目标无明显大回退）。
5. WebUI `/api/state` 能报 `bpm`、`beatConf` 便于调试。

## 测试说明

- 串口或 WebUI：播放节拍器或已知 BPM 曲目时观察 `bpm` / `beatConf`。
- 验证不应期：单次底鼓不双触发。
- 音乐结束后安静：置信度衰减，猫回到慢走。

---

英文对照（非优先）：[`2026-08-09-beat-sync-cat-design.md`](./2026-08-09-beat-sync-cat-design.md)
