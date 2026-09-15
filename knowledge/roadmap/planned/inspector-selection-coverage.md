---
type: Roadmap
title: Whole-object selection across renderer categories
status: planned
execution_order: 6
tags: [roadmap, inspector, picking]
---

# Whole-object selection across renderer categories

## Problem

Only selected producer paths associate triangles with objects. Pedestrians, wheels and other parts can remain unlabelled triangles. The current draw-stream pick is approximate and does not reproduce depth, alpha or clipping.

## Intended behaviour

Expand selection coverage and clearly distinguish selecting a logical object, a component, a material or a face.

## Scope

Adapters for pedestrians/characters, wheels, animated props and remaining static paths; parent-child relationships; stable selected handles; reliable visibility-aware picking and diagnostic fallback.

## Non-goals

Do not claim universal selection until each category is verified. Do not equate a draw batch with a source mesh, or implement every exporter inside the picking layer.

## Dependencies and risks

Order 06 after [04](asset-catalog-identity.md); share alpha policy from [01](../done/texture-alpha-semantics.md). Enables camera focus and complete component export in later stages.

## Suggested execution order

Overall order: **06** in the [planned catalog](index.md).
Execute the following milestones sequentially. A request for one milestone
is not authorization to implement all later milestones or dependent features.

1. Build a coverage inventory with sample scenes and explicit known gaps; record current false selections and lost provenance.
2. Attach component/parent identities at each producer, beginning with vehicle wheels and pedestrian body parts.
3. Implement selection modes and lifecycle checks without altering gameplay geometry, packet layouts or animation.
4. Choose and prototype a visibility-aware picking method, comparing an ID pass with a source-aware CPU method. Account for PGXP, viewport, depth, cutout alpha and clipping.
5. Retain a labelled diagnostic fallback for unsupported paths and measure overhead with the panel closed and open.

## Acceptance criteria

Supported characters and vehicles can select the logical whole or a component. Transparent holes and occluded geometry obey the defined picking policy. Selections survive LOD changes but invalidate when the instance disappears. Unsupported categories are explicitly reported.

## Validation plan

Exercise wheels, moving pedestrians, articulated characters, overlapping buildings, transparent foliage, resize/high-DPI and PGXP modes. Validate input capture and no gameplay changes.

## Starting points

`src_rebuild/PsyCross/src/gpu/PsyX_GPU.cpp`, `src_rebuild/Game/C/pedest.c`, `cars.c`, `draw.c`, `tile.c`, `objanim.c`, and the catalog.
Paths are relative to the repository root. Inspect current source and local
diffs before editing; these proposals are not proof of current behavior.

## Handoff and completion

For each requested milestone, report changed files, checks actually run,
remaining limitations and the next unblocked milestone. Preserve C++11 and
platform guards; maintain game-specific behavior outside PsyCross and carry
submodule changes in the project patch. Do not include game assets in tests
or commits. Only after all acceptance criteria are met, publish
`knowledge/product/inspector-selection-coverage.md`, move this record to `done/`, update
status/date/catalog links, and record the delivered change in Unreleased.
