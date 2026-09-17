---
type: Product
title: Palette variant texture exports
description: How car and pedestrian palette variants get distinct exported PNGs and manifest identities via the clut field.
tags: [product, textures, mods, export, manifest]
---

# Palette variant texture exports

Cars and pedestrians draw one texture region through several runtime palettes
(`civ_clut`), so under the plain `(texture, texturePage, textureIndex)`
identity every palette collapsed into the same registration. A palette variant
export gives each palette its own PNG and manifest entry.

## Manifest `clut` field

An entry may carry an integer `clut` (the GP0 CLUT word, `(y << 6) | ((x >> 4) & 0x3F)`):

- it is part of the override identity, so several entries can share
  `(texture, texturePage, textureIndex)` with different `clut` values;
- the override is registered with that CLUT, so it only replaces draws that use
  that exact palette;
- an entry without `clut` keeps the previous behaviour: it uses the CLUT the
  level registered for the texture, and it is matched only by exports without a
  palette variant, so the two identities never overwrite each other.

At load time every matching entry registers its own override; when two entries
declare the same CLUT the higher mod index (the later enabled mod) wins.

## Exporting

The 3D Debug tab shows **Export all palette variants (PNG)** for a car or
pedestrian selection. It enumerates the CLUTs for the selected texture from
`civ_clut[paletteBlock][textureIndex][0..5]` (palette block is
`GetCarPalIndex(tpage)` for cars and `0` for pedestrians), skips unset (zero)
and duplicate entries, then writes one PNG per variant:

- file name `NAME_p<page>_i<index>_clut<clut>.png`, so variants never overwrite
  each other;
- a manifest entry with `"clut": <clut>`, plus the usual `type`/`level`
  metadata and model references.

The existing **Export original full texture (PNG)** keeps exporting a single
image without a `clut`, which is a distinct entry from every variant.

## Runtime

`HdTextureOverrides_RegisterTexture` collects the distinct CLUTs declared for
the texture and registers one override per CLUT, so a car with several palettes
gets several override mappings from a single registration. A single variant can
also be identified and replaced by a further export of the same `clut`.

Measured in the Chicago debug start: a test mod with two `clut` entries for
`GRASS01C` reported `loaded RGBA images: 2` and `active renderer mappings: 2`,
confirming that both variants register.

## Limits

- `civ_clut` has six palette slots per texture, so at most six variants exist
  for one texture; the exporter enumerates those that are set.
- Only car and pedestrian selections expose the action; other object types have
  no runtime palette variants.
- The enumeration indexes `civ_clut` with the level page-local `textureIndex`,
  which is the same index the car/pedestrian lumps use; a texture drawn from a
  page whose ids do not line up would enumerate the wrong slots. A CLUT of `0`
  is treated as unset, so a palette that legitimately lives at VRAM (0,0) is
  skipped.
- The pipeline is `civ_clut`-driven and does not infer palettes from the draw
  stream.

## Related

- Manifest merging: [`knowledge/product/texture-manifest-merge.md`](texture-manifest-merge.md)
- Export alpha: [`knowledge/product/texture-export-alpha-fidelity.md`](texture-export-alpha-fidelity.md)
- Tests: [`src_rebuild/tests/InspectorExportTests.cpp`](../../src_rebuild/tests/InspectorExportTests.cpp)
