---
type: Roadmap
title: Playable testing playground for renderer development
status: planned
execution_order: 13
created: 2026-09-15
tags: [roadmap, playground, levels, testing, rendering]
---

# Playable testing playground for renderer development

## Problem

Original cities combine streaming, mission setup, complex visibility and asset
dependencies, making isolated graphics experiments harder to reproduce. A
dedicated small driving environment would provide controlled geometry and
camera positions for modern meshes, materials and lighting experiments.

## Intended behaviour

Provide a finite, resident procedural test map with a flat driving surface,
one original player car and a small set of generated collidable obstacles.
Players can reset the car and return to the frontend. The completed feature
is selectable from Take a Ride and has a reproducible developer launch path.

The existing PsyCross/OpenGL path renders the initial environment. Modern
rendering is a consumer of this test map, not a prerequisite for creating it.

## Adoption and related records

Adopted for planning on 2026-09-15 at the user's request: implement the
playground first as the test environment for renderer modernization. This
request creates roadmap records; it does not start implementation.

- [Playground discussion and source investigation](../../discussions/playable-testing-playground/index.md)
- [Renderer modernization roadmap](renderer-modernization.md): depends on the
  minimum playground handoff below before its first in-game modern mesh slice.
- [Renderer discussion](../../discussions/renderer-modernization/index.md)

## Scope

- Explicit playground scene/session identity, independent of legacy city indices.
- One finite flat surface with matching visible and physical geometry, simple
  road markings, a fixed initial car and several generated box obstacles.
- Correct wheel surface queries, scenery/camera collision and coherent empty
  or generated spatial data for every active world consumer.
- Existing vehicle physics and controls, with traffic, police, mission scripts
  and on-foot play disabled for this initial mode only.
- Explicit resource ownership, stable reset/spawn/camera presets, bounds recovery
  and safe repeated load/unload. Keep the small scene resident.
- A shared state-machine launch operation, one reproducible developer entry
  point, and later Take a Ride selection through the same operation.
- A stable scene/fixture identifier usable by the renderer's capture workflow.

## Non-goals

No modern renderer, PBR, custom lighting engine, Vulkan/Metal, world editor,
JSON scene loader, hot reload, cross-city asset composition, general model
import, traffic road network, multiplayer, streamed city or arbitrary playable
map support in this first feature. These remain discussion candidates and
must not block the minimum playground handoff.

Do not replace original city files or redistribute game assets. Loading the
original car/sounds still requires local game data. Do not hide an original
city's geometry while leaving its roads/collisions active and call it a new map.

## Dependencies and risks

This is the first item in the dedicated modernization track (catalog label 13).
It does not require all earlier catalog entries to be completed. Reuse existing
debug capture facilities after verifying their actual source and build support.
Preserve C++11, platform guards, and the project-owned PsyCross patch workflow.

The loader currently expects city-specific LEV/lump data and mission headers;
world queries can dereference region buffers even with traffic disabled. Audit
music, sky, car resources, map/cell initialization and cleanup before bypassing
the original loader. A donor resource set is allowed only with explicit lifetime
management and no leaked original-world geometry, events or collisions.

Specify the initial tested desktop target and hardware during milestone P1;
do not infer Apple support. Preserve other build targets with guards. Decide
save/replay behaviour explicitly: unsupported playground persistence must be
disabled clearly rather than serialized as an original-city session.

## Suggested execution order

Each milestone is a bounded implementation task; a request for one does not
implicitly authorize all later work. All milestones are currently unimplemented.

1. **P1 - Session and world contract.** Trace launch, allocation, surface and
   collision queries, map/streaming consumers and teardown. Specify scene ID,
   units, vertical axis, resource source, initial target, safe empty data,
   persistence policy and fixed fixture/spawn/camera values. Capture an original
   city baseline. Produce a source-backed integration design.
2. **P2 - Drivable generated floor.** Initialize the dedicated scene, a visible
   plane, compatible floor queries and one car through the existing renderer.
   Verify acceleration, braking, turning, suspension and camera alignment.
3. **P3 - Obstacles and scene lifecycle.** Add a few generated collidable boxes,
   reset/respawn, finite bounds and frontend exit. Verify scenery/camera collision
   and repeated transitions back to original cities with no stale state.
4. **P4 - Reproducible test fixture and handoff.** Expose one developer entry
   point through the shared launcher, fixed scene/spawn/camera presets and
   repeatable captures. Measure CPU/GPU timing where available and memory;
   document fixture identity, resource ownership and validation evidence.
5. **P5 - Take a Ride integration.** Add a Playground option using the same
   launcher, with navigation, labels, reset/return behaviour and clear supported
   settings. Verify access in the intended user-facing build, including Release,
   without depending on debug-only command-line parsing. Fixed car/time settings
   are sufficient initially; do not accidentally use four-city menu tables.

### Minimum handoff to renderer modernization

**P1-P4 must pass before renderer milestone R2.** The handoff includes a driving
surface and obstacle fixture rendered through the legacy path, stable scene
identity, reset/camera controls, reproducible captures, measured baseline and
safe return to original levels. This lets the modern mesh test verify mutual
occlusion against legacy-rendered objects.

P5 may follow alongside renderer work after the handoff. JSON authoring,
asset mixing and a general editor are not prerequisites. Keep this roadmap
`planned` after P4: the complete feature also requires P5 and final acceptance.
Record partial milestone evidence here when it actually exists.

The renderer roadmap uses the user-selected Meshy MCP for external PBR fixtures
in R3/R4. That authoring workflow consumes the playground; it is not needed to
generate this roadmap's initial floor and obstacles.

## Acceptance criteria

- The separate scene loads with initialized world data and one controllable car.
- The visible floor matches physical height/normal; obstacle and camera collision
  work, with finite bounds recovery and repeatable reset.
- Original physics and original-city behaviour remain intact outside this mode.
- The fixed fixture is reproducible and useful for comparing legacy/modern draws.
- Take a Ride and the developer entry point invoke the same initialization path.
- Repeated enter/reset/exit cycles leave no stale objects, state leakage or
  unbounded resource growth. Unsupported saves/replays are handled explicitly.
- Supported platforms/build configurations and measured resource costs are stated.

## Validation plan

Use generated geometry and local assets without redistribution. Test floor
queries at spawn, corners and boundaries; compare them with rendered geometry.
Drive and collide at varied speeds, move the camera behind obstacles, reset,
exit and reload. Revisit original cities and verify traffic, missions, audio
and input state are restored. Exercise menu navigation and a Release launch.
Capture identical fixture/camera states and track frame times and memory over
repeated cycles. Run relevant builds and `git diff --check` for implementation.

## Starting points

Repository-relative source paths: `src_rebuild/Game/C/main.c` (`State_GameInit`,
`LoadGameLevel`), `glaunch.c` (`State_GameStart`), `mission.c` (`LoadMission`),
`system.c` (`SetCityType`), `dr2roads.c`, `wheelforces.c`, `cell.c`, `map.c`,
`spool.c`, `objcoll.c`, `camera.c`, `src_rebuild/Game/Frontend/FEmain.c`, and
`src_rebuild/utils/DeveloperDebugStart.*`. Verify these against the live source.

## Handoff and completion

Report only completed milestones with source/build/runtime evidence. Once P1-P5
and all acceptance criteria pass, add `knowledge/product/playable-testing-playground.md`,
move this record to `done/`, set status/date, and update both discussion links,
the renderer dependency, catalog and changelog. Never mark this feature complete
merely because the rendering handoff is available.
