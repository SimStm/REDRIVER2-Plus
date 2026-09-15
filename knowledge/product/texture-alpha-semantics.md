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
50% blended, and the PSX transparent colour is a specific CLUT entry. The
exporter writes the PNG alpha channel as:

- `0` for the PSX transparent colour;
- `128` for `STP=1` texels (just above the 0.5 cutout);
- `255` otherwise.

Re-importing the exported PNG therefore preserves original semitransparency.
An opaque override PNG (`255`) makes a semitransparent surface fully opaque
unless the author reintroduces `128`.

## Blend modes

The blend mode comes from the primitive's tpage (the PSX semi-transparency
mode), not from the texture, so an override inherits the primitive's blend:

| Mode | GL blend | Texel alpha effect |
| --- | --- | --- |
| `BM_AVERAGE` | `SRC_ALPHA, ONE_MINUS_SRC_ALPHA` | Proportional; `128` blends at 50%. |
| `BM_ADD` / `BM_ADD_QUATER_SOURCE` | `ONE, ONE` | Ignored; colour adds at full intensity. |
| `BM_SUBTRACT` | reverse subtract | Ignored; colour subtracts at full intensity. |

## Limits

- No soft alpha below 0.5: such texels are cut out on every primitive.
- Alpha does not fade additive or subtractive effects; only the 0.5 cutout
  reads it there. Use colour, not alpha, to shape glows and lens flares.
- Smooth sub-0.5 alpha on `BM_AVERAGE` primitives would require disabling the
  cutout for semitransparent draws; that is an explicit, opt-in change and not
  the compatibility default.

## Related

- Rule: [`knowledge/rules/texture-alpha-semantics.md`](../rules/texture-alpha-semantics.md)
- Roadmap record: [`knowledge/roadmap/done/texture-alpha-semantics.md`](../roadmap/done/texture-alpha-semantics.md)
- Deterministic scenes and captures: [`knowledge/rules/developer-debug-start.md`](../rules/developer-debug-start.md)
