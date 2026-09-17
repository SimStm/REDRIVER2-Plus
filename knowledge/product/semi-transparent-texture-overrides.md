---
type: Product
title: Semi-transparent texture overrides
description: Opt-in proportional override alpha on BM_AVERAGE draws, how a hole is authored, and the measured five-step ramp.
tags: [product, textures, rendering, mods]
---

# Semi-transparent texture overrides

Override PNG alpha can be read as opacity instead of a binary cutout. The
behaviour is **opt-in** so existing mods keep the cutout they were authored
against.

## The setting

`Proportional override alpha` on the Mods tab, persisted as
`overrideProportionalAlpha` in `developer_graphics.ini` (default `0`). It is a
live renderer setting: no reload is needed for it to take effect on the next
frame.

The renderer picks an override alpha policy per draw:

| `overrideAlphaMode` | When | Behaviour |
| --- | --- | --- |
| 0 | not an override | Original PSX sampling and blending, unchanged. |
| 1 | override, compatibility | Discard `alpha < 0.5`. The default when the flag is off, and always used for opaque, additive and subtractive draws. |
| 2 | override, flag on, `BM_AVERAGE` | Discard only a fully transparent texel (`alpha < 0.5/255`); everything else blends through `SRC_ALPHA, ONE_MINUS_SRC_ALPHA`. |

`0000h` still exports as `alpha = 0` and still punches through, so foliage,
sprites and pedestrian cutouts keep working in every mode.

## Authoring convention

Once proportional alpha is on for a blending surface:

- `alpha = 0` is a hole (not drawn);
- `alpha = 64/128/192` blend at roughly 25/50/75%;
- `alpha = 128` matches the original `STP=1` 50% blend;
- `alpha = 255` draws the override fully.

Because `alpha = 0` remains the only hole, an author who wants a punch-through
hole must clear the alpha completely; sub-0.5 values are no longer holes when
the flag is on.

## Measured ramp (2026-09-17)

A flat red override at five alpha steps over a `BM_AVERAGE` scene overlay (the
tree shadow on the Chicago debug-start ground). The overlay's un-overridden
green channel at one pixel was `129`; the table shows the same pixel with the
override active:

| Override alpha | Green | Blend of the base |
| --- | --- | --- |
| off (compatibility) | 129 | 100% base (alpha 100 is a hole) |
| 0 | 129 | 100% base (hole) |
| 64 | 97 | ~75% base |
| 128 | 65 | ~50% base |
| 192 | 33 | ~25% base |
| 255 | 1 | 0% base |

The steps are equal (a 32-level drop per 64 alpha), so the imported alpha is
honoured proportionally, `alpha = 128` reproduces the `STP=1` halving, and
`alpha = 0` still discards.

## Picking

The CPU picker mirrors the shader mode: in proportional mode it skips only
texels whose alpha is `0` (the retained coverage mask), and in compatibility
mode it keeps skipping `alpha < 0.5`. A partially transparent texel therefore
stays pickable, matching what is drawn.

## Limits

- Additive (`BM_ADD`, `BM_ADD_QUATER_SOURCE`) and subtractive (`BM_SUBTRACT`)
  effects stay colour-driven: alpha does not fade them and they keep the binary
  cutout, so holes still work but partial alpha does not.
- Opaque (`BM_NONE`) draws keep the binary cutout; proportional alpha is scoped
  to `BM_AVERAGE`, which is what the record's five-step acceptance covers.
- The flag does not change exported PNGs or the batch export path; it only
  changes how an already imported override is blended.

## Revisiting the picking agreement

Picking agreement was verified by construction (same coverage mask and mode in
`DrawSplit` and `ResolvePickVertex`) rather than by an in-game click, because
synthetic mouse input does not reach the ImGui panel in the current test
environment. If a partially transparent override ever renders but cannot be
picked, or a hole becomes pickable:

1. Identify it: click the texel in 3D Debug pick mode and read the selection's
   `cutoutSampled` and the reported alpha policy; the panel prints the effective
   rule for the selected draw.
2. Compare modes: run with `overrideProportionalAlpha` `0` and `1`; a
   `BM_AVERAGE` surface must change and an opaque surface must not.
3. Where to go: `DrawSplit` (`overrideAlphaMode`), `OverrideProportionalSampling`
   and `SampleCutoutMask` in `src_rebuild/PsyCross/src/gpu/PsyX_GPU.cpp`, and
   the shader branch in `src_rebuild/PsyCross/src/render/PsyX_render.cpp`.
4. Re-test: override a known `BM_AVERAGE` overlay with the five-step ramp and
   confirm each step is pickable, or add a check to `InspectorExportTests` if
   the regression is in the exported alpha instead.

## Related

- Rule: [`knowledge/rules/texture-alpha-semantics.md`](../rules/texture-alpha-semantics.md)
- Cutout semantics: [`knowledge/product/texture-alpha-semantics.md`](texture-alpha-semantics.md)
- Export alpha: [`knowledge/product/texture-export-alpha-fidelity.md`](texture-export-alpha-fidelity.md)
- Roadmap record: [`knowledge/roadmap/done/semi-transparent-texture-overrides.md`](../roadmap/done/semi-transparent-texture-overrides.md)
