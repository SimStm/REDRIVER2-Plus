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
- The required stack is Diligent/DiligentFX and FMOD Core. Preserve existing
  effects through project-owned native presentation. PopcornFX is optional E08,
  last, only explicitly requested after E00-E07; never a cutover/completion gate.
  Extract only required PsyCross contracts into focused services; no renamed monolith.
- Remove each dependency after its last consumer passes. Preserve memory/error/
  synchronization/cleanup contracts and explicit loading/error states throughout.
- Diligent mutable SRB replacement on Vulkan/D3D12 requires GPU synchronization or
  a fresh SRB. `ALLOW_OVERWRITE` permits rebinding but does not synchronize pending
  GPU accesses. Rare live settings changes may wait for idle; steady frames must not.
- Conversion workers consume owned geometry/pixel/colour snapshots, never live
  model/VRAM pointers. Poll in normal frames; join at level-load/shutdown boundaries.
  Validate publication provenance after completion and bound GPU upload batches.
  Preserve persisted cache-key bytes/seeds unless a deliberate version change is adopted.
- Keep published visual placement values/resource keys independent of a mutable
  preparation catalog. Replace instances before retiring old keys; reset at level
  boundaries/errors and diagnose bounded staging-capacity loading explicitly.
- A resident inventory union is not active visibility. Preserve all cell/list aliases
  and select original event lists explicitly; do not replace cell-count draw distance
  with a world-origin radius or union event/default objects in the presented scene.
- Never recover a native scene by drawing the old world. Backend/platform support
  remains an independent declared contract; no premature OpenGL deletion.
- Record real validation and pending scope. A build or old-renderer screenshot
  does not accept a stage; roadmap text does not authorize all implementation.
- Do not commit original/converted game assets, local captures or proprietary SDKs.
