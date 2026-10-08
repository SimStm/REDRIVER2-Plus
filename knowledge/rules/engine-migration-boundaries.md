---
type: Rule
title: Preserve game authority during engine migration
created: 2026-10-08
tags: [okf, engine, timing, validation, psycross]
---

# Preserve game authority during engine migration

**When** working on `feat/new-engine`, **then** use
[engine-migration](../roadmap/planned/engine-migration.md) and its E00-E07 contracts.
The old rework branch is reference-only; do not resume its unfinished R03 here.

- Fixed simulation ticks own mechanics/input/replays/RNG; display frames only
  consume snapshots. Preserve NTSC/PAL semantics initially, not a forced new rate.
- Validate mechanics with canonical tick states/events; validate conversion by
  inventories/identity/source data; validate presentation by specified native
  behavior/resources and targeted pixels. Intentional modern visuals may differ.
- Convert original assets into versioned local packages with readable metadata;
  preserve stable IDs, dynamic bindings, mod precedence and transactional ownership.
- The adopted stack is Diligent/DiligentFX, FMOD Core and PopcornFX. Extract only
  required PsyCross contracts into focused project services; no renamed monolith.
- Remove each dependency after its last consumer passes. Preserve memory/error/
  synchronization/cleanup contracts and explicit loading/error states throughout.
- Never recover a native scene by drawing the old world. Backend/platform support
  remains an independent declared contract; no premature OpenGL deletion.
- Record real validation and pending scope. A build or old-renderer screenshot
  does not accept a stage; roadmap text does not authorize all implementation.
- Do not commit original/converted game assets, local captures or proprietary SDKs.
