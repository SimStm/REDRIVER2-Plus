---
type: Rule
title: Design override PNG alpha for the 0.5 cutout and the effect blend mode
description: Override fragments below 0.5 alpha are cut out by default, BM_AVERAGE honours texel alpha proportionally, additive/subtractive modes ignore it, and proportional alpha is an opt-in flag.
tags: [okf, textures, rendering, mods]
---

# Design override PNG alpha for the 0.5 cutout and the effect blend mode

**When** authoring or re-importing an override PNG, **then** treat alpha as a
hard cutout plus an optional 50% blend by default, or as proportional opacity
when the opt-in `Proportional override alpha` setting is on.

- The RGBA override shader uses `overrideAlphaMode` (`GR_SetOverrideAlphaMode`):
  `1` (compatibility, the default) discards every fragment with `color.a < 0.5`;
  `2` (opt-in `overrideProportionalAlpha` on a `BM_AVERAGE` draw) discards only
  a fully transparent texel and blends the rest through
  `GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA`. Verified in Chicago with `GRASS01C`:
  alpha `128` renders opaque, alpha `100` leaves a hole through the ground in
  compatibility mode. With the flag on, a `BM_AVERAGE` overlay was measured as
  five equal steps (`alpha 0/64/128/192/255` leaving 100/75/50/25/0% of the
  base), and `alpha 128` reproduces the `STP=1` halving.
- Original PSX texels have no fractional alpha. `0000h` is *always* fully
  transparent on hardware, so opaque black is written as `8000h` (`STP=1`
  black). The exporter writes `0` for `0000h`, `128` for `STP=1` on a
  semi-transparent draw, and `255` otherwise — including `STP=1` on an opaque
  draw, where PSX ignores the STP bit. An opaque texture therefore exports
  `255` everywhere. With no known blend context (batch/catalog export) `STP=1`
  stays `128`. Round-tripping is identity-preserving; an export made with the
  earlier "every STP=1 is 128" convention renders identically but can be
  re-exported so the stored alpha describes opacity precisely.
- `BM_AVERAGE` uses `GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA`, so texel alpha is
  honoured proportionally: exported `128` blends at 50%, and upscaled edge
  texels with alpha `>= 0.5` blend softly.
- `BM_ADD`, `BM_SUBTRACT` and `BM_ADD_QUATER_SOURCE` use `GL_ONE, GL_ONE` or
  reverse subtract. Texel alpha does not modulate those effects; the override
  colour adds or subtracts at full intensity and alpha only participates through
  the cutout. Do not rely on alpha to fade glows, lens flares or other
  additive/subtractive effects.
- The blend mode comes from the primitive's tpage (the PSX semi-transparency
  mode), never from the texture, so overriding a texture cannot change how an
  effect blends; the override simply inherits the primitive's blend.
- Proportional alpha on `BM_AVERAGE` is an explicit, opt-in change
  (`Proportional override alpha`, default off); opaque, additive and
  subtractive draws keep the binary cutout, so `alpha = 0` remains the only hole
  and picking mirrors the mode.

A `0000h` texel is a hole, not surface, so if a tree, sprite or facade is ever
reported as missing or as selecting the geometry behind it, check the exported
alpha at that texel first and follow
[Revisiting the ambiguous case](../product/texture-export-alpha-fidelity.md#revisiting-the-ambiguous-case)
before changing the rule.

See [Reproduce gameplay with debug start snapshots](developer-debug-start.md)
for the deterministic scenes and captures used to verify these behaviours.
