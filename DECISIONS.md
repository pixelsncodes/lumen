# DECISIONS.md — approved deviations & spec-silent choices

Format: date · phase · decision · why.

- 2026-07-02 · P0 · **JUCE pinned to `8.0.14`** — latest stable 8.x tag on the JUCE repo at time of Phase 0.
- 2026-07-02 · P0 · **Toolchain = existing VS 2022 Community 17.14 + its bundled CMake 3.31.6 and Git** — everything the spec requires was already installed; nothing new was added. Builds are driven from WSL by invoking the Windows `cmake.exe`, so compilation is native MSVC exactly as specified.
- 2026-07-02 · P0 · **`Tools/` and `tools/` are the same directory** (NTFS is case-insensitive). The repo uses `Tools/` for render/tests sources *and* for `grant_vst3_write.ps1` + `pluginval.exe`, matching both spellings in CLAUDE.md.
- 2026-07-02 · P1 · **Master gain range −60..+6 dB, −60 displayed as "-inf dB", default 0 dB** — SPEC §10 says "−inf..+6 dB" without a floor; −60 dB is inaudible and keeps the range finite for the slider/automation.
- 2026-07-02 · P1 · **JUCE splash screen left enabled** — disabling it (`JUCE_DISPLAY_SPLASH_SCREEN=0`) is only licensed under AGPLv3 or a paid JUCE license. Needs a user decision on how Lumen will be licensed; until then the default splash stays (it briefly overlays the editor's bottom-right corner).
- 2026-07-02 · P1 · **`juce::OpenGLContext` attachment deferred to Phase 5** (SPEC §2). All drawing already goes through normal `paint()`, so behavior is identical; the context is purely an acceleration layer for the heavy Phase 5 visualizers.
- 2026-07-02 · P1 · **Test voice = polyBLEP saw** (SPEC/PHASES say only "band-limited saw"): naive saw with a 2-sample polynomial correction at the wrap. Good-enough band-limiting for a temporary voice; the mip-mapped wavetable engine replaces it in Phase 2. Velocity maps to level as `0.2 * (0.3 + 0.7 * vel)`.
- 2026-07-02 · P1 · **`lumen_render` writes 32-bit float stereo WAV**; `--analyze` currently emits `peak_dbfs`, `rms_dbfs`, `dc_offset`, `nan_count` (+ `schema_phase: 1`). The remaining SPEC §18 metrics land in Phase 2 with the real engine.
- 2026-07-02 · P1 · **Phase 1 editor is a placeholder**: header + the 8 frozen parameters and master gain as labelled knobs, SPEC §14 palette, 1040×660 resizable 70–200% with fixed aspect. Full Play/Deep views are Phase 5.
