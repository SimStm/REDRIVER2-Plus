---
type: Product
title: Texture flicker diagnosis and sampling stability
description: Root cause and filtering rules for override shimmer, including mipmaps and anisotropic filtering.
tags: [product, textures, rendering, mods]
---

# Texture flicker diagnosis and sampling stability

The reported `GRASS01C` shimmer was diagnosed as minification aliasing of a
large opaque override, not a binding, depth or LOD regression. This document
describes the sampling rules shipped on the modular-mod-system branch.

## Diagnosis

- `GRASS01C` (page 1, index 5) was measured as fully opaque, so its own alpha
  was not the cause.
- The sample grew from `64x64` to `1024x1024` and the override path had no
  mipmaps, so every distant texel sampled aliased detail.
- Original versus override, stationary versus moving camera, and PGXP/depth
  settings were compared; no binding or depth instability reproduced, so the
  root cause is aliasing.

## Filtering rules

| Override content | Filtering |
| --- | --- |
| Fully opaque | Mipmaps (`glGenerateMipmap`) with `GL_LINEAR_MIPMAP_LINEAR` or `GL_NEAREST_MIPMAP_NEAREST`. |
| Contains transparency | Plain `GL_LINEAR`/`GL_NEAREST`, no mip levels. |

- Opaque overrides are the shimmer fix: mip generation plus mipmap
  minification removes most of the distant aliasing on ground and roads.
- Transparent overrides deliberately keep plain filtering. Mip averaging would
  shrink the alpha coverage that the 0.5 cutout depends on, thinning or
  punching holes in foliage and fences. Keeping a single mip level preserves
  coverage exactly; the trade-off is that cutout edges can still alias at a
  distance.
- Anisotropic filtering is applied to mipmapped overrides when the driver
  exposes `GL_EXT_texture_filter_anisotropic`. The level is capped at `4x`;
  the detected driver maximum is logged at startup (for example `16x` on an
  RTX 3060 Ti). Drivers without the extension keep isotropic filtering.
- The original PSX nearest-filtered path is unchanged, and toggling overrides
  off disables this sampling path entirely.

## Limits

- No frame-time or memory benchmark was recorded; anisotropic filtering adds
  sampling cost only for mipmapped overrides and is bounded by the `4x` cap.
- Cutout overrides still alias at a distance because they have no mip levels;
  adding them would require alpha-coverage-preserving mip generation.
- Scripted reproduction uses the debug-start snapshot and
  `scripts/run_debug_start.ps1 -Capture`.

## Related

- Rule: [`knowledge/rules/developer-debug-start.md`](../rules/developer-debug-start.md)
- Roadmap record: [`knowledge/roadmap/done/texture-flicker-diagnostics.md`](../roadmap/done/texture-flicker-diagnostics.md)
