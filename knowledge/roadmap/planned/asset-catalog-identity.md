---
type: Roadmap
title: Source-aware asset catalog and material identity
status: planned
execution_order: 4
tags: [roadmap, assets, inspector]
---

# Source-aware asset catalog and material identity

## Problem

The inspector currently derives much of its information from submitted triangles. That cannot reliably identify hidden faces, separate pieces, all LODs or source files. Texture names and page/index pairs are not globally unique across levels and variants.

## Intended behaviour

Introduce a game-owned catalog connecting source asset, model, LOD, material, texture, palette and runtime instance, without coupling PsyCross to Driver 2 formats.

## Scope

Level/variant-aware resource identity; runtime instance generations; logical versus physical texture page distinction; many-to-many model/texture references; bounded lifetime and invalidation on streaming, reload and slot reuse.

## Non-goals

Do not infer archive filenames from display labels or claim all models use CCARS.RAW. Do not expose raw pointers as persistent mod identifiers or replace every loader in one step.

## Dependencies and risks

Order 04. Incorporate schema decisions from [03](texture-manifest-merge.md). Provides the shared contract for [05](object-texture-batch-export.md), [06](inspector-selection-coverage.md) and [09](model-export-import-roundtrip.md).

## Suggested execution order

Overall order: **04** in the [planned catalog](index.md).
Execute the following milestones sequentially. A request for one milestone
is not authorization to implement all later milestones or dependent features.

1. Inventory source loaders, existing metadata and missing provenance for each resource category. Define an identity and lifetime specification before implementation.
2. Add versioned catalog records with source/context, model/LOD/material relationships and palette variants; maintain legacy manifest matching compatibility.
3. Populate the first adapters for existing car, building and tile paths and attach source identity to renderer hooks.
4. Invalidate stale handles on level changes, streaming replacement and car-slot reuse; keep resource identity distinct from live instance identity.
5. Expose truthful availability/completeness and provenance in diagnostics. Measure memory and frame-time overhead.

## Acceptance criteria

Two instances of a shared model remain distinguishable; their shared textures are not duplicated as different assets. Reused runtime slots cannot silently change a retained selection. Source identifiers are verified or explicitly unknown. Hidden source materials can be enumerated for supported adapters.

## Validation plan

Test repeated names across synthetic level contexts, different palettes, LOD transitions, streamed unload/reload, level changes and slot reuse. Verify bounded memory and original fallback.

## Starting points

`src_rebuild/Game/C/models.c`, `texture.c`, `spool.c`, `cars.c`, `main.c`, and public generic inspector hooks in PsyCross.
Paths are relative to the repository root. Inspect current source and local
diffs before editing; these proposals are not proof of current behavior.

## Handoff and completion

For each requested milestone, report changed files, checks actually run,
remaining limitations and the next unblocked milestone. Preserve C++11 and
platform guards; maintain game-specific behavior outside PsyCross and carry
submodule changes in the project patch. Do not include game assets in tests
or commits. Only after all acceptance criteria are met, publish
`knowledge/product/asset-catalog-identity.md`, move this record to `done/`, update
status/date/catalog links, and record the delivered change in Unreleased.
