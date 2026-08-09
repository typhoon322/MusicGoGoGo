# 节拍同步跳舞猫 — 实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 用底鼓/军鼓频带 onset 估计 BPM，驱动顶栏猫步速与跳跃；低置信时回退 VU 假节拍；WebUI 只读显示 bpm/置信度。

**Architecture:** 新建无显示依赖的 `BeatTracker`，每帧消费 `SpectrumFrame::linear32`；经 `DisplayDriver`/`VfxDrawContext` 传入 `drawDancingCat_`。BPM 只跟 kick；snare 只做小动作。

**Tech Stack:** PlatformIO、Arduino-ESP32、现有 FFT/`SPECTRUM_BARS=30`、Adafruit ST7789（S3）、ESPAsyncWebServer

**Spec:** [`docs/superpowers/specs/2026-08-09-beat-sync-cat-design.zh.md`](../specs/2026-08-09-beat-sync-cat-design.zh.md)

**说明：** 仓库暂无主机单元测试框架；各 Task 以 `pio run` 编译 + 串口/WebUI 手工验收代替 TDD 红绿循环。

---

## 文件地图

| 文件 | 职责 |
|------|------|
| `src/dsp/beat_tracker.h` | `BeatState` + `BeatTracker` API |
| `src/dsp/beat_tracker.cpp` | kick/snare 能量、onset、BPM、脉冲衰减 |
| `src/main.cpp` | 构造 tracker、`process`、传入 render、Web 回调 |
| `src/display/display_driver.h/.cpp` | `render(..., const BeatState &)` |
| `src/display/vfx_renderer.h/.cpp` | `VfxDrawContext` 增加 beat 字段；猫动画映射 |
| `src/net/webui.h/.cpp` | `getBpm`/`getBeatConf`；JSON + 状态行 |

**频带下标（与 `kThirdOctaveEdgesHz` 对齐，边沿 20…20000）：**

- Kick **40–200 Hz**：band `3..9`（40–50 … 160–200）
- Snare **2–4 kHz**：band `21..23`（2000–2500 … 3150–4000）

---

### Task 1: 实现 `BeatTracker`

**Files:**
- Create: `src/dsp/beat_tracker.h`
- Create: `src/dsp/beat_tracker.cpp`

- [ ] **Step 1:** 创建头文件：

```cpp
#pragma once

#include <stddef.h>
#include <stdint.h>

struct BeatState {
  float bpm = 0.0f;
  float confidence = 0.0f;
  float kickPulse = 0.0f;
  float snarePulse = 0.0f;
};

class BeatTracker {
 public:
  void reset();
  // levels: SPECTRUM_BARS raw linear32; nowMs = millis()
  void process(const float *levels, size_t count, uint32_t nowMs);
  const BeatState &state() const { return state_; }

 private:
  float sumBands_(const float *levels, size_t count, size_t i0, size_t i1) const;
  void detectOnset_(float energy, float &prevEnergy, float &slowEnv, uint32_t &lastOnsetMs,
                    uint32_t nowMs, uint32_t refractoryMs, float &pulseOut, bool isKick);
  void updateBpm_(uint32_t nowMs);

  BeatState state_;
  float prevKick_ = 0.0f;
  float prevSnare_ = 0.0f;
  float slowKick_ = 0.0f;
  float slowSnare_ = 0.0f;
  uint32_t lastKickMs_ = 0;
  uint32_t lastSnareMs_ = 0;
  uint32_t lastBpmOnsetMs_ = 0;

  static constexpr size_t kIoiCap = 8;
  uint32_t ioiMs_[kIoiCap] = {};
  size_t ioiCount_ = 0;
  size_t ioiHead_ = 0;
};
```

- [ ] **Step 2:** 实现 `beat_tracker.cpp`（完整逻辑）：

```cpp
#include "dsp/beat_tracker.h"

#include <Arduino.h>
#include <math.h>
#include <string.h>

namespace {
constexpr size_t kKickLo = 3;
constexpr size_t kKickHi = 9;    // inclusive
constexpr size_t kSnareLo = 21;
constexpr size_t kSnareHi = 23;
constexpr uint32_t kKickRefractoryMs = 120;
constexpr uint32_t kSnareRefractoryMs = 80;
constexpr float kFluxMul = 1.8f;
constexpr float kEnvAttack = 0.35f;
constexpr float kEnvRelease = 0.08f;
constexpr float kPulseDecay = 0.72f;
constexpr float kBpmSmooth = 0.25f;
constexpr float kConfDecay = 0.97f;
constexpr uint32_t kIoiMinMs = 375;   // 160 BPM
constexpr uint32_t kIoiMaxMs = 857;   // 70 BPM
}  // namespace

void BeatTracker::reset() {
  state_ = BeatState{};
  prevKick_ = prevSnare_ = 0.0f;
  slowKick_ = slowSnare_ = 0.0f;
  lastKickMs_ = lastSnareMs_ = lastBpmOnsetMs_ = 0;
  ioiCount_ = ioiHead_ = 0;
  memset(ioiMs_, 0, sizeof(ioiMs_));
}

float BeatTracker::sumBands_(const float *levels, size_t count, size_t i0, size_t i1) const {
  float s = 0.0f;
  if (levels == nullptr || count == 0) {
    return 0.0f;
  }
  const size_t hi = (i1 < count) ? i1 : (count - 1);
  for (size_t i = i0; i <= hi; ++i) {
    const float v = levels[i];
    if (v > 0.0f) {
      s += v;
    }
  }
  return s;
}

void BeatTracker::detectOnset_(float energy, float &prevEnergy, float &slowEnv, uint32_t &lastOnsetMs,
                               uint32_t nowMs, uint32_t refractoryMs, float &pulseOut, bool isKick) {
  const float flux = energy > prevEnergy ? (energy - prevEnergy) : 0.0f;
  prevEnergy = energy;

  if (flux > slowEnv) {
    slowEnv += (flux - slowEnv) * kEnvAttack;
  } else {
    slowEnv += (flux - slowEnv) * kEnvRelease;
  }
  const float thr = slowEnv * kFluxMul + 1e-5f;
  const bool ready = (lastOnsetMs == 0) || (nowMs - lastOnsetMs >= refractoryMs);
  if (flux > thr && ready && energy > 0.02f) {
    lastOnsetMs = nowMs;
    pulseOut = 1.0f;
    if (isKick) {
      if (lastBpmOnsetMs_ != 0) {
        const uint32_t dt = nowMs - lastBpmOnsetMs_;
        if (dt >= kIoiMinMs && dt <= kIoiMaxMs) {
          ioiMs_[ioiHead_] = dt;
          ioiHead_ = (ioiHead_ + 1) % kIoiCap;
          if (ioiCount_ < kIoiCap) {
            ++ioiCount_;
          }
        }
      }
      lastBpmOnsetMs_ = nowMs;
      updateBpm_(nowMs);
    }
  }
}

void BeatTracker::updateBpm_(uint32_t /*nowMs*/) {
  if (ioiCount_ < 3) {
    state_.confidence *= kConfDecay;
    return;
  }
  uint32_t tmp[kIoiCap];
  for (size_t i = 0; i < ioiCount_; ++i) {
    const size_t idx = (ioiHead_ + kIoiCap - ioiCount_ + i) % kIoiCap;
    tmp[i] = ioiMs_[idx];
  }
  // insertion sort
  for (size_t i = 1; i < ioiCount_; ++i) {
    const uint32_t key = tmp[i];
    size_t j = i;
    while (j > 0 && tmp[j - 1] > key) {
      tmp[j] = tmp[j - 1];
      --j;
    }
    tmp[j] = key;
  }
  const uint32_t median = tmp[ioiCount_ / 2];
  float bpm = 60000.0f / static_cast<float>(median);
  if (bpm < 70.0f) {
    bpm = 70.0f;
  }
  if (bpm > 160.0f) {
    bpm = 160.0f;
  }

  float mean = 0.0f;
  for (size_t i = 0; i < ioiCount_; ++i) {
    mean += static_cast<float>(tmp[i]);
  }
  mean /= static_cast<float>(ioiCount_);
  float var = 0.0f;
  for (size_t i = 0; i < ioiCount_; ++i) {
    const float d = static_cast<float>(tmp[i]) - mean;
    var += d * d;
  }
  var /= static_cast<float>(ioiCount_);
  const float cv = (mean > 1.0f) ? (sqrtf(var) / mean) : 1.0f;
  float conf = 1.0f - cv * 2.0f;
  if (conf < 0.0f) {
    conf = 0.0f;
  }
  if (conf > 1.0f) {
    conf = 1.0f;
  }

  if (state_.bpm <= 1.0f) {
    state_.bpm = bpm;
  } else {
    state_.bpm += (bpm - state_.bpm) * kBpmSmooth;
  }
  state_.confidence = conf;
}

void BeatTracker::process(const float *levels, size_t count, uint32_t nowMs) {
  state_.kickPulse *= kPulseDecay;
  state_.snarePulse *= kPulseDecay;
  if (state_.kickPulse < 0.02f) {
    state_.kickPulse = 0.0f;
  }
  if (state_.snarePulse < 0.02f) {
    state_.snarePulse = 0.0f;
  }

  const float kick = sumBands_(levels, count, kKickLo, kKickHi);
  const float snare = sumBands_(levels, count, kSnareLo, kSnareHi);

  detectOnset_(kick, prevKick_, slowKick_, lastKickMs_, nowMs, kKickRefractoryMs, state_.kickPulse,
               true);
  detectOnset_(snare, prevSnare_, slowSnare_, lastSnareMs_, nowMs, kSnareRefractoryMs,
               state_.snarePulse, false);

  // 长时间无 kick → 置信度衰减
  if (lastKickMs_ != 0 && (nowMs - lastKickMs_) > 2000) {
    state_.confidence *= kConfDecay;
  }
}
```

- [ ] **Step 3:** 编译（仅确认新文件进构建；此时尚未接线也可先编通）：

```bash
export PATH="$HOME/.platformio/penv/bin:$PATH"
cd /Users/yanx/ESP32/MusicGoGoGo
pio run -e esp32-s3-dev
```

Expected: SUCCESS（若因未引用被优化掉，Task 2 接线后再编）。

- [ ] **Step 4:** Commit

```bash
git add src/dsp/beat_tracker.h src/dsp/beat_tracker.cpp
git commit -m "$(cat <<'EOF'
Add BeatTracker for kick/snare onset and BPM.

EOF
)"
```

---

### Task 2: `main` 接线 + `DisplayDriver` / `VfxDrawContext` 传 BeatState

**Files:**
- Modify: `src/display/vfx_renderer.h`（`VfxDrawContext` 增加字段）
- Modify: `src/display/display_driver.h`
- Modify: `src/display/display_driver.cpp`
- Modify: `src/main.cpp`

- [ ] **Step 1:** 在 `VfxDrawContext` 增加（Cardputer 与 S3 共用）：

```cpp
  float beatBpm = 0.0f;
  float beatConfidence = 0.0f;
  float kickPulse = 0.0f;
  float snarePulse = 0.0f;
```

- [ ] **Step 2:** 扩展 `DisplayDriver::render` 签名，在现有参数后增加：

```cpp
  void render(..., float rms, float peak, const BeatState &beat
#if defined(BOARD_CARDPUTER_ADV)
              , const MicDebugInfo &micDebug
#endif
  );
```

（若预处理器参数顺序难写：两个重载或一律最后加 `const BeatState &beat`，Cardputer 把 `micDebug` 放 beat 前保持现有顺序，**推荐**：在 `#endif` 之后统一追加 `const BeatState &beat`，避免打乱 Cardputer 实参顺序。）

推荐签名：

```cpp
  void render(const SpectrumFrame &spec, const float *smoothLevels, const float *peakLevels,
              size_t bandCount, float rms, float peak
#if defined(BOARD_CARDPUTER_ADV)
              ,
              const MicDebugInfo &micDebug
#endif
              ,
              const BeatState &beat);
```

在 `display_driver.cpp` 填入：

```cpp
  ctx.beatBpm = beat.bpm;
  ctx.beatConfidence = beat.confidence;
  ctx.kickPulse = beat.kickPulse;
  ctx.snarePulse = beat.snarePulse;
```

顶部 `#include "dsp/beat_tracker.h"`。

- [ ] **Step 3:** `main.cpp`：

```cpp
#include "dsp/beat_tracker.h"
// ...
BeatTracker beatTracker;
```

在 `spectrum.analyze` 之后：

```cpp
  beatTracker.process(frame.linear32, SPECTRUM_BARS, frameStartMs);
  const BeatState &beat = beatTracker.state();
```

更新两处 `display.render(...)`，末尾传入 `beat`。

- [ ] **Step 4:** 编译双环境：

```bash
pio run -e esp32-s3-dev -e cardputer-adv
```

Expected: SUCCESS。

- [ ] **Step 5:** Commit

```bash
git add src/main.cpp src/display/display_driver.h src/display/display_driver.cpp src/display/vfx_renderer.h
git commit -m "$(cat <<'EOF'
Wire BeatTracker into render context.

EOF
)"
```

---

### Task 3: 猫动画跟拍

**Files:**
- Modify: `src/display/vfx_renderer.cpp`（`drawDancingCat_`）

- [ ] **Step 1:** 在 `drawDancingCat_` 开头保留 VU/`kMusicOn`；增加：

```cpp
  constexpr float kConfUse = 0.45f;
  const bool beatLock = dancing && ctx.beatConfidence >= kConfUse && ctx.beatBpm >= 70.0f;
```

- [ ] **Step 2:** 替换 dancing 分支运动（保留非 dancing 的慢走逻辑）：

```cpp
  float targetHop = 0.0f;
  int walkFrame = 0;
  if (beatLock) {
    const float bpm = ctx.beatBpm;
    const float beatPeriodMs = 60000.0f / bpm;
    walkFrame = static_cast<int>(fmodf(static_cast<float>(ctx.frameMs), beatPeriodMs) /
                                 (beatPeriodMs * 0.5f)) &
                1;
    // 步速：70→~1.2px/帧，160→~2.8px/帧（按 ~30FPS 粗调）
    const float t = (bpm - 70.0f) / 90.0f;
    const float speed = 1.2f + t * 1.6f;
    catX_ += static_cast<float>(catDir_) * speed;

    if (ctx.kickPulse > 0.15f) {
      targetHop = 10.0f + ctx.kickPulse * 16.0f;
    } else if (ctx.snarePulse > 0.15f) {
      targetHop = 3.0f + ctx.snarePulse * 6.0f;
    }
  } else if (dancing) {
    // 原 VU 假节拍路径（保留现有 sin 逻辑）
    const int wf = static_cast<int>((ctx.frameMs / 110) % 2);
    walkFrame = wf;
    const float tempo = 0.018f + vu * 0.045f;
    const float beat = sinf(static_cast<float>(ctx.frameMs) * tempo);
    targetHop = (beat > 0.0f ? beat : 0.0f) * (10.0f + vu * 16.0f);
    catX_ += static_cast<float>(catDir_) * (0.4f + vu * 1.2f);
  } else {
    walkFrame = static_cast<int>((ctx.frameMs / 110) % 2);
    targetHop = walkFrame ? 1.5f : 0.0f;
    catX_ += static_cast<float>(catDir_) * 2.2f;
  }
```

- [ ] **Step 3:** 甩尾：在 `beatLock` 且 `snarePulse` 高时加大 `tailSwing`：

```cpp
  const int tailSwing = beatLock
                            ? static_cast<int>(ctx.snarePulse * 10.0f +
                                              sinf(ctx.frameMs * 0.028f) * 3.0f)
                            : (dancing ? static_cast<int>(sinf(ctx.frameMs * 0.028f) * 6.0f)
                                       : (walkFrame ? 4 : -3));
```

（删除旧的仅用 `dancing`/`walkFrame` 的 tailSwing 赋值，避免重复定义。）

- [ ] **Step 4:** 编译并刷 S3：

```bash
pio run -e esp32-s3-dev -t upload --upload-port /dev/cu.usbserial-A5069RR4
```

手工：放清晰底鼓歌 → 猫步频应变；静音 → 回慢走。

- [ ] **Step 5:** Commit

```bash
git add src/display/vfx_renderer.cpp
git commit -m "$(cat <<'EOF'
Sync header cat walk and hops to beat tracker.

EOF
)"
```

---

### Task 4: WebUI 只读 BPM

**Files:**
- Modify: `src/net/webui.h`
- Modify: `src/net/webui.cpp`
- Modify: `src/main.cpp`

- [ ] **Step 1:** `WebCallbacks` 增加：

```cpp
  float (*getBpm)() = nullptr;
  float (*getBeatConf)() = nullptr;
```

- [ ] **Step 2:** `/api/state` JSON：

```cpp
    json += ",\"bpm\":" + String(cb_.getBpm ? cb_.getBpm() : 0.0f, 1);
    json += ",\"beatConf\":" + String(cb_.getBeatConf ? cb_.getBeatConf() : 0.0f, 2);
```

状态行（在现有 `st-*` 旁）加一小段 HTML，例如：

```html
<tr><td>BPM</td><td id="st-bpm">—</td></tr>
<tr><td>节拍置信</td><td id="st-beatconf">—</td></tr>
```

JS `refresh` 内：

```javascript
    $('st-bpm').textContent = (s.bpm>1)?s.bpm.toFixed(0):'—';
    $('st-beatconf').textContent = Number(s.beatConf).toFixed(2);
```

- [ ] **Step 3:** `main.cpp` 回调：

```cpp
static float wbGetBpm() { return beatTracker.state().bpm; }
static float wbGetBeatConf() { return beatTracker.state().confidence; }
// setupWebUi:
cb.getBpm = wbGetBpm;
cb.getBeatConf = wbGetBeatConf;
```

- [ ] **Step 4:** 编译、刷机；打开 WebUI 确认 `bpm`/`beatConf` 随音乐变化。

- [ ] **Step 5:** Commit

```bash
git add src/net/webui.h src/net/webui.cpp src/main.cpp
git commit -m "$(cat <<'EOF'
Expose BPM and beat confidence in WebUI.

EOF
)"
```

---

### Task 5: 验收与规格状态

**Files:**
- Modify: `docs/superpowers/specs/2026-08-09-beat-sync-cat-design.zh.md`（状态改为已实现）
- Optional: `docs/vfx-effects-2026-08-09.zh.md` 一行提及节拍猫（若该文档描述顶栏猫）

- [ ] **Step 1:** 对照规格验收：

1. 已知 BPM 曲目：WebUI `bpm` 约 ±10，猫步频肉眼跟拍  
2. kick 跳 > snare 点缀  
3. 说话/静音不狂跳  
4. FPS 仍约 25–30  
5. `/api/state` 含 `bpm`、`beatConf`

- [ ] **Step 2:** 规格状态改为：`状态：已实现`。

- [ ] **Step 3:** Commit docs

```bash
git add docs/superpowers/specs/2026-08-09-beat-sync-cat-design.zh.md
git commit -m "$(cat <<'EOF'
Mark beat-sync cat spec as implemented.

EOF
)"
```

---

## 规格覆盖自检

| 规格项 | Task |
|--------|------|
| BeatTracker kick+snare | 1 |
| BPM 仅 kick、70–160、置信度 | 1 |
| main + render 接线 | 2 |
| 猫：BPM 步速 + kick 大跳 + snare 小跳/甩尾 | 3 |
| 低置信回退 VU | 3 |
| WebUI 只读 bpm/beatConf | 4 |
| 不写 NVS、无踩镲、无屏上数字 | 遵守非目标 |
| 验收 | 5 |

## 类型一致性

- 结构体名：`BeatState`（非 BeatInfo）
- JSON 字段：`bpm`、`beatConf`
- Context：`beatBpm`、`beatConfidence`、`kickPulse`、`snarePulse`
- 置信使用阈值：`kConfUse = 0.45f`
