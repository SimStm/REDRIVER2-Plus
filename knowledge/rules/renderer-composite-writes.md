---
type: Rule
title: Write composite results instead of blending above 1.0
description: Screen-space composites must sample the scene colour, and the PSX depth top band is backdrop.
tags: [okf, rendering, vulkan, opengl]
---

# Write composite results instead of blending above 1.0

**When** a full-screen composite multiplies or tints the already-rendered scene,
**then** sample a copy of the scene colour and write the result, rather than
expressing the tint as a blend factor. Vulkan and OpenGL clamp the source colour
(and the blend factors) to `[0,1]` for a fixed-point colour attachment, so a
tint above 1.0 - any brightening term - is silently reduced to 1.0 and the
effect disappears. Darkening terms (`tint <= 1`) work either way, which is why
the shadow-only composite looked correct before the sun term was added.

Implementations in this repository:

- Vulkan: `RecordGameModernSceneCopy` copies the swapchain image into
  `sceneColorImage` next to the depth copy, the composite samples it at binding
  3, and `gameCompositePipeline` has `blendEnable = VK_FALSE`.
- OpenGL: `glCopyTexSubImage2D` fills `g_sceneColorTexture` next to the depth
  blit, and the composite draws with `glDisable(GL_BLEND)`.

**When** reconstructing world position and normals from the PSX depth buffer,
**then** treat the top band as backdrop, not geometry. The emulated PSX depth
spends roughly `>= 0.995` on flat backdrop layers (sky, painted skyline,
cloud/haze sheet) and on camera-facing sprites, whose reconstruction saturates:
the derived normal carries no information and shading them with the sun washes
the sky out. Real surfaces in the mission 50 spawn resolve below the band
(ground `0.94-0.98`, buildings `0.985-0.99`). The receptivity term is gated to
`depth < 0.999` (revised 2026-09-21: `0.995` cut the mid-distance city off) and
fades out with distance before the gate, so the boundary is not a hard edge.

**When** deriving a normal from the depth buffer, **then** use a multi-tap
cross of the neighbour world positions with an edge test, not single-pixel
`dFdx`/`dFdy`. The per-pixel derivative amplifies the PGXP depth quantisation
and the polygon-edge steps, which showed as flickering light on moving
vehicles; a neighbour whose depth differs by more than `0.0015` keeps the
fallback up normal so an adjacent surface cannot bend it.

**When** the composite reads the depth buffer, **then** remember it depends on
both PGXP options: PGXP texture mapping writes the per-vertex depth (otherwise
every legacy vertex takes the 2D path at a constant depth) and PGXP Z-buffer
gates depth writes (otherwise the legacy scene writes no depth at all). State
this in any UI that exposes them; with either off the composite finds no world
pixels.

Evidence: 2026-09-20 legacy lighting receptivity. With the multiply blend and
`legacyLightReceptivity=2.0`, two frames with the term on and off were identical
at every sampled surface while the `legacyScale` and `ndl` probe modes showed
non-zero values, which is what identified blending as the cause. The depth bands
were measured with the composite's two-channel depth probe (mode 8) after the
sky was observed brightening by `+29%`. The five-tap normal and the PGXP
dependency were confirmed on 2026-09-21 by the mode 6 (normal) and mode 10
(sun coverage) captures and by the user's PGXP A/B report.
