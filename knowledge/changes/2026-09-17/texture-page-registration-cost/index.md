---
type: Change
title: Measure texture-page registration cost in the spool
description: Load-time cost of RegisterHdTextureOverridesForPage from SendTPage, with a permanent panel diagnostic.
tags: [okf, textures, performance, mods, spool]
---

# Measure texture-page registration cost in the spool

## Context

After streamed pages began registering their texture names
(`SendTPage` → `RegisterHdTextureOverridesForPage`, change
[`texture-identity-and-export-metadata`](../texture-identity-and-export-metadata/index.md)),
a page's VRAM slot assignment now runs a name-registration pass on the loading
thread. That pass also decodes override PNGs (`EnsureImageLoaded`), so its
cost needed measuring before it is treated as free.

## Decision

Add a lightweight, always-on accumulator in `HdTextureOverrides`: the panel's
`Mods` tab reports the total, the page count, the texture count and the average
microseconds per page. Timing covers the whole registration loop in
`RegisterHdTextureOverridesForPage`.

## Verification

Chicago debug start, Release_dev, 1280x720, `SendTPage` registration pass:

| Mods enabled | Pages | Textures | Total | Average |
| --- | --- | --- | --- | --- |
| `inspector-export` (79 entries) + `remaster-textures` | 18 | 364 | **927.6 ms** | 51.5 ms/page |
| none (all mods disabled) | 18 | 364 | **0.176 ms** | 9.9 us/page |

The name-registration work itself is negligible (0.176 ms for the level). The
cost is the PNG decode performed by `EnsureImageLoaded` inside
`HdTextureOverrides_RegisterTexture`; with ~79 override images in the
`inspector-export` mod it dominates the pass. `remaster-textures` uses large
upscaled PNGs, so a load with those enabled is heavier still.

## Consequence

- The measurement is now visible live; no behaviour changed.
- Known follow-up (not done here): decode override PNGs lazily on first draw
  instead of during page registration, which would remove this load spike. The
  spool cost is acceptable at the measured magnitude but scales with the number
  and size of override images.
