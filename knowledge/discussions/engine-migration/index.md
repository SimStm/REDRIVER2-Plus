---
type: Discussion
title: Engine migration decision
status: adopted
created: 2026-10-08
updated: 2026-10-08
tags: [okf, architecture, engine]
---

# Engine migration decision

## Adopted direction

The user chose Diligent Engine/DiligentFX, FMOD Core and PopcornFX with a
project-owned simulation/service boundary and final PsyCross removal. Original
assets are converted to readable/native packages using OpenDriver2Tools as format
reference. Physics, gameplay, missions and cutscenes remain Driver 2. Later work
includes FMOD Studio, RmlUi, internal editors and supplementary physics.
The user confirms middleware permitted use and adopts OpenDriver2Tools usage.

The [engine-migration roadmap](../../roadmap/planned/engine-migration.md) owns
the adopted stages. This conversation authorizes branch preservation and planning,
not implementation of the engine. No GitHub issues were requested for publication.

## Verified evidence and limits

- Inspected baseline parent `2e93f215`, PsyCross `37d399df`; current game source
  still uses GTE math in handling, `FilterFrameTime`/`VSync` in the main loop,
  SPU calls in sound and CD/DrawSync callbacks in spooling.
- `CameraCnt` advances on simulation steps; replay records use frame repeats.
  Extracting clock ownership must preserve those tick-indexed semantics and the
  actual regional timer behavior, not make physics display-rate dependent.
- OpenDriver2Tools' [region exporter](https://github.com/OpenDriver2/OpenDriver2Tools/blob/master/DriverLevelTool/exporter/export_regions.cpp)
  writes region OBJ or Unity instantiation scripts with converted placements;
  [model export](https://github.com/OpenDriver2/OpenDriver2Tools/blob/master/DriverLevelTool/exporter/export_models.cpp)
  writes positions/UVs/material page references. This is not gameplay conversion.
  The converter must preserve extra IDs/flags/LOD/animation/damage metadata.
- [DiligentFX](https://github.com/DiligentGraphics/DiligentFX) supplies reusable
  glTF/PBR/shadows/post components. [PopcornFX integration](https://documentation.popcornfx.com/PopcornFX/v2.19/popcornfx-sdk/integration.html)
  recommends engine-owned rendering integration for resource/shader/sort control.
  These capabilities were inspected in documentation, not integrated or tested.

## Recommendations adopted into the plan

Use conversion inventories and deterministic game-state regressions, plus targeted
native rendering tests. New lighting/filtering/effects do not need universal legacy
pixel equivalence. Deliver an early converted driveable slice rather than waiting
for a complete middleware stack. Keep necessary compatibility math/services until
their contracts are independently validated; extraction is not wholesale copying.

Conversion removes repeated source-format interpretation from presentation but
does not solve every overlap/alpha/streaming/dynamic integration problem. No "99%"
reduction or schedule estimate is established. Native failures remain explicit.

## Open implementation decisions

E00 pins SDKs/compiler/targets and inventories remaining consumers; E02 chooses
the compiled package/GLB boundary and readable schema; E01 establishes tested
regional/replay compatibility and backlog policy. Later physics changes require
separate game/replay versioning. No unresolved choice authorizes a silent fallback.

## History

2026-10-08: user adopted migration, requested preservation/push of current work
and a fresh branch from baseline. Rework `e68b6ea6` published; local `main` aliases
existing `origin/master` because no main existed, without remote renaming.
The new branch contains planning changes only; all E00-E07 implementations pending.
