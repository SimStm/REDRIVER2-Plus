---
type: Discussion
title: A playable testing playground separate from the original cities
status: roadmap-adopted
created: 2026-09-15
updated: 2026-09-15
tags: [discussions, world, levels, procedural, testing, frontend]
---

# A playable testing playground separate from the original cities

## Context and decision status

The user asks whether the game can provide a separate, flat driving test level:
roads or blocks, buildings/objects, and a car that can move freely. Questions
cover procedural generation, a declarative JSON scene, reuse of original city
assets, effort, and selection through Take a Ride versus debug UI or CLI.

**Assessment:** technically feasible while retaining PsyCross and the original
vehicle simulation, but the inspected code has no general procedural-level or
JSON world loader ready to use. A floor's visible mesh is separate from wheel
surface queries and scenery collision. The initial playable scene requires a
deliberate initialization path, not merely drawing a plane.

On 2026-09-15 the user requested a roadmap and selected the playground as the
first implementation stage for renderer modernization. The
[playground roadmap](../../roadmap/planned/playable-testing-playground.md)
defines the adopted bounded scope and the
[renderer roadmap](../../roadmap/planned/renderer-modernization.md) consumes it.
No implementation has started through this documentation task.

This is an adopted sequencing dependency, not a claim that rendering could
never be prototyped in an original city. Playground P1-P4 now precede renderer
R2; read-only renderer audits can happen earlier. Take a Ride is P5, while JSON
authoring and cross-city composition remain outside the initial feature scope.

## Verified source evidence

Inspected 2026-09-15 at parent HEAD `ff87f0b9`, PsyCross HEAD `e56e4cd`, with
existing local changes. This is source inspection, not a prototype or runtime
test. Source links are relative to this subject directory.

### Level and session initialization

- [system.c](../../../src_rebuild/Game/C/system.c): `LevelNames`, `LevelFiles`
  and `LoadingScreenNames` have four city entries. `SetCityType` opens a
  city-specific `.LEV`, including day/night and multiplayer variants, and reads
  sector/lump offsets. `citylumps[8][4]` does not establish eight playable
  city slots; several other arrays still only have four entries.
- [main.c](../../../src_rebuild/Game/C/main.c): `LoadGameLevel` loads cosmetics,
  two data blocks, permanent texture pages and spool information. `ProcessLumps`
  initializes models, texture metadata, map cells, roads and other resources.
  `State_GameInit` loads a mission before the level, then initializes spooling,
  the map, car handling, players, camera and other systems. It also contains
  city-specific music logic. An isolated playground must initialize or safely
  bypass these consumers and release their state when leaving.
- [glaunch.c](../../../src_rebuild/Game/C/glaunch.c): `State_GameStart` maps
  `GAME_TAKEADRIVE` to an existing mission number using city, day/night, player
  count and subgame. It is not a generic map identifier dispatch.
- [mission.c](../../../src_rebuild/Game/C/mission.c): `LoadMission` accepts
  `.D2MS` overrides or reads `MISSIONS.BLK`, then assigns
  `GameLevel = MissionHeader->city` and sets spawn, vehicle, time and weather.
  Setting `GameLevel` once or passing `-level 4` does not create a new city.

### Driving surface, objects and spatial data

- [wheelforces.c](../../../src_rebuild/Game/C/wheelforces.c) calls
  `FindSurfaceD2` for wheel positions and uses the returned height, normal and
  surface type. Grass and other types affect grip and effects.
- [dr2roads.c](../../../src_rebuild/Game/C/dr2roads.c): `MapHeight` and
  `FindSurfaceD2` use `sdGetCell` / `sdHeightOnPlane`. These read region surface
  data, with BSP/multiple-height support. A flat procedural surface can be
  represented by compatible data or an isolated query adapter. The existing
  `default_plane` is not a world generator: `sdGetCell` dereferences a region
  buffer before its fallback, so leaving world buffers uninitialized is unsafe.
- [objcoll.c](../../../src_rebuild/Game/C/objcoll.c) builds collision candidates
  from cell objects and reads model collision boxes (`COLLISION_PACKET`).
  Drawing a box does not register it as a solid obstacle.
- [cell.c](../../../src_rebuild/Game/C/cell.c): `GetFirstPackedCop` uses loaded
  region identifiers, cell lists and object indices. Rendering, vehicle scenery
  collision and camera collision rely on these structures through their own
  callers. Empty worlds still need coherent empty structures or explicit adapters.
- [map.c](../../../src_rebuild/Game/C/map.c) and
  [spool.c](../../../src_rebuild/Game/C/spool.c) assume city cells/regions and
  resident/streamed model and texture data. Disabling background traffic alone
  does not eliminate all world-data dependencies.

### Frontend extension

- [FEmain.c](../../../src_rebuild/Game/Frontend/FEmain.c): `LoadFrontendScreens`
  reads `DATA/SCRS.BIN`, then already adjusts screens in code for time-of-day,
  replay selection and other additions. This is evidence that a new option can
  be integrated in code without requiring edits to original menu data files.
- `PSXSCREEN` holds eight buttons and the normal array has 42 screens. Navigation,
  capacity, labels, localization and callbacks must be accounted for. Other
  frontend tables such as `gfxNames`, `CarAvailability` and area names assume
  four cities. A fifth displayed choice cannot blindly become array index four.

## What can be generated without a 3D editor or new level binary?

A programmer can generate the visible floor, road markings, boxes and simple
buildings from vertices/indices or existing compatible primitives. A separate
procedural scene path can also build its required spatial/surface/collision data
in memory. It therefore need not require a hand-authored mesh or a new `.LEV`.

That is a proposed implementation, not a capability currently exposed by the
loader. Keeping the loader unchanged instead requires a valid legacy level
package, mission/session data and compatible resource references, with tools
or code to generate them. A `.LEV` is a structured world resource, not merely
a file containing the floor mesh.

The scene can be generated on entry and kept resident while driving. Live
editing/hot reload is a separate capability requiring safe resource replacement
and collision updates; it is not implied by procedural generation.

## Alternatives and effort assessment

### A. Controlled test location in an original city

Use a reproducible spawn in a suitable existing area and a limited set of test
objects. This has the smallest initial world-initialization burden and can help
graphics experiments immediately. It is not the user's fully separate flat map;
invisible original collisions and streaming remain if geometry is merely hidden.

### B. Dedicated procedural playground with a bounded asset source

Recommended direction to investigate for the requested environment. Start with
one car, one flat finite area, generated geometry, a few collidable boxes, reset
and exit actions. Disable traffic, police and missions for this mode only;
initialize all remaining consumers correctly. Keep a small scene resident and
use boundaries or reset behaviour instead of claiming an infinite map.

Reuse a known resource set initially for the car, sounds and required common
assets. This does not make the game independent of original data. Audit any
temporary donor-city bootstrap so its actual streets, collisions, events and
streaming cannot leak into the separate scene. Do not treat donor loading as
the final architectural contract or promise it is automatically the cheapest path.

Overall this is a moderate-to-substantial integration feature, even though the
geometry is simple. The major unknown is the smallest coherent initialization
of the existing world consumers. No time estimate is justified before a spike.

### C. New fully compatible legacy level

Build a proper level package and integrate it into the city/session/frontend
flow. This can better exercise original streaming and road systems, but brings
format generation, resource packaging and many city-index assumptions. It is
a broader effort than a bounded procedural test mode.

## Declarative JSON option

A versioned descriptor could become an authoring layer over option B. JSON would
describe the scene; new C/C++ code would validate it, instantiate objects and
build/register visual and collision data. Existing JSON texture manifests do
not provide this functionality.

For illustration only, **this is not a supported schema or existing file**:

```json
{
  "schemaVersion": 1,
  "id": "flat-playground",
  "units": "meters",
  "ground": { "size": [200, 200], "height": 0, "surface": "asphalt" },
  "spawn": { "position": [0, 1, 0], "headingDegrees": 0 },
  "traffic": false,
  "objects": [
    { "shape": "box", "size": [8, 12, 8], "position": [20, 6, 20], "solid": true }
  ]
}
```

The unit conversion, handedness, vertical axis, spawn clearance, supported
rotations, bounds and collision semantics must be specified before implementing
such a schema. No particular dimensions or format have been adopted.

### Reusing objects from other cities

Feasible with a resource adapter; not simply copying an object number. The
current `modelpointers` / LOD tables and texture-page/CLUT references belong to
the loaded resource context. Region changes can load or replace resources.
Cross-city composition requires stable source identities, dependency loading,
texture/palette/index remapping, collision data and explicit resource lifetimes.

Begin with procedural shapes, then a controlled set from one source city,
then mixed-city resources if needed. Reference locally installed data rather
than packaging original game assets inside a distributable playground mod.

## Take a Ride, debug GUI and CLI are all possible entry points

Recommend one shared scene/session launch operation, called by any chosen UI.
Its name and contract remain to be designed. It selects the scene independently
of raw legacy city indices and enters a controlled initialization transition.

- **CLI:** useful for reproducible development; a dedicated scene argument
  would need implementing. Existing `-level` selects a legacy city and cannot
  request this proposed environment.
- **Debug GUI:** can request the same transition; it should queue a load/reset
  for the state machine, not swap world allocations inside an ImGui draw callback.
- **Take a Ride:** a selectable Playground entry can call that same launch
  operation. It need not remain a developer-only feature. Integrate navigation,
  vehicle/time choices, return/restart behaviour and labels; do not assume the
  regular city-to-mission formula or four-city resource tables apply.

Starting through a CLI or debug action is a testing convenience, not an engine
restriction. User-facing availability in Release builds is an explicit product
choice; the current `DEBUG_OPTIONS` argument guards must not accidentally make
the normal-menu feature inaccessible. Save/replay identity also needs an explicit
policy so playground state is not misinterpreted as an original-city session.

## Earlier suggested sequence and adopted scope

The sequence below preserves the exploration that preceded adoption. The linked
roadmap now governs execution: P1-P4 deliver the minimum playground, P5 adds
Take a Ride, and JSON authoring is deferred rather than a prerequisite for either.

1. Trace and specify session/world initialization and safe teardown, including
   resource dependencies and all active floor/object queries.
2. Spawn one existing car above a generated, finite flat driving surface;
   provide matching visible ground and physical surface data.
3. Add a few collidable generated obstacles, reset/respawn and exit controls.
4. Enter through one developer entry point and verify repeated load/exit cycles.
5. Add a small declarative descriptor after the in-memory representation works.
6. Add Take a Ride selection through the same launch operation; broaden asset
   sources and road/AI support only as separately chosen extensions.

Validate acceleration/braking/turning and suspension on the plane, obstacle
and camera collision, surface height/normal consistency, out-of-bounds recovery,
resource lifetime, and return to original cities without persistent changes.
For JSON, test unsupported fields/versions, bounds and asset resolution before
instantiation. No prototype or these runtime checks were executed here.

## Remaining implementation decisions

- Exact world-query adapter/compatible-data design and minimum resource set.
- Initial tested desktop/hardware target, fixture dimensions and capture budgets.
- Save/replay policy and the shared scene launcher contract.
- Which original-city regression fixtures complement the controlled scene.

The adopted initial plan uses one car, generated objects and a finite resident
pad without traffic/police/on-foot play. Take a Ride follows core validation.

## Discussion history

### 2026-09-15 - Initial feasibility investigation

User asked about a separate flat driving playground, code/JSON generation,
reuse of city objects, implementation effort and Take a Ride selection.
Inspected level/mission loading, wheel surface lookup, spatial object collision
and frontend modifications. Concluded all proposed entry points are feasible
after implementing a dedicated scene path; current code is not a generic level
registry. Recommended a small procedural resident scene with explicit physical
surfaces, followed by declarative authoring and normal-menu integration.

### 2026-09-15 - Playground-first roadmap adopted

User requested a playground roadmap linked with its discussion to the rendering
roadmap, preferring a dedicated map before modern rendering implementation.
Created both records and a minimum handoff at playground P1-P4. The playground
uses existing graphics and therefore does not depend on PBR or new backends.
Take a Ride remains part of playground completion at P5; JSON and asset mixing
are deferred. This supersedes the earlier optional sequencing recommendation,
while retaining original-city regression tests and not starting implementation.
