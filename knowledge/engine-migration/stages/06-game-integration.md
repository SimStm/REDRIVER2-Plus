---
type: Plan
title: E06 - Mechanics and full-game integration acceptance
status: planned
created: 2026-10-08
tags: [engine, gameplay, integration, acceptance]
---

# E06 - Mechanics and full-game integration acceptance

## Objective and dependencies

Complete E02-E05 category coverage and prove the original game works with the new
presentation/audio/VFX. Mechanics remain in REDRIVER2; Diligent consumes state,
not gameplay decisions. E01 deterministic acceptance remains a regression gate.

## Implementation and scenario matrix

- Whole-city visual residency/streaming independent of gameplay activation,
  with stable sector/model IDs and bounded IO/upload/memory. Validate all four
  cities, day/night/weather/bridges, LOD/transparency/water where actually present.
- Cars/wheels/damage/pedestrians/animation/event objects/destructibles follow
  original current/interpolated state. Preserve mesh-to-damage and object-to-game
  mappings; a moved or destroyed object must not leave a stale baked visual.
- Driving, collisions, damage, pursuit/felony, traffic/pedestrians, missions,
  cutscenes/chase replays, free roam, replay record/playback, pause/loading,
  frontend and every supported single/split-screen mode.
- Region transitions, teleport/camera cuts, reload, lost device/input, interrupted
  conversion/upload/audio/VFX and save/config/mod compatibility.

## Acceptance criteria

- [ ] Approved tick-state checksums and event timelines match the E00/E01 baseline
  across display schedules; changed mechanics are not masked as visual differences.
- [ ] Existing replay/cutscene samples play through all supported content variants
  with correct camera, event, sound and mission timing; unsupported cases are named.
- [ ] Coverage inventory has no unexplained static/dynamic/auxiliary omissions,
  stale instances or hidden legacy renders in accepted native modes.
- [ ] Native image checks show correct assets/transforms/occlusion and intended
  materials/effects; universal PSX pixel equality is not required.
- [ ] Route/reload/stress runs meet predeclared CPU/GPU/memory/IO budgets and
  preserve resource lifetime/error guarantees. Target-specific results are explicit.
- [ ] E00 consumer checklist identifies zero remaining *required* PsyCross runtime
  consumers for the planned cutover. Remaining build/vendor ownership is E07 work.

## Validation and next step

Run a checked-in reproducible scenario matrix with local assets and versioned
inputs, canonical snapshots and native capture/audio/resource evidence. A single
Take a Ride run cannot accept missions/replays or all cities. Reuse component
regressions and investigate targeted defects; avoid repeating exhaustive legacy
pixel archaeology for intentionally modern shading. E07 removes the dependency
only after this full-game evidence supports the declared target scope.
