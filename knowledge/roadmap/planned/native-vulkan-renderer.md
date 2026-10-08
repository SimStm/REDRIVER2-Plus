---
type: Roadmap
title: Complete native Vulkan graphics renderer
status: planned
created: 2026-10-06
tags: [okf, roadmap, rendering, vulkan, assets, lighting]
---

# Complete native Vulkan graphics renderer

**Execution paused2026-10-08:** the user cancelled the goal; R03 is incomplete
and R04 unstarted. See the [resumption checkpoint](../../rendering/r03-pause-checkpoint.md).
No stage acceptance or feature lifecycle status changes with this pause.

## Problem

The Vulkan backend still translates PSX primitives. Hybrid depth-derived lighting,
partial original-model migration and gameplay-resident shadow collection limit
geometry, materials, shadow correctness and useful distance. Separate older
graphics plans overlap and do not describe complete graphics replacement.

## Intended behaviour

Render all currently functional original graphics through a native Vulkan scene,
with original-file decoding, persistent meshes/materials/instances, complete static
and dynamic coverage, global sun shadows over validated range, scalable local
lights, larger visual reach, AA, optional modern GLB/glTF/PBR assets and native UI.
Preserve content, file/mod compatibility, physics, collision, AI, missions and replays.
The 2026-10-07 user decision prohibits automatic native-to-old-scene fallback.
Expected loading and real errors are distinct, visible states. Native recovery
must have pixel/resource evidence. Add no legacy recovery/features; retain and
record unmigrated consumers, then remove their dependencies progressively.
OpenGL backend choice remains independent and its existing path is preserved.

## Scope and stages

The authoritative detailed sequence is
[knowledge/rendering/implementation-plan.md](../../rendering/implementation-plan.md),
R00-R12. Each linked stage defines steps, dependencies, deliverables, acceptance,
validation and native failure handling. All were pending at creation. The active
[architecture](../../rendering/architecture.md),
[file contract](../../rendering/asset-and-file-compatibility.md) and
[completion gates](../../rendering/validation-and-completion.md) govern this scope.

This is one feature lifecycle with reviewable stages, not thirteen competing
roadmaps. Record partial delivery here and in the active plan, with product/source
evidence. The adopted scope covers all thirteen stages; the 2026-10-08
adopted order is R00-R07 -> R10 -> R11 -> R08 -> R09 -> R12, retaining stage IDs.
Implementation advances only after the current stage's gates pass. R10 no longer
requires production modern imports; R11 still follows effects/UI migration and
the draw-time mutation audit. R08/R09 regression-test those accepted consumers.
Chicago water is not applicable to R03; actual Rio water remains future
city-specific coverage before full-game acceptance. No stage was accepted by
this documentation-only scope/order update; R03 remains first incomplete.

## Non-goals

No gameplay/physics rewrite, mandatory installation conversion, original-asset
redistribution, forced removal of useful PsyCross services, unlimited distance
or shadowed lights, automatic PBR inference, full editor, universal glTF extension
support, restoration of previously unsupported FMV or simultaneous new OpenGL/
ray-tracing/temporal-upscaling implementations. Existing fallback target guards remain.

## Dependencies and risks

Start with verified current source/diffs and R00 contracts/budgets. Original city
tests complement the existing playground. Source format complexity, prelit artwork,
draw-time mutations, region lifetime and GPU synchronization require explicit
adapters. Whole-city residency, shadows and lighting have bounded measured costs.
AA needs actual device support. Current artifact causes need matching frame evidence.

The user requires every Vulkan change in
[the OpenGL compatibility backlog](port-vulkan-to-opengl-compatibility.md), which
remains unscheduled and does not block this feature. Shared asset/mod/tool contracts
remain active; archived graphics documents do not govern this plan.

## Acceptance criteria

- Every currently functional graphical category/mode on explicitly supported
  native desktop targets uses Vulkan native resources/passes, with zero native-mode
  PSX GPU world/UI draws and no hidden PGXP/filter/mod gating.
- Original installation structure, mods, catalog identity and gameplay/replay/
  regional simulation contracts remain compatible.
- Original static/dynamic geometry, cutouts, off-camera blockers and distant
  receivers pass lighting/shadow tests; sky/HUD remain excluded.
- Increased range, large light registry, GLB/glTF/PBR import and AA work within
  measured CPU/GPU/memory budgets with clear capability/failure fallback.
- All R00-R12 gates have executed evidence; platform and optional feature limits
  are stated honestly. The native default changes only after acceptance.

## Validation plan

Follow the [validation matrix](../../rendering/validation-and-completion.md):
Visual Studio MCP builds/debugging, configured Vulkan docs and actual device queries,
RenderDoc captures/pass/pixel inspection, deterministic original-city routes,
synthetic/owned fixtures, affected standalone tests, gameplay/replay comparisons,
resource/mode stress, hardware-tagged performance and memory measurements.

## Handoff and lifecycle

2026-10-06: documentation adopted and organized; no new rendering implemented.
Next: R00. Old dedicated graphics plans/history were physically archived by user
request; [review inventory](../../rendering/requirements-and-document-review.md).

2026-10-07: user goal authorizes sequential R00-R12 implementation with executed
acceptance before advancing. R00 is implemented; see
[product](../../product/native-renderer-baseline.md) and
[measurements/contracts](../../rendering/r00-measured-baseline.md). Source coverage
diagnostics, hardware query/budgets, repeatable cases and single-owner contracts
passed their baseline gates. R01 is also accepted for the declared Windows
single-player analytic slice: [product](../../product/native-renderer-bootstrap.md),
[executed evidence](../../rendering/r01-validation.md), fork `99fa641` published.
R02 is also accepted for the declared Windows original-import slice:
[product](../../product/native-original-assets.md),
[executed source/GPU/lifetime evidence](../../rendering/r02-validation.md).
R03-R12 remain pending; next R03. This umbrella does
not move to done before full-game native acceptance.

After all gates pass, publish `knowledge/product/native-vulkan-renderer.md`, move
this record to `done/`, set implemented/date, update catalogs and status/changelog.
Do not move it early because the backend is already Vulkan or one city looks correct.
