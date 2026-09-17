---
type: Roadmap
title: Texture export alpha fidelity
status: implemented
completed: 2026-09-17
execution_order: 15
tags: [roadmap, textures, mods, export]
---

# Texture export alpha fidelity

Completed on 2026-09-17 on the `master` branch. Product behaviour is documented
in
[`knowledge/product/texture-export-alpha-fidelity.md`](../../product/texture-export-alpha-fidelity.md).

> Correction recorded during implementation: the original problem statement
> assumed pure black is normally an opaque palette entry. Measurement showed the
> opposite — Driver 2 writes opaque black as `8000h`, and `0000h` is genuinely
> transparent on PSX in every context. The delivered rule follows the hardware
> and the draw's blend context; see the milestone evidence at the end of this
> record.

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
  consume the texture. Corrected by measurement (see milestone evidence): PSX
  treats `0000h` as fully transparent in every context, so a black `0000h`
  texel stays `alpha = 0`; an opaque black on PSX is written as `8000h`, which
  exports `255` on an opaque draw and `128` on a semi-transparent draw.
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

## Milestone evidence (2026-09-17)

### M15.1 — evidence and rule decision

The record's premise that "pure black is an ordinary opaque palette entry in the
majority of Driver 2 textures" did not survive measurement and must be
corrected:

- **Hardware rule.** psx-spx ("Texture Color Black Limitations"): texel `0000h`
  is *always* fully transparent — "textures cannot contain Black pixels";
  opaque black must be written as `8000h`, which is non-transparent on an opaque
  command and semi-transparent on a semi-transparent command.
- **Renderer and mask are already faithful.** The 4/8/16-bit sampler discards a
  raw `0x0000` texel (`PsyX_render.cpp`), and the CPU mask (`alpha >= 128`)
  mirrors the RGBA shader discard (`alpha < 0.5`) exactly.
- **Measured exports** (115 PNGs under `bin/Release_dev/mods/inspector-export`):
  cutout assets (TREE01/02/04, pedestrian limbs) carry black with `alpha = 0`
  (`0000h`); building facades (BWINGC1, DOORC1, FRNTLC1…) carry black with
  `alpha = 128` (`8000h`). No opaque-context texture whose black is `0000h` was
  found.
- **Record's mask reproduced** byte for byte on the current TREE01 export
  (`bits[0] = 0x00`, `bits[60] = 0x00`, `bits[120] = 0xFC`); those zero runs are
  the leaf-gap cutout.

**Decision (confirmed by the maintainer).** Alpha is decided from the STP bit
and the consuming primitive's blend context:

- `0000h` → `0` (fully transparent in every context; preserves foliage and
  pedestrian cutouts);
- STP=1 on a semi-transparent primitive → `128`;
- STP=1 on an opaque primitive → `255` (PSX ignores STP there), which is what
  makes an opaque texture export `255` everywhere;
- everything else → `255`;
- when the blend context is unknown (batch/catalog export), STP=1 stays `128`
  so a semi-transparent surface cannot silently become opaque.

### M15.2 — implementation, tests and in-game evidence

Implemented in `ExportTextureCore`; the primitive's semi-transparency is carried
from the pick (`PsyXInspectorSelection.semiTransparent`, set from the draw
split's blend mode) through `HdTextureExportContext.blendModeKnown` /
`semiTransparent`. The developer panel shows the effective rule for the current
selection.

- `InspectorExportTests`: 88 checks, 0 failures (six new assertions: opaque STP
  export `255`, semi STP export `128`, `0000h` cutout next to opaque texels).
- `AssetCatalogTests`: 145 checks, 0 failures. `Release_dev|x64`: 0 failed
  projects. `git diff --check` clean; PsyCross patch regenerated and verified.
- In-game (Chicago debug start, trees in front of the camera): the
  un-overridden frame and the override frame both show the genuine `0000h`
  canopy holes. Re-exporting the canopy with all alpha forced to `255` (the
  literal rule) produces solid black rectangles around the canopy — the failure
  this record warns about — while keeping `0000h` at `0` and moving STP texels
  from `128` to `255` leaves the frame unchanged.

### M15.3 — documentation and acceptance

Published
[`knowledge/product/texture-export-alpha-fidelity.md`](../../product/texture-export-alpha-fidelity.md),
updated [`knowledge/product/texture-alpha-semantics.md`](../../product/texture-alpha-semantics.md)
and [`knowledge/rules/texture-alpha-semantics.md`](../../rules/texture-alpha-semantics.md),
and moved this record to `done/`.

Acceptance review (2026-09-17):

- opaque texture with black round-trips with no new holes — met (opaque black is
  `8000h`, exported `255`; the `0000h` holes are original PSX transparency and
  are reproduced identically);
- mask equals the renderer discard set for black, `STP` and `0000h` textures —
  met by construction: the mask is `alpha >= 128` and the shader discards
  `alpha < 0.5`, and the export alpha values are covered by tests;
- a tree/sprite/facade is pickable across its surface — met for every non-hole
  texel, including `8000h` black (`alpha = 255`, mask bit set). A `0000h` texel
  is a hole by hardware design and resolves the geometry behind it; the
  product document records how to identify and revisit that case;
- existing `STP=1` semi-transparency still round-trips — met (`128` unchanged
  on `BM_AVERAGE` draws);
- behaviour and re-export requirement documented — met (product document and
  alpha rule).
