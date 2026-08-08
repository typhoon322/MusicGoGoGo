# VFX polish + four new modes + WebUI frequency guide

Date: 2026-08-09  
Status: approved for planning  
Target: ESP32-S3 (primary); Cardputer ADV where paths already share code

## Goal

Ship one firmware pass that (1) fixes several S3 display issues, (2) adds WebUI control for on-screen frequency labels plus a 12-band instrument/voice reference table, and (3) adds Bounce / Dot / Glow / Ring visual modes. LinePeaks tear/flicker is explicitly deferred.

## Decisions (locked)

| Topic | Choice |
|-------|--------|
| Delivery | Single pass (fixes + WebUI + all four new modes) |
| Bar gap | 3px for bar-family modes |
| Freq guide depth | 12 log-band rows in WebUI only |
| LinePeaks flicker | Defer (no change this pass) |
| Waterfall full height | Nearest-neighbor vertical stretch; keep `VFX_WATERFALL_HISTORY=80` |
| New effects visual | Bounce / Dot / Glow / Ring as previously mockup’d |

## Scope

### Fixes

1. **Rainbow frequency labels** — Rainbow already calls `drawBars_`; ensure label cache resets on mode change (`lastBarLabelCount_ = -1` in `resetBarCache`) so labels redraw after area clear.
2. **Waterfall only upper half** — History stays at 80 rows; when pushing to TFT, map each destination row `y ∈ [0, areaH)` to `src = y * rows / areaH` so the plot area is filled.
3. **Mirror center line** — Remove `drawFastHLine` at `midY`.
4. **Bar gap** — Use `gap = 3` in bar-family layouts (Bars32, Log12, Rainbow, Bounce, Glow, VU mini-bars). Dot / Ring / LinePeaks / Waterfall / Mirror layout unchanged except Mirror keeps gap 0 or shared layout as today (Mirror currently gap 0; leave Mirror at 0 unless shared helper forces otherwise — **Mirror stays gap 0** for dense reflection look).

### WebUI

1. Toggle **show frequency labels** (`freqLabels` bool, default `true` on S3).
2. On toggle: `clearPlotArea()` + `resetBarCache()` so the next frame is a full plot refresh (no stale kHz digits or gap ghosts).
3. Read-only **12-band log table** (Chinese): approximate Hz range + instrument/voice description. Not drawn on TFT.
4. Optional serial alias `labels on|off` wired to the same flag.

### New `VfxMode` values (after `LinePeaks`)

| Enum | Name | Behavior |
|------|------|----------|
| Bounce | Bounce | Bars with spring-damped peak caps |
| Dot | Dot | One circle per band; Y and radius from level |
| Glow | Glow | Bars + 2–3 fading tip segments (intentional trail) |
| Ring | Ring | Radial spokes from center; center VU readout |

Update `vfxModeName`, WebUI mode list, serial help (`m 0..Count-1`), encoder wrap.

## Architecture

```
WebUI / serial ──► showFreqLabels_ ──► VfxRenderer::drawBars_ (reserve label strip + draw once)
                     │
                     └─ on change ► clearPlotArea + resetBarCache

VfxMode switch ──► drawBars_ / drawMirror_ / drawWaterfall_ / drawLinePeaks_
                   drawBounce_ / drawDot_ / drawGlow_ / drawRing_
```

### Frequency labels

- S3 only (Cardputer sprite path unchanged unless trivial).
- When `showFreqLabels_` is false: `barBottom = bottom` (no reserved strip); do not draw labels.
- When true: reserve `kBarLabelH` and draw vertical kHz labels when `lastBarLabelCount_ != count`.

### Bounce

- Per-bar state: displayed height + velocity (or peak-only spring). Attack follows audio; decay uses spring toward `levels[i]`.
- Incremental fill like current bars; peak cap redrawn each frame when moved.

### Dot

- Full clear of plot (or erase previous circles) each frame — count is 32, cheap enough vs LinePeaks polylines.
- Color via existing `rainbowColor_` by index.

### Glow

- Draw solid bar, then 2–3 stacked short rects above tip with decreasing brightness/alpha (RGB565 darkened copies).
- Gap = 3.

### Ring

- Center at plot midpoint; angle `i * 2π / count`; spoke length from level.
- Draw by erasing previous spoke endpoints or clearing plot on mode entry then redrawing spokes (prefer clear-on-mode + redraw all spokes each frame if cost OK; else store prev length).
- Small VU text or colored disc at center from `ctx.vu`.

### Waterfall stretch

- Keep framebuffer height = `rows = min(areaH, HISTORY)` historically filled as now, **or** keep writing only `HISTORY` rows into `waterfallFb_` and when calling `drawRGBBitmap`, either:
  - draw row-by-row with vertical repeat, or
  - build stretched scanlines into a temporary row buffer.
- Must not increase `waterfallHistory_` float storage beyond current HISTORY × BINS.

## WebUI API

- `GET /api/state`: add `"freqLabels":0|1`
- `GET /api/control?freqLabels=0|1`: set flag + trigger full plot refresh
- HTML: switch under 屏幕 card; static 12-row reference table below

### 12-band copy (implement with Hz from `logEdgeHz`)

Bands approximate: kick/bass foundation → bass body → male low → chest/guitar low → voice fundamental → voice body → presence → clarity/strings → bite → cymbals onset → air → ultra air / hiss. Exact Hz labels computed at render from analyzer edges.

## Out of scope

- LinePeaks tear fix / full-frame sprite on S3
- Enabling OPI PSRAM / offscreen canvas for S3
- Drawing instrument names on the TFT
- Changing Cardputer-only UX beyond shared enum names

## Acceptance

- [ ] Rainbow shows kHz labels when labels ON; none when OFF; toggle leaves no ghosts
- [ ] Waterfall fills plot height (not ~80px band)
- [ ] Mirror has no center H-line
- [ ] Bar-family gap visibly ~3px
- [ ] Bounce / Dot / Glow / Ring selectable via encoder, serial `m`, and WebUI
- [ ] WebUI shows 12-band instrument table
- [ ] LinePeaks unchanged (still may flicker)
- [ ] Both `esp32-s3-dev` and `cardputer-adv` build

## Risks

- Ring/Dot full redraw may cost a few ms; profile if FPS drops below ~20
- Glow tip segments must not leave trails when labels toggle or mode switches — rely on `areaInit_` / `clearPlotArea`
- Waterfall stretch may look blocky; acceptable for this pass
