---
type: Change
title: Compose modern geometry before final gameplay overlays
description: Replace forced screen-space depth with an explicit ordering-table boundary.
tags: [rendering, vulkan, opengl, ui, regression]
---

# Compose modern geometry before final gameplay overlays

## Report and cause

The previous UI fix forced always-pass depth writes for any batch whose first
vertex used the 2D shader path. User testing found that meshes disappeared
behind translucent menus, effects appeared over world geometry, and some view
angles lost the scene. The original acceptance claim is superseded.

Two separate mistakes explain the regression: depth rejection removes the mesh
colour a translucent panel needs to blend with, and `AddSplit` does not separate
2D and 3D vertices. Screen-space primitives also include world/background
effects, not only UI. A first-vertex classification could therefore change the
depth policy of an entire mixed batch and poison depth for subsequent geometry.

## Implementation

PsyCross fork commit: `13705cf` (pushed to `origin/master`). Parent integration
is in `src_rebuild/Game/C/main.c`; the parent gitlink records the fork commit.

- Remove the forced-depth variants and first-vertex classification from both
  backends. Preserve the legacy draw's existing depth and blend policy.
- Single-view `DrawGame` updates modern camera/instances after `RenderGame2`
  and before `SwapDrawBuffers`, and supplies `current->ot + 10` as the final
  overlay boundary. Lens flare uses 10, fades use 8, HUD/map/menu use 0..1.
- Generic `PsyX_SetModernSceneBoundary` records the vertex position as the OT
  is parsed. `DrawSplit` handles a boundary inside an otherwise shared state
  batch without changing primitive order or inspector geometry.
- OpenGL composes immediately at that position and restores texture bindings
  on units 0..4 and the active unit, in addition to the existing saved state.
  A frame guard prevents a second modern draw at `GR_EndScene`.
- Vulkan records contiguous world/overlay draw ranges around its modern pass.
  VRAM upload generations remain ordered across both ranges; the overlay tail
  restores PSX bindings and can close/reopen the loaded pass for transfers.
- Existing callers without a boundary retain end-of-frame composition. The
  split-screen path remains on that fallback and is not part of this validation.

## Executed validation

- Visual Studio `Release_dev|x64` and `Release_dev_gl|x64`: zero failed projects.
- Expanded `REDRIVER2_dev.exe -vkpsxtest`: PASS. All five blend modes now run
  both normally and across the composition boundary with a VRAM copy in the
  overlay tail. Boundary readbacks: `(124,0,0)`, `(93,31,31)`, `(186,62,62)`,
  `(0,62,62)`, `(124,62,62)`; maximum channel error 1. Existing CLUT, offscreen,
  stencil restart, partial-presentation and full VRAM export checks also pass.
  This synthetic test exercises range replay, not a modern mesh silhouette.
- Vulkan live scene: user's saved mission 50, car 3, X=9046, Z=-217980,
  direction=2785. Before: sun over a building and rectangular flare lighting
  holes. After: building occludes the sun, flare holes gone, world and eight
  modern draw calls present, pause panel blends with the scene.
- Drove with Up+Left and inspected the changed view. Then froze simulation
  and set `player[0].cameraAngle` to 0, 1024, 2048 and 3072 through the debugger:
  all four captures contain world geometry and the car. No blank scene was
  observed after the fix. The original total-loss angle was not separately
  isolated in a before capture, so this is bounded regression evidence.
- OpenGL live pause capture using its existing mission-50 snapshot. To prove
  mesh identity, moved imported fixture 0 (bollard) to fixture 2's position and
  moved fixture 2 aside, in debugger memory only: the yellow bollard remains
  visible and darkened beneath the Sfx Volume line while text stays in front.
  Debug session stopped afterwards; no saved snapshot/settings were changed.
- Inspected Vulkan game debug output: validation layer enabled, no VUID.
- `git diff --check` in parent and PsyCross: passed (line-ending notices only).

## Scope and remaining limits

No gameplay, assets, primitive layout, shader or persistent debug-start changes.
This preserves the original ordering of world sprites; it does not introduce
geometric depth for effects whose original vertices have no PGXP depth.
Split-screen, other platforms and an exhaustive sweep of all camera angles
were not tested. Existing R5 shadow parity differences remain separate.

Upstream baseline rechecked on the
[release page](https://github.com/OpenDriver2/REDRIVER2/releases): 8.0, b2d8857.
Product contract: [Vulkan game renderer](../../../product/vulkan-game-renderer.md).
