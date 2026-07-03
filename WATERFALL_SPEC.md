# WATERFALL_SPEC.md — Play view 3D spectral waterfall (SPEC.md §14 addendum)

Addendum to SPEC.md §14. This scene (perspective floor grid + spectral surface) is the
**permanent Play view large visualizer** (restyle round, 2026-07-02; it originally replaced
only the "audio active" scope role). The Deep view footer scope strip and the Deep view
cards are unchanged. The reference implementation is the user's `vDepth` renderer (HTML
canvas); this document started as a faithful port of it — the restyle round then replaced
the gradient/glow look with solid silhouettes and added the floor grid. Where a constant is
given, use it exactly — "looks close" is not the acceptance bar, the constants are.

## 1. Concept

A pseudo-3D ridgeline waterfall (Unknown Pleasures style) over a synthwave perspective
floor grid. It is NOT a real 3D projection: each spectrum snapshot is a 2D polyline row,
rows are stacked with linear perspective (shrinking width, rising baseline), drawn
back-to-front, and each row is closed to its own baseline and filled opaquely — the opaque
fill is the hidden-line removal. Newest row is at the FRONT (bottom, widest); time recedes
toward the horizon. Idle = the grid with a drained/flat surface; playing = ridges rise;
release = the surface drains in place.

## 2. Data pipeline

- Source: the existing UI-thread audio tap (the lock-free FIFO already feeding the scope
  and the filter-card FFT). No new audio-thread work; `processBlock` untouched.
- FFT: 2048-point (juce::dsp::FFT order 11), Hann window, mono sum of the master output.
  Hop: one FFT per animation frame (see §6); consuming the most recent 2048 samples from
  the FIFO each frame is correct — overlapping windows are expected and desired.
- Emulate Web Audio AnalyserNode output, because the reference look depends on it:
  1. Compute magnitude per bin: `mag[i] = sqrt(re² + im²) / (N/2)` (N = 2048).
  2. Temporal smoothing on LINEAR magnitudes, before dB:
     `smooth[i] = 0.8 * smooth[i] + 0.2 * mag[i]`  (the 0.8 is critical — it is what makes
     the ridges undulate instead of flicker; expose as a member constant `kSmoothing`).
  3. dB conversion and normalization: `db = 20 * log10(max(smooth[i], 1e-10))`,
     `v[i] = clamp((db - (-100)) / ((-30) - (-100)), 0, 1)`  → v in [0,1].
- Column mapping (precompute on prepare / sample-rate change):
  - `DEPTH_C = 120` columns, log-spaced `fLo = 30 Hz` to `fHi = min(18000, sr/2 - 1000)`.
  - Column c → frequency `f = fLo * pow(fHi/fLo, c / (DEPTH_C - 1))`.
  - Column c samples ONE nearest bin: `idx[c] = clamp(round(f * 2048 / sr), 1, 1023)`.
    Single-bin sampling, NOT bucket averaging — this preserves the spiky HF detail.
- History ring: `DEPTH_R = 22` rows of `float[DEPTH_C]` (restyle round: was 44). The ring
  advances one row every OTHER animation frame (§6), so 22 rows @ ~30 rows/s ≈ 0.73 s of
  visible history — same span, half the ridge density, calmer motion. The FFT + smoothing
  run on advancing frames only (the analyser timeline follows the row cadence).

## 3. Projection & geometry (all relative to component width w, height h)

Per frame:
- `cx = w/2`, `halfW = w * 0.47`, `groundY = h * 0.84`, `horizonY = h * 0.30`,
  `hScale = h * 0.36 * gain` with `gain = 1.2` (clamp any future gain param to ≤ 2.2).
- For row j (0 = newest/front … 43 = oldest/back):
  - `depth = j / (DEPTH_R - 1)`
  - `scale = 1 - depth * 0.42`            // rows shrink to 58% width at the back
  - `baseY = groundY + (horizonY - groundY) * depth`   // baseline rises linearly
- For column c with normalized position `n = c / (DEPTH_C - 1)` and value `hv = row[c]`:
  - Amplitude shaping: `hv = hv*hv*0.55 + hv*0.45`   // soft expander: flattens noise
    floor, exaggerates peaks — do not omit
  - `px = cx + (n - 0.5) * halfW * 2 * scale`
  - `py = baseY - hv * hScale * scale`

## 4. Painting (back to front, j = 21 → 0)

For each row, build one juce::Path through the 120 points, then (restyle round — the
original 3-stop gradient fill and glow underlay are REMOVED):
1. FILL: close the path down to that row's `baseY` (append (px_last, baseY), (px_0, baseY),
   close). Fill SOLID with the background color `#070708`, fully opaque — the fill is the
   occlusion mechanism. Clean silhouetted ridgelines; no gradient, no glow.
2. MAIN stroke: fixed width 1.0 px at all sizes, color `mix(bright, accent, depth * 0.5)` —
   near-white at the front fading toward pure accent at the back.

Floor/axis (draw before the rows; cached to the resize-only floor image):
- Perspective floor grid (synthwave), accent @ alpha 0.12 (pick within 0.10–0.15):
  - Horizontals: one 1 px line at each row's baseline `baseY(j)`, spanning that row's
    width `cx ± halfW * scale(j)` — spacing and width both compress toward the horizon.
  - Verticals: one line per 1-2-5 tick frequency, connecting the tick's front position
    `(cx + off, groundY)` to its depth-scaled position at the horizon
    `(cx + off * 0.58, horizonY)` (0.58 = the row-depth scale at the back), so the lines
    converge toward the horizon. `off = ((log(f) - log(fLo)) / (log(fHi) - log(fLo))
    - 0.5) * halfW * 2`.
- Baseline: 1 px line at `groundY`, accent @ alpha 0.28, spanning `cx ± halfW`.
- Ticks: 1-2-5 series (100, 200, 500, 1k, 2k, 5k, 10k) within [fLo, fHi]; each a small
  vertical tick of height `h * 0.018` below the baseline, accent @ alpha 0.16, at
  `px = cx + off`.
- Labels: "100", "1k", "10k" centered under their ticks, and "Hz" right-aligned at
  `cx + halfW`; monospace (the UI's existing mono face), size `max(8, h * 0.028)`,
  accent @ alpha 0.5 (0.65 for "Hz").
- Background behind the waterfall: `#070708`.

## 5. Palette — neon yellow (exact values)

Derivation (record it so retuning is one hex change): given `accent`,
`bright = mix(accent, #FFFFFF, 0.5)`, `dim = mix(accent, #04060A, 0.78)`,
`darkBase = mix(dim, #000000, 0.55)`, where `mix(a,b,t)` is per-channel linear interp.

| Role     | Value     |
|----------|-----------|
| accent   | `#FAFF00` |
| bright   | `#FCFF80` |
| dim      | `#3A3D08` |
| darkBase | `#1A1B04` |

These are UI-layer constants in one place (e.g. `WaterfallLookAndFeel` namespace), not
APVTS parameters. Since the restyle round only `accent`, `bright` and the background
`#070708` are painted (solid fills, no gradient); `dim`/`darkBase` stay pinned in code so
the derivation survives a future retune.

## 6. Behavior & animation

- The scene (grid + surface) is PERMANENT in the Play view large visualizer (restyle
  round — supersedes the original context-switch role): idle = the grid with a
  drained/flat surface; playing = ridges rise; release = the surface drains in place.
  There is NO switching to the wavetable stack or the image view anymore (the Deep view
  cards are unchanged; the Lens panel still shows the image).
- Advance one history row every OTHER animation frame at ~60 fps (~30 rows/s; restyle
  round — was every frame) using the existing 60 Hz UI timer; do not add a second timer.
  Non-advancing frames change nothing on screen and skip the repaint.
- Silence handling ("never look dead", SPEC pillar 1): when audio goes inactive, do NOT
  freeze. Keep advancing with all-zero rows so the surface visibly drains toward the
  horizon (~0.73 s) and settles as the idle flat surface. Once the whole ring is drain
  rows the scene is static — the analyser keeps decaying but repaints stop until audio
  returns.
- Resize-safe: all geometry is derived from w/h each paint (it already is, per §3).
  DEPTH_R/DEPTH_C stay fixed at all sizes.

## 7. Performance & safety constraints

- No allocations in paint: preallocate the point arrays (float[120] x/y), the history ring,
  FFT buffers, and reuse one Path via `clearWithReservedSpace` (reserve ~256 points).
- All work on the message thread; audio thread contributes only the existing FIFO writes.
- Repaint only the waterfall component's bounds; the floor + labels may be cached to a
  juce::Image regenerated on resize only.
- Budget: the Phase 5 stress numbers (6.44 ms avg / 9.26 ms worst) must not regress past
  the 16.7 ms worst-case criterion with the waterfall active and 8 voices playing.

## 8. Acceptance criteria

1. Screenshots (`Lumen.exe --screenshot wf.png --view play`):
   - idle (no notes): perspective floor grid visible (horizontals compressing toward the
     horizon, verticals converging), drained flat surface, baseline + 100/1k/10k/Hz labels;
   - holding a chord (`--notes ...`): visible silhouetted ridgeline surface (solid `#070708`
     fills, no glow), neon-yellow palette per §5, front rows brighter than back rows, no
     ridge drawn above the horizon region.
2. Unit test: log column mapping — `idx[0]` corresponds to ~30 Hz and `idx[119]` to
   ~min(18k, sr/2−1000) at sr = 44100 and 48000; all indices in [1, 1023]; monotonic
   non-decreasing.
3. Unit test: Web-Audio emulation — a full-scale 1 kHz sine yields v ≈ 1-ish in its column
   after settling (> 0.8), and digital silence decays every column below 0.01 within
   DEPTH_R rows (drain check).
4. Frame-time stress re-run in Play view with waterfall active: worst case < 16.7 ms,
   zero audio dropouts.
5. pluginval strictness 10 green. Existing Deep view scope and filter FFT unchanged.
6. Commit message references this file; changes logged in DECISIONS.md.
