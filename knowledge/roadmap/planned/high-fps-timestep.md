---
type: Roadmap
title: High-FPS rendering with fixed 30 Hz simulation
status: planned
execution_order: 17
tags: [roadmap, timing, frame-rate, renderer, replays]
---

# High-FPS rendering with fixed 30 Hz simulation

## Problem

The desktop loop gates simulation and rendering with the same check:
`FilterFrameTime()` (`src_rebuild/Game/C/main.c:1284-1296`) admits one iteration
every two vblanks (30 FPS NTSC / 25 FPS PAL), and `State_GameLoop()`
(`main.c:1565-1624`) runs `StepGame()` and `DrawGame()` in that single
iteration. Removing the gate makes the simulation run at display rate: car
physics, AI, felony, event timers, mission timers and replay tapes all advance
about twice as fast, which is the reported "extremely fast, broken replays and
missions, unplayable" behaviour. There is no delta-time abstraction anywhere in
game simulation; every gameplay counter is frame-counted and several visual
effects are coupled to the render frame counter.

The intended result is therefore not a higher simulation rate but a **fixed
30 Hz simulation with presentation at the display refresh rate (60+ FPS)**,
plus render-side interpolation so the higher rate is visually real.

## Intended behaviour

- Simulation advances exactly once per 30 Hz step (the native PSX rate), so
  replays, missions, AI and physics keep their established behaviour and
  wall-clock timing.
- The renderer presents every accepted vblank up to the display refresh,
  independent of simulation cadence, using an interpolation alpha between the
  previous and current simulation state for smooth motion.
- High-FPS mode is opt-in while the milestones are validated (developer panel /
  `developer_graphics.ini` and/or a launch flag). The default stays 30 FPS until
  every acceptance criterion passes; changing the default is a separate
  decision.
- A **Take a Ride** session on any city/map is the primary low-risk validation
  surface: free roam exercises car physics, traffic, scenery, damage and cop
  behaviour without involving replays or missions. The Playground fixture is
  not required for this and is not the only place the mode must work.

## Verified coupling evidence (inspected revision `ecf3d4eb`, submodule `647ea0b`)

Simulation is frame-indexed and must stay one tick per 30 Hz step:

- `src_rebuild/Game/C/replays.c:368-442` - `Get()`/`Put()` consume/produce one
  record per call; `PADRECORD.run` is a frame repeat count with no time field,
  and analogue input is a 15-level compression table (`replays.c:15-18`).
  `cjpPlay` is called once per sim step (`main.c:1033-1138`).
- `src_rebuild/Game/C/mission.c:1077-1140` - `HandleTimer` adds/subtracts 100
  per call with 3000 units = 1 second (30 frames x 100); message timers are
  `seconds * 30` (`mission.c:1210,1227`, decremented at `mission.c:3217-3221`).
- `src_rebuild/Game/C/cutscene.c:260-263` - chase timer is
  `(length / 30) * 3000`, an explicit 30 fps assumption on the replay tape.
- AI, physics and world counters (`handling.c:279,893-944`,
  `cop_ai.c:877-1036`, `civ_ai.c:1655-1686`, `felony.c:248-264`,
  `event.c:1751-1871`, `debris.c:3889-3969`) integrate per call with no dt.
- `src_rebuild/Game/C/main.c:1164` - `CameraCnt++` once per sim step; this is
  the timeline used by traffic, AI, cutscenes and replays.

Render-rate coupling that changes speed or effects at a higher render rate:

- `src_rebuild/Game/C/main.c:2372` - `HandleDebris()` is called from the scene
  render path and mutates smoke/debris integration, life, transparency and the
  police light position (`debris.c:3504-3525`); at 60 renders/s these run 2x.
- `src_rebuild/Game/C/debris.c:2251-2259,2348` - bullet trails and lamp clocks
  index 4-slot rings by `FrameCnt`; trail length halves at 60 FPS.
- `src_rebuild/Game/C/debris.c:3968,3975` - cop light flash `FrameCnt & 7`;
  `src_rebuild/Game/C/director.c:2281-2284` - director strobe
  `FrameCnt & 0x1f`; `src_rebuild/Game/C/gamesnd.c:1562,2584` - sound pitch and
  randomization read `FrameCnt`.
- `src_rebuild/Game/C/objanim.c:238` - `ColourCycle()` is sim-called
  (`main.c:1457-1458`) but skips on `FrameCnt & 1`; with 60 FPS render and
  30 Hz sim, consecutive calls see the same parity and the palette cycle
  doubles in speed.
- `src_rebuild/Game/C/overlay.c:522-527` - map flash uses `FrameCnt`, the
  non-map path uses `CameraCnt`.
- `src_rebuild/Game/C/camera.c:224,235` - view-change debounce reads
  `FrameCnt`; `src_rebuild/Game/C/system.c:649,681` - draw-buffer parity uses
  `FrameCnt & 1` (safe only because one swap equals one increment).

Pacing and presentation:

- `src_rebuild/Game/C/main.c:827-829` - `State_GameInit` requests
  `PsyX_SetSwapInterval(2)`; on Vulkan the interval is ignored
  (`src_rebuild/PsyCross/src/render/PsyX_render.cpp:485-496`) and present is
  always `VK_PRESENT_MODE_FIFO_KHR` (`PsyX_Vk.cpp:1185`, env override
  `:1186-1218`).
- `src_rebuild/PsyCross/src/PsyX_main.cpp:180-210` - vblank counter thread at
  60 Hz NTSC / 50 Hz PAL; `VSync(-1)` is a non-blocking read
  (`src_rebuild/PsyCross/src/psx/LIBETC.C:31-49`).
- `src_rebuild/Game/C/main.c:1612-1614` - desktop already supports
  `FastForward ? 7 : 1` sim steps per rendered frame, which de-risks the
  scheduler change.
- PAL currently runs 25 sim steps/s while the timer math assumes 30; the new
  scheduler should derive the intended simulation rate explicitly instead of
  vblank parity.

## Scope

Milestones, executed sequentially. A request for one milestone is not
authorization for the later ones.

1. **M1 - Fixed-timestep scheduler (opt-in).** Split simulation gating from
   rendering in `State_GameLoop`: run the fixed sim step when two vblanks have
   elapsed, render on every accepted vblank, and track the remainder for an
   interpolation alpha. Set the swap interval to 1 for the mode and wire the
   Vulkan present-mode decision for high-refresh displays. Keep `FastForward`
   as N sim steps per render. No interpolation yet.
2. **M2 - Counter separation and draw-path cleanup.** Introduce a render frame
   counter distinct from the simulation step counter; convert every
   `FrameCnt` reader in the draw path (trails, strobes, flashes, sound
   randomization, view-change debounce, buffer parity) to documented
   sim/render semantics; move state mutation out of `HandleDebris` into the sim
   step so drawing only emits primitives.
3. **M3 - Render-side interpolation.** Interpolate camera and player car
   transforms between the previous and current sim state using the M1 alpha,
   then extend to other vehicles. The replay tape cannot provide sub-frame
   data, so interpolation is render-side only.
4. **M4 - Validation and exposure.** Run the full acceptance suite (below),
   document the mode in the developer panel and product doc, and decide
   separately whether it becomes default.

## Non-goals

- Running simulation above 30 Hz. It would require converting every
  frame-counted timer, AI, physics and event to time-based units plus a new
  replay format, and it is what makes the game break.
- Changing recorded replay data, replay format, or interpolating input tapes.
- Making high-FPS the default before the acceptance criteria pass.
- Exceeding display refresh on Vulkan without an explicit present-mode
  decision.
- Behaviour or timing changes on the PSX target.
- Rewriting reconstructed game logic semantics beyond the counter/mutation
  separation needed for M2.

## Dependencies and risks

- Prerequisite: none hard. M2 depends on M1; M3 depends on M2.
- Related: Vulkan present-mode/vsync work in the completed
  [Vulkan game renderer](../done/vulkan-game-renderer.md); developer settings
  persistence in `DeveloperGraphicsSettings.*`.
- Risk: touching reconstructed render code for interpolation can change
  reverse-engineered visuals; interpolate presentation state only, never
  simulation state.
- Risk: two-player split-screen renders twice per frame
  (`main.c:1629-1651`); pacing and interpolation must handle both viewports.
- Risk: attract mode and the replay theater may be expected to look exactly
  30 Hz; decide per surface whether interpolation applies.
- Risk: at 60 FPS the sim/AI step remains 30 Hz, so input latency does not
  improve by itself; only presentation smoothness does. This must be stated
  honestly in the product doc.
- Risk: determinism of replay comparison must be proven before claiming parity;
  world traffic randomness and attract mode are not part of the comparison.

## Acceptance criteria

1. The same saved replay played in 30 FPS and high-FPS modes produces
   identical per-`CameraCnt` simulation outcomes (car positions/hash) and the
   same wall-clock duration.
2. A timed mission and a chase/escape mission show identical wall-clock timer
   behaviour in both modes.
3. `FrameCnt`-driven visual effects (bullet trails, cop flash, director strobe,
   map flash, palette cycling) have the same duration in both modes.
4. A Take a Ride session on at least one original city map runs physics,
   traffic, scenery, damage and cop behaviour correctly at high FPS with no
   visible 2x-speed effects.
5. `-vkpsxtest` reports PASS; `scripts/run_inspector_tests.ps1` passes; the
   game builds and runs on `-opengl` and `-vulkan`.
6. Two-player split-screen and frontend/pause states remain responsive and
   correct.
7. The mode is opt-in while implemented; default configuration is unchanged.

## Validation plan

- Replay comparison harness: run a scripted start, play the same replay in both
  modes, sample `car_data` positions every N sim steps and compare.
- Mission timer check: timed mission and a chase mission; compare wall-clock
  end times.
- Controlled captures of the `FrameCnt`-driven effects at 30 vs 60 render rate.
- Take a Ride runs on at least one original map (not only the playground).
- Build matrix: VS `Release_dev` (Vulkan) and `Release_dev_gl` (OpenGL);
  NTSC default; note PAL behaviour separately.
- `git diff --check` and the submodule status check before any commit.

## Starting points

- `src_rebuild/Game/C/main.c` (`FilterFrameTime`, `State_GameLoop`,
  `DrawGame`, `StepSim` counters).
- `src_rebuild/Game/C/replays.c`, `mission.c`, `cutscene.c`, `debris.c`,
  `objanim.c`, `system.c`, `overlay.c`, `camera.c`, `director.c`.
- `src_rebuild/PsyCross/src/PsyX_main.cpp`, `src/psx/LIBETC.C`,
  `src/render/PsyX_render.cpp`, `src/render/PsyX_Vk.cpp`.
- `src_rebuild/utils/DeveloperGraphicsSettings.*`,
  `DeveloperGraphicsPanel.cpp`, `src_rebuild/redriver2_psxpc.cpp` (launch
  flags).
  Paths are relative to the repository root. Inspect current source and local
  diffs before editing; this record is not proof of current behavior.

## Handoff and completion

For each milestone report changed files, checks actually run, remaining
limitations and the next unblocked milestone. Keep changes narrow and preserve
the game's established wall-clock behaviour at 30 Hz. Partial delivery keeps
`status: planned` and records completed milestones with evidence without
implying the whole feature is done. Only after all acceptance criteria are met,
publish `knowledge/product/high-fps-timestep.md`, move this record to `done/`,
update status/date/catalog links, and record the delivered change in
Unreleased.

## References

- Discussion: [`knowledge/discussions/high-fps-timestep/index.md`](../../discussions/high-fps-timestep/index.md).
