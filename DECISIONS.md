# DECISIONS.md — approved deviations & spec-silent choices

Format: date · phase · decision · why.

- 2026-07-02 · P0 · **JUCE pinned to `8.0.14`** — latest stable 8.x tag on the JUCE repo at time of Phase 0.
- 2026-07-02 · P0 · **Toolchain = existing VS 2022 Community 17.14 + its bundled CMake 3.31.6 and Git** — everything the spec requires was already installed; nothing new was added. Builds are driven from WSL by invoking the Windows `cmake.exe`, so compilation is native MSVC exactly as specified.
- 2026-07-02 · P0 · **`Tools/` and `tools/` are the same directory** (NTFS is case-insensitive). The repo uses `Tools/` for render/tests sources *and* for `grant_vst3_write.ps1` + `pluginval.exe`, matching both spellings in CLAUDE.md.
