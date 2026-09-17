---
type: Roadmap
title: Texture export alpha fidelity
status: planned
execution_order: 15
tags: [roadmap, textures, mods, export]
---

# Texture export alpha fidelity

## Problem

The exporter derives the PNG alpha channel from the palette colour instead of
from any evidence about the texture's own transparency
(`HdTextureOverrides.cpp`, `ExportTextureCore`):

```cpp
pixel[3] = colour == 0 ? 0 : ((colour & 0x8000) ? 128 : 255);
```

`colour == 0` is treated as "the PSX transparent colour" for every texture and
every draw. Pure black is an ordinary opaque palette entry in the majority of
Driver 2 textures, so exporting such a texture punches holes in it:

- the exported PNG marks every black texel `alpha = 0`;
- `PsyX_CreateRGBATexture` builds a 1 bit/texel cutout mask from that alpha;
- the renderer discards those texels (`GR_SetOverrideTextureCutout`), so the
  surface renders with holes once the export is enabled as a mod;
- the picker mirrors the same mask (`SampleCutoutMask`), so clicking those
  texels resolves nothing and the geometry behind them wins.

Measured on `TREE01` in the Chicago debug-start scene: the exported mask has
long zero runs (`bits[0] = 0x00`, `bits[60] = 0x00`,
`bits[120] = 0xFC`), and clicking a canopy or trunk texel inside them resolves
the object behind the tree instead of the tree.

The authoring loop is therefore not identity-preserving: exporting an untouched
texture and re-importing it changes what is drawn.

## Intended behaviour

Alpha in an exported PNG must describe the transparency of the texture, never
"the colour happens to be black".

- Opaque textures export `alpha = 255` everywhere, including palette entries
  that are black.
- Genuine transparency is decided from real evidence, not from the colour value:
  the STP bit of the palette entry and the blend context of the primitives that
  consume the texture. PSX index-0 transparency is a property of
  semi-transparent/sprite draws, not of the texture in isolation.
- Round-trip identity: export, re-import and enable the result, and the frame
  must be unchanged from the un-overridden draw.
- The CPU cutout mask must describe exactly the set of texels the renderer
  discards, and the picker must keep skipping only those.

## Scope

- The alpha encoding in `ExportTextureCore` and any shared helper it gains.
- The evidence used to decide transparency (STP bit, palette entry, and the
  blend mode of the consuming primitives where it can be resolved).
- The mask builder and the documented meaning of its bits.
- Exported-PNG naming/metadata documentation touchpoints that describe alpha.
- Regression coverage for at least: a texture containing black, a texture with
  `STP=1` entries, and a texture used only by opaque primitives.

## Non-goals

- Free-form or sub-0.5 opacity and smooth blending. That is
  [`semi-transparent-texture-overrides`](semi-transparent-texture-overrides.md).
- Changing original (non-overridden) PSX sampling or the transparency of
  unmodified draws.
- Re-deriving the blend mode of primitives from anything other than the tpage,
  which already carries it.

## Dependencies and risks

- Depends on order 01,
  [texture alpha and PSX blending correctness](../done/texture-alpha-semantics.md),
  whose cutout threshold and export encoding this record corrects.
- PNGs already exported and shipped by users carry the old convention. Changing
  the encoder does not rewrite them; the record must state that affected
  textures need re-exporting, and the developer panel should make the
  distinction visible.
- Misclassifying a genuinely transparent index-0 entry as opaque would make
  foliage and sprite edges render as solid rectangles. The
  `STP`/blend-mode evidence must be validated against a known tree, a known
  sprite and a known opaque facade before the change is accepted.

## Acceptance criteria

- Exporting an opaque texture that contains black and re-importing it produces
  a frame identical to the un-overridden draw; no holes appear.
- The mask built from an export equals the set of texels the renderer discards
  for that override, for a texture with black, one with `STP` entries and one
  with index-0 transparency.
- A tree, a sprite and a facade can be clicked across their full surface,
  including black texels, and resolve the object itself.
- Existing `STP=1` semi-transparency still round-trips as before.
- The behaviour and the re-export requirement are documented in
  `knowledge/product/texture-alpha-semantics.md` (or a successor product
  document) and in the rule that governs alpha.

## Validation plan

- Fixed Chicago debug-start scene, as required by
  [`developer-debug-start`](../../rules/developer-debug-start.md), so results are
  comparable between runs.
- Export → enable as a mod → screenshot and numeric pixel probes of the same
  view, before and after, for: a facade with black in its palette, `TREE01`, and
  a road texture with no black.
- Compare the mask bit count and the picker's cutout verdict at the same pixels
  that were failing before.
- Re-run the standalone suites and rebuild `Release_dev|x64`.

## Starting points

- Encoder: `src_rebuild/utils/HdTextureOverrides.cpp` — `ExportTextureCore`.
- Mask: `PsyX_CreateRGBATexture` / `RetainCutoutMask` / `SampleCutoutMask` in
  `src_rebuild/PsyCross/src/gpu/PsyX_GPU.cpp`.
- Renderer cutout: `GR_SetOverrideTextureCutout`, `overrideCutout`.
- Current semantics and limits:
  [`knowledge/product/texture-alpha-semantics.md`](../../product/texture-alpha-semantics.md).
