---
type: Discussion
title: High-FPS rendering with fixed 30 Hz simulation
status: exploring
created: 2026-09-20
updated: 2026-09-20
tags: [discussions, timing, frame-rate, replays, missions, psycross]
---

# High-FPS rendering with fixed 30 Hz simulation

## Context and decision status

The user asked whether the fork can support 60+ FPS without the game breaking,
and where exactly the coupling is. Naively removing the frame gate makes the
game run at display rate (~60 step/s), which doubles physics/AI/mission/replay
speed and breaks escape missions because they are replay tapes.

**No implementation is adopted.** The user adopted the direction as a planned
scope on 2026-09-20: the full spec lives in
[`knowledge/roadmap/planned/high-fps-timestep.md`](../../roadmap/planned/high-fps-timestep.md)
(execution order 17). A planned record is intent only and is not evidence that
any of it exists.

**Current recommendation:** a bounded roadmap item that *decouples render rate
from a fixed 30 Hz simulation* (fixed-timestep scheduler plus render-side
interpolation), instead of raising the simulation rate. Raising the sim rate
would require converting every frame-counted timer and the replay tape format
to time-based units and is not recommended.

Inspected revision: parent `ecf3d4eb` (clean tree), PsyCross submodule
`647ea0b` on `redriver2-plus`.

## Verified evidence from the current checkout

Frame pacing and loop:

- `src_rebuild/Game/C/main.c:1284-1296` - `FilterFrameTime()` is the hard gate:
  it returns 0 unless `VSync(-1) - frame >= 2`. `VSync(-1)` is a non-blocking
  read of the emulated vblank counter, so the gate yields 30 FPS NTSC / 25 FPS
  PAL regardless of renderer load.
- `src_rebuild/PsyCross/src/PsyX_main.cpp:180-210` - vblank counter thread at
  60 Hz NTSC / 50 Hz PAL (`PsyX_globals.h:8-9`). `VSync(0)` busy-waits one
  vblank; `VSync(n>0)` does not wait n times (`src_rebuild/PsyCross/src/psx/LIBETC.C:31-49`).
- `src_rebuild/Game/C/main.c:1565-1624` - `State_GameLoop()` runs
  `StepGame()` and `DrawGame()` in the same gated call: one simulation step and
  one rendered frame per accepted gate. There is no accumulator and no delta
  time anywhere in game simulation.
- `src_rebuild/Game/C/main.c:1612-1614` - desktop executes exactly
  `FastForward ? 7 : 1` steps per rendered frame. Multi-step-per-render already
  exists as a debug path, which de-risks the scheduler change.
- `src_rebuild/Game/C/main.c:827-829` - `State_GameInit` requests
  `PsyX_SetSwapInterval(2)` (two display refreshes) on desktop. On Vulkan the
  interval is ignored (`src_rebuild/PsyCross/src/render/PsyX_render.cpp:485-496`)
  and present is always `VK_PRESENT_MODE_FIFO_KHR`
  (`src_rebuild/PsyCross/src/render/PsyX_Vk.cpp:1185`, env override at
  `:1186-1218`).
- `src_rebuild/Game/C/main.c:1164` - `CameraCnt++` once per simulation step
  inside `StepSim`; this is the timeline used by traffic/AI/cutscenes/replays.
- `src_rebuild/Game/C/main.c:1666` - `FrameCnt++` once per rendered frame in
  `DrawGame`.

Simulation is frame-indexed (must stay one tick per 30 Hz step):

- `src_rebuild/Game/C/replays.c:368-442` - `Get()`/`Put()` consume/produce
  exactly one record per call; `PADRECORD.run` is a frame repeat count with no
  time field, and analogue input is compressed to 15 levels
  (`replays.c:15-18`). `cjpPlay` is called once per sim step from
  `main.c:1033-1138`.
- `src_rebuild/Game/C/mission.c:1077-1140` - `HandleTimer` adds/subtracts 100
  per call with 3000 units = 1 second (30 frames x 100). Message timers are
  `seconds * 30` (`mission.c:1210,1227`), decremented once per sim frame
  (`mission.c:3217-3221`).
- `src_rebuild/Game/C/cutscene.c:260-263` - chase timer is computed as
  `(length / 30) * 3000`, an explicit 30 fps assumption on the replay tape.
- AI/physics/counters (`handling.c:279`, `handling.c:893-944`, `cop_ai.c:877-1036`,
  `civ_ai.c:1655-1686`, `felony.c:248-264`, `event.c:1751-1871`,
  `debris.c:3889-3969`) all integrate per call with no dt.

Render-rate coupling (the part that silently changes speed at 60 FPS):

- `src_rebuild/Game/C/main.c:2372` - `HandleDebris()` is called from the scene
  render path, not the sim step. It mutates smoke/debris state (integration,
  life, transparency) and increments police light position at
  `debris.c:3504-3525`; at 60 renders/s these simulations run twice as fast.
- `src_rebuild/Game/C/debris.c:2251-2259,2348` - bullet trails and lamp clocks
  index 4-slot ring buffers by `FrameCnt`; trail length halves at 60 FPS.
- `src_rebuild/Game/C/debris.c:3968,3975` - cop light flash uses
  `FrameCnt & 7`; `src_rebuild/Game/C/director.c:2281-2284` - director strobe
  uses `FrameCnt & 0x1f`; `src_rebuild/Game/C/gamesnd.c:1562,2584` - sound
  pitch/randomization reads `FrameCnt`.
- `src_rebuild/Game/C/objanim.c:238` - `ColourCycle()` (palette cycling) is
  called from the sim step (`main.c:1457-1458`) but skips on `FrameCnt & 1`.
  With 60 FPS render and 30 Hz sim, consecutive calls see the same parity, so
  the cycle doubles in speed.
- `src_rebuild/Game/C/overlay.c:522-527` - map flash uses `FrameCnt` while the
  non-map path uses `CameraCnt`.
- `src_rebuild/Game/C/camera.c:224,235` - view-change debounce compares
  `FrameCnt`; `src_rebuild/Game/C/system.c:649,681` - draw-buffer parity uses
  `FrameCnt & 1` (safe only because one swap equals one increment).

## Why a naive FPS unlock breaks the game

Removing `FilterFrameTime()` makes the sim advance at display rate. Every
system above then runs ~2x: car physics integration, cop/traffic timers, felony
decay, event timers, bomb detonators. Replay tapes (`Get`/`Put`) play twice as
fast and occupy half the wall-clock time, mission timers drain twice as fast
(`HandleTimer`), and escape/chase segments built from replay streams
(`cutscene.c:231-275`) desynchronize from their scripts. That is the reported
"extremely fast, broken replays and missions, unplayable" behaviour.

## Gap analysis for "render at 60+ with a correct game"

1. **Scheduler (core change).** `State_GameLoop` must run the fixed sim only
   when 2 vblanks have elapsed and render on every vblank, tracking an
   accumulator/remainder for the interpolation alpha. `FilterFrameTime` must
   stop gating input (`pause.c:528`) and rendering. `FastForward` keeps working
   as N sim steps per render.
2. **Counter separation.** Introduce a sim step counter distinct from a render
   frame counter. `CameraCnt` stays sim-owned; `FrameCnt` consumers in the draw
   path (`HandleDebris`, trails, strobes, flash, sound randomization, buffer
   parity) must either read the sim counter or a new render counter with
   documented semantics. Buffer parity must remain tied to actual swaps.
3. **Draw-path state mutation.** Move smoke/debris integration, police light
   position and weather stepping out of `HandleDebris` into the sim step;
   `HandleDebris` should only emit primitives. Same audit for any other
   `Draw*` function that writes simulation state.
4. **Judder vs interpolation.** Rendering at 60 while sim stays 30 shows each
   simulation state twice (or unevenly). Without interpolation the result is
   smoother presentation of HUD/static elements but visible stutter on the
   camera/cars. Real perceived smoothness needs a render-side transform
   interpolation between the previous and current sim state for camera and
   player car at minimum, ideally other moving vehicles; replay tapes provide
   no sub-frame data (`replays.c:15-18`), so interpolation cannot come from
   the tape.
5. **Platform pacing.** Swap interval must be 1 (or unset for renderer FIFO)
   in the new mode; Vulkan FIFO caps at display refresh, and high-refresh
   displays need mailbox/immediate handling. PAL currently yields 25 steps/s
   while the timer math assumes 30 (`cutscene.c:260`, `mission.c:1137-1139`);
   the new scheduler should derive the sim rate explicitly instead of vblank
   parity.
6. **Split-screen and menus.** Two-player mode renders twice per frame
   (`main.c:1629-1651`); the scheduler must keep one sim per 2 vblanks and
   keep frontend/pause states responsive at render rate.

## Recommended staged scope (proposal, not adopted)

- **M1 - Fixed-timestep scheduler (dev-gated).** Split sim and render cadence,
  keep 30 Hz sim, render at vblank rate, unify swap interval. No interpolation.
  Success criterion: replays and missions behave identically in wall-clock
  time; visual smoothness gain is limited but the architecture is correct.
- **M2 - Counter/mutation cleanup.** Separate sim and render counters, move
  draw-path state mutation into the sim step, audit every `FrameCnt` reader.
  Success criterion: visual effect durations identical at 30 vs 60 FPS renders;
  `-vkpsxtest` and inspector tests still pass.
- **M3 - Camera/car interpolation.** Render-side alpha blending of the previous
  and current transform for the camera and player car, then other vehicles.
  Success criterion: continuous motion at 60+ FPS with no physics or replay
  divergence.

A cheaper bounded alternative is an optional high-FPS mode validated in a
**Take a Ride** session on any city/map, not only in the Playground. Free roam
exercises car physics, traffic, scenery and cop behaviour without involving
replays or missions, which validates M1/M2 while leaving mission compatibility
untouched. The user explicitly noted on 2026-09-20 that this validation surface
does not need to be restricted to the Playground fixture.

## Non-goals

- Running simulation above 30 Hz (breaks replays, mission timers, AI and
  physics; would require a rework of every frame-counted timer and a new replay
  format).
- Changing recorded replay data or interpolation of the input tape.
- Exceeding the display refresh rate on the Vulkan path without an explicit
  present-mode decision.

## Validation plan (for the roadmap item, if adopted)

- Scripted replay comparison: play the same saved replay at 30 and 60 FPS
  render modes, compare per-`CameraCnt` car positions/hashes and final mission
  outcome; wall-clock duration must match.
- Mission timer wall-clock check for a timed mission and a chase/escape
  mission (`HandleTimer`, `cutscene.c:260`).
- Controlled captures of `FrameCnt`-driven effects (bullet trails, cop flash,
  director strobe, map flash) at both render rates.
- `-vkpsxtest` PASS, `scripts/run_inspector_tests.ps1`, and a `-opengl` vs
  `-vulkan` spot check.

## Open questions

- Is interpolation acceptable to add on top of reconstructed render code
  without changing reverse-engineered behaviour, or should high FPS be limited
  to HUD/menu smoothness (no world interpolation)?
- Should the feature be always-on or opt-in (developer panel / `config.ini`)?
- How should attract mode and the replay theater be treated - fixed 30 render
  for authenticity, or interpolated?
- Does the PSX target need the same scheduler behind `#ifdef PSX`, or is
  high-FPS desktop-only?

## History

- 2026-09-20: Created. User asked for the exact gap between the current 30 Hz
  loop and 60+ FPS support. Verified the loop gate, vblank source, swap
  interval, replay/mission/timer coupling and draw-path `FrameCnt` usage in the
  source. Recommendation recorded: fixed 30 Hz sim + render-rate presentation
  with staged M1-M3; no roadmap item adopted yet.
- 2026-09-20: Scope adopted as roadmap record 17
  (`knowledge/roadmap/planned/high-fps-timestep.md`) with the verified evidence,
  milestones, non-goals, acceptance criteria and validation plan. The cheap
  validation alternative was generalized: Take a Ride on any city/map, not only
  the Playground, is sufficient to exercise physics, traffic, scenery and cop
  behaviour without replays or missions.
