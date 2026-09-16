---
type: Change
title: Implement the resident playground scene (roadmap item 13, P1-P5)
description: Generate a flat drivable fixture that replaces the loaded world while reusing donor car, texture and sound resources.
tags: [okf, playground, world, rendering, testing, spool, frontend]
---

# Implement the resident playground scene (roadmap item 13, P1-P5)

## Context

Roadmap item 13 needs a bounded, reproducible driving environment for renderer
experiments without depending on original-city streaming, visibility and
mission setup. The discussion established that a floor's visible mesh, wheel
surface queries and scenery collision are separate consumers and that the
loader is not a generic level registry.

## Decision

Add a game-owned module (`src_rebuild/Game/C/playground.c` and `playground.h`)
and a shared `Playground_RequestLaunch` / `Playground_BuildScene` operation:

- **Surface adapter.** Point the four `RoadMapDataRegions` at a buffer whose tag
  is not `2`, so the existing `sdGetCell` returns `default_plane`: a flat
  concrete surface at height 0 with an up normal. No original roads are left
  active.
- **World replacement.** Allocate generated `cells`/`cell_objects`, repoint the
  global pointers, clear `cell_ptrs`, register two procedurally built models (a
  one-cell ground tile and a collidable box) at reserved model slots, and place
  19x19 floor tiles plus six boxes as cell objects. Rendering, scenery collision
  and camera collision all read the same generated structures.
- **Isolation.** Disable spooling, special-car spooling and traffic/police, set
  `Mission.active = 0`, and call `InitEvents()` so donor camera events cannot
  suppress the follow camera or draw original-world objects. `GotRegion`,
  `UnpackRegion` and `CheckSpecialSpool` are guarded so a donor stream cannot
  overwrite the generated world.
- **Lifecycle.** The donor level supplies car models, textures, sky and sounds
  only. `Playground_Shutdown` clears state when returning to the frontend, and
  the allocation arena is reclaimed by the next `NewLevel` reset.
- **Entry points.** `-playground` under `DEBUG_OPTIONS` and a **Playground**
  choice on the Take a Ride city screen (installed at runtime in
  `LoadFrontendScreens`, working in every build) both call the shared launcher.

Scene identity is `playground.flatpad.v1`. Operational behaviour is in
[`knowledge/product/playable-testing-playground.md`](../../../product/playable-testing-playground.md)
and the reusable technique in
[`knowledge/rules/world-scene-generation.md`](../../../rules/world-scene-generation.md).

## Impact and evidence

- `State_GameInit` builds the scene after the donor level is initialised;
  `SetupDrawMapPSX` fills visibility for the fixture; `StepGame` ticks the reset
  control; `ReInitFrontend` shuts it down.
- Windows `Release_dev` x64 links cleanly and logs
  `Playground: scene playground.flatpad.v1 active (367 objects, spawn
  6216,-222456, 9336 bytes generated)`.
- Instrumented draw diagnostics confirmed the cell walker returned only the
  generated slots (1500/1501) after the globals were repointed; diagnostics were
  removed before the final build.
- The frontend entry was verified installed as
  `action=0x200 var=0x1000 name=Playground`; it shares the launcher whose runtime
  path is exercised by `-playground`.

## Limitations

- CPU/GPU profiling and automated menu navigation were not performed.
- Only the Windows `Release_dev` x64 configuration was exercised at runtime;
  other targets are compile-guarded.
- Visual sky/horizon artifacts remain because the donor sky is retained.
- Reusing real city meshes for streets is deferred (see the rules document).

## Validation

- Built `Release_dev` x64 after each change; final build 0 failed projects.
- Ran with `-nointro -nofmv -playground` for 20-30 s repeatedly; the process
  stayed alive and the scene log was emitted once per launch.
- `git diff --check` passed for the source and documentation changes.
