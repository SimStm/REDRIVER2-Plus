---
type: Product
title: Legacy lighting receptivity
description: The modern light set shading legacy scene geometry, its controls, coverage and limits.
tags: [product, rendering, psycross, lighting, shadows]
---

# Legacy lighting receptivity

The modern path can shade the legacy scene with the same light set that lights
modern meshes. It runs as a full-screen composite over the rendered legacy frame
in both backends (Vulkan and OpenGL), reusing the shadow composite pass. Legacy
shading itself is preserved: the term only scales the already-lit colour.

Delivery history and evidence: change record
[`knowledge/changes/2026-09-20/legacy-lighting-receptivity/index.md`](../changes/2026-09-20/legacy-lighting-receptivity/index.md)
and roadmap record
[`knowledge/roadmap/done/legacy-lighting-receptivity.md`](../roadmap/done/legacy-lighting-receptivity.md).

## Controls

- **Graphics panel, "Legacy geometry receives modern lighting"** toggles it;
  **"Legacy light strength"** scales the added sun (`0.00` to `1.00`).
- The same panel section (**Modern sun and look**) exposes the whole
  `developer_modern_mesh.ini` light set live: sun azimuth and height (degrees),
  sun intensity, modern ambient, modern exposure, shadow volume size and a
  shadow debug view. `[`/`]`, `;`/`'`, `-`/`=` and `\` remain the in-game
  equivalents; the panel reads the live direction vector so both agree.
- **F9** toggles it in game, like F10 toggles the enhanced renderer.
- Persisted in `developer_modern_mesh.ini` as `legacyLighting` (default `0`) and
  `legacyLightReceptivity` (default `0.350`, clamped to `0.0`-`2.0`).
- Requires the enhanced renderer. Independent of the shadow and AO toggles: it
  runs with or without them.

## Behaviour

- The sun term is `colour = legacy colour * (1 + strength * N·L * light colour)`.
  The light comes from the first directional light of the modern light set, so
  the sun controls affect legacy geometry too.
- There is no surface normal in the legacy frame: the composite reconstructs the
  world position from the copied scene depth with the modern camera projection
  and derives the normal from a five-tap cross of the neighbour positions,
  flipped towards the camera. The wider taps average out the PGXP depth
  quantisation and polygon-edge steps that made a single-pixel derivative
  flicker on moving vehicles; a pixel whose neighbour depth differs by more than
  `0.0015` keeps the fallback up normal, so a neighbouring surface cannot bend
  it. Grazing surfaces and silhouettes are still approximate.
- Surfaces whose depth falls in the backdrop band (`depth >= 0.999`) are
  excluded. The PSX depth buffer spends that band on flat backdrop layers (sky,
  painted skyline) whose reconstruction saturates; shading them washed the sky
  out.
- The sun term fades out with the reconstructed camera distance between `2x` and
  `4x` the shadow volume half-size, so the term does not end in a hard edge.
- The shadow volume and the fade centre follow the **player vehicle** (the
  camera when no car is active), not the spawn: the old fixed anchor left a
  lit/shadowed bubble behind as soon as the car drove away. The gallery fixtures
  themselves stay fixed in the world. The car's position storage is briefly
  absent during level/mission transitions, so a null pointer falls back to the
  camera instead of crashing.
- The same volume is **snapped to the light-space texel grid** before the light
  matrix is built (`PsyX_ModernShadowSnapCentre`, shared by both backends):
  both backends sample the map with `NEAREST`, and the sub-texel motion of a
  centre that follows a moving car made shadow edges swim. Snapping rounds the
  two plane coordinates to whole `2 * extent / shadowSize` steps and leaves the
  coordinate along the light untouched, so the volume moves by at most half a
  texel and the receiver keeps its exact depth. Measured while driving: the
  raw centre's fractional texel position sweeps `0.38 -> 0.63 -> 0.06 -> 0.78`
  while the snapped position stays at `0.0000`.
- The composite rewrites the scene from a copy of the framebuffer colour. A
  blend on a fixed-point attachment clamps the source to `[0,1]`, so a
  blend-factor implementation can only darken; the copy is what lets the sun
  brighten. This replaced the earlier multiply blend.
- The classic renderer (`modernRenderer=0`, and the modern path disabled) never
  runs the composite and is unchanged.

## Measured coverage

Windows `Release_dev|x64`, mission 50 deterministic spawn, low sun, strength
`0.350`, one 1280x720 capture per state:

- Vulkan, camera-facing surfaces: near road `+12%`, car `+29%`, tree canopy
  `+31%`, tree left of the car `+32%`, jersey barrier `+4%`, grass and plaza
  `+30%`; sky, painted skyline, HUD and minimap `0%`.
- OpenGL, same setup: car `+33%`, tree canopy `+31%`, tree left `+32%`,
  barrier `+27%`; sky `0%`.
- Direction dependence (Vulkan): the same frame with the sun overhead
  (`0,1,0`), low (`-0.25,0.22,-0.38`) and to the side (`0.9,0.22,0.38`) measured
  `+34%`, `+12%` and `+10%` on the near road - the `N·L` falloff expected for a
  horizontal surface.
- Shadow path regression: the shadow-only composite before and after the copy
  change differs by `meanAbs 0.225/255` over the frame (`0.2%` above 32), from
  frame timing. `legacyShadowPass=1` and the shadow shapes are unchanged.
- Classic renderer regression: a `modernRenderer=0` capture against the
  pre-change one differs by `meanAbs 0.290/255` (`0.48%` above 8), frame timing
  on the animated scene.

## Limits

- The composite reads the depth the legacy renderer stores, so it needs both
  PGXP options on: **PGXP texture mapping** writes the per-vertex depth (with it
  off every legacy vertex takes the 2D path at a constant depth) and **PGXP
  Z-buffer** gates depth writes (with it off the legacy scene writes no depth).
  The Graphics tab states this on both checkboxes; the composite simply finds no
  world pixels when either is off.
- Billboard/sprite layers (trees drawn as camera-facing sprites, the painted
  skyline) carry a fixed depth in the PSX depth buffer, so their reconstructed
  position and normal are not trustworthy. They end up either excluded by the
  backdrop band or lit with an approximate normal.
- Legacy geometry does not cast into the modern shadow map; that stays a
  separate follow-up ([planned record](../roadmap/planned/legacy-shadow-casters.md)).
  This feature only makes legacy surfaces *receive* the light and shadows. The
  game also publishes only a directional sun, so the receptivity term has no
  point lights to consume; authoring them is planned
  ([point-light-sources](../roadmap/planned/point-light-sources.md)).
- Backend agreement: the two composites are ports of each other and the
  backends receive the shadow identically under the controlled capture method
  (aligned developer state, traffic-free playground, same-backend control):
  shadowed-pixel IoU 0.985, counts within the 2499-pixel capture noise floor,
  and switching shadows on moves the Vulkan-versus-OpenGL difference by 10
  pixels of 921600. The earlier `7722 vs 2264 (IoU 29.2 %)` figure came from
  drifted developer state (sun direction, `shadowextent` 2500 vs 6991, spawn)
  and moving-traffic capture noise; see
  [`changes/2026-09-21/shadow-receive-parity`](../changes/2026-09-21/shadow-receive-parity/index.md).
- Cost is one full-screen colour copy plus one full-screen composite draw while
  the composite is active (depth copy + composite already ran for shadows).
  Measured at 1280x720 with `vsync=0` from the backend's `perf:` lines, with
  identical scene content in both runs: `frame_ms` mean `33.49` off vs `34.11`
  on over frames 360-960, i.e. about `+0.6 ms` (`+1.9%`) within the sample
  noise. The game's 30 Hz frame gate dominates the frame time, so this is not a
  GPU throughput measurement, and no minimum-GPU-class number exists. The
  composite is free when off.
