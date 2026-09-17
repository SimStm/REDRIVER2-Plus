---
type: Change
title: Fix texture identification for cars, pedestrians, sprites and streamed pages; add export metadata and whole-object selection
description: Root causes and fixes for textures that could not be named or exported, plus manifest type/level metadata, optional organized export and double-click whole-object selection.
tags: [okf, textures, inspector, export, mods, picking]
---

# Fix texture identification for cars, pedestrians, sprites and streamed pages; add export metadata and whole-object selection

## Context

After roadmap item 06 shipped, in-game testing reported four problems that the
per-category acceptance criteria did not cover, because each one is a property
of a texture or producer rather than of the picking policy:

1. cars and most pedestrians could be selected but their textures could not be
   exported, while a minority exported normally;
2. buildings could be selected but a facade reported that its texture was not
   registered;
3. sprite scenery (trees) selected a single triangle, blinked, did not select
   its trunk, and reported an unsupported draw source;
4. exports carried no information about what they belonged to.

## Decision

- **Palette variants are nameable (1).**
  `HdTextureOverrides_FindTextureInfo` required an exact `(tpage, CLUT)` match
  against the level's named textures, but car and pedestrian palettes are
  generated at runtime (`civ_clut`) and never registered. Measured on the player
  car: page 14 holds 30 named textures and the car's CLUT `19133` matched none
  of them (base CLUT `21565`). A named texture on the same page whose VRAM
  region contains the selection is now used as a fallback. The PNG still reads
  VRAM with the primitive's own CLUT, so the palette is preserved; the player
  car now resolves `ROOF...`.
- **Streamed pages are nameable (2).**
  Only `texture.c`'s page loaders called `RegisterHdTextureOverridesForPage`,
  so a page whose VRAM slot is assigned by the texture spool had no names.
  `SendTPage` now registers the page once `texture_pages[set]` is known.
  Measured: the named-texture count rose from 300 to 363, and a facade on page 8
  that previously matched nothing resolves `DTGE...`.
- **Sprites carry identity (3).**
  Trees are `SHAPE_FLAG_SPRITE` objects drawn by `DrawSprites` as a subdivided
  mesh, which registered no producer range at all - hence "one triangle", the
  blink and the unreachable trunk. Each billboard is now registered as a
  resource-backed object (`sprite:` key, model slot, placement), with the ground
  shadow deliberately outside the range.
- **Registered sources no longer hide unregistered geometry.**
  The identified-source preference that fixed the atmospheric haze only
  outranks the nearest candidate when that candidate carries no depth (a
  screen-space overlay; measured `z = 0` against `z ~ 109` for real geometry).
  Real geometry keeps draw-order priority, so an unregistered object stays
  selectable.
- **Names are sanitized.**
  `GetModelNameByIndex` now returns NULL for entries that are not printable
  ASCII, which is the case for several sprite-only name-table entries. Labels,
  catalog records and export file names no longer receive binary characters.
- **Exports describe what they belong to (4).**
  Batch items and the single export accept an `HdTextureExportContext`
  (`type` from the object-key prefix, `level` from `LevelNames[GameLevel]`).
  Manifest entries gain `type` and `level` as descriptive metadata that never
  participates in override matching. An opt-in `Organize exports by type and
  level` developer setting (default off, persisted in `developer_graphics.ini`)
  writes files under `assets/inspector/<type>/<level>/` and records that path in
  the manifest.
- **Palette choice at export time.**
  The `Export textures in base colours` developer setting (default on) decides
  which palette an exported PNG carries. Enabled, the export core substitutes
  the palette the level registered for the exported region; disabled, it uses
  the CLUT of the clicked primitive. Applied inside
  `ExportTextureCore` so the single export and both batches behave identically,
  and covered by the standalone suite (a registered `STP` palette exports alpha
  `128`, the primitive palette exports `255`).
- **Whole-object selection.**
  A double-click in pick mode strips the component suffix from the key
  (`PsyX_Inspector_SetWholeObjectPick`), sets `wholeObject` and matches ranges by
  key prefix, so every part of the instance highlights and exports together.
  Measured: a wheel double-click highlights 136 triangles (whole car) instead of
  the wheel's 30 vertices. The plain single-click part selection is unchanged.

## Verification

- `AssetCatalogTests`: 145 checks, 0 failures.
- `InspectorExportTests`: 77 checks, 0 failures (fresh working directory),
  including the new object-type mapping and organized-export assertions.
- `Release_dev|x64`: 0 failed projects.
- `git diff --check`: clean. PsyCross patch regenerated and verified with
  `git apply --reverse --check`.
- In-game (Chicago day drive, 1280x720): car resolves `ROOF...`; facade on page
  8 resolves `DTGE...`; sprite resolves `sprite:...` (`modelIndex 163`);
  double-click keeps `wholeObject = 1` with 136 highlighted triangles.

## Follow-ups

- Transparency is currently a hard cutout, and the exporter encodes "PSX
  transparent" as pure black, which punches holes in opaque textures. Both are
  completed as
  [`texture-export-alpha-fidelity`](../../../roadmap/done/texture-export-alpha-fidelity.md)
  and
  [`semi-transparent-texture-overrides`](../../../roadmap/done/semi-transparent-texture-overrides.md).
- Catalog-driven batches export the registered base CLUT while a single export
  uses the clicked primitive's CLUT. Palette variants are now modelled through a
  manifest `clut` field and an **Export all palette variants (PNG)** action; see
  [`texture-palette-variants`](../../../product/texture-palette-variants.md).
- The load cost of registering streamed pages was measured; see
  [`texture-page-registration-cost`](../texture-page-registration-cost/index.md).
