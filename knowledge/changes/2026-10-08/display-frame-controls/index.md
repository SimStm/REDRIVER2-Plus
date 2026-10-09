---
type: Change
title: Display FPS, regional startup mode and live VSync
description: Complementary developer-panel frame controls without migration-stage acceptance.
tags: [settings, timing, presentation, replay, vulkan]
---

# Display FPS, regional startup mode and live VSync

## Scope and behavior

The user requested FPS, NTSC/PAL and VSync beside resolution, persistent edits,
and display-style confirmation when an apply step is required. This complements
the prior car/camera motion fix; E00-E03 retain their existing in-progress status.

- FPS is a live presentation ceiling, 15..360, default 30 NTSC/25 PAL. It is
  autosaved when slider/text editing finishes. Counter-based rational deadlines
  carry remainders and keep small overruns on schedule. This pacer never resets
  the simulation clock, mutates gameplay or changes tick/replay sampling.
- VSync moves to the display group, applies next frame and autosaves. Vulkan
  previously ignored it; the existing generic swap-interval hook now requests
  FIFO or an available unsynchronized mode. Recreation occurs before acquiring
  the next frame with existing ownership/fence handling; failed recreation keeps
  the retry pending. Unsupported unsynchronized presentation explicitly warns
  and falls back to FIFO. Explicit benchmark environment overrides still win.
- Region is a confirmed next-startup preference. The bottom-left confirmation
  shares the existing 15-second timer/revert mechanism and remains input-captured
  with the main panel closed. Selecting it does not change the active session.
  The panel explains the application restart requirement after confirmation.
- Startup activates region after settings load and before game/video/audio
  setup. The same desktop binary selects original 30/25 Hz scheduling, 240/256
  screen height, NTSC/PAL draw/video modes, PAL aspect, sound sample-frame math,
  splash selection, replay metadata and deterministic-dump region headers.
  Existing replay mismatch warnings still continue at active cadence; select the
  recorded region and restart to preserve its original wall-clock timing.
- Schema9 `developer_graphics.ini` owns `presentationFps`, `videoMode` (0 NTSC,
  1 PAL) and `vsync`. Partial autosave passes only frame keys to the transactional
  writer, preserving unknown keys/comments, unrelated edits and provisional
  resolution. Full save/default actions are disabled during confirmation.
  `config.ini` remains user-owned. Saved custom FPS survives region selection.

## Source and fork ownership

`utils/ProjectClock.*` owns the preference, active mode and presentation pacer;
`Game/C/state.c` paces desktop game/frontend loops, outside simulation. Original
PSX compile-time branches remain; web callback pacing is unchanged. Runtime
region consumers are in `system.*`, `sound.c`, `main.c`, `replays.c` and
`DeterministicBaseline.cpp`, with activation in `redriver2_psxpc.cpp`.
`DeveloperGraphicsSettings.*` and `DeveloperGraphicsPanel.cpp` own persistence
and widgets. Production tests link these sources directly with SDL/window stubs.

PsyCross renderer changes are committed/pushed to the project fork at
`76d83df0e400bdff5c1e83c7be74d9e01ba0c660`, branch
`codex/new-engine-live-vsync`, based on the migration's `37d399d` pin. The parent
gitlink is staged. Publishing this scoped branch avoids importing the preserved
rework already on fork `origin/master` (`ae83dd2`) or rewriting that history.
The earlier uncommitted `src/PsyX_main.cpp` InputService edit is retained and
excluded from this commit. No upstream fork, asset modification or PR was used.

## Executed validation

- Direct MSBuild of `build_e00/REDRIVER2.sln` and
  `build_e01_motion_pal/REDRIVER2.sln`, `Release_dev|x64`, succeeded. Existing
  pointer/JPEG/runtime-library warnings remain; builds alone are not acceptance.
- `pwsh -NoProfile -File scripts/run_presentation_timing_tests.ps1`: 145354
  timing/snapshot checks and 21 production settings checks per regional build,
  zero failures. Includes exact simulation scheduling across presentation
  rates, rational pacing at 25/30/60/120/144/360, 16ms coarse timer wakeups,
  unchanged tick/alpha on FPS edit, startup preference activation, schema8 load,
  atomic backups, reload, invalid ranges and preservation of unrelated files.
- Live Vulkan UI mouse edits saved FPS=60 and VSync=0 without a manual Apply.
  `psyx_vk.log` changed FIFO mode=2 to IMMEDIATE mode=0 at the next boundary.
  Regional PAL selection displayed bottom-left confirmation, persisted
  `videoMode=1`, and showed the still-active NTSC/restart notice after applying.
  The same NTSC-built executable then restarted in PAL and saved a V2 tape with
  PAL/25 Hz/40000us metadata at byte260. Vulkan validation layer was enabled;
  application logs showed no validation error during these runs.
- `python scripts/engine_migration/e00_compare_state.py`: NTSC moving route
  matched 900 shared ticks against the prior fixed binary. Final runtime PAL
  (same NTSC executable, cap60 and cap120) matched 1378 reference ticks each,
  zero authoritative mismatches. These compare the existing dump's fields;
  they are not universal camera/pixel, audio or full-game acceptance.
- Final OpenGL run sustained the configured 60.0 FPS (16.67ms) and matched all
  900 moving-route ticks against the previous fixed reference. All owned game
  processes were stopped after validation.
- Live Vulkan throughput at higher caps was about 40..48 FPS in this environment;
  the setting is a ceiling, not a throughput guarantee. Synthetic-clock checks
  also isolate pacing from GPU/driver cost. No sustained 120 FPS Vulkan claim.
- Sky scroll/click actions reached ImGui, but synthetic F11 did not reach the
  SDL handler. A temporary panel-open/key-event probe enabled the click tests;
  it was removed and the final source recompiled. Complete F11/input, new mode
  timeout/cancel, fullscreen and platform coverage remain manual validation.
  All runtime tests use isolated INIs under ignored
  `src_rebuild/build/display-settings/`; user runtime INIs/assets are unchanged.
- Upstream release8.0 at `b2d8857` was rechecked against the
  [release page](https://github.com/OpenDriver2/REDRIVER2/releases/tag/8.0) before
  editing Unreleased. No release version/date or migration completion changed.
- Final parent/fork `git diff --check` passed; temporary panel/key-event probes
  are absent from final sources. CURRENT_STATUS/RECENT_CONTEXT remain under350 lines.

## Remaining work

Continue only the migration block requested next. Broader presentation coverage,
complete regional mission/cutscene/re-record acceptance, other supported
platforms and eventual renderer/service cutover remain separate scopes.

## Publication boundary (2026-10-09)

This FPS/effects publication includes clock/snapshot/control dependencies and
reachable live-VSync fork commit 76d83df. Earlier input-service, versioned replay
metadata and native renderer migration edits remain separate in the working tree.
The earlier metadata/round-trip evidence above describes those local builds;
it does not claim that this scoped commit publishes a new replay format.
