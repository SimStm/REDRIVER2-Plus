---
type: Plan
title: E00 - Dependency inventory and behavioral baseline
status: planned
created: 2026-10-08
tags: [engine, baseline, psycross]
---

# E00 - Dependency inventory and behavioral baseline

## Objective and dependencies

Identify every required PsyCross contract and establish reproducible game
behavior before replacing it. Start from this branch's actual source/gitlink.
This is the first stage of [engine migration](../../roadmap/planned/engine-migration.md).

## Deliverables

- Consumer/owner inventory covering GTE/fixed math, clocks/vblank, pad input,
  SPU/music/CD/XA, spool callbacks, filesystem, GPU/VRAM/offscreen, window/present,
  ImGui/platform hooks, diagnostics and vendored libraries/build dependencies.
- Classify each as project extraction, modern adapter, temporary compatibility
  consumer or unused code. Record lifetime/thread/callback guarantees and callers.
- Pin required Diligent/DiligentFX and FMOD SDK/build requirements, compiler
  standard and a tested-target plan. Inspect actual integration interfaces.
  PopcornFX SDK acquisition/integration is deferred to optional E08, only on request.
- Record NTSC/PAL replay/timer/arithmetic contracts, input sampling and draw-time
  mutation/RNG sites. Do not infer runtime behavior from folder boundaries.
- Repeatable input/tick scenarios for driving, collision/damage, police/traffic,
  a mission, a chase replay/cutscene, pause/loading and a region transition.

## Acceptance criteria

- [ ] All source includes/symbols and build dependencies have an identified
  owner/replacement; unknown consumers are explicit and cannot be declared removed.
- [ ] Baseline fixtures repeat with matching canonical game state at checkpoints;
  record nondeterministic fields rather than hiding them with broad tolerances.
- [ ] Regional and old replay formats are identified with real sample results.
- [ ] Fresh baseline build/run uses the branch's gitlink, not old rework outputs.
- [ ] Platform/compiler and SDK acquisition requirements are concrete; no game
  assets or proprietary SDK contents are accidentally staged.

## Validation and next step

Use source/build scans plus a clean generated baseline build and repeated
deterministic runs. Inspect VSync/DrawSync/CD callback behavior, not only calls.
Use the [shared validation policy](../validation.md). No baseline run is claimed
by this document. Publish evidence and proceed to E01 with the inventory as its
removal checklist. Inventory gaps remain pending throughout later stages.
