---
type: Change
title: Legacy lighting receptivity on both backends
description: Apply the modern sun to the already-rendered legacy scene by rewriting the frame from a colour copy, gated to the depth range real surfaces occupy.
tags: [rendering, vulkan, opengl, lighting, shadows]
---

# Legacy lighting receptivity on both backends

Implements
[`legacy-lighting-receptivity.md`](../../roadmap/done/legacy-lighting-receptivity.md).
The compound returned by the light set is now applied to legacy geometry on both
backends. The work also had to fix the composite itself: the previous
multiply-blend implementation could only darken.

## What changed

- `PsyXModernLightSet.legacyLightingScale` and
  `PsyXModernMeshStats.legacyLightPass` in `PsyX_public.h`.
- Vulkan (`PsyX_Vk.cpp`): `sceneColorImage`/`sceneColorView` and a colour copy
  in `RecordGameModernSceneCopy`; the composite descriptor's binding 3; the
  composite pipeline blends disabled; `lightInfo[3]` carries the scale.
- Vulkan shader (`psx_composite.frag`, header regenerated): the scene colour is
  sampled from the copy, the world position is reconstructed for every pixel
  (derivatives are undefined in non-uniform control flow), the legacy normal
  comes from `cross(dFdx(world), dFdy(world))` flipped towards the camera, and
  the sun term scales the sampled colour. Diagnostics: modes 5-7 (receptivity
  probes), 8 (two-channel depth), 9 (two-channel distance), 10 (sun coverage).
- OpenGL (`PsyX_ModernMesh.cpp`): `g_sceneColorTexture` filled with
  `glCopyTexSubImage2D`, blend disabled, the matching uniform set and the same
  shader logic and debug modes.
- `DeveloperModernMesh.*`: `legacyLighting` / `legacyLightReceptivity`
  persisted, clamped and applied; F9 toggle; `legacyLightPass` in the baseline
  log. Panel checkbox and strength slider with help text.

## The two findings

**Blend clamping.** Two captures with the term on (strength `2.0`) and off were
pixel-identical at every sampled surface, while probe 7 showed the shader
receiving `legacyScale >= 1` and probe 5 showed non-zero `ndl` on the same
pixels. A fixed-point attachment clamps the source colour to `[0,1]` before
blending, so `tint = 1 + strength * N·L` was clamped to 1. Sampling a colour
copy and writing `scene * tint` removes the clamp; the first capture after the
change showed the frame brightening (`meanAbs 15.5/255` at strength `0.35`).

**Backdrop depth band.** That first fixed frame washed the sky out
(`(189,205,229) -> (244,255,255)`). A two-channel depth probe measured the PSX
depth: sky and painted skyline `0.9997`, cloud/haze sheet and the tree billboards
`0.990`, ground `0.94-0.98`, buildings `0.985-0.99`. The reconstruction
saturates in the top band, so the sun term is gated to `depth < 0.995`; after
the gate the sky is byte-identical to the unlit frame and the ground, car,
barrier, trees and near buildings all brighten.

## Executed evidence

Windows `Release_dev|x64`, 0 failed projects; `-vkpsxtest` reports
`psx self-test: PASS`. Deterministic spawn (mission 50), 1280x720 captures,
strength `0.350`, low sun `-0.25,0.22,-0.38`:

| Surface | Vulkan base -> lit | OpenGL base -> lit |
| --- | --- | --- |
| near road | `(113,125,85) -> (127,141,96)` | unchanged in that sample |
| car | `(100,112,94) -> (129,144,121)` | `(105,117,99) -> (140,156,132)` |
| tree canopy | `(54,47,16) -> (71,62,21)` | `(59,52,21) -> (77,68,28)` |
| left tree | `(67,90,41) -> (89,119,54)` | `(72,95,46) -> (95,125,60)` |
| jersey barrier | `(51,51,58) -> (53,53,60)` | `(54,54,61) -> (69,69,78)` |
| sky top | unchanged | unchanged |

- Direction dependence (Vulkan, near road): overhead `0,1,0` `+34%`, low
  `-0.25,0.22,-0.38` `+12%`, side `0.9,0.22,0.38` `+10%`.
- Shadow-only regression: old capture vs new capture `meanAbs 0.225/255`,
  `0.2%` above 32 (frame timing), `legacyShadowPass=1` and the same shapes.
- Classic renderer regression: `modernRenderer=0` capture vs pre-change
  `meanAbs 0.290/255`, `0.48%` above 8 (frame timing).
- Coverage probe (mode 10) shows the sun reaching the ground, road, car,
  barrier, trees and buildings, and not the sky, painted skyline, HUD or
  minimap.

Captures and scripts:
`src_rebuild/build/vulkan-debt-20260920/` (`vk-gate-*`, `gl-ll2-*`,
`vk-sun-overhead`, `vk-sun-side`, `vk-shadow-check`, `vk-classic-check`,
`capture-mesh.ps1`, `capture-gl.ps1`, `diff.ps1`).

## Not done

- Point-light receptivity, legacy shadow casters and per-material response stay
  open as scoped.
- The parent gitlink for PsyCross is not bumped: the change is in the fork
  working tree (`src/render/PsyX_Vk.cpp`, `PsyX_ModernMesh.cpp`,
  `PsyX_Vk_Shaders.h`, `vk_shaders/psx_composite.frag[.spv]`).

## Cost

The game is frame-gated to 30 Hz, so the composite cost is hidden inside the
gate. Measured with `vsync=0` on the 1280x720 captured runs, reading the
backend's own `perf:` lines (identical draw and vertex counts in both runs, so
the scene content matches):

| Composite | `frame_ms` mean, frames 360-960 |
| --- | --- |
| off | 33.49 ms |
| on (colour copy + full-screen draw) | 34.11 ms |

`+0.62 ms` (`+1.9%`) at 1280x720, with `+/-0.5 ms` frame-to-frame noise in the
same samples. No frame-time delta on the minimum GPU class was measured, and the
30 Hz gate stays the dominant cost.
