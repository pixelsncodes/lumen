# Start here — building Lumen with Claude Code

Four files are in this package. They are the only files you need — Claude Code creates everything else.

- `START_HERE.md` — this guide (for you).
- `CLAUDE.md` — project rules. Claude Code loads this automatically at the start of every session.
- `SPEC.md` — the complete design of the synth.
- `PHASES.md` — build order, acceptance gates, and the prompt template for each phase.

## Setup (2 minutes)

1. Create a folder with no spaces in the path, e.g. `C:\Dev\Lumen`, and put these four files in it.
2. Open a terminal in that folder and run `claude`.
3. Paste the kickoff prompt below. That's it — approve prompts as they appear.

## Kickoff prompt (Phase 0 + 1)

```
Read CLAUDE.md, SPEC.md and PHASES.md in this folder in full. Then execute
Phase 0 and Phase 1: set up the toolchain (ask before installing anything),
initialize the repo, and build the Lumen skeleton — VST3 + standalone with
the temporary test voice — until every Phase 0 and Phase 1 acceptance
criterion passes, including pluginval. Show me the verification results,
then tell me exactly how to load and test it in Ableton Live, Maschine 3,
and standalone.
```

For every later phase, use the paste-prompt template at the bottom of `PHASES.md` (swap in the phase number).

## What you'll be asked to do

- Approve installer/admin prompts (Build Tools, CMake, Git) the first session.
- Run `tools\grant_vst3_write.ps1` once from an elevated PowerShell when asked — this lets normal builds copy the plugin into the system VST3 folder from then on.
- At each phase gate (5–10 minutes): load the plugin in your hosts and react. Rescan first:
  - Ableton Live: Settings → Plug-Ins → make sure VST3 is enabled → Rescan.
  - Maschine 3: Preferences → Plug-ins → check locations → rescan.
  - Standalone: Claude Code will give you the exact `Lumen.exe` path; pick the Scarlett and the Launchkey in its audio/MIDI settings.
- Give feedback in plain sensory words — "the attack clicks", "reverb sounds metallic", "labels too small at 100% size". That's the one input it can't generate for itself; it converts your words into fixes and re-verifies with its metrics and screenshots.

## Tips that make this go smoothly

- One session per phase keeps context clean. A new session re-reads `CLAUDE.md` automatically; just paste the next phase prompt.
- Want to review the approach before it touches files? Use Claude Code's plan mode for that phase.
- If it reports a criterion as passed, you can always ask to see the JSON metrics or the screenshot it checked.
- If something about Claude Code itself confuses you: https://docs.claude.com/en/docs/claude-code/overview

## What to expect

Phases 0–1 fit in the first session and end with a sound-making plugin visible in Live and Maschine. You'll have a genuinely playable synth by the end of Phase 3. Phase 5 (UI and visualizers) is the longest and the one where your taste matters most — plan on a few feedback rounds. Lens (Phase 6) and the factory bank (Phase 7) finish it off. Roughly one to two weeks of on-and-off sessions for a polished v1.0.
