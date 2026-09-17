---
type: Product
title: Texture export alpha fidelity
description: How the inspector encodes exported PNG alpha from PSX transparency and the consuming draw's blend context.
tags: [product, textures, mods, export]
---

# Texture export alpha fidelity

The 3D Debug inspector exports a texture region as a PNG whose alpha channel
describes the texture's PSX transparency, not the RGB colour. This document
covers the encoding, the round-trip guarantee, and how to revisit the one
ambiguous case.

## Encoding rule

PSX texels have no fractional alpha. `0000h` is *always* fully transparent on
hardware — "textures cannot contain Black pixels" — so opaque black in Driver 2
is written as `8000h` (`STP=1` black). The exporter therefore writes:

| Palette entry | Consuming draw | Exported alpha |
| --- | --- | --- |
| `0000h` | any | `0` (cutout) |
| `STP=1` (`8000h` or coloured) | semi-transparent | `128` |
| `STP=1` | opaque | `255` |
| `STP=0`, non-zero colour | any | `255` |

The blend context is the clicked primitive's PSX semi-transparency flag, which
lives in the primitive command and is recorded on the selected draw split
(`PsyXInspectorSelection.semiTransparent`). A batch or catalog export has no
primitive and keeps `STP=1` at `128`, so a semi-transparent surface can never
silently become opaque.

Consequences:

- an opaque texture exports `255` everywhere, including its black texels;
- foliage, sprites and pedestrian parts keep their `0000h` holes;
- `STP=1` semi-transparency round-trips unchanged on `BM_AVERAGE` draws;
- the CPU cutout mask (`alpha >= 128` keeps a texel) still mirrors the renderer
  discard (`alpha < 0.5`), so the picker skips exactly the texels the renderer
  discards.

The 3D Debug tab shows the effective rule for the current selection
(`0000h -> cutout`, plus `STP=1 -> 128` or `-> 255`).

## Re-exporting older PNGs

PNGs exported before this change wrote every `STP=1` texel as `128`, including
on opaque draws. They remain valid and render identically on an opaque draw
(STP is ignored there) and on a semi-transparent draw (blending uses `128`), so
they do not have to be replaced. Re-export a texture only if the stored alpha
should describe opacity precisely. `0000h` cutouts were already `0` and never
need re-exporting.

## Where the rule lives

- Encoder: `ExportTextureCore` in `src_rebuild/utils/HdTextureOverrides.cpp`.
- Blend context: `HdTextureExportContext.blendModeKnown` / `semiTransparent`
  in `src_rebuild/utils/HdTextureOverrides.h`, fed by the panel.
- Selection flag: `PsyXInspectorSelection.semiTransparent` in
  `src_rebuild/PsyCross/include/PsyX/PsyX_public.h`, set in
  `ResolveInspectorPickInternal` in `src_rebuild/PsyX_GPU.cpp` from the draw
  split's `blendMode`.
- Mask and picker mirror: `RetainCutoutMask` / `SampleCutoutMask` in
  `src_rebuild/PsyCross/src/gpu/PsyX_GPU.cpp`.

## Revisiting the ambiguous case

A `0000h` texel is a hole, not surface: it is transparent on PSX in every
context, and the original game renders it that way. If a tree canopy, sprite
edge or facade is ever reported as "missing" or as selecting the geometry
behind it, check first whether the clicked texel is genuinely `0000h`:

1. Identify it: export the region and read the PNG alpha at the clicked texel
   (or use the panel's **Explain texture lookup** / **Locate pickable pixel**
   actions and the `g_hdDebug*` counters). `alpha = 0` is a hole by design;
   `alpha >= 128` is surface and must be pickable.
2. Compare with the un-overridden draw: `developer_graphics.ini`
   `hdTextureOverrides=0` renders the same holes, which proves they are
   original PSX transparency, not an export defect.
3. Where to go: `ExportTextureCore` (alpha decision),
   `PsyXInspectorSelection.semiTransparent` (blend context), and
   `SampleCutoutMask` (picker). Do not re-introduce "colour is black" as the
   transparency rule; that is the defect this feature corrected.
4. Re-test: add or extend a check in `InspectorExportTests` for the alpha value
   at that texel class, then reproduce the frame with the deterministic Chicago
   debug start. A quick way to test the rule in game without the panel is to
   edit a copy of an existing export under `mods/` and toggle it through
   `mods/enabled.json`; forcing all alpha to `255` must turn a cutout into the
   solid black rectangle the rule prevents.

## Related

- Rule: [`knowledge/rules/texture-alpha-semantics.md`](../rules/texture-alpha-semantics.md)
- Core alpha semantics: [`knowledge/product/texture-alpha-semantics.md`](texture-alpha-semantics.md)
- Roadmap record: [`knowledge/roadmap/done/texture-export-alpha-fidelity.md`](../roadmap/done/texture-export-alpha-fidelity.md)
- Deterministic scenes and captures: [`knowledge/rules/developer-debug-start.md`](../rules/developer-debug-start.md)
