# 增加歌曲 BPM 检测（精简实现说明）

> 对应：FFT 后独立 Beat Detector 分支，不重构现有 I2S / FFT / 30 段频谱架构。

## 数据流

```
I2S → FFT → SpectrumFrame.linear32
              ├─ BandProcessor → Spectrum Display（不变）
              └─ BeatTracker → BPM / BEAT ● / LED / 猫跳
```

## 算法（`src/dsp/beat_tracker.*`）

1. **Kick 能量**：1/3 倍频程 bands `4..8` ≈ 50–160 Hz 求和
2. **Flux**：`current_low - previous_low`（只取正）
3. **可选 Spectral Flux**：全频段正差分加权叠加
4. **动态阈值**：`avgEnergy * 1.5 + floor`
5. **静音门**：平均低能量过低不触发
6. **Cooldown**：150 ms
7. **BPM**：最近最多 20 个合法 IOI（375–1000 ms）取**中位数**，再轻度平滑；置信度用 IOI 变异系数

## 显示

- 左上角：`BPM:128` / `BPM:--`
- 下一行：`BEAT:` + 实心圆闪烁
- RGB：beat 白闪；弱 bass/high 底色

## 验收自测

| 曲目 | 期望 |
|------|------|
| 60 / 90 / 120 / 140 BPM | 数字稳定跟随，● 跟 kick |
| 无音乐 / 仅拨弦 | 不乱闪、BPM 回 `--` |
| 频谱模式切换 | 柱状图表现与改前一致，FPS ≥ 25 |
