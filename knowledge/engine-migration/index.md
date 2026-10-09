---
type: Catalog
title: Engine migration
created: 2026-10-08
tags: [okf, engine, architecture]
---

# Engine migration

This is the active planning directory for `feat/new-engine`. The lifecycle owner
is [engine-migration](../roadmap/planned/engine-migration.md). All stages are
planned; the first implementation issue starts with E00/E01. Current source
still uses PsyCross. Do not treat the preserved native rework as present here.

Read the roadmap, [validation policy](validation.md), [issue briefs](issue-briefs.md),
then the relevant stage. [Future extensions](future-extensions.md) cover Studio,
RmlUi, authoring tools and supplementary physics without blocking the first slice.

The required stack is Diligent Engine/DiligentFX and FMOD Core. Existing effects
use project-owned native presentation. PopcornFX is optional final
[E08](stages/08-optional-popcornfx.md), only upon explicit request after accepted
E00-E07; it does not block migration completion.
OpenDriver2Tools supplies format/export knowledge for our versioned conversion
pipeline; its Unity export is not a gameplay migration or a runtime dependency.
Original game logic stays authoritative and publishes snapshots/events to the
new services. Simulation ticks and presentation frames are separate clocks.

The earlier native renderer is retained on
[`feat/vulkan-native-rework`](https://github.com/SimStm/REDRIVER2-Plus/tree/feat/vulkan-native-rework)
at `e68b6ea6`. Its unfinished R03 is not a prerequisite of this new sequence.
Old hybrid-roadmap text in this branch is historical context, not execution order.
Do not cherry-pick the whole rework to start this migration. Reuse specific proven
code/test cases only after checking their contracts and documenting the scope.

Planning documentation does not authorize implementing every stage or creating
GitHub issues. [Issue briefs](issue-briefs.md) are ready for separately requested
implementation tasks; no external issue was created during this planning change.

Stage evidence: [E00 dependency inventory](e00/dependency-inventory.md) and its
[baseline scenarios](e00/baseline-scenarios.md) record the first implementation
block's progress (inventory, scan tooling, isolated build/run).
