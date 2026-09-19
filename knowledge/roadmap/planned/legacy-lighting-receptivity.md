---
type: Roadmap
title: Legacy lighting receptivity
status: planned
tags: [roadmap, rendering, psycross, lighting]
---

# Legacy lighting receptivity

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
