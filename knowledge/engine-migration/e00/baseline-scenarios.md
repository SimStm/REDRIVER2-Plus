---
type: Plan
title: E00 - Baseline input/tick scenarios
status: in-progress
created: 2026-10-08
updated: 2026-10-08
tags: [engine, baseline, scenarios, replays]
---

# E00 - Baseline input/tick scenarios

Repeatable routes that exercise the contracts in
[dependency-inventory.md](dependency-inventory.md). They become the E01
determinism fixtures and the E06 regression matrix. A scenario is only
"captured" once canonical tick state is recorded; booting the scene alone is
not evidence.

## Command surface (already implemented)

`redriver2_main` accepts (see `Game/C/main.c`): `-ini`, `-cdimage`, `-nointro`,
`-nofmv`, `-replay <file.d2rp>`, and under `DEBUG_OPTIONS` (`Release_dev`):
`-mission N`, `-gametype N`, `-level N`, `-chase N`, `-startpos x z`,
`-startdir a`, `-players N`, `-playercar N`, `-player2car N`, `-playground`.
`CUTSCENE_RECORDER` adds `-recordcutscene <ini>` and
`-chaseautotest <ini>`. `DeveloperDebugStart` replays a saved
mission/city/vehicle/position snapshot when no command line was given
(`scripts/run_debug_start.ps1` generates the arguments).

## Scenarios

| ID | Route | Command / entry | Contracts exercised |
|---|---|---|---|
| S1 | Take a Ride, Chicago, default car | `-nointro -nofmv` then frontend "Take a Ride" | frontend, level load, tick loop, HUD |
| S2 | Resident playground | `-playground` | flat surface/collidable boxes, reset/return, donor data |
| S3 | Mission start, fixed spawn | `-mission 1 -level 0 -startpos 4785 -223340 -startdir 0` | mission setup, traffic, timers, HUD |
| S4 | Attract/chase replay | `-replay DRIVER2\REPLAYS\ATTRACT.400` (or `CUT*.R`) | replay header/streams, run repeats, camera director |
| S5 | Collision/damage | S3 + drive into a fixed obstacle (`-startpos` next to it) | GTE handling, bcollide, denting, damage HUD |
| S6 | Police/traffic | mission with pursuit (e.g. `-mission 24`) | cop AI, felony, roadblocks, sirens |
| S7 | Pause/loading | S3 + `Escape` pause menu, continue, restart | pause ownership, sound pause, reload |
| S8 | Region transition | `-mission N -level 1` (Havana) after Chicago | level swap, streaming, `SetCityType`, spool |
| S9 | Weather/night | S3 with time-of-day/weather from mission data | rain particles, lightning, lights-on state |
| S10 | Replay re-record | S3 + `-recordcutscene` (Release_dev) | recorder format, tick repeats, cutscene events |

## Determinism rules

- Same command line and same tick inputs must produce equal canonical state at
  the same tick ID: positions/velocities of player + active cars, collision and
  damage values, mission/AI timers, `CameraCnt`, RNG cursor, replay cursor.
- Do not hash padding, pointers, wall-clock or logging fields.
- Run at 25/30/60/120/144 FPS and uncapped presentation once E01 separates
  clocks; equal tick inputs must yield equal states and only add intermediate
  poses. Injected uneven frames must follow the explicit backlog policy.
- Record the libc `rand` sequence as a first-class fixture: `srand(0x1234)`
  must reproduce the same stream on every supported toolchain, or be replaced
  by a project RNG with a migration note (E01 decision).

## Current capture status (2026-10-08)

- Fresh isolated builds (`build_e00` NTSC, `build_e00_pal` PAL,
  `Release_dev|x64`) boot and run.
- `utils/DeterministicBaseline` + `scripts/engine_migration/e00_compare_state.py`
  now provide canonical per-tick checkpoints. Executed, all with 0 mismatches
  between independent runs of the same command:
  - S4 chase replay NTSC (`ATTRACT.400`): 714 shared ticks; PAL: 595;
    NTSC-vs-PAL: 597 shared ticks identical.
  - S3 mission start NTSC (`-mission 1`): 726 shared ticks.
- `.R` cutscene/chase files are a different format from `-replay` tapes; loading
  one through `-replay` is rejected explicitly (recorded).
- Still pending: S5/S6/S7/S8 interactive routes with recorded input, S10
  re-record, PAL-of-NTSC-tape diagnostics for non-identical cases, GPU captures.

