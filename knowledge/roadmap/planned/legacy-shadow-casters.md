---
type: Roadmap
title: Legacy geometry as shadow casters
status: planned
tags: [rendering, shadows, vulkan, opengl, performance]
---

# Legacy geometry as shadow casters

## Problem

The modern shadow map contains only the imported modern meshes. The legacy
scene (buildings, trees, barriers, the player's and traffic cars) never casts
into it, so a car parked next to a building shows a contact shadow only under
itself and the world reads as unlit. Legacy surfaces already *receive* the
shadow (`knowledge/product/legacy-lighting-receptivity.md`).

## Intended behaviour

Legacy world geometry within the shadow volume casts into the modern shadow
map, so the sun's shadows from buildings, trees and vehicles appear on the
legacy scene. The modern meshes keep casting as they do today.

## Scope

- A depth-only pass over the legacy draw splits with the light matrix, on both
  backends (OpenGL depth program, Vulkan shadow pipeline), reusing the
  already-queued vertex data rather than re-transforming on the CPU.
- Caster selection by distance to the shadow centre (the volume is a few
  thousand units wide), so the pass does not walk the whole level.
- PGXP world positions are available in the vertex path; geometry without them
  (or with unreliable positions) is skipped rather than projected wrongly.

## Non-goals

- Per-caster shadow filtering or per-object shadow toggles.
- Shadow-casting sprites/billboards whose depth is not a world position.
- Changing the receiver path; it already exists.

## Dependencies and risks

- **Recording cost.** The Vulkan backend records the whole frame at present
  time and already spends ~6.4 ms CPU per frame
  (`knowledge/changes/2026-09-21/frame-submission-profiling/index.md`); a
  second pass over the same geometry adds a comparable cost. Caster selection
  and an optional lower shadow-map update rate are the mitigations.
- Legacy draw splits are grouped per state (textures, blend modes) that do not
  matter for a depth pass; the pass should iterate the geometry, not the
  original state batches.
- The PSX scene has no per-object frustum culling on the CPU path, so the
  distance test is the only cheap filter.

## Acceptance criteria

- With the modern sun low, a building and the player's car cast visible shadows
  onto the street that move consistently with the sun as it is rotated.
- No shadow acne on lit legacy surfaces and no shadow where the volume's depth
  test says lit.
- The frame rate stays at the game's 30 Hz gate with the same `submit_ms`
  budget headroom on Vulkan.

## Planned validation

- Deterministic captures (traffic-free playground and the mission 50 spawn) with
  shadows on/off, per backend, plus the same-backend control capture that the
  receive parity work established
  (`knowledge/changes/2026-09-21/shadow-receive-parity/index.md`).
- `PSYX_PERF_LOG=1` `submit_ms` before and after on Vulkan and OpenGL.
