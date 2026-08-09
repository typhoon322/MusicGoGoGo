# Beat-sync dancing cat (kick + snare)

> **Preferred Chinese version:** [`2026-08-09-beat-sync-cat-design.zh.md`](./2026-08-09-beat-sync-cat-design.zh.md)

Date: 2026-08-09  
Status: approved for planning  
Target: ESP32-S3 primary (header cat); Cardputer ADV may compile the tracker but need not draw the cat if it has no header cat path

## Goal

Estimate tempo from room-mic audio using kick-band onsets, drive the header cat’s walk speed from BPM, trigger big hops on kick and small dance accents on snare. When beat confidence is low or the room is quiet, fall back to today’s VU-driven stroll + fake hop.

## Decisions (locked)

| Topic | Choice |
|-------|--------|
| Motion model | **C**: BPM sets walk speed; kick onset → big hop |
| Low-confidence / quiet | **A**: fall back to current VU slow-walk + fake hop |
| Detection approach | **2**: kick + snare dual track (BPM from kick only) |
| FFT | Reuse existing spectrum; no second FFT |
| Persist BPM to NVS | No (live estimate only) |
| WebUI | Read-only `bpm` + `beatConf` in `/api/state`; no threshold sliders this pass |
| On-screen BPM numeral | Out of scope |
| Hi-hat track | Out of scope |

## Architecture

```
SpectrumAnalyzer (existing FFT + 30 linear bands)
        │
        ▼
BeatTracker::process(linear32[30])
   ├─ kick energy  ~40–200 Hz  → envelope / adaptive gate → kick onset
   │                                ├─ IOI median → bpm (smoothed)
   │                                └─ kickPulse (decaying)
   └─ snare energy ~2–4 kHz    → envelope / adaptive gate → snare onset
                                    └─ snarePulse (decaying)
        │
        ▼
main → display.render(..., BeatState)
        │
        ▼
VfxRenderer::drawDancingCat_
   high confidence → walk ∝ bpm; kickPulse → big hop; snarePulse → small hop + tail flick
   low confidence / quiet → existing VU path
```

## Algorithm

### Band sums (1/3-octave `linear32`)

Use fixed band-index ranges matching `kThirdOctaveEdgesHz` in `spectrum_analyzer.cpp`:

- **Kick**: bands whose centers fall in **40–200 Hz** (sum of those bins’ post-EQ levels before or after display smoothing — prefer **raw frame levels** from `SpectrumFrame::linear32` so gate is not slowed by bar EMA).
- **Snare**: bands in **2–4 kHz** (crack only; intentionally skip 150–300 Hz body to reduce vocal bleed).

Exact index constants live in `beat_tracker.cpp` and are documented in comments next to the edge table.

### Onset detection (per band group)

Each frame (~30 Hz hop):

1. `flux = max(0, energy - prevEnergy)`.
2. Maintain a slow adaptive threshold from recent flux (mean × multiplier, or envelope follower).
3. Fire onset if `flux > threshold` and refractory elapsed:
   - Kick refractory ≈ **120 ms**
   - Snare refractory ≈ **80 ms**

### BPM (kick only)

- Keep a small ring of recent kick inter-onset intervals (IOIs), e.g. last **8**.
- Accept IOIs that map to about **70–160 BPM** (`interval_ms ∈ [375, 857]`).
- `bpm = 60000 / median(IOI)`; apply one-pole smooth toward that value.
- **Confidence**: high when ≥3 recent IOIs are consistent (low coefficient of variation); otherwise decay toward 0.
- Clamp displayed/used BPM to **70–160**.

### Outputs (`BeatState`)

| Field | Meaning |
|-------|---------|
| `bpm` | Smoothed estimate (or last good / 0 when unknown) |
| `confidence` | 0…1 |
| `kickPulse` | 0…1, set to 1 on kick onset, exponential decay each frame |
| `snarePulse` | same for snare |

## Cat animation mapping

Replace the hard-coded `sin(frameMs * tempo)` dance path when `confidence >= kConfUse` (e.g. 0.45) and VU indicates music:

| Input | Cat behavior |
|-------|----------------|
| `bpm` | Horizontal walk speed scaled from ~70 (slow) to ~160 (fast); leg phase period = `60000/bpm` (or half-note subdivision if one step-per-beat looks too slow — pick one and keep it consistent; default **one walk-cycle per beat**) |
| `kickPulse` | Large vertical hop target (priority over snare hop) |
| `snarePulse` | Small vertical bob + stronger tail flick |
| Simultaneous kick+snare | Big hop + tail flick; do not stack two vertical targets |

When `confidence < kConfUse` or VU below existing quiet threshold: **unchanged** stroll + VU fake hop path.

## Module boundaries / files

| Piece | Change |
|-------|--------|
| `src/dsp/beat_tracker.h/.cpp` | **New** — pure DSP, no display/WiFi |
| `src/main.cpp` | Construct tracker; `process` after `spectrum.analyze`; pass state into render |
| `src/display/display_driver.*` | Extend render context / signature with `BeatState` (or thin POD) |
| `src/display/vfx_renderer.*` | `drawDancingCat_` consumes beat fields |
| `src/net/webui.*` | JSON `bpm`, `beatConf`; optional status line text |
| Settings / NVS | **No change** for beat fields |

## Error handling / edge cases

- Silence or sparse onsets → confidence falls → behavior A (VU fallback).
- Out-of-range IOIs discarded; do not update BPM from them.
- Mic hiss / continuous bass: adaptive threshold + refractory should suppress chatter; if confidence stays low, fallback protects the cat.
- CPU: a few float sums and a tiny ring buffer per frame; no extra FFT allocation.

## Non-goals (this pass)

- Hi-hat / full drum separation
- WebUI editable onset thresholds
- TFT BPM / confidence numerals
- ML models or offline drum-stem separation

## Acceptance criteria

1. With clear kick-heavy music at moderate level, estimated BPM settles within roughly ±10 of a known track tempo within a few seconds, and the cat’s walk cadence visibly tracks it.
2. Kick hits produce larger hops than snare accents; snare adds a distinct small motion/tail flick.
3. Speech, silence, or ambient hiss: cat reverts to current VU stroll/fake-hop without frantic jumping.
4. Frame rate remains acceptable on S3 (no large FPS regression vs current ~25–30 target).
5. WebUI `/api/state` reports `bpm` and `beatConf` for debugging.

## Testing notes

- Serial or WebUI: watch `bpm` / `beatConf` while playing a metronome or a known-BPM track.
- Verify refractory: single kick does not double-trigger.
- Quiet room after music: confidence decays and cat returns to stroll.
