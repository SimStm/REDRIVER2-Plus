---
type: Discussion
title: Source-aware asset catalog and material identity
status: implemented
created: 2026-09-15
updated: 2026-09-15
tags: [discussions, assets, inspector, identity, lifetime]
---

# Source-aware asset catalog and material identity

## Context and decision status

Roadmap item 04 needs a game-owned catalog that connects a source asset, model,
LOD, material, texture, palette and runtime instance. The inspector currently
derives identity from submitted triangles and position, which cannot name hidden
faces, separate pieces, other LODs, or the source file, and texture names are
not globally unique across levels and day/night variants.

This record is the milestone 1 inventory and the identity/lifetime specification
required before implementation. All milestones are implemented
(`src_rebuild/Game/C/assetcatalog.{h,c}`, `assetcatalog_game.{h,c}`, the loader
hooks and the developer-panel diagnostics); the roadmap moved to `done/` on
2026-09-15. Evidence below is from source inspection at the current branch plus
runtime verification noted in the history.

## Source inventory

### Level models

- `main.c` `ProcessLumps` routes `LUMP_MODELS` to `models.c`
  `ProcessMDSLump`, which fills `modelpointers[i]` and `pLodModels[i]`;
  `LUMP_MODELNAMES` reaches `Models_SetInspectorNameBuffer` (name strings);
  `LUMP_LOWDETAILTABLE` fills `Low2HighDetailTable` / `Low2LowerDetailTable`.
- Existing metadata: runtime model index, name (via `GetModelNameByIndex`),
  vertex/polygon counts, collision block, and the instance position/yaw from
  the packed cell object. The inspector builds a `building:<level>:<type>:<x>:
  <y>:<z>:<yang>` key from this.
- Missing: any verified source file/archive (must be *unknown*, not guessed),
  the material/texture set of hidden faces, LOD identity for a given model, and
  level/variant context beyond `GameLevel`.

### Textures

- `main.c` routes `LUMP_TEXTURENAMES` to `texturename_buffer` and
  `LUMP_TEXTUREINFO` to `texture.c` `ProcessTextureInfo`, which fills
  `tpage_ids`, `tpage_texamts`, `texture_pages[128]` and
  `texture_cluts[128][32]`.
- Identity used by the mod manifest is the triple
  `(texture name, texturePage, textureIndex)` (`HdTextureOverrides.cpp`
  `ParseTextureArray`; `-1` means wildcard). `RegisterHdTextureOverridesForPage`
  already passes that triple to the override layer.
- Variants: `system.c` `GetCityType()` distinguishes day/night/multi-day/
  multi-night, and `LevelNames[GameLevel]` names the city. The same texture name
  can appear in more than one set, so the triple alone is unique within a
  variant but the catalog must record the variant.

### Cars, palettes and cosmetics

- `models.c` `ProcessCarModelLump` loads clean/damaged/low-detail car models from
  `car_models_lump` into `gCarCleanModelPtr` / `gCarDamModelPtr` /
  `gCarLowModelPtr`, keyed by `residentCarModels[]`. `cars.c` uses
  `car_data[].ap.model` as the live resident slot. `texture.c`
  `ProcessPalletLump` / `cars.c` `load_civ_palettes` supply palettes and
  `carTpages`.
- The live instance is the car slot; the source asset is the model number. The
  two must stay separate: a slot can be reused by a different source model.

### Streaming and slot lifetime

- `spool.c` `UnpackRegion`/`GotRegion` populate cell objects, and
  `CleanSpooledModelSlots` resets non-`permanentModelSlotBitfield` model slots
  to `dummyModel`. Region changes therefore reuse model slots and re-upload
  texture pages. A retained selection must detect this.
- The catalog must not allocate per frame and must bound its memory; the
  baseline inspector cost is dominated by the existing primitive readback.

## Identity specification

1. **Resource identity is not instance identity.** A resource is a model,
   texture, car model or palette record; an instance is a placement (cell
   object or car slot) that references it. Two instances of one model share one
   resource record and remain distinguishable by their instance identity
   (position + yaw + owning generation).
2. **Stable resource ids are strings, not pointers.** Models:
   `model:<level>:<variant>:<index>`. Textures: `tex:<name>:<page>:<index>`
   (the variant is carried in the record's context, because the manifest triple
   is variant-scoped). Cars: `car:<level>:<modelNumber>`. Ids never embed a raw
   runtime pointer.
3. **Texture manifest compatibility is preserved.** The catalog stores exactly
   the `(name, page, index)` triple the manifest already matches on and exposes
   it unchanged. The catalog decides nothing about override loading; it only
   provides identity and provenance.
4. **Provenance is explicit.** Every record carries a source state:
   `UNKNOWN` (no verified identifier), `DECLARED` (name from the level's own
   name table), or `VERIFIED` (matched a known archive/definition). The catalog
   never invents an archive filename from a display label.
5. **Materials are relationships, not derived polygons.** A model has zero or
   more texture references; a texture is referenced by zero or more models
   (many-to-many). Hidden faces and separate pieces can be enumerated from the
   relationships without a triangle list.
6. **LOD is a relationship.** A model record can point to its high-detail
   parent/sibling so a selection maps across LOD transitions instead of
   changing identity.

## Lifetime specification

1. **Generations.** Entering a new level/variant starts a new catalog
   generation; records created before it are stale. The catalog exposes the
   current generation.
2. **Handles.** External holders store `{index, revision, generation}`. A handle
   is valid only while the record is used, its generation matches the current
   one, and its revision matches. Reusing a model slot or re-registering a
   record bumps its revision, so a retained selection cannot silently point at
   the new occupant.
3. **Invalidation points.** Level/variant change (full reset), streaming slot
   reuse (`CleanSpooledModelSlots` / `ReplaceModel`), and car-slot reuse all
   invalidate handles. `Reset` clears records and bumps the generation.
4. **Bounded memory.** Fixed capacities, no dynamic allocation, no per-frame
   work. Registration happens during load/streaming, not during draw.

## Open questions

- Whether vehicle cosmetics and Tanner/pedestrian models get a distinct kind or
  reuse the model kind with a context flag.
- Whether the catalog should own texture *region* (sub-rectangle) identity or
  only set-level identity; the manifest is set-level, so region identity may
  stay in the inspector.
- How `VERIFIED` provenance is established for each platform's local data set
  without shipping identifiers.

## Discussion history

### 2026-09-15 - Milestone 5 diagnostics and roadmap completion

Added catalog diagnostics to the developer panel and reorganised it: a new
**Mods** tab holds the override toggle, mod diagnostics, mods root, reload, the
enabled mod order and the declared override list; the **3D Debug** tab is now
render-debug only (picking, highlight, selection, catalog context/counts/
footprint, the selected building/tile's stable id, source state, LOD link and
source-material list, plus export and the mod reload action); the **Graphics**
tab keeps renderer controls. The playground fixture registers its generated
models. All acceptance criteria are met; the roadmap record and product document
were published and the record moved to `done/`.

### 2026-09-15 - Milestone 4 streaming invalidation

Per the lifetime specification, model slots now invalidate on reuse:
`AssetCatalog_InvalidateModel` marks a freed slot dead and bumps its revision,
and `RegisterModel` reuses a record for a re-registered slot and bumps again.
`CleanSpooledModelSlots` invalidates every freed streamed slot and
`init_spooled_models` registers each streamed model, so a retained handle
becomes stale instead of silently pointing at the new occupant. Tests grew to 73
checks (0 failures); the debugger hit the streamed registration. Panel
diagnostics (M5) remain open.

### 2026-09-15 - Milestones 2-3 implementation

Added the catalog records (`Game/C/assetcatalog.{h,c}`) and, per this
specification, wired the first adapters: `LoadGameLevel` enters the context and,
once models, textures and car slots are loaded, registers used models with names
and LOD links, textures on the manifest triple, and car resident slots; model
polygons are walked to link materials many-to-many (hidden faces included). The
building/tile inspector keys now use the catalog model id and the car key adds
the city variant and source model number. Runtime log:
`AssetCatalog: level 0 variant 0 has 497 models (497), 303 textures, 6 cars,
219 material refs` (confirmed in the Visual Studio debugger). Streaming
invalidation (M4) and panel diagnostics (M5) remain open.

### 2026-09-15 - Milestone 1 specification

Inventoried the level-model, texture, car/palette and streaming loaders and the
existing inspector/mod metadata. Defined the resource-versus-instance
distinction, string stable ids, explicit provenance, many-to-many
model/texture relationships, LOD relationships, and generation/revision handle
lifetime. Recorded open questions before implementing the catalog records.
