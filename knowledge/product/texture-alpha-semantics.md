---
type: Product
title: Texture alpha and PSX blending
description: How override PNG alpha, PSX transparent colour, STP semi-transparency and primitive blend modes interact.
tags: [product, textures, rendering, mods]
---

# Texture alpha and PSX blending

Override PNG alpha is a hard cutout plus an optional 50% blend. It is not
free-form opacity. This document describes the behaviour shipped on the
modular-mod-system branch and its known limits.

## Cutout threshold

While an override image is active for a drawn texture region, the RGBA shader
discards fragments with `color.a < 0.5`
(`GR_SetOverrideTextureCutout` / the `overrideCutout` uniform). The cutoff
applies to every overridden region, on both opaque and semitransparent
primitives.

Verified in the Chicago debug-start scene by overriding `GRASS01C`
(page 1, index 5) with a flat red PNG:

- alpha `128` renders as an opaque red ground;
- alpha `100` is discarded and the ground becomes a hole.

## Export encoding

Original PSX texels have no fractional alpha: `STP=0` is opaque and `STP=1` is
50% blended, and the PSX transparent colour is a specific CLUT entry. Value
`0000h` is *always* fully transparent on hardware, which is why opaque black in
a Driver 2 texture is normally written as `8000h` (`STP=1` black). The exporter
encodes the PNG alpha channel from the palette entry and the blend context of
the primitive that consumes the texture:

- `0` for `0000h`, which PSX discards in every context (foliage and pedestrian
  cutouts keep their holes);
- `128` for `STP=1` texels on a **semi-transparent** draw, reproducing the
  original 50% blend;
- `255` for everything else, including `STP=1` texels on an **opaque** draw,
  because PSX ignores the STP bit there. An opaque texture therefore exports
  `255` everywhere.
- When the blend context is not known (batch/catalog export) `STP=1` stays
  `128`, so a semi-transparent surface can never silently become opaque.

Round-tripping an export is identity-preserving: `0000h` still cuts out, `STP=1`
still blends, and an opaque draw stays opaque. PNGs exported with the earlier
convention (every `STP=1` written as `128`, including on opaque draws) remain
valid and render identically; re-export them only if the stored alpha should
describe opacity precisely. `0000h` PNGs do not need re-exporting.

The developer panel's 3D Debug tab shows the effective rule for the current
selection (`0000h -> cutout`, plus `STP=1 -> 128` or `-> 255`).

## Blend modes

The blend mode comes from the primitive's tpage (the PSX semi-transparency
mode), not from the texture, so an override inherits the primitive's blend.
The STP flag itself is the primitive's semi-transparency bit, which PsyCross
records on the selected draw split.

| Mode | GL blend | Texel alpha effect |
| --- | --- | --- |
| `BM_AVERAGE` | `SRC_ALPHA, ONE_MINUS_SRC_ALPHA` | Proportional; `128` blends at 50%. |
| `BM_ADD` / `BM_ADD_QUATER_SOURCE` | `ONE, ONE` | Ignored; colour adds at full intensity. |
| `BM_SUBTRACT` | reverse subtract | Ignored; colour subtracts at full intensity. |

## Limits

- No soft alpha below 0.5 in the default cutout mode: such texels are cut out on
  every primitive. The opt-in `Proportional override alpha` setting makes
  `BM_AVERAGE` draws honour sub-0.5 alpha proportionally; see
  [`semi-transparent-texture-overrides`](semi-transparent-texture-overrides.md).
- Alpha does not fade additive or subtractive effects; only the 0.5 cutout
  reads it there. Use colour, not alpha, to shape glows and lens flares.
- Smooth sub-0.5 alpha on `BM_AVERAGE` primitives is the opt-in
  `Proportional override alpha` flag (default off, compatibility cutout); opaque,
  additive and subtractive draws keep the binary cutout.

## Related

- Rule: [`knowledge/rules/texture-alpha-semantics.md`](../rules/texture-alpha-semantics.md)
- Roadmap record: [`knowledge/roadmap/done/texture-alpha-semantics.md`](../roadmap/done/texture-alpha-semantics.md)
- Deterministic scenes and captures: [`knowledge/rules/developer-debug-start.md`](../rules/developer-debug-start.md)
