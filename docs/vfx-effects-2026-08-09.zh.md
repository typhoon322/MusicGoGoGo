# MusicGoGoGo 效果更新说明（2026-08-09）

本版在 ESP32-S3 主目标上完成：显示问题修复、WebUI 频率标注开关与 12 段乐器参考表，以及四个新视觉模式。

## 修了什么

1. **Rainbow 频率数字** — 切模式时强制刷新标签缓存。
2. **Mirror 中线已去掉**；柱间隙与柱宽见下。
3. **柱布局** — Bars / Log / Rainbow / Bounce / Glow / Mirror：**间隙 3px**，柱宽按屏宽均分铺满。
4. **Waterfall 已移除**（不好看）。
5. **LinePeaks 闪动** — 本版刻意不改。
6. **Glow** — 改为增量绘制，避免整柱擦除闪动。
7. **频率标注开关** — 延后到渲染线程再清屏，避免 WebUI 异步切换导致柱子下半截消失。
8. **背光** — 修复 WebUI 参数名 `bright`→`brightness` 不匹配。
9. **设置持久化** — 模式/增益/亮度/轮播/帧间隔/attack·decay·peak/AGC/频率标注/电位器开关等写入 NVS；重启、重新烧录 app 后仍保留（整片擦除 Flash 会丢）。
10. **静音回落** — 音乐停下后 AGC 不再把底噪抬满；柱子快速落下。
11. **30 段 1/3 倍频程** — 20–25 … 16–20kHz（Bars/Mirror/Rainbow/…）；**Log 12** 仍为 12 段倍频程。采样率 44.1kHz + FFT 2048。
12. **频率标注默认关闭**。
13. **三段 EQ** — WebUI / 串口 `eq` 可调低/中/高增益（0–2，默认 1.0 平坦）；已去掉内置低频硬衰减。

## 效果模式

| 编号 | 名称 | 说明 |
|------|------|------|
| 0 | Bars 30 | 30 段 1/3 倍频程（3px 间隙，柱宽均分） |
| 1 | Log 12 | 12 段倍频程宽柱 |
| 2 | Mirror | 中心对称镜面（30 段） |
| 3 | VU Meter | VU + 迷你频谱 |
| 4 | Rainbow | 彩虹色相滚动柱（30 段） |
| 5 | Line Peaks | 折线峰值（仍可能闪） |
| 6 | Bounce | 弹性柱，落回带弹簧阻尼 |
| 7 | Dot | 星点：位置与大小随电平 |
| 8 | Glow | 辉光柱，顶端渐隐拖尾 |
| 9 | Ring | 环形放射，中心 VU |

切换方式：旋转编码器 / 串口 `m 6`～`m 9` / WebUI 效果按钮。

## WebUI

连接板子 AP 后打开控制页：

- **显示频率标注** — 开关 TFT 柱底 kHz；切换时会整区刷新，避免残影。
- **频带参考** — 页面内 30 段 Hz 表；Log 12 仍用倍频程划分。
- **背光亮度** — 滑块应可调节（串口也可用 `bl 128` 测试）。

## 串口命令（节选）

```
m / mode [n]           切模式（0–9）
fl / labels [on|off]   频率标注开/关
bl / bright [0-255]    背光亮度
al [on|off]            自动电平 AGC
eq [b m t]             低/中/高 EQ 增益 0–2（默认 1 1 1）
g / gain [val]         麦克风增益（可低至 0.05）
pot [on|off]           电位器是否接管增益
cfg / cfg save|reset   查看 / 强制保存 / 恢复默认
help                   完整帮助
```

改过的设置约 0.5s 后自动写入 NVS；也可用 `cfg save` 立刻保存。
## 验收建议

- [ ] Bars 30：铺满屏宽；间隙约 3px；停乐后柱子落下
- [ ] Log 12：仍为 12 根更宽柱
- [ ] Rainbow / Mirror / Bounce / Glow 用 30 段
- [ ] WebUI / `fl off` 后整柱正常
- [ ] WebUI / `bl` 可调背光
## 相关文档

- 设计：`docs/superpowers/specs/2026-08-09-vfx-effects-webui-design.md`
- 实现计划：`docs/superpowers/plans/2026-08-09-vfx-effects-webui.md`
