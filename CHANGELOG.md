# Changelog

## Unreleased — Melody side panel redesign

- Replaced the popup MELODY window with a right-docked side panel (a true
  window extension — the base 1040×660 UI never moves) and a GENERATED
  info readout under the Lens image, with a visible, editable, lockable
  32-bit seed.
- Fixed the Lens SCAN/SPECTRAL toggle icons: SCAN now shows the horizontal
  line glyph, SPECTRAL the vertical line glyph (previously reversed).
- Removed the dead popup class and its floating-window move/resize code;
  extracted the shared sampling-grid drawing into `Source/UI/MelodyGrid.{h,cpp}`.
- See `docs/redesign-notes.md` for the full phase-by-phase history.
