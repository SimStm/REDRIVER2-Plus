---
type: Change
title: Snap the shadow volume centre to the light-space texel grid
description: The modern sun's shadow map no longer swims while the volume follows a moving receiver.
tags: [rendering, vulkan, opengl, shadows, quality]
---

# Snap the shadow volume centre to the light-space texel grid

## Report and cause

The modern shadow volume follows the player vehicle (see
[legacy lighting receptivity](../legacy-lighting-receptivity.md)), so its
light-space origin advances by a fraction of a texel every frame while the car
moves. Both backends sample the shadow map with `NEAREST` filtering, so that
fraction flips the compared texel back and forth and shadow edges appear to
swim over otherwise stable silhouettes.

## Implementation

PsyCross fork, `src/render/PsyX_ModernMesh.{h,cpp}` and the Vulkan call site in
`src/render/PsyX_Vk.cpp`:

- `PsyX_ModernShadowSnapCentre()` (shared by both backends, next to the other
  cross-backend helper) derives the light basis from the sun direction, projects
  the requested centre onto it, rounds the two plane coordinates to whole
  `2 * extent / shadowSize` steps and rebuilds the centre. It also returns the
  up vector it used so the caller's look-at matrix shares the basis.
- Both shadow-matrix builders call it and construct their look-at/ortho matrix
  from the snapped centre. The duplicated vertical-light up-vector rule moved
  into the helper.
- Only the plane perpendicular to the light is snapped. The coordinate along
  the light keeps following the receiver exactly, so the volume stays centred on
  its depth; the snap moves the centre by at most half a texel.

## Evidence

Windows `Release_dev|x64`, mission 50, driving forward for six seconds, a
temporary log of the fractional texel position of the centre in the light plane:

```
rawFrac=(0.3773,0.0498) snapFrac=(0.0000,0.0000) deltaTexels=(-0.38,-0.05)
rawFrac=(0.6301,0.7528) snapFrac=(0.0000,0.0000) deltaTexels=( 0.37, 0.25)
rawFrac=(0.0612,0.2462) snapFrac=(0.0000,0.0000) deltaTexels=(-0.06,-0.25)
rawFrac=(0.7769,0.3740) snapFrac=(0.0000,0.0000) deltaTexels=( 0.22,-0.37)
```

The raw fraction sweeps across the texel (0.38 -> 0.63 -> 0.06 -> 0.78) while
the snapped fraction is stationary, and the snap stays within half a texel
(`|deltaTexels| <= 0.42`). The instrumentation was removed after the run; the
sphere of driver 2 world coordinates (~2 * 10^5 units, float ulp ~0.016) is why
the snap is computed in double.

- A live capture during the same run showed the world, the car contact shadow
  and the fixture shadows unchanged, and the tree/barrier shadows still
  coherent.
- `REDRIVER2_dev.exe -vkpsxtest` passed afterwards (`VkFixture: psx self-test
  PASS`), which covers the shadow pass and the composition boundary.
- The OpenGL backend shares the helper and builds; it was not captured
  separately for this change.
