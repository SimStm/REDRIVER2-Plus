---
type: Catalog
title: Planned features
description: Feature proposals that are not implementation evidence.
tags: [okf, roadmap, planned]
---

# Planned features

Each feature in this directory is a proposal. It must not be described as
implemented, available, or shipped during repository analysis.

Create one `<slug>.md` record per planned feature according to the lifecycle
in the [roadmap catalog](../index.md).

## Recommended execution sequence

The numbers below are suggested scheduling priorities, not release versions.
Every record contains its own milestone sequence, prerequisites, non-goals,
acceptance criteria and validation plan. Dependencies inside the record take
precedence over the simple reading order.

1. **04 — [Source-aware asset catalog and material identity](../done/asset-catalog-identity.md)**:
   implemented 2026-09-15.
2. **05 — [Batch texture export with model references](../done/object-texture-batch-export.md)**:
   implemented 2026-09-17.
3. **06 — [Whole-object selection across renderer categories](../done/inspector-selection-coverage.md)**:
   implemented 2026-09-16.
4. **07 — [Inspector navigation and independent previews](inspector-camera-preview.md)**
5. **08 — [OpenDriver2Tools interoperability and legacy overrides](modding-toolchain-integration.md)**
6. **09 — [Model export and validated re-import](model-export-import-roundtrip.md)**
7. **10 — [Runtime settings GUI and safe persistence](runtime-settings-gui.md)**
8. **11 — [Draw distance, LOD and streaming budgets](draw-distance-streaming.md)**
9. **12 — [Measured graphics improvements and quality profiles](graphics-quality-profiles.md)**

## Playground and renderer modernization track

The user adopted this track on 2026-09-15 with the playground first. Labels
13 and 14 extend the catalog without renumbering existing records; they do not
require completing every unrelated entry above before starting this track.

1. **13 — [Playable testing playground](../done/playable-testing-playground.md)**:
   implemented 2026-09-15. The resident driving fixture is available on the
   existing renderer, and its P1-P4 handoff is in place; P5 added Take a Ride
   access.
2. **14 — [Renderer modernization](renderer-modernization.md)**:
   use that fixture for modern meshes, PBR, lighting and measured pipeline/backend
   work. R2 waits for verified playground P1-P4; R1 audits can happen earlier.
   The backend port (R7b) is delivered - see the completed
   [Vulkan game renderer](../done/vulkan-game-renderer.md) record.

The playground has no dependency on modern rendering. JSON authoring, an editor
and cross-city assets are outside its initial scope. Original-city regression
scenes remain required alongside controlled playground tests. See the linked
discussions inside both records for the reasoning and adopted decision.

## Completed

- **04 — [Source-aware asset catalog and material identity](../done/asset-catalog-identity.md)**:
  implemented 2026-09-15; see
  [`knowledge/product/asset-catalog-identity.md`](../../product/asset-catalog-identity.md).
- **13 — [Playable testing playground](../done/playable-testing-playground.md)**:
  implemented 2026-09-15; see
  [`knowledge/product/playable-testing-playground.md`](../../product/playable-testing-playground.md).
- **00 — [Mod-system branch PR readiness](../done/mod-system-pr-readiness.md)**:
  implemented 2026-09-15; see
  [`knowledge/product/mod-system-pr-readiness.md`](../../product/mod-system-pr-readiness.md).
- **01 — [Texture alpha and PSX blending correctness](../done/texture-alpha-semantics.md)**:
  implemented 2026-09-15; see
  [`knowledge/product/texture-alpha-semantics.md`](../../product/texture-alpha-semantics.md).
- **02 — [Texture flicker diagnosis and sampling stability](../done/texture-flicker-diagnostics.md)**:
  implemented 2026-09-15; see
  [`knowledge/product/texture-flicker-diagnostics.md`](../../product/texture-flicker-diagnostics.md).
- **03 — [Append-only texture registration and safe manifest merging](../done/texture-manifest-merge.md)**:
  implemented 2026-09-15; see
  [`knowledge/product/texture-manifest-merge.md`](../../product/texture-manifest-merge.md).
- **06 — [Whole-object selection across renderer categories](../done/inspector-selection-coverage.md)**:
  implemented 2026-09-16; see
  [`knowledge/product/inspector-selection-coverage.md`](../../product/inspector-selection-coverage.md).
- **05 — [Batch texture export with model references](../done/object-texture-batch-export.md)**:
  implemented 2026-09-17; see
  [`knowledge/product/object-texture-batch-export.md`](../../product/object-texture-batch-export.md).
- **15 — [Texture export alpha fidelity](../done/texture-export-alpha-fidelity.md)**:
  implemented 2026-09-17; see
  [`knowledge/product/texture-export-alpha-fidelity.md`](../../product/texture-export-alpha-fidelity.md).
- **16 — [Semi-transparent texture overrides](../done/semi-transparent-texture-overrides.md)**:
  implemented 2026-09-17 (opt-in proportional alpha, compatibility default); see
  [`knowledge/product/semi-transparent-texture-overrides.md`](../../product/semi-transparent-texture-overrides.md).
- **[Vulkan game renderer](../done/vulkan-game-renderer.md)** (renderer item 14,
  R7b): implemented 2026-09-19; see
  [`knowledge/product/vulkan-game-renderer.md`](../../product/vulkan-game-renderer.md).

The order labels of the remaining entries are unchanged, so the sequence has
intentional gaps at 01, 02, 03, 05, 06, 15 and 16.

## Current-branch merge boundary

> Order 00 was completed on 2026-09-15 (PR
> [#2](https://github.com/SimStm/REDRIVER2-Plus/pull/2), merge `5f1da8a2`).
> The guidance below is the historical merge boundary for the mod-system
> foundation; the next planned order is 07.

Start with order 00 as an audit, not a requirement to complete orders 01–12.
Transparent-PNG rendering is an existing correctness defect and should be
fixed before merging the current texture-override foundation. Classify the
reported flicker before merge: binding/depth regressions block it; measured
filtering-quality improvements can be a follow-up. Manifest append is strongly
recommended if the PR promises a complete export-to-mod workflow.

Universal selection, source-model batch export, previews, model re-import,
new configuration controls and increased draw distance can ship in later PRs.
The current foundation must state its diagnostic picking and export limits.
Do not delete unrelated local data to obtain a clean PR; stage only scoped
changes after inspecting tracked and untracked files.

## Assigning a bounded task to an agent

Example request:

> Read knowledge/roadmap/planned/inspector-camera-preview.md and the applicable
> project instructions. Implement only milestones 1–2, preserving unrelated
> local changes. Report tests, remaining risks and the next milestone. Do not
> mark the whole feature implemented unless all acceptance criteria are met.

Replace the filename and milestones for the intended task. Before coding,
the agent must verify that prerequisites are actually implemented; a planned
record is not evidence that its dependency exists. Separate dependent work
into reviewable changes rather than asking an agent to implement the whole
roadmap implicitly.

For partial delivery, retain `status: planned` and record completed milestones
with evidence without implying the whole feature is done. Once complete,
create the matching product document and follow the move-to-`done/`
lifecycle, updating this catalog's links at the same time.
