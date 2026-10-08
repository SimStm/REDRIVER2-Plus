---
type: Plan
title: E03 - Diligent renderer and first playable slice
status: planned
created: 2026-10-08
tags: [engine, diligent, rendering, playable]
---

# E03 - Diligent renderer and first playable slice

## Objective and dependencies

Combine accepted E01 simulation snapshots and E02 converted content into an early
driveable native region. DiligentCore owns GPU resources/presentation; DiligentFX
provides reusable glTF/PBR/shadow/post components where appropriate. Start with
Vulkan on the selected desktop scope and preserve existing platform guards.

## Implementation blocks

1. Pin/build the SDKs and choose an intentional compiler standard/module boundary.
   Integrate the existing window with one device/swapchain/present owner; avoid
   simultaneous independent PsyCross and Diligent swapchains on the same window.
2. Load immutable converted meshes/materials/instances with conventional world
   camera/depth. Use shared resources, stable IDs, culling and bounded residency.
   Remove dependency on GTE projection/ordering-table generation for migrated draws.
3. Preserve alpha-tested trees, painted sky/horizon, terrain overlays and material
   color-space policy. Resolve original coplanar art through explicit native
   decal/layer semantics; file conversion does not remove depth overlap problems.
4. Render the original player's current/interpolated vehicle pose and game camera
   over the converted region. Drive and collide using the original simulation.
   Minimal car visuals suffice for this first slice; full deformation is E06.
5. Add an initial sun/shadow/PBR demonstration using owned/reference materials.
   Keep original prelit art policy explicit rather than claiming inferred PBR.

## Acceptance criteria

- [ ] A real original-game driveable slice displays converted static assets and
  the player through Diligent resources, without old world draws concealing gaps.
- [ ] Camera movement/high-FPS interpolation preserve approved tick states.
- [ ] Source-selected coverage, winding, cutouts, UVs, overlap/depth and visible
  native shadow/lighting fixtures pass the intended new material policy.
- [ ] Native unavailable/loading/error states are distinct; failed resources
  produce clear diagnostics and recover only through valid native resources.
- [ ] Resize/minimize/reload/cancellation/shutdown preserve bounded ownership and
  synchronization; actual CPU/GPU/memory costs and draw paths are recorded.
- [ ] Pending HUD, menus, effects and audio consumers are explicitly accounted
  for; this slice is not a completed engine or proof of every game mode.

## Validation and limits

Run analytic/native resource tests and a fixed route in converted Chicago with
camera turns, an overlap/decal case and cutout trees. Capture actual resources
with RenderDoc1.41; inspect pixels for defects, not universal legacy equivalence.
Measure the accepted scope and state hardware/platform limits. More modern
lighting, whole-city residency and full dynamics may arrive as coherent follow-up
blocks without blocking the first correctly labeled playable slice.
