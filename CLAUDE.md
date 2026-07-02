# CLAUDE.md — Lumen

Lumen is a wavetable synthesizer: C++20, JUCE 8, CMake, targeting VST3 + Windows standalone.

- `SPEC.md` = WHAT to build (source of truth).
- `PHASES.md` = the ORDER and the acceptance gates.
- Read both in full before writing code. Work strictly one phase at a time. A phase is done only when every one of its acceptance criteria has been verified and shown to the user.
- JUCE tag pinned in Phase 0: `8.0.14`.

## Commands

```
cmake -B build -G "Visual Studio 17 2022" -A x64        # configure
cmake --build build --config Release                     # build everything
build\lumen_tests_artefacts\Release\lumen_tests.exe      # unit tests
build\lumen_render_artefacts\Release\lumen_render.exe --preset init --note 60 --out t.wav --analyze
build\Lumen_artefacts\Release\Standalone\Lumen.exe --screenshot ui.png --view deep
Tools\pluginval.exe --strictness-level 10 --validate build\Lumen_artefacts\Release\VST3\Lumen.vst3
```

Dev machine note: this checkout lives on Windows (`C:\Users\pixel\Projects\lumen`) but Claude Code runs in WSL. Builds go through the Windows toolchain — invoke the VS-bundled CMake:
`"/mnt/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe" -S C:/Users/pixel/Projects/lumen -B C:/Users/pixel/Projects/lumen/build -G "Visual Studio 17 2022" -A x64`
(pass `-DGIT_EXECUTABLE="C:/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/TeamFoundation/Team Explorer/Git/cmd/git.exe"` on a fresh configure so FetchContent can clone JUCE). The exes run fine directly from WSL.

## Non-negotiable rules

1. **Real-time safety.** In `processBlock`: no allocation, no locks, no logging, no file or system calls. All buffers sized in `prepareToPlay`. `ScopedNoDenormals` present. UI <-> audio communication only via APVTS, atomics, and lock-free FIFOs. Wavetable swaps = atomic pointer swap; retire old tables on the message thread.
2. **Parameters.** Everything in APVTS with versioned IDs, human-readable names and units, and ~20 ms smoothing on all continuous parameters. NEVER reorder, remove, or insert before the first 8 parameters (Maschine knob page 1 — SPEC section 12).
3. **Layout.** `Source/Engine` (pure DSP — must compile with zero `juce_gui_*` includes), `Source/UI`, `Source/Lens` (image engine), `Source/State`, `Tools/` (render + tests). Engine code must run headless.
4. **Dependencies.** JUCE + stdlib only. Ask the user before adding anything else.
5. **Warnings.** `/W4`, treated as errors in project code. Fix root causes; do not suppress.
6. **Determinism.** Lens results depend only on the image bytes (seeded FNV-1a). No `rand()`, no `time()`, no global state in the Engine.

## Verify yourself before involving the user

- After any DSP change: build Release, run `lumen_render --analyze`, and compare the JSON metrics against the current phase's thresholds in `PHASES.md`. Render WAVs and read the numbers — never ask the user to listen for something a metric can catch.
- After any UI change: run `Lumen.exe --screenshot shot.png` and actually inspect the image against SPEC section 14 (layout, clipping, colors, spacing).
- At every phase end: full unit-test suite, pluginval at the phase's strictness, and a smoke check that the standalone launches and is silent when no notes play.
- The user is the ear and eye of last resort — reserve them for taste (does it sound and feel good), not correctness.

## Git

- Commit small and often; imperative messages ("Add SVF drive stage"). Every commit must build.
- Tag each completed phase: `phase-0` ... `phase-7`, then `v1.0.0`.

## When the spec is silent

Pick the most musical / most obvious option, log it in `DECISIONS.md` (create the file on first use), and keep moving. Batch questions for phase boundaries. Interrupt mid-phase only for true blockers: missing tools, admin permissions, license acceptances.

## What always needs the user

Installer/admin approval prompts · the one-time elevated run of `tools\grant_vst3_write.ps1` · listening and look-and-feel judgment at every phase gate · loading the VST3 in Ableton Live and Maschine 3 to confirm host behavior (Claude Code cannot see those hosts).
