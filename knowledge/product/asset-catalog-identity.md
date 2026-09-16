---
type: Product
title: Source-aware asset catalog and material identity
description: How the game names and relates models, textures, cars and palettes, and where the inspector shows them.
tags: [product, assets, inspector, identity, catalog, mods]
---

# Source-aware asset catalog and material identity

A game-owned catalog connects the source asset, model, LOD, material, texture,
palette and runtime instance without coupling PsyCross to Driver 2 formats. It
answers "what is this object, really?" for the developer panel and for future
export/import work, and it keeps that answer stable across LOD changes, texture
streaming and slot reuse.

Implementation: `src_rebuild/Game/C/assetcatalog.{h,c}` (records and lifetime),
`assetcatalog_game.{h,c}` (loader adapters), and the inspector in
`src_rebuild/utils/DeveloperGraphicsPanel.cpp`.

## Identity

- **Stable ids are strings, not pointers.** Models are
  `model:<level>:<variant>:<index>`, textures `tex:<name>:<page>:<index>`,
  cars `car:<level>:<modelNumber>`. `<variant>` is the city type (day, night,
  multiplayer day/night).
- **Resource identity is separate from instance identity.** Two placements of
  one model share one model record; the instance is the cell placement
  (position + yaw) or the live car slot, and inspector keys combine the stable
  id with the placement, e.g. `building:model:0:0:37:12000:0:8000:0`.
- **Textures key on the mod manifest triple** `(name, texturePage,
  textureIndex)`. Shared textures are a single record referenced by many
  models; the override loader is untouched.
- **Provenance is explicit.** A record is `unknown`, `declared` (name from the
  level name table) or `verified` (matched a known definition). No record
  invents an archive filename from a display label; `verified` has no data
  source yet, so records are `declared` or `unknown`.

## Lifetime

- Entering a level/variant starts a new generation; earlier records are stale.
- A retained `AssetCatalogHandle` stores `{index, revision, generation}` and is
  valid only while all three match. Freeing a streamed model slot
  (`CleanSpooledModelSlots`) invalidates its record, and re-registering the slot
  (region streaming or a level load) bumps the revision, so a retained
  selection cannot silently point at the new occupant.
- The catalog is fixed-size and allocation-free: registration happens during
  load and region streaming, never during a draw.

## Where it appears

- **Mods tab** (developer panel): HD texture override toggle, mod diagnostics,
  mods root, reload, the enabled mod order, and the declared texture override
  list.
- **3D Debug tab**: picking and highlight controls, the selected primitive,
  the **Asset catalog** section (context, generation, counts vs capacity, static
  footprint), and — for a selected building or tile — the stable id, source
  state, LOD link and the **source materials** list (all linked textures,
  including hidden faces). Texture and model export and the mod reload button
  stay here.

## Measured costs

- Fixed footprint: **384,256 bytes** of static storage, independent of level
  size.
- No per-frame work during gameplay: the catalog is written only while loading
  or streaming a region; the panel only reads counters and records.
- Runtime counts (Chicago day, single player): 497 models, 303 textures, 6 cars
  and 219 material links at level load, growing to about 614 models and 225
  links once the playground fixture and streamed regions register.

## Limits

- The catalog is built for the desktop inspector and is compiled out of the PSX
  target; other platforms compile the shared core.
- Car models are not linked to level model records (separate packs); a car
  record carries its source model number and slot.
- `verified` provenance and palette-variant completeness are not populated yet.
- It supplements, not replaces, the legacy per-draw provenance: unlabelled draw
  paths still fall back to the old key format.

## Configuration

No configuration is required. The catalog follows `GameLevel`, the city type
from `GetCityType()`, and `gMultiplayerLevels`; the panel renders it only when
the developer panel is available (Windows and Linux desktop builds).
