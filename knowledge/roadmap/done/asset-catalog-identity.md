---
type: Roadmap
title: Source-aware asset catalog and material identity
status: implemented
execution_order: 4
created: 2026-09-15
completed: 2026-09-15
tags: [roadmap, assets, inspector]
---

# Source-aware asset catalog and material identity

Implemented 2026-09-15. Operational behaviour is documented in
[`knowledge/product/asset-catalog-identity.md`](../../product/asset-catalog-identity.md);
the identity/lifetime specification is in
[`knowledge/discussions/asset-catalog-identity/index.md`](../../discussions/asset-catalog-identity/index.md)
and the durable contract in
[`knowledge/rules/asset-identity-lifetime.md`](../../rules/asset-identity-lifetime.md).

## Problem

The inspector currently derives much of its information from submitted triangles. That cannot reliably identify hidden faces, separate pieces, all LODs or source files. Texture names and page/index pairs are not globally unique across levels and variants.

## Intended behaviour

Introduce a game-owned catalog connecting source asset, model, LOD, material, texture, palette and runtime instance, without coupling PsyCross to Driver 2 formats.

## Scope

Level/variant-aware resource identity; runtime instance generations; logical versus physical texture page distinction; many-to-many model/texture references; bounded lifetime and invalidation on streaming, reload and slot reuse.

## Non-goals

Do not infer archive filenames from display labels or claim all models use CCARS.RAW. Do not expose raw pointers as persistent mod identifiers or replace every loader in one step.

## Dependencies and risks

Order 04. Incorporate schema decisions from [03](texture-manifest-merge.md). Provides the shared contract for [05](object-texture-batch-export.md), [06](inspector-selection-coverage.md) and [09](../planned/model-export-import-roundtrip.md).

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

## Completion record

### 2026-09-15 - Milestones 1-5 delivered

All acceptance criteria are met; the roadmap moved to `done/`. Panel
diagnostics also reorganised the developer panel: mod information now lives in a
dedicated **Mods** tab and the **3D Debug** tab is render-debug only.

- **M1 specification:** recorded in
  [the asset catalog discussion](../../discussions/asset-catalog-identity/index.md):
  resource-versus-instance identity, string stable ids
  (`model:<level>:<variant>:<index>`, `tex:<name>:<page>:<index>`), explicit
  provenance (`UNKNOWN`/`DECLARED`/`VERIFIED`), many-to-many model/texture
  relationships, LOD relationships, and generation/revision handle lifetime.
- **M2 records:** `src_rebuild/Game/C/assetcatalog.{h,c}` adds a dependency-free,
  allocation-free catalog with model, texture, car and palette records,
  relationships and generation/revision handles. Texture identity is the exact
  `(name, texturePage, textureIndex)` triple the mod manifest matches on, so
  legacy manifest compatibility is unchanged (the catalog does not touch the
  override loader). The module is guarded out of the PSX build.
- **M3 adapters:** `src_rebuild/Game/C/assetcatalog_game.{h,c}` enters the
  context in `LoadGameLevel` and, once models, textures and car slots are
  loaded, registers every used level model (name from the level name table,
  `Low2HighDetailTable` LOD link) and each car resident slot. `texture.c`
  registers each texture on the manifest triple and walks model polygons to link
  materials many-to-many; `RegisterCatalogModelTextures` enumerates hidden
  faces. The building and tile inspector keys now use the catalog model id
  (`building:model:<level>:<variant>:<index>:<pos>`, `tile:...`) and the car key
  adds the city variant and source model number.
- **M4 invalidation:** `AssetCatalog_InvalidateModel` marks a freed slot's
  record dead and bumps its revision; `RegisterModel` now reuses the record for
  a re-registered slot and bumps the revision again. `models.c`
  `CleanSpooledModelSlots` invalidates every freed streamed slot, and
  `spool.c` `init_spooled_models` registers each streamed model. Level/variant
  changes already invalidate through the generation; car slots through
  re-registration.
- **M5 diagnostics:** the developer panel **3D Debug** tab shows catalog
  context, generation, counts against capacity and the fixed footprint, plus the
  selected building/tile's stable id, source state, LOD link and **source
  materials** list (including hidden faces). Mod information moved to a new
  **Mods** tab (enable toggle, diagnostics, mods root, reload, enabled mod
  order, declared overrides); the **Graphics** tab keeps only renderer
  controls; the 3D Debug tab keeps picking, highlight, export and the mod reload
  action. The playground fixture registers its generated models too.
- **Evidence:** `src_rebuild/tests/AssetCatalogTests.cpp` runs 77 checks with 0
  failures (generation/context, slot-revision handle staleness, explicit
  invalidation and reactivation, texture-triple dedup, shared-texture
  many-to-many, explicit provenance, palette links, completeness vs capacity,
  graceful bounds). The Windows `Release_dev` x64 solution builds with the
  modules included. Runtime log: `AssetCatalog: level 0 variant 0 has 497
  models (497), 303 textures, 6 cars, 219 material refs`; the Visual Studio
  debugger confirmed those values at `AssetCatalogGame_PopulateLevel` and hit the
  streamed-model registration in `init_spooled_models`; the panel was verified
  live, including a picked car whose key is `car:0:0:0:0:0`. Fixed footprint
  384,256 bytes, no per-frame work.

## Handoff and completion

For each requested milestone, report changed files, checks actually run,
remaining limitations and the next unblocked milestone. Preserve C++11 and
platform guards; maintain game-specific behavior outside PsyCross and carry
submodule changes in the project patch. Do not include game assets in tests
or commits. Only after all acceptance criteria are met, publish
`knowledge/product/asset-catalog-identity.md`, move this record to `done/`, update
status/date/catalog links, and record the delivered change in Unreleased.
