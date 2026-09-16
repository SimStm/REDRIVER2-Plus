---
type: Product
title: Playable testing playground
description: A resident generated driving fixture used as a controlled scene for renderer and tooling work.
tags: [product, playground, world, rendering, testing]
---

# Playable testing playground

The playground is a finite, resident, procedurally generated driving scene. It
reuses the original vehicle simulation and the existing renderer, and replaces
the loaded world's surface, roads, geometry and collisions with generated data.
It exists to give renderer work (roadmap item 14) and asset tooling (items 04,
05, 06) a controlled, reproducible scene.

Scene identity: **`playground.flatpad.v1`**.

## Entering and leaving

- **Take a Ride:** on the city screen, select **Playground** (below Rio). It
  calls the same launcher as everything else and works in every build,
  including `Release`.
- **Developer entry point:** `Debug` and `Release_dev` accept
  `-playground` (see [Command-line parameters](../../README.md#command-line-parameters)).
- **Exit:** the in-game pause menu's quit returns to the frontend as usual. The
  launch request is cleared on return, so a later session does not silently
  become a playground.
- **Reset:** press `R` to place the player car back at the spawn. The pause
  menu's restart also re-places the car.

## The fixture

- A flat concrete surface at height 0 with an up normal, generated as a 19x19
  grid of one-cell (2048 unit) tiles.
- Six collidable box obstacles near the spawn.
- One player car using the donor level's car model; car and time are fixed
  (take-a-ride, day). Traffic, police, missions and donor camera events are
  disabled. The donor level supplies only car, texture, sky and sound
  resources.

## Behaviour and limits

- The fixture is bounded; drive far enough and the generated surface ends.
  There is no out-of-bounds recovery beyond driving back.
- Saves and replays are **not** supported for the playground: it has no separate
  session identity, so it is never serialized as a playground session. Returning
  to the frontend discards it and the generated memory is reclaimed by the next
  level load.
- Only the desktop/OpenGL path is targeted. The car-reset key uses SDL and is
  compiled out on Android; the scene code itself is portable and inert on PSX.
- The sky/horizon is the donor city's sky dome, so the backdrop may not match
  the generated floor. Visual polish is deliberately out of scope.

## Measured costs

Measured on Windows `Release_dev` x64 (the initial target):

- 367 generated objects (361 floor tiles + 6 boxes).
- 9336 bytes of generated model, cell-object and cell data.
- Rendered through the legacy path alongside the original car; repeated 20-30 s
  sessions stayed stable with no growth.

CPU/GPU frame-time distribution and peak process memory were **not** profiled.
Only `Release_dev` x64 was exercised at runtime; the other targets were only
compile-checked through the shared source.

## Working on it

Read [Generate a coherent world for a playground or new map](../rules/world-scene-generation.md)
before changing the generator, and keep the scene identity stable so captures
remain comparable. The generator lives in
`src_rebuild/Game/C/playground.c` (`PG_BuildTileModel`, `PG_BuildBoxModel`,
`PG_BuildScene`); the shared launcher is `Playground_RequestLaunch` /
`Playground_BuildScene`. The frontend entry is installed at runtime in
`LoadFrontendScreens` (the embedded `FEscreens.inc` is not used on PC).
