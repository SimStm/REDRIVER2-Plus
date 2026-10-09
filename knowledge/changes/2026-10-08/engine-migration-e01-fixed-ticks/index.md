---
type: Change
title: E01 fixed simulation ticks owned by the project clock
created: 2026-10-08
tags: [okf, engine, timing, determinism, e01]
---

# E01 fixed simulation ticks owned by the project clock

## Outcome

### Fixed ticks (project clock)

The desktop simulation tick no longer depends on the emulated PsyCross vblank.
`utils/ProjectClock.*` derives whole fixed ticks from the real monotonic counter
(30 Hz NTSC, 25 Hz PAL), with bounded catch-up (max 4 ticks per frame; excess is
dropped and counted) and a pause API. `State_GameLoop` steps by the returned
tick count; `State_GameInit` no longer forces a 30 FPS swap interval. Gameplay
reads of the draw counter `FrameCnt` were moved to the simulation tick
(`CameraCnt`) in `camera.c`, `debris.c`, `gamesnd.c`, `objanim.c`, `overlay.c`,
`director.c` and the `main.c` startup/pause gates.

### Versioned replay contract

`REPLAY_SAVE_HEADER` keeps its on-disk size: the versioned metadata lives in the
previously unused `reserved1`/`reserved2` fields. New tapes are written with
`DRIVER2_REPLAY_MAGIC_V2` and carry an explicit format version, region, tick
rate and tick period, plus a 32-bit FNV-1a content checksum over everything
after the header (stream data, parameter block, waypoints, cameras, pings).
Legacy tapes keep `DRIVER2_REPLAY_MAGIC` and load unchanged. On load, an unknown
magic, an unsupported version, a region/rate mismatch or a checksum mismatch is
diagnosed explicitly instead of silently accepted or silently resampled to the
build cadence. A developer fixture (`-savereplay <file> -saveat <tick>`, under
`DEBUG_OPTIONS`) saves the in-progress recording without driving the pause menu
(it defaults to tick 300 when `-saveat` is omitted).

### Visual snapshots and interpolation

`utils/VisualSnapshot.*` publishes, once per tick (`VisualSnapshot_Capture` at
the end of `StepGame`), the previous and current world position of every active
car slot. `ProjectClock_GetInterpolationAlpha` exposes the fractional part of
the tick accumulator as Q8 (0..256); `DrawCar` renders each car between its two
tick poses with that factor. The seam only reads authoritative state, so it
cannot feed back into physics. A tick-counter reset or a per-axis jump beyond
3000 units (spawn/teleport/level load) drops the previous pose so no streak is
drawn. The interpolated position is applied before the culling corners, shadow
and wheels are computed, so all of them stay consistent.

### Input service extraction

`utils/InputService.*` is now the project owner of the SDL2 keyboard/game
controller polling and the input mapping: `InputService_Init` binds the two raw
PSX pad buffers, `InputService_UpdateRaw` fills them (keyboard, controller,
analog toggle, keyboard capture) and the pad state/rumble contract
(`GetPadState`/`IsAnalogCapable`/`InitActuators`/`Rumble`) replaces the libpad
calls in `Game/C/pad.c`. The mapping types and the four binding tables moved from
`PsyX_public.h` to `InputService.h` (`InputKeyboardMapping`/`InputControllerMapping`,
`g_kbGameMappings`/`g_kbMenuMappings`/`g_gcGameMappings`/`g_gcMenuMappings`),
`redriver2_psxpc.cpp` applies the active table to `g_inputKeyboardMapping`/
`g_inputControllerMapping`, and `DeveloperInputMapping`/`DeveloperGraphicsPanel`
use the project types and `InputService_SetCapture`. `InputService` has no
PsyCross include/link dependency. In the project PsyCross fork, `PsyX_main.cpp`
no longer calls `PsyX_Pad_InitSystem`, `PsyX_Pad_InternalPadUpdates` or the
controller event dispatch, so `InputService` is the single SDL controller owner;
`PsyX_UpdateInput` now only pumps events and the `PsyX_Pad_*` code stays compiled
as the libpad compatibility surface.

### Bounded backlog policy

`ProjectClock` bounds catch-up at 4 ticks per frame and counts the excess as
dropped instead of replaying an unbounded backlog. The drop is now surfaced:
`State_GameLoop` logs `ProjectClock: dropped N simulation tick(s) to bound
catch-up (total M)` (rate-limited to once per second) so discarded ticks are
never silent. `ProjectClock_SetPaused` re-bases the counter on resume so a
paused interval is not replayed as a burst, and the first presented frame
re-bases so world-setup time is not accumulated. A debug fixture
(`RED2_CLOCKJUMP_MS=<ms>`) arms a one-shot synthetic stall through the normal
bounded path.

## Determinism evidence (executed)

Canonical dump via `utils/DeterministicBaseline` (authoritative fields only) and
`scripts/engine_migration/e00_compare_state.py`, comparing E01 against the
pre-E01 E00 baselines (0 mismatches on every shared tick):

- NTSC attract replay: two E01 runs 572 shared ticks, 0 mismatches; E01 vs E00
  baseline 572 shared ticks, 0 mismatches.
- NTSC `-mission 1`: E01 vs E00 mission baseline 578 shared ticks, 0 mismatches.
- PAL attract replay: two E01 runs 466 shared ticks, 0 mismatches; E01 vs E00
  PAL baseline 466 shared ticks, 0 mismatches.
- Presentation-rate independence (NTSC attract replay, E01): vsync
  (`config.ini`, ~30 FPS) vs uncapped (`config_novsync.ini`, ~250 FPS / 4 ms
  frame time, read from the developer overlay) gave 572 shared ticks, 0
  mismatches, and the vsync run also matched the E00 baseline with 0 mismatches.
- Presentation-rate independence (NTSC `-mission 1`, E01): vsync vs uncapped
  gave 486 shared ticks, 0 mismatches; the vsync run matched the E00 mission
  baseline with 0 mismatches.
- Presentation-rate independence (PAL attract replay, E01): vsync vs uncapped
  gave 472 shared ticks, 0 mismatches; the vsync run matched the E00 PAL
  baseline with 0 mismatches.
- The dump was extended to the full E01 acceptance surface (mission/AI timers,
  message timers, `tannerDeathTimer`, and the run-length replay cursor per
  stream) and the comparator now checks those entities. A 3x-vsync + 3x-uncapped
  NTSC `-mission 1` matrix (all 12 pairs) matched with 0 mismatches, and an
  enriched PAL attract replay matched with 0 mismatches. One early mission pair
  diverged; it did not reproduce in the controlled matrix and is attributed to a
  start-up focus/pause artifact, not to the fixed-tick scheduler.
- Smoke runs render the attract replay correctly at ~30 FPS (vsync) and
  ~250 FPS (uncapped); the scene is visible in both.
- Interpolation active: the attract replay and `-mission 1` still match the E00
  baselines with 0 mismatches on all shared ticks (authoritative state is
  unaffected by the presentation interpolation), and a visual capture of the
  attract replay shows the scene and cars rendering with no streaks or artifacts.
- Input service: the attract replay and `-mission 1` still match the E00
  baselines (NTSC and PAL) with 0 mismatches, and a live keyboard test in the
  playground drove the car from `z=-222456` to `z=-203692` (351 distinct
  positions) through `InputService_UpdateRaw`, with no `InputService`/SDL
  controller errors in the log. After disabling PsyX's pad backend in the fork,
  the NTSC/PAL baselines still match and a repeat playground drive moved the car
  from `z=-222456` to `z=-197011` (211 distinct positions), confirming
  `InputService` alone drives input.
- Bounded backlog: `RED2_CLOCKJUMP_MS=2000` (a 2 s stall) produced exactly 4
  applied ticks and `dropped 56 simulation tick(s)`, confirming the cap. A
  normal attract replay still matched the E00 baseline with 0 mismatches while
  reporting its own startup streaming hitch (`dropped 16`), so the drop is both
  bounded and visible and does not change the input-determined state at each tick.

## Replay contract evidence (executed)

All runs below are `Release_dev|x64` on this branch; diagnostics read from
`REDRIVER2.log`:

- New NTSC tape (`-playground -savereplay pg.d2rp -saveat 400`): magic
  `0x1479320A`, version 2, region 0, 30 Hz, period 33333 us, checksum set; loads
  with `replay: format v2 region 0 rate 30Hz period 33333us`, no mismatch.
- New PAL tape: region 1, 25 Hz, period 40000 us; the PAL build loads it cleanly
  and the NTSC build reports
  `replay: region/rate mismatch (tape region 1 25Hz, build region 0 30Hz)`.
- Corrupt version byte: `replay: unsupported replay format version 99 (expected 2)`
  and the loader rejects the file.
- Corrupt content byte (waypoints region): `replay: content checksum mismatch`.
- Legacy regression: NTSC and PAL `ATTRACT.400` (V1) still load, and the NTSC run
  matches the E00 baseline with 0 mismatches on all shared ticks.
- Re-record: `-savereplay` writes a V2 tape with real input when the player is
  controlled (playground: 3 run-length records, `len 499`), and an empty tape
  when the player is uncontrolled for the whole window (mission intro). Replaying
  a re-recorded tape launches the scenario from the tape's `GameType`: a
  playground tape (GameType 1) replays as a normal game with traffic, so it does
  not reproduce the playground. A full mission re-record → replay reproduction
  test remains pending; the V1 attract tapes are the accepted replay evidence.

## Findings and limits

- The camera collider (`car_data[MAX_CARS+1]`, `CONTROL_TYPE_CAMERACOLLIDER`) is
  recomputed from the camera in the draw path (`RenderGame2 -> InitCamera ->
  PlaceCameraFollowCar`), confirmed in the Visual Studio debugger. It is
  presentation state and is excluded from the authoritative dump together with
  camera fields and the cosmetic indicator phases (`ap.life`/`ap.life2`, which
  the draw path advances and which must move to the presentation layer).
- Drawing every presentation frame is safe with the current draw path once the
  display unmask runs on every frame. The earlier black screen (stuck near
  `FrameCnt == 5`, ~20 FPS, `legacyVertices` near zero) was caused by gating
  `SetDispMask(1)` on `ticks > 0`: with the game drawing every frame but
  stepping only on due ticks, a presentation frame could pass `FrameCnt == 5`
  while `ticks == 0` and the display stayed masked. Moving `if (FrameCnt == 5)
  SetDispMask(1);` outside the tick gate fixed it; the draw path itself was
  never the problem.
- `FastForward` now multiplies the due tick count by 7 through the project
  clock instead of the vblank gate; presentation draws every frame regardless.
- The input extraction now has a single SDL controller owner: PsyX's pad system
  is disabled in the project fork and the game uses `InputService` for polling
  while still calling `PsyX_UpdateInput` for the SDL event pump. `InputService`
  keeps the `Pads[]`/per-tick-sampling contract; the keyboard-controller
  selection (F4) is fixed at port 0 for now.
- Dropped ticks affect wall-clock pacing, not the authoritative state sequence:
  the state at a given `CameraCnt` depends only on the inputs up to that tick, so
  a slow machine runs the simulation in more wall-clock time but produces the
  same state sequence. The startup streaming hitch (a bounded ~0.5 s here) is
  therefore visible but not a determinism break.
- The clock pause API is deliberately not wired to the pause menu: on PSX the
  vblank gate keeps producing ticks while paused (the simulation is frozen by
  `pauseflag` checks inside `StepGame` and `PauseMissionTimer`), so the desktop
  clock running during pause matches the original behavior. Freezing the clock
  would stop `CheckForPause` (it runs inside the tick loop) and change
  `CameraCnt` pacing, diverging from PSX.
- Interpolation only shifts car translation; car orientation
  (`hd.drawCarMat`), pedestrians, debris and effects still use the current tick
  pose. Extending interpolation to those consumers belongs to the E05 native
  presentation work. Interpolation is currently always on for the desktop build;
  a developer toggle is a follow-up.

## Validation actually run

MSBuild `Release_dev|x64` for NTSC and PAL; replay/mission dump runs with the
comparator (including vsync vs uncapped presentation rates); the versioned replay
round-trip and its version/region/checksum diagnostics; legacy V1 replay
regression; the car-translation interpolation with the baselines unchanged; the
bounded-backlog stall fixture and its drop log; the input-service build (before
and after disabling PsyX's pad backend in the fork) with live keyboard drive
tests and NTSC/PAL replay regressions; Vulkan smoke runs with visual screenshots
at both rates; and a Visual Studio debugger session confirming the
camera-collider update path. No performance budget or GPU capture was measured.
The PsyCross fork change is in the submodule working tree and is not committed.
