# LUMEN SITE REDESIGN — MASTER PLAN

Target file: `site/lumen/index.html` (single self-contained HTML/CSS/JS file).
Deploy: commit → push `main` → Vercel → kaziahmed.net/lumen.
Local test: `python3 -m http.server 8000` from `site/` → `localhost:8000/lumen/`.

Execute ONE PHASE PER SESSION. Do not start a phase until the previous
phase's acceptance criteria are all demonstrated. After each phase:
local test, screenshot desktop + mobile widths, then commit with the
phase name in the message.

---

## GLOBAL RULES (apply to every phase)

1. Asset paths stay ROOT-ABSOLUTE: `/lumen/web-assets/...`. Never relative.
2. Brand tokens only. Existing CSS custom properties in `:root` are the
   palette. Macro arc colors (sampled from the instrument):
   - Tone `#FF2E9F` · Motion `#19E3E3` · Space `#FAFF00` · Texture `#E8E6E3`
   Use magenta/cyan sparingly as *macro/mod* accents; `#FAFF00` remains the
   primary accent; `#FF3B30` stays warnings-only.
3. NO EM DASHES anywhere in rendered content, including data attributes,
   figcaptions, alt text, and any new copy. Rewrite sentences (period,
   comma, colon) rather than swapping in hyphens. `–` and `—` both banned.
   (Code comments exempt.)
4. Every animation respects `prefers-reduced-motion` (the global kill rule
   exists; canvas/JS animations need explicit handling like the existing
   waterfall's reduced path).
5. One sound at a time site-wide: every audio module dispatches/listens to
   the `lumen-audio` CustomEvent on `document` (see existing pb/lg modules).
6. Keyboard accessibility for anything clickable; `aria-current` for
   selection state; `aria-label` for icon buttons.
7. Performance: images `loading="lazy"` (except hero stage image),
   audio `preload="none"`, no new network dependencies, no libraries.
8. Don't touch: download section mechanics, FAQ content, MIDI content,
   footer, the three pending video slots, the commented TODO(pack) block.

## COMPONENT INVENTORY (reuse, don't rebuild)

- `waterfallRenderer` — the canvas ridgeline in the presets module.
  Phase 1 refactors it into a shared factory: `makeWaterfall(canvas)`
  returning `{attach(analyser, actx), draw(), startLoop(), setPlaying(bool)}`
  so hero and preset browser each own an instance. Keep tuning constants
  (ROWS 26, BINS 72, 55–16k log map, 46ms slice push, painter's occlusion).
- Knob component — `.lg-knob` SVG (track arc, value arc, indicator, num
  readout) from the image-lab module. Reuse markup + `setKnobs()` math
  (sweep 270°, start −135°, r=25, C=157.08).
- Image manifest — 24 entries with name + 4 macro floats, currently in
  `.lg-thumb` data attributes. Phase 1 moves it to one JS array used by
  the hero; thumbs are then generated or kept static (executor's choice,
  static preferred for no-JS graceful degradation).
- Old hero CSS demo widgets (`.d-fall`, `.d-knob`, `.d-wave`, `.d-sliders`,
  `.d-matrix`, `.d-lens`, `.d-keys`) — DO NOT DELETE in Phase 1. They get
  recycled into Phases 2 and 4, then orphans are purged in Phase 6.

---

## PHASE 1 — HERO: THE PLAYABLE INSTRUMENT  (largest phase)

**Goal:** Replace the CSS-animation hero demo with a functional replica of
the Lumen UI wired to real audio: the Image Lab promoted to hero.

**Layout (desktop):** headline block on top, instrument panel below,
full `--maxw` width. Headline: H1 wordmark, the "plays images" subline,
meta line, two CTA buttons; keep `.grid-floor`, `.hero-fade`, `.scan-beam`
backdrop. Instrument panel structure, mirroring the real plugin top to
bottom:

1. **Header bar:** LUMEN wordmark + neon dot · center: `<` name `>`
   steppers that walk the 24 images · right: PLAY / DEEP tabs (see
   stretch goal) + a small fake output meter (CSS, animates only while
   audio plays).
2. **Main row (two columns):** left = waterfall canvas (shared renderer,
   real analyser on the hero's audio element, idle ridge when silent).
   Right = LENS panel: selected image with white scan beam tied to
   playback progress (reuse image-lab beam logic), filename-style label
   above ("LENS · <name>.jpg"), chip row below: SCAN (active) ·
   SPECTRAL (inactive) · COLORS (active). Chips are decorative, styled
   like the plugin's.
3. **Knob row:** four macro knobs (reused component), animating to the
   selected image's values, labels + numeric readouts.
4. **Image grid (replaces plugin keyboard):** 24 thumbs, 8 columns × 3
   rows desktop (grid-template-columns repeat(8,1fr); wraps 6 or 4
   columns below 900px/560px). Same interaction as image-lab: click =
   select + play, click active = toggle, neon ring on active, arrow-key
   nav.
5. **Transport strip:** play/pause, seek, time (reuse `.pb-transport`
   pattern). Clips loop.

**Audio:** own `Audio()` element, `loop = true`, `preload="none"`,
AnalyserNode feeding the hero waterfall, `lumen-audio` cross-pause id
`"hero"`. AudioContext created on first user gesture.

**Removals in this phase:** the old `.demo` hero markup and its JS
auto-toggle interval; the entire `#image-lab` section (hero replaces it)
including its nav link; hero copy rewritten without em dashes.
KEEP the old demo CSS classes (see inventory).

**Mobile (<900px):** headline stacks center; instrument stacks: header
bar → lens image (capped ~320px, centered) → waterfall (min 180px) →
knobs (4-across, shrink) → grid (4 cols) → transport.

**Stretch (only if phase is otherwise green):** make PLAY/DEEP tabs real;
DEEP swaps the main row for the recycled `.d-deep-grid` widgets. Skip
freely; Phase 4 covers DEEP representation anyway.

**Acceptance:**
- [ ] Click any thumb: audio plays, knobs sweep to that image's values,
      lens image swaps, beam scans in sync, waterfall draws live spectrum.
- [ ] `<`/`>` steppers change image (and keep playing state).
- [ ] Idle state: waterfall shows static ridge, beam hidden, meter dark.
- [ ] Preset browser and hero pause each other.
- [ ] Reduced motion: no knob sweep transition, beam hidden, waterfall
      uses the reduced path, backdrop animations off.
- [ ] No console errors; page works with JS disabled (hero degrades to
      static image + grid of images; no broken controls).
- [ ] 380px, 768px, 1200px screenshots all sane.
- [ ] No em dashes in any rendered hero text.

---

## PHASE 2 — "ONE SYNTH, FOUR PILLARS" → VISUAL PILLAR CARDS

**Goal:** Kill the two text panels; replace with a 4-card grid (2×2
desktop, 1-col mobile), one card per pillar, each led by a small living
graphic recycled from the old hero demo CSS, with ≤2 sentences beneath.

- **Wavetable engine** → `.d-wave` scrolling waveform SVG, restyled wider.
- **Four live knobs** → `.d-knob` quartet, but tint each ring's active arc
  with its macro color (Tone/Motion/Space/Texture) and use real labels.
- **The Lens** → `.d-lens` gradient + beam mini, add a tiny "image in →
  wave out" glyph.
- **The waterfall** → `.d-fall` animated bars strip.

Remove the `play_view.png` figure from this section (the hero now IS the
play view). Rewrite card copy, no em dashes, each card: mono uppercase
title, graphic, max 2 sentences.

**Acceptance:** 4 cards, animated at rest, reduced-motion static,
mobile stacks, section height shrinks vs current, no em dashes.

---

## PHASE 3 — "THE LENS" SECTION VISUAL UPGRADE

**Goal:** Replace the Scan/Spectral text panels with one animated
comparison graphic; tighten prose.

- Build an SVG/CSS diagram: a small shared "image" swatch (CSS gradient,
  like `.d-lens`) with two output lanes: SCAN lane draws a waveform line
  being traced left-to-right (brightness → waveform), SPECTRAL lane grows
  a row of harmonic bars (structure → partials). Label lanes in mono
  caps; animate the trace/bars on loop; static end-state under reduced
  motion.
- Keep: COLORS paragraph (tighten), "every note is a journey" +
  "reversible" (merge into one shorter block), the note box, the video
  slot, the `lens_panel.png` figure.
- All copy de-em-dashed.

**Acceptance:** diagram reads Scan vs Spectral without the old panels,
animates, reduced-motion safe, section is visually led rather than
text-led, no em dashes.

---

## PHASE 4 — QUICKSTART + SOUND DESIGN: SHOW PLAY *AND* DEEP

**Goal:** Replace screenshot-plus-list walls with an annotated two-view
walkthrough.

- Build a tabbed (or side-by-side ≥900px) pair of stylized UI mocks from
  recycled demo components, NOT screenshots:
  - **PLAY mock:** waterfall strip, 4 knobs, lens mini, key strip, with
    numbered callout pins (01–05) matching a slimmed 5-step list beside
    it. Steps become one short line each; details move into pin tooltips
    (title attr or small reveal on hover/focus).
  - **DEEP mock:** the `.d-deep-grid` (osc/ADSR/matrix/lens) with pins for
    oscillators, filter/env, matrix, FX.
- The `deep_view.png` figure is replaced by the DEEP mock; drop the
  `tooltip.png` figure in sound design; keep `deep_view_voice.png` in MIDI
  (untouched section).
- Sound design keeps its 3 recipes but each recipe compresses to ≤4
  steps, one line each, no em dashes. Keep the house-rule note.

**Acceptance:** both views represented, callouts navigable by keyboard,
tab switch (if tabs) works without JS errors, text volume visibly cut,
mobile stacks mock above steps, no em dashes.

---

## PHASE 5 — ENGINE DIAGRAM: FIX OVERFLOW + ANIMATE

**Goal:** Repair and bring the signal-flow SVG alive.

- **Overflow fix:** the OSC box caption ("morph · mip-mapped ·
  voices/mono/legato + glide") exceeds its rect; widen rects / shorten
  captions / reduce font so every text fits with ≥8px padding at 380px
  render width. Audit every box.
- **Color language:** audio path strokes stay ink; macro-related arrows
  magenta `#FF2E9F` accents where macros act; Lens paths stay `#FAFF00`;
  add cyan `#19E3E3` for LFO/env mod arrows. Add a small legend row.
- **Animation:** SMIL or CSS on SVG: a pulse dot traveling OSC → filter →
  FX → limiter → out on loop (~4s); mod arrows gently pulsing opacity,
  staggered; waterfall tap dash-marching. All off under reduced motion
  (static full-color diagram remains).
- Keep the spec chips and table; de-em-dash the section prose.

**Acceptance:** no text overflow at any width ≥320px, pulse visibly
travels the audio path, legend present, reduced-motion static, colors
match token list, no em dashes.

---

## PHASE 6 — SWEEP, PURGE, SHIP

- Global em-dash grep must return zero in rendered content:
  `grep -n "—" site/lumen/index.html` and `grep -n "–" ...`
  (allow hits only inside `<script>` comments / code, ideally zero).
- Delete now-orphaned CSS (old `.demo`/`.d-*` classes not reused,
  `.asset` if unused, anything else dead). Verify with a class-usage
  audit before deleting.
- Nav audit: links match final section order.
- Full pass: 380/768/1200 widths, reduced motion on/off, JS disabled,
  Chrome + Firefox, console clean, Lighthouse perf sanity (images lazy,
  no layout shift from hero).
- Confirm all audio/img paths resolve on the deployed URL (subpath!).
- Single final commit or squashed series; push; verify production.

---

## SECTION ORDER AFTER REDESIGN

hero (playable instrument) → overview (pillar cards) → presets (browser,
unchanged) → download → quickstart (PLAY/DEEP walkthrough) → lens →
sound-design → engine → midi → build → faq

Nav: Presets · Download · Quick start · The Lens · Sound design ·
Engine · MIDI · How it's built · FAQ  (Image lab link removed; hero is
unlabeled as always.)
