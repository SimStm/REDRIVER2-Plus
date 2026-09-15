---
type: Roadmap
title: OpenDriver2Tools interoperability and legacy overrides
status: planned
execution_order: 8
tags: [roadmap, tooling, compatibility]
---

# OpenDriver2Tools interoperability and legacy overrides

## Problem

OpenDriver2Tools already understands several source formats, while the current inspector and legacy overrideContent pipeline expose different capabilities with little shared documentation.

## Intended behaviour

Reuse verified format knowledge and compatible conversion workflows instead of introducing another incompatible asset format.

## Scope

Evaluate DriverLevelTool/model compiler, texture/TIM and audio tools; document legacy TIM/MDL/COS/DEN paths and load timing; define a versioned conversion contract shared with the catalog.

## Non-goals

Do not assume the tools are a drop-in runtime dependency. Do not copy code without checking licenses, vendor game assets or require audio tooling for texture fixes.

## Dependencies and risks

Order 08; read-only format/license investigation can happen earlier. Implement adapters after the catalog contract in [04](asset-catalog-identity.md). Supply evidence for [09](model-export-import-roundtrip.md).

## Suggested execution order

Overall order: **08** in the [planned catalog](index.md).
Execute the following milestones sequentially. A request for one milestone
is not authorization to implement all later milestones or dependent features.

1. Inspect https://github.com/OpenDriver2/OpenDriver2Tools at a recorded revision, including license/dependency terms and supported game variants.
2. Create a capability matrix in project documentation: inspect/export/compile/import, formats, platform and limitations, based on tested tools rather than README claims alone.
3. Map existing overrideContent consumers, directory conventions, source variants, reload timing and fallback behavior.
4. Choose subprocess tooling, a reusable library or a small adapter based on isolation, licensing and build portability; keep runtime and offline responsibilities distinct.
5. Demonstrate one local texture/model conversion round trip. Plan audio/whole-level adapters as separate later milestones within this feature.

## Acceptance criteria

The selected integration approach has recorded provenance/license review, explicit supported formats and a reproducible local example. Existing manifest PNG mods remain usable independently. Legacy overrideContent behavior is documented without claiming hot reload.

## Validation plan

Use synthetic or locally owned fixtures without redistribution. Compare source counts, geometry, UVs, palette/alpha and compiled output. Test unavailable/mismatched tool versions and clear failure messages.

## Starting points

OpenDriver2/OpenDriver2Tools; `src_rebuild/Game/C/texture.c`, `cars.c`, `models.c`, `cosmetic.c`, `denting.c`, `spool.c`.
Paths are relative to the repository root. Inspect current source and local
diffs before editing; these proposals are not proof of current behavior.

## Handoff and completion

For each requested milestone, report changed files, checks actually run,
remaining limitations and the next unblocked milestone. Preserve C++11 and
platform guards; maintain game-specific behavior outside PsyCross and carry
submodule changes in the project patch. Do not include game assets in tests
or commits. Only after all acceptance criteria are met, publish
`knowledge/product/modding-toolchain-integration.md`, move this record to `done/`, update
status/date/catalog links, and record the delivered change in Unreleased.
