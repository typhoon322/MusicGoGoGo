# MusicGoGoGo 效果更新说明（2026-08-09）

本版在 ESP32-S3 主目标上完成：显示问题修复、WebUI 频率标注开关与 12 段乐器参考表，以及四个新视觉模式。

## 修了什么

1. **Rainbow 频率数字** — 切模式时强制刷新标签缓存，Rainbow 与 Bars 一样能显示柱底 kHz。
2. **Waterfall 只占上半屏** — 历史仍按 80 行存储，推屏时纵向拉伸铺满绘图区（不额外吃大量 RAM）。
3. **Mirror 中线** — 去掉中间那根水平线。
4. **柱子间隙** — Bars / Log / Rainbow / Bounce / Glow / VU 迷你柱默认 **3px** 间隙。
5. **LinePeaks 闪动** — 本版刻意不改（已知问题，等以后整帧缓冲再消）。

## 新效果模式

| 编号 | 名称 | 说明 |
|------|------|------|
| 0 | Bars 32 | 32 柱线性频谱 |
| 1 | Log 12 | 12 段对数频谱 |
| 2 | Mirror | 中心对称镜面 |
| 3 | VU Meter | VU + 迷你频谱 |
| 4 | Waterfall | 瀑布图（已铺满高度） |
| 5 | Rainbow | 彩虹色相滚动柱 |
| 6 | Line Peaks | 折线峰值（仍可能闪） |
| 7 | Bounce | 弹性柱，落回带弹簧阻尼 |
| 8 | Dot | 星点：位置与大小随电平 |
| 9 | Glow | 辉光柱，顶端渐隐拖尾 |
| 10 | Ring | 环形放射，中心 VU |

切换方式：旋转编码器 / 串口 `m 7`～`m 10` / WebUI 效果按钮。

## WebUI

连接板子 AP 后打开控制页：

- **显示频率标注** — 开关 TFT 柱底 kHz；切换时会整区刷新，避免残影。
- **12 段对数频带参考** — 页面内只读表（约 Hz + 听感/乐器说明），不画在屏幕上。

## 串口命令（节选）

```
m / mode [n]           切模式（0–10）
fl / labels [on|off]   频率标注开/关
al [on|off]            自动电平 AGC
g / gain [val]         麦克风增益（可低至 0.05）
pot [on|off]           电位器是否接管增益
help                   完整帮助
```

## 背光

S3 背光改走 LEDC PWM，WebUI「背光亮度」滑块应可正常调节。

## 验收建议

- [ ] Rainbow 有频率数字；WebUI / `fl off` 后消失且无残影
- [ ] Waterfall 铺满绘图区高度
- [ ] Mirror 无中线；柱间隙约 3px
- [ ] Bounce / Dot / Glow / Ring 可切换、可轮播
- [ ] WebUI 能看到 12 段参考表

## 相关文档

- 设计：`docs/superpowers/specs/2026-08-09-vfx-effects-webui-design.md`
- 实现计划：`docs/superpowers/plans/2026-08-09-vfx-effects-webui.md`
