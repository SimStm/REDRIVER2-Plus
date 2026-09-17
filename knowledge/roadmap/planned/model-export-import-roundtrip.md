---
type: Roadmap
title: Model export and validated re-import
status: planned
execution_order: 9
tags: [roadmap, models, exports]
---

# Model export and validated re-import

## Problem

Current car OBJ export is geometry-only. Arbitrary objects, materials and source structures need dedicated adapters; changing packed game models can affect collision, damage and gameplay.

## Intended behaviour

Export supported source models with geometry, UVs and material references, then introduce explicit validated import for narrowly defined categories.

## Scope

Start with static source models, then vehicle variants and animated characters. Define coordinate units, handedness, winding, transforms, LODs, component hierarchy and material metadata. Use OBJ/MTL where sufficient and document animation limitations.

## Non-goals

Do not promise OBJ preserves skeletons or every PSX material semantic. Do not silently rebuild collision/damage data, mutate active allocations unsafely or enable general model import before validation.

## Dependencies and risks

Order 09 after [04](../done/asset-catalog-identity.md), [05](object-texture-batch-export.md), [06](../done/inspector-selection-coverage.md) and the format decision in [08](modding-toolchain-integration.md). Export-only milestones may precede import.

## Suggested execution order

Overall order: **09** in the [planned catalog](index.md).
Execute the following milestones sequentially. A request for one milestone
is not authorization to implement all later milestones or dependent features.

1. Specify source-model versus deformed-frame export and local versus world transforms; add export metadata that supports reconstruction.
2. Implement static-model OBJ/MTL export with UVs, materials and texture batch references; reject unsupported polygon encodings explicitly.
3. Extend vehicle export with supported clean/damaged/low-detail variants and separate wheels; validate indices and material relationships.
4. Prototype compile/import using the selected converter; enforce topology, vertex/index, material, memory and source-format constraints.
5. Define safe replacement boundaries, rollback and restart/reload requirements. Add animated-character support only after hierarchy and pose contracts exist.

## Acceptance criteria

Supported exported meshes load with correct orientation, UVs and materials. A controlled no-edit round trip preserves the agreed invariants. Invalid imports fail before touching live assets; unsupported types and reload requirements are visible.

## Validation plan

Check counts, index ranges, coordinate transforms, texture references, deformation/collision assumptions and source variants. Compare no-edit round trips and deliberate invalid input, including rollback after load failure.

## Starting points

`src_rebuild/Game/engine/mdl.h`, `src_rebuild/Game/C/models.c`, `cars.c`, `pedest.c`, the catalog and selected OpenDriver2Tools conversion path.
Paths are relative to the repository root. Inspect current source and local
diffs before editing; these proposals are not proof of current behavior.

## Handoff and completion

For each requested milestone, report changed files, checks actually run,
remaining limitations and the next unblocked milestone. Preserve C++11 and
platform guards; maintain game-specific behavior outside PsyCross and carry
submodule changes in the project patch. Do not include game assets in tests
or commits. Only after all acceptance criteria are met, publish
`knowledge/product/model-export-import-roundtrip.md`, move this record to `done/`, update
status/date/catalog links, and record the delivered change in Unreleased.
