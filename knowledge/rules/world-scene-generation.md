---
type: Rule
title: Generate a coherent world for a playground or new map
description: How the legacy world/surface/cell/collision consumers fit together and what a generated scene must provide.
tags: [okf, world, levels, playground, collision, rendering]
---

# Generate a coherent world for a playground or new map

**When** adding a generated map, extending the playground, or changing the
scene launcher, **then** provide every world consumer with the same coherent
data set. Drawing is only one of five consumers: surface queries, scenery
collision, camera collision, PVS/visibility and streaming all read the world
independently. Hiding geometry without replacing the other four leaves invisible
roads and collisions active.

## The world model (verified in source)

- **Cells and regions.** Game units map to cells of `MAP_CELL_SIZE` (2048) and
  regions of `MAP_REGION_SIZE` (32) cells, so a region is 32x32 cells. Four
  "barrel" region slots (`index = (cellx/32 & 1) + (cellz/32 & 1) * 2`) hold the
  player's neighbourhood; `cell_ptrs` is 4096 `u_short` entries (4 x 1024).
- **Cell object lists.** `cell_ptrs[cbr]` is an index into `cells`. A cell's
  `CELL_DATA` entries are a run of object indices (low 14 bits) into
  `cell_objects`, terminated by `0x4000`; `0x8000` ends the whole array. With no
  event lists (`level == -1`), `GetFirstPackedCop` reads the default run.
- **Packed objects.** `PACKED_CELL_OBJECT { USVECTOR_NOPAD pos; u_short value; }`.
  `value = (slot & 0x3ff) << 6 | (yang & 63)`; the model index's bit 10 lives in
  `pos.vy & 1`; `pos.vx/vz` are the absolute world X/Z truncated to 16 bits and
  reconstructed against the cell base by `QuickUnpackCellObject`. Model slots
  are `< MAX_MODEL_SLOTS` (1536).
- **Surface queries.** `dr2roads.c` `sdGetCell` selects one of the four
  `RoadMapDataRegions` buffers. If `buffer[0] != 2` it returns `default_plane`
  (flat concrete at height 0, up normal); otherwise it decodes the packed
  level surface/BSP. `MapHeight`, `FindSurfaceD2`, `RoadInCell` and
  `GetSurfaceIndex` all funnel through it. Wheel forces (`wheelforces.c`) use
  `FindSurfaceD2`.
- **Collision.** `objcoll.c` builds candidates from `GetFirstPackedCop` and
  reads each model's `collision_block`: an `int` count followed by
  `COLLISION_PACKET` entries (`type, xpos, ypos, zpos, flags, yang, empty,
  xsize, ysize, zsize`). Drawing a box does not register it as solid; the
  collision packet does. Camera collision uses the same packets through the
  camera collider.
- **Models (PC, non-PSX).** `MODEL` fields `vertices`, `normals`,
  `point_normals`, `poly_block`, `collision_block` are byte offsets from the
  `MODEL` base; `instance_number` must be `-1` for non-instances. Polygons are
  keyed by `id & 31` and advanced by `PolySizes[ptype]`; only types 11, 21 and
  23 are drawn by the building renderer. Type 21 is a flat quad whose size is
  20 bytes (`POLYFT4` layout, including the trailing `RGB`), not the 16-byte
  `PL_POLYFT4`. The legacy renderer culls by winding, so emitting both windings
  keeps generated quads visible.
- **Resource lifetime.** Level allocations use the `D_MALLOC` bump arena and are
  released by the next `NewLevel` reset. Generated model slots must set their
  `permanentModelSlotBitfield` bit so `CleanSpooledModelSlots` does not drop
  them.

## Rules for a generated scene

1. **Replace, do not hide.** Point all four `RoadMapDataRegions` at a buffer
   whose first `short` is not `2`, and rebuild `cell_ptrs`/`cells`/`cell_objects`
   so the original world cannot leak into rendering or collision.
2. **Repoint the globals.** Allocate generated `cells`/`cell_objects` and assign
   the global pointers; writing only to local arrays leaves the walkers reading
   the donor arrays.
3. **Stop and guard streaming.** Set `doSpooling = 0`, `allowSpecSpooling = 0`,
   `startSpecSpool = -1`, and guard `UnpackRegion`, `GotRegion` and
   `CheckSpecialSpool` while the generated scene is active, or a donor region
   completion silently overwrites `cell_ptrs` and `RoadMapDataRegions`.
4. **Clear events and missions.** Donor `events.cameraEvent` suppresses the
   follow camera in `InitCamera`, so call `InitEvents()` and set
   `Mission.active = 0`; disable traffic/police (`maxCivCars`, `maxCopCars`,
   `CopsAllowed`, `gDontPingInCops`).
5. **Fill visibility.** A generated scene has no encoded PVS. Provide a filled
   `CurrentPVS` (via `SetupDrawMapPSX`) instead of decoding a donor region.
6. **Respect draw limits.** Generated objects are drawn as buildings
   (`MAX_DRAWN_BUILDINGS`, 384 on desktop) or tiles (`MAX_DRAWN_TILES`, 750);
   keep the resident count inside those budgets or geometry silently drops.
7. **Keep one launcher.** Any entry point (developer CLI, panel, Take a Ride)
   must call the same `Playground_RequestLaunch`/`BuildScene` operation and let
   the state machine perform the transition; never swap world allocations inside
   a draw or UI callback.

## Extending the playground

- Floor and obstacles are generated by `Game/C/playground.c`
  (`PG_BuildTileModel`, `PG_BuildBoxModel`, `PG_BuildScene`). The scene identity
  is `playground.flatpad.v1`; keep it stable so captures stay comparable.
- Reusing real city meshes for streets is possible but requires a resource
  adapter: model indices, LOD tables and texture pages belong to the loaded
  resource context, so cross-city reuse needs stable source identities and
  explicit lifetimes. Generator shapes should come first.
- Validate changes at the boundaries as well as the spawn: drive to the grid
  edge, collide with a box, reset the car, exit to the frontend, and enter an
  original city afterwards to confirm no state leaked.
