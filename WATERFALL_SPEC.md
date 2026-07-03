# WATERFALL_SPEC.md — Play view 3D spectral waterfall (SPEC.md §14 addendum)

Addendum to SPEC.md §14. This replaces the live scope as the "audio active" visual in the
**Play view large visualizer only**. The Deep view footer scope strip is unchanged. The
reference implementation is the user's `vDepth` renderer (HTML canvas); this document is a
faithful port of it, not an approximation. Where a constant is given, use it exactly —
"looks close" is not the acceptance bar, the constants are.

## 1. Concept

A pseudo-3D ridgeline waterfall (Unknown Pleasures style). It is NOT a real 3D projection:
each spectrum snapshot is a 2D polyline row, rows are stacked with linear perspective
(shrinking width, rising baseline), drawn back-to-front, and each row is closed to its own
baseline and filled opaquely — the opaque fill is the hidden-line removal. Newest row is at
the FRONT (bottom, widest); time recedes toward the horizon.

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
- History ring: `DEPTH_R = 44` rows of `float[DEPTH_C]`. Per animation frame while audio
  is active: push new row at front, drop oldest. 44 rows @ 60 fps ≈ 0.73 s of history.

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

## 4. Painting (back to front, j = 43 → 0)

For each row, build one juce::Path through the 120 points, then:
1. FILL: close the path down to that row's `baseY` (append (px_last, baseY), (px_0, baseY),
   close). Fill with a vertical juce::ColourGradient from `minY` of the row to `baseY`:
   - stop 0.0: `topCol = mix(accent, dim, depth * 0.55)`
   - stop 0.5: `mix(topCol, darkBase, 0.7)`
   - stop 1.0: `darkBase`
   The fill must be fully opaque — it is the occlusion mechanism.
2. GLOW stroke (front half only, `depth < 0.5`): stroke the ridge polyline (not the closed
   fill path) width `max(2.5, h * 0.013)`, color `accent` at alpha `0.14 * (1 - depth*2)`.
3. MAIN stroke: width `max(1.0, h * 0.004)`, color `mix(bright, accent, depth * 0.5)` —
   near-white at the front fading toward pure accent at the back.

Floor/axis (draw before the rows):
- Baseline: 1 px line at `groundY`, accent @ alpha 0.28, spanning `cx ± halfW`.
- Ticks: 1-2-5 series (100, 200, 500, 1k, 2k, 5k, 10k) within [fLo, fHi]; each a small
  vertical tick of height `h * 0.018` below the baseline, accent @ alpha 0.16, at
  `px = cx + ((log(f) - log(fLo)) / (log(fHi) - log(fLo)) - 0.5) * halfW * 2`.
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
APVTS parameters.

## 6. Behavior & animation

- Runs inside the Play view large visualizer's existing context-switch logic, replacing
  the scope role: audio active → waterfall; the wavetable stack / image view logic for
  idle and Lens contexts is unchanged.
- Advance one history row per animation frame at ~60 fps (VBlankAttachment or the existing
  60 Hz UI timer — whichever Play view already uses; do not add a second timer).
- Silence handling ("never look dead", SPEC pillar 1): when audio goes inactive, do NOT
  freeze-and-swap. Keep advancing with all-zero rows so the surface visibly drains toward
  the horizon (~0.73 s), THEN hand back to the idle visual per existing logic.
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

1. Screenshot (`Lumen.exe --screenshot wf.png --view play` while rendering a held chord via
   the harness, or with injected synthetic FFT data if the screenshot path can't produce
   audio): visible ridgeline surface, neon-yellow palette per §5, floor labels 100/1k/10k/Hz
   present, front rows brighter than back rows, no ridge drawn above the horizon region.
2. Unit test: log column mapping — `idx[0]` corresponds to ~30 Hz and `idx[119]` to
   ~min(18k, sr/2−1000) at sr = 44100 and 48000; all indices in [1, 1023]; monotonic
   non-decreasing.
3. Unit test: Web-Audio emulation — a full-scale 1 kHz sine yields v ≈ 1-ish in its column
   after settling (> 0.8), and digital silence decays every column below 0.01 within 44
   frames (drain check).
4. Frame-time stress re-run in Play view with waterfall active: worst case < 16.7 ms,
   zero audio dropouts.
5. pluginval strictness 10 green. Existing Deep view scope and filter FFT unchanged.
6. Commit message references this file; note in DECISIONS.md that the Play view "active"
   visual is now the waterfall (scope remains in Deep view).
