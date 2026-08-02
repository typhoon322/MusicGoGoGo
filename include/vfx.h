#pragma once

#include <stddef.h>
#include <stdint.h>

// Visual effect modes (参考 redchenjs / ESP32-AudioInI2S / G6EJD 常见效果)

enum class VfxMode : uint8_t {
  Bars32 = 0,    // 32 柱线性 + 渐变 + peak 点
  Log12,         // 12 段对数频谱
  Mirror,        // 中心对称镜面频谱
  VuMeter,       // VU 电平表 + 迷你频谱
  Waterfall,     // 瀑布图 / 频谱图
  Rainbow,       // 彩虹色柱 + 色相滚动
  LinePeaks,     // 折线峰值
  Count
};

const char *vfxModeName(VfxMode mode);
