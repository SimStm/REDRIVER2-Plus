---
type: Change
title: Add the source-aware asset catalog and panel diagnostics (roadmap item 04, M1-M5)
description: A game-owned identity and lifetime module for models, textures, cars and palettes, populated from the loaders, invalidated on slot reuse, and surfaced in a reorganized developer panel.
tags: [okf, assets, inspector, identity, lifetime, catalog, panel]
---

# Add the source-aware asset catalog and panel diagnostics (roadmap item 04, M1-M5)

## Context

The inspector identifies objects from submitted triangles and position, so it
cannot name hidden faces, separate pieces, other LODs, or a source file, and
texture names are not unique across levels and day/night variants. Item 04
requires an identity/lifetime specification before implementation, then
versioned catalog records that stay compatible with the existing mod manifest.

## Decision

- **Specification (M1)** in
  `knowledge/discussions/asset-catalog-identity/index.md`: resource versus
  instance identity; string stable ids (`model:<level>:<variant>:<index>`,
  `tex:<name>:<page>:<index>`); explicit provenance
  (`UNKNOWN`/`DECLARED`/`VERIFIED`); many-to-many model/texture and model/LOD
  relationships; generation/revision handles; bounded, allocation-free storage.
- **Records (M2)** in `src_rebuild/Game/C/assetcatalog.{h,c}`: model, texture,
  car and palette records with relationships, `Find`/`Get`/`Enumerate` access,
  `MakeModelId`/`MakeTextureId`, and `HandleValid`. Texture identity is the
  exact `(name, texturePage, textureIndex)` triple the manifest uses, so the
  catalog is additive and the override loader is untouched. The implementation
  is guarded out of the PSX build.
- **Adapters (M3)** in `src_rebuild/Game/C/assetcatalog_game.{h,c}`,
  `texture.c`, `draw.c`, `tile.c` and `cars.c`: `LoadGameLevel` enters the
  context and then registers every used level model (name, `Low2HighDetailTable`
  LOD link), each texture on the manifest triple, and each car resident slot;
  model polygons are walked to link materials many-to-many, including hidden
  faces. The building/tile inspector keys now use the catalog model id and the
  car key adds the city variant and source model number.

- **Invalidation (M4)**: `AssetCatalog_InvalidateModel` marks a freed slot dead
  and bumps its revision; `RegisterModel` reuses the record for a
  re-registered slot and bumps again. `models.c CleanSpooledModelSlots`
  invalidates each freed streamed slot and `spool.c init_spooled_models`
  registers each streamed model. Level changes invalidate through the
  generation; car slots through re-registration.

- **Diagnostics (M5)**: the developer panel shows catalog context, generation,
  counts against capacity and the fixed footprint; a selected building/tile
  shows its stable id, source state, LOD link and source-material list (hidden
  faces included). The panel was reorganised: mod information moved to a new
  **Mods** tab, the 3D Debug tab is render-debug only, and the Graphics tab
  keeps renderer controls. The playground fixture registers its generated
  models. All acceptance criteria are met and the roadmap moved to `done/`.

## Impact and evidence

- `src_rebuild/tests/AssetCatalogTests.cpp`: 77 checks, 0 failures, covering
  context/generation lifetime, slot-revision handle staleness, explicit
  invalidation and reactivation, texture-triple deduplication, shared-texture
  many-to-many references, explicit provenance, car/palette relationships,
  completeness against capacity, and graceful bounds handling.
- Windows `Release_dev` x64 builds with the modules included in `Game/**.c`.
- Runtime (`-playground` and mission 1) logs
  `AssetCatalog: level 0 variant 0 has 497 models (497), 303 textures, 6 cars,
  219 material refs`; the Visual Studio debugger confirmed those values at
  `AssetCatalogGame_PopulateLevel`, hit the streamed-model registration in
  `init_spooled_models` (slot 99), and the game rendered normally. The panel was
  verified live through the computer-control MCP: the new **Mods** tab lists the
  active mods and declared overrides, and the **3D Debug** tab shows the catalog
  context, `Models 614/1536 | textures 303/2048 | cars 6/32`,
  `Material links 225/8192`, a 384,256-byte fixed footprint, and a picked car
  key `car:0:0:0:0:0`.
- Documentation: product `knowledge/product/asset-catalog-identity.md`, rule
  `knowledge/rules/asset-identity-lifetime.md`, updated discussion catalog and
  roadmap record moved to `done/`; Unreleased changelog entries.

## Limitations

- `VERIFIED` provenance has no data source on any platform yet; records are
  registered as `UNKNOWN`/`DECLARED`.
- Car source-model linking is by resident slot; car models are not linked to
  level model records because they live in a separate pack.
- The catalog is desktop-only; the shared core compiles everywhere but the
  storage is compiled out on PSX.

## Validation

- `AssetCatalogTests.exe` exits 0 with 77 checks and 0 failures.
- `Release_dev` x64 solution build: 0 failed projects.
- Runtime log, Visual Studio debugger evaluation, and live developer-panel
  inspection as above.
- `git diff --check` passed for the source and documentation changes.
