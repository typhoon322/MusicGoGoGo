# VFX Polish + New Modes Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fix Rainbow labels / Waterfall height / Mirror midline / 3px bar gaps; add WebUI freq-label toggle + 12-band guide; add Bounce, Dot, Glow, Ring modes.

**Architecture:** Extend `VfxMode` and `VfxRenderer` draw paths; gate S3 bar labels with `showFreqLabels_`; stretch waterfall vertically without growing HISTORY; wire WebUI/serial to the same flag. Defer LinePeaks flicker.

**Tech Stack:** PlatformIO, Arduino-ESP32, Adafruit ST7789 (S3), M5GFX (Cardputer), ESPAsyncWebServer

**Spec:** `docs/superpowers/specs/2026-08-09-vfx-effects-webui-design.md`

**Post-ship:** Write Chinese summary MD at `docs/vfx-effects-2026-08-09.zh.md`

---

## File map

| File | Responsibility |
|------|----------------|
| `include/vfx.h` | Enum + mode count |
| `src/display/vfx_names.cpp` | Mode display names |
| `src/display/vfx_renderer.h/.cpp` | Draw + label flag + new modes + waterfall stretch |
| `src/display/display_driver.h/.cpp` | Expose `setShowFreqLabels` / getter; forward to renderer |
| `src/net/webui.h/.cpp` | API + HTML toggle + 12-band table |
| `src/main.cpp` | Web callbacks + serial `labels` + help text |
| `README.md` | Brief mention of new modes / labels command |

---

### Task 1: Enum + names + gap + mirror + label cache reset

**Files:**
- Modify: `include/vfx.h`
- Modify: `src/display/vfx_names.cpp`
- Modify: `src/display/vfx_renderer.cpp` (`resetBarCache`, `drawBars_` gap, `drawMirror_`)
- Modify: `src/display/vfx_renderer.h` (declare `setShowFreqLabels` / `showFreqLabels` later in Task 2 — for Task 1 only reset `lastBarLabelCount_`)

- [ ] **Step 1:** Add modes to enum after `LinePeaks`:

```cpp
  LinePeaks,
  Bounce,
  Dot,
  Glow,
  Ring,
  Count
```

- [ ] **Step 2:** Add names `"Bounce"`, `"Dot"`, `"Glow"`, `"Ring"` in `vfx_names.cpp`.

- [ ] **Step 3:** In `resetBarCache()`, set `lastBarLabelCount_ = -1`.

- [ ] **Step 4:** S3 `drawBars_`: change `const int gap = 0` to `const int gap = 3`. Cardputer bar gap may stay 0 (small screen) unless shared — per spec bar-family on S3 uses 3; Cardputer keep 0.

- [ ] **Step 5:** Remove `GFX->drawFastHLine(kMarginL, midY, areaW, kAccent);` from `drawMirror_`.

- [ ] **Step 6:** Build both envs:

```bash
~/.platformio/penv/bin/pio run -e esp32-s3-dev -e cardputer-adv
```

Expected: SUCCESS (new modes may fall through to default bars until Task 4).

- [ ] **Step 7:** Commit `feat: add VfxMode enum for Bounce/Dot/Glow/Ring; fix gap/mirror/label cache`

---

### Task 2: Frequency label toggle plumbing

**Files:**
- Modify: `src/display/vfx_renderer.h`
- Modify: `src/display/vfx_renderer.cpp`
- Modify: `src/display/display_driver.h`
- Modify: `src/display/display_driver.cpp`

- [ ] **Step 1:** Add to `VfxRenderer`:

```cpp
  void setShowFreqLabels(bool on);
  bool showFreqLabels() const { return showFreqLabels_; }
private:
  bool showFreqLabels_ = true;
```

`setShowFreqLabels`: if value changes, set `showFreqLabels_`, call `clearPlotArea()` + `resetBarCache()`.

- [ ] **Step 2:** In S3 `drawBars_`, compute:

```cpp
  const int gap = 3;
  constexpr int kBarLabelH = 26;
  const int barBottom = showFreqLabels_ ? (bottom - kBarLabelH) : bottom;
```

Only draw label strip / labels when `showFreqLabels_` is true. When false, skip label block entirely (`lastBarLabelCount_` stays -1 or force -1 when turning off so re-enable redraws).

- [ ] **Step 3:** `DisplayDriver` forwards:

```cpp
  void setShowFreqLabels(bool on);
  bool showFreqLabels() const;
```

to the owned `VfxRenderer` instance (find existing member — currently renderer is likely local/static inside cpp; add accessor matching existing pattern).

- [ ] **Step 4:** Build both envs. Commit `feat: gate S3 bar frequency labels behind showFreqLabels flag`

---

### Task 3: Waterfall vertical stretch

**Files:**
- Modify: `src/display/vfx_renderer.cpp` `drawWaterfall_`

- [ ] **Step 1:** Keep writing `rows = min(areaH, VFX_WATERFALL_HISTORY)` into `waterfallFb_` as today.

- [ ] **Step 2:** When `areaH > rows`, push with stretch. Preferred approach for Adafruit path:

```cpp
  for (int y = 0; y < areaH; ++y) {
    const int srcY = y * rows / areaH;
    // drawRGBBitmap one scanline: use pointer into waterfallFb_ + srcY * TFT_WIDTH
    GFX->drawRGBBitmap(0, top + y, waterfallFb_ + srcY * TFT_WIDTH, TFT_WIDTH, 1);
  }
```

Cardputer `pushImage` equivalent: same loop or `pushImage` per line.

- [ ] **Step 3:** Build S3, flash optional. Commit `fix: stretch waterfall to fill plot height`

---

### Task 4: drawBounce_ / drawDot_ / drawGlow_ / drawRing_

**Files:**
- Modify: `src/display/vfx_renderer.h`
- Modify: `src/display/vfx_renderer.cpp` (declarations, implementations, `draw` + `drawPlotMode_` switches)
- Modify: `src/main.cpp` serial help mode list

**State members (S3 incremental):**

```cpp
  float bounceVel_[64] = {};
  float bounceH_[64] = {};
  int prevDotY_[64] = {};
  int prevDotR_[64] = {};
  int prevSpokeLen_[64] = {};
```

- [ ] **Step 1: Bounce** — each frame spring peak/height toward `levels[i]`; draw like bars with gap 3; white peak cap.

Spring sketch:

```cpp
  float target = level * barAreaH;
  bounceVel_[i] += (target - bounceH_[i]) * 0.35f;
  bounceVel_[i] *= 0.75f;
  bounceH_[i] += bounceVel_[i];
```

- [ ] **Step 2: Dot** — erase prev circle (fillCircle bg), draw new; radius `1 + level * 6`; y from bottom.

- [ ] **Step 3: Glow** — bar fill + for `k = 1..3` draw tip rect height 2 with color darkened by `k`.

- [ ] **Step 4: Ring** — `cx, cy` center; for each i angle; erase old spoke length then draw new; center fill based on `ctx.vu`.

- [ ] **Step 5:** Wire switch cases in `draw` and Cardputer `drawPlotMode_`.

- [ ] **Step 6:** Update `printSerialHelp` mode list to include 7–10.

- [ ] **Step 7:** Build both. Commit `feat: add Bounce Dot Glow Ring VFX modes`

---

### Task 5: WebUI + serial + main callbacks

**Files:**
- Modify: `src/net/webui.h` (callbacks)
- Modify: `src/net/webui.cpp` (state/control/HTML)
- Modify: `src/main.cpp`

- [ ] **Step 1:** Add to `WebCallbacks`:

```cpp
  bool (*getFreqLabels)() = nullptr;
  void (*setFreqLabels)(bool) = nullptr;
```

- [ ] **Step 2:** `/api/state` include `"freqLabels":` + HTML switch `id="flabels"` bound like `alevel`.

- [ ] **Step 3:** Insert 12-row Chinese reference table (Hz approx + instrument copy from spec).

- [ ] **Step 4:** `wbGetFreqLabels` / `wbSetFreqLabels` in `main.cpp`; wire in `setupWebUi`.

- [ ] **Step 5:** Serial:

```
labels [on|off]   show/hide TFT frequency labels
```

Parse before single-char shortcuts that could collide (`l` alone OK if exact `labels` / `l` with care — use `labels` / `fl` to avoid clashing). Prefer command `labels` and short `fl`.

- [ ] **Step 6:** Build both. Commit `feat: WebUI/serial frequency label toggle and 12-band guide`

---

### Task 6: Verify + Chinese doc

- [ ] **Step 1:** `pio run -e esp32-s3-dev -e cardputer-adv` — SUCCESS

- [ ] **Step 2:** Flash S3; smoke: modes 0–10, `fl off`/`fl on`, waterfall full height, mirror no line, gaps visible

- [ ] **Step 3:** Write `docs/vfx-effects-2026-08-09.zh.md` summarizing features, serial/WebUI usage, mode list in Chinese

- [ ] **Step 4:** Commit docs. Optionally update README modes section briefly.

---

## Spec coverage checklist

| Spec item | Task |
|-----------|------|
| Rainbow labels via cache reset | 1 |
| Waterfall stretch | 3 |
| Mirror no midline | 1 |
| Gap 3px | 1 |
| Freq label toggle + full refresh | 2, 5 |
| 12-band WebUI table | 5 |
| Bounce/Dot/Glow/Ring | 4 |
| LinePeaks deferred | — |
| Chinese MD after done | 6 |

## Notes for implementer

- Existing uncommitted WIP (gain floor, LEDC backlight, `al` command, fixed bar colors) should be folded into the same working tree and included in logical commits — do not revert those fixes.
- Do not wrap Adafruit draws in outer `startWrite`/`endWrite` on S3.
