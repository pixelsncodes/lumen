# demo/showcase — the mood-space gallery

Eight procedurally generated images (no third-party or copyrighted material —
see `make_images.py`, deterministic, fixed seeds) chosen to hit distinct
corners of the key detector's mood space: average hue picks the
circle-of-fifths root; saturation + luminance pick the scale family
(washed-out → pentatonics, vivid & dark → blues / harmonic minor, otherwise
diatonic modes along a dark → bright axis); per-region contrast drives the
rhythm-density axis.

Per image `<name>`:

- `<name>.png` — the source image fed to the Lens
- `<name>.mid` — the generated melody (120 BPM SMF, default settings)
- `<name>.txt` — the generation summary exactly as the plugin shows it
  (key / mood / form / seed / note count)
- `<name>-panel.png` — the full editor with the MELODY panel open on that
  image (the same readout, visually)

| image | detected | mood bucket | seed |
|---|---|---|---|
| 01-sunrise-bright | D Major | bright (major) | 5eed0001 |
| 02-neon-dusk | Bb Harmonic Minor | vivid & dark (minor) | 5eed0002 |
| 03-fog-washedout | C#/Db Major Pentatonic | washed-out | 5eed0003 |
| 04-forest-dark | E Minor | dark (minor) | 5eed0004 |
| 05-ocean-mid | F# Dorian | dusky | 5eed0005 |
| 06-amber-warm | G Dorian | dusky | 5eed0006 |
| 07-ember-highcontrast | C Harmonic Minor | vivid & dark (minor) | 5eed0007 |
| 08-pastel-lowcontrast | Ab Mixolydian | warm (major) | 5eed0008 |

Regenerate everything (Windows, repo root — seeds are pinned so the output
is reproducible):

```bat
python demo\showcase\make_images.py
build\Lumen_artefacts\Release\Standalone\Lumen.exe --lens-image demo\showcase\01-sunrise-bright.png --melody --melody-seed 5eed0001 --melody-export demo\showcase\01-sunrise-bright --screenshot demo\showcase\01-sunrise-bright-panel.png
:: ...same pattern for 02..08 with seeds 5eed0002..5eed0008
```

Generated with the showcase combo: parent `feature/phase5-minimal-ui`,
submodule `feature/clock-unification` tip (the unmerged Phase 4.5 engine).
