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

1. **00 — [Mod-system branch PR readiness](mod-system-pr-readiness.md)**
2. **02 — [Texture flicker diagnosis and sampling stability](texture-flicker-diagnostics.md)**
3. **03 — [Append-only texture registration and safe manifest merging](texture-manifest-merge.md)**
4. **04 — [Source-aware asset catalog and material identity](asset-catalog-identity.md)**
5. **05 — [Batch texture export with model references](object-texture-batch-export.md)**
6. **06 — [Whole-object selection across renderer categories](inspector-selection-coverage.md)**
7. **07 — [Inspector navigation and independent previews](inspector-camera-preview.md)**
8. **08 — [OpenDriver2Tools interoperability and legacy overrides](modding-toolchain-integration.md)**
9. **09 — [Model export and validated re-import](model-export-import-roundtrip.md)**
10. **10 — [Runtime settings GUI and safe persistence](runtime-settings-gui.md)**
11. **11 — [Draw distance, LOD and streaming budgets](draw-distance-streaming.md)**
12. **12 — [Measured graphics improvements and quality profiles](graphics-quality-profiles.md)**

## Completed

- **01 — [Texture alpha and PSX blending correctness](../done/texture-alpha-semantics.md)**:
  implemented 2026-09-15; see
  [`knowledge/product/texture-alpha-semantics.md`](../../product/texture-alpha-semantics.md).
  The order labels of the remaining entries are unchanged, so the sequence has
  an intentional gap at 01.

## Current-branch merge boundary

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

> Read knowledge/roadmap/planned/texture-manifest-merge.md and the applicable
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
