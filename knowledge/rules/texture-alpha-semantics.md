---
type: Rule
title: Design override PNG alpha for the 0.5 cutout and the effect blend mode
description: Override fragments below 0.5 alpha are cut out, BM_AVERAGE honours texel alpha, and additive/subtractive modes ignore it.
tags: [okf, textures, rendering, mods]
---

# Design override PNG alpha for the 0.5 cutout and the effect blend mode

**When** authoring or re-importing an override PNG, **then** treat alpha as a
hard cutout plus an optional 50% blend, not as free-form opacity.

- The RGBA override shader discards every fragment with `color.a < 0.5` while
  an override image is active (`GR_SetOverrideTextureCutout` /
  `overrideCutout`). Verified in Chicago with `GRASS01C`: alpha `128` renders
  opaque, alpha `100` leaves a hole through the ground. There is no soft alpha
  below 0.5 on any primitive.
- Original PSX texels have no fractional alpha. `STP=0` is opaque and `STP=1`
  is 50% blended. The exporter writes `0` for the PSX transparent colour, `128`
  for `STP=1`, and `255` otherwise, so an exported `STP` texel sits just above
  the 0.5 cutout and survives export/import unchanged.
- `BM_AVERAGE` uses `GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA`, so texel alpha is
  honoured proportionally: exported `128` blends at 50%, and upscaled edge
  texels with alpha `>= 0.5` blend softly.
- `BM_ADD`, `BM_SUBTRACT` and `BM_ADD_QUATER_SOURCE` use `GL_ONE, GL_ONE` or
  reverse subtract. Texel alpha does not modulate those effects; the override
  colour adds or subtracts at full intensity and alpha only participates through
  the 0.5 cutout. Do not rely on alpha to fade glows, lens flares or other
  additive/subtractive effects.
- The blend mode comes from the primitive's tpage (the PSX semi-transparency
  mode), never from the texture, so overriding a texture cannot change how an
  effect blends; the override simply inherits the primitive's blend.
- Dropping the cutout for semitransparent primitives would add smooth sub-0.5
  alpha on `BM_AVERAGE` only. Keep that an explicit, opt-in change; it is not
  the compatibility default.

See [Reproduce gameplay with debug start snapshots](developer-debug-start.md)
for the deterministic scenes and captures used to verify these behaviours.
