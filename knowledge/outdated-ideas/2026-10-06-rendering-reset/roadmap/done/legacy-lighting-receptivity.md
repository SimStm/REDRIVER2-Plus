---
type: Roadmap
title: Legacy lighting receptivity
status: implemented
completed: 2026-09-20
tags: [roadmap, rendering, psycross, lighting]
---

# Legacy lighting receptivity

Status: implemented 2026-09-20. Product behaviour is documented in
[`knowledge/product/legacy-lighting-receptivity.md`](../../product/legacy-lighting-receptivity.md).
This record is the delivery history; the product document and the source are the
authoritative evidence for current behaviour.

## Delivered

The modern light set now shades the legacy scene on both backends. The shadow
composite pass grew a diffuse sun term driven by the light set's first
directional light, applied to the already-rendered legacy colour, with a
depth-derived screen-space normal. `developer_modern_mesh.ini` gained
`legacyLighting` and `legacyLightReceptivity`; the Graphics panel gained a
checkbox and a strength slider and F9 toggles it.

Two findings shaped the implementation, both recorded in
[`knowledge/rules/renderer-composite-writes.md`](../../rules/renderer-composite-writes.md):

- A blend on a fixed-point attachment clamps the source to `[0,1]`, so the
  original multiply-blend composite could only darken. The composite now samples
  a copy of the framebuffer colour and writes the tinted result (Vulkan: an
  extra `swapchainImages[imageIndex]` -> `sceneColorImage` copy; OpenGL:
  `glCopyTexSubImage2D` into a new `g_sceneColorTexture`).
- The PSX depth buffer spends its top band on flat backdrop layers, so the sun
  term is gated to `depth < 0.995`. Without it the sky and the painted skyline
  were shaded and the sky washed out.

## Acceptance

- Both backends apply the sun to legacy surfaces with the expected `N·L`
  behaviour and leave the sky, HUD and minimap untouched; measurements per
  backend are in the product document.
- The sun term follows the light-set direction: the near road measured `+34%`,
  `+12%` and `+10%` for overhead, low and side suns.
- Toggle and strength persist in `developer_modern_mesh.ini` and are independent
  of the shadow and AO toggles.
- The classic renderer is unchanged (`meanAbs 0.290/255` against the pre-change
  capture, frame timing) and the shadow-only composite is unchanged
  (`meanAbs 0.225/255`).
- Windows `Release_dev|x64` built with 0 failed projects; `REDRIVER2_dev.exe
  -vkpsxtest` reports `psx self-test: PASS`.

## Not delivered

- Per-point-light receptivity, per-material response, legacy casters into the
  shadow map and screen-space ambient occlusion remain open, as scoped.
- No minimum-GPU-class frame-time measurement. At 1280x720 with `vsync=0` the
  composite measured `+0.6 ms` (`+1.9%`, `33.49` -> `34.11 ms` mean over
  frames 360-960) against frame-to-frame noise, inside a 30 Hz frame gate that
  dominates the frame time.


## Problem

The experimental modern path can already cast shadows onto legacy scenery: the
shadow map is projected over the rendered frame so modern objects darken the
road, walls and trees. Legacy geometry is still lit exclusively by the original
PSX vertex-colour/`Apply_Ambient` model, so the modern light set - directional
sun, point lights, ambient and exposure - has no effect on it beyond shadows.
Walking from a legacy wall into the light of a modern object therefore changes
nothing, and an added street lamp cannot brighten the scenery it stands on.

## Intended behaviour

- Legacy world geometry reacts to the modern light set: at minimum the
  directional sun contributes a diffuse term, and later point lights with range
  falloff.
- The reaction is measured and toggleable separately from shadows, so a build
  can keep legacy shading exactly as shipped (the current default) or enable
  receptivity, and the two states can be compared frame by frame.
- Modern and legacy surfaces under the same light look consistent: the same
  light direction, colour, intensity, ambient and exposure drive both.
- The classic/enhanced switch keeps the classic renderer byte-for-byte
  unchanged when the modern path is disabled.

## Scope

- A legacy-fragment shading term driven by the modern light set, with the world
  position already reconstructable from the shared depth buffer (the shadow
  composite proves the path).
- Reuse of the existing light-set API (`PsyXModernLightSet`) and the
  `PsyX_ModernMesh_SetCamera` frame camera.
- New developer settings: a `legacyLighting` toggle plus a receptivity strength
  or diffuse scale, persisted in `developer_graphics.ini`, exposed in the
  Graphics panel and reachable from a key for A/B capture.
- Documentation of the measured coverage: which legacy materials/vertex-colour
  paths are affected, and what is deliberately left untouched.

## Non-goals

- Reconstructing full PSX `Apply_Ambient`/lighting semantics or replacing the
  legacy shading model.
- Legacy geometry casting into the modern shadow map (that is a separate
  bounded follow-up; today only modern casters exist).
- Per-pixel PBR on legacy surfaces, normal-mapped legacy materials or
  screen-space ambient occlusion.
- Changing original-camera lighting for missions; the feature stays a developer
  experiment behind the enhanced-renderer switch until validated.

## Dependencies and risks

- The legacy scene has no world-space vertex attributes left: the term must be
  reconstructed from the copied depth buffer, which is depth-precision limited
  and cannot recover the surface normal. A conservative normal estimate (for
  example from depth derivatives) may be needed, with visible artefacts at
  silhouettes.
- The 2D path shares the same fragment stage; the HUD and overlays must not pick
  up lighting (the shadow composite already excludes the constant 0.5 depth and
  is the precedent to follow).
- Cost: the term is full-screen per-pixel work at window resolution on the base
  GPU class (GTX 1050 Ti / RX 460); it must be measurable and toggleable.

## Acceptance criteria

- With receptivity on, a legacy surface facing the modern sun is measurably
  brighter than the same surface with the modern path disabled, and rotating the
  sun (`[`/`]`, `;`/`'`) moves the bright side in real time.
- The toggle persists across relaunch and is independent from the shadow and AO
  toggles in the Graphics panel.
- The classic renderer output is unchanged when the enhanced renderer is off;
  the game still runs on targets that only support OpenGL.
- Coverage and limitations are recorded with measurements in the renderer
  discussion before any original-city rollout.

## Validation plan

- Windows `Release_dev|x64` build with 0 failed projects; `git diff --check`
  clean; inspector suites still pass.
- Playground A/B captures with receptivity off/on under a known sun, plus one
  original city pass; shadow and AO toggles exercised independently.
- Frame-time delta recorded at 1080p on the stated minimum GPU class when one is
  available; otherwise the measurement limitation is stated explicitly.
