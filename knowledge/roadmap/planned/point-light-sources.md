---
type: Roadmap
title: Point-light sources and receptivity
status: planned
tags: [rendering, lighting, vulkan, opengl]
---

# Point-light sources and receptivity

## Problem

The shaders and the light-set structure support up to eight lights with point
and directional types, and the modern meshes shade them, but the game publishes
exactly one directional sun (`DeveloperModernMesh.cpp`: `set.count = 1`) and the
legacy composite's receptivity term reads `lights[0]` only. So nothing in the
legacy scene reacts to a light that is not the sun.

## Intended behaviour

- A small set of point lights derived from game state is published each frame
  (candidates to choose from: the player's headlights, emergency/siren lights,
  the nearest street lamps, mission script lights).
- Modern meshes already shade them; the legacy composite gains the same
  diffuse term per point light with distance attenuation, so a lamp lights the
  road and walls around it.

## Scope

- Light authoring in the game layer (`src_rebuild/utils/DeveloperModernMesh.*`
  or the calling game code), with a persisted enable/limit setting and a cap
  well under the eight-light budget.
- The legacy composite loops the point lights on both backends: attenuation
  from the reconstructed world position, the same five-tap normal the sun term
  uses, and additive contribution into the existing sun term.
- A per-light range so the loop can reject distant lights cheaply.

## Non-goals

- Shadow casting from point lights (the shadow map stays directional).
- Per-pixel light culling or clustered/tiled lighting.
- Emissive materials or light bakes.

## Dependencies and risks

- Choosing the sources is a gameplay/visual decision; headlights only help
  where the car points and street lamps need level data the game may not have
  in a convenient form.
- Cost: the composite is full-screen; each point light adds a few ALU
  operations per pixel plus the same number of taps. With the 30 Hz gate there
  is budget, but the count must stay small (two to four).
- Both backends must share the loop so the visual result matches; the existing
  shaders are line-for-line ports and must stay that way.

## Acceptance criteria

- With a published point light near a wall, the legacy wall brightens with
  distance falloff and the effect disappears when the light's range is
  exceeded; the modern meshes shade consistently.
- The effect is identical on Vulkan and OpenGL under the controlled capture
  method (traffic-free scene, per-backend difference below the noise floor).
- No measurable `frame_ms` change with the light count at the configured cap.

## Planned validation

- Fixture-side first: the standalone Vulkan fixture (`-vkfixture`) can publish
  a point light and its capture/readback path shows the result without game
  state; then the game with a debug light anchored at the player.
- `PSYX_PERF_LOG=1` before/after on both backends.
