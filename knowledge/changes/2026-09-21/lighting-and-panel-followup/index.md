---
type: Change
title: Legacy lighting range/stability and panel follow-up
description: Follow the player with the shadow/lighting centre, stabilise the reconstructed normal, expose the modern sun in the panel, and make the display confirmation and help markers usable.
tags: [rendering, lighting, panel, imgui, input, display]
---

# Legacy lighting range/stability and panel follow-up

Follow-up to
[`legacy-lighting-receptivity`](../../roadmap/done/legacy-lighting-receptivity.md)
and [`runtime-settings-gui`](../../roadmap/done/runtime-settings-gui.md), after
user testing of both.

## Lighting: what changed

- **The shadow volume and the sun fade centre follow the player vehicle** (the
  camera when no car is active). `ApplyLightSet` reads
  `car_data[MainPlayer.playerCarId].hd.where.t` each frame and
  `DeveloperModernMesh_Update` pushes the set every frame. The old centre was a
  spawn anchor 3000 units ahead of the spawn, so driving away left a
  lit/shadowed bubble behind. The gallery fixtures still stay fixed in the world.
- **The reconstructed normal uses a five-tap cross** of the neighbour world
  positions instead of `cross(dFdx(world), dFdy(world))`. The single-pixel
  derivatives amplified PGXP depth quantisation and polygon-edge steps, which
  showed as flickering light on moving vehicles. A neighbour whose depth differs
  by more than `0.0015` from the centre keeps the fallback up normal, so an
  adjacent surface cannot bend the normal; the normal is built in view space and
  the light direction is rotated into view space once, which avoids four extra
  world transforms per pixel.
- **The backdrop cutoff moved from `depth < 0.995` to `depth < 0.999`**, so the
  mid-distance city is no longer cut off while the sky/skyline at ~0.9997 stays
  excluded.
- **The sun term fades with the reconstructed camera distance** between `2x` and
  `4x` the shadow volume half-size, read from the shadow matrix itself
  (`1 / shadowMatrix[0][0]`), so a hard edge does not replace the old cutoff.
- Both backends carry the identical change: `PsyX_ModernMesh.cpp` (GLSL 1.30
  string) and `vk_shaders/psx_composite.frag` with the regenerated header.

## The crash the follow-up exposed

Pushing the light set every frame dereferenced `hd.where.t` unconditionally. The
game keeps that pointer absent while a level, mission or replay re-initialises,
so the game hit an access violation at offset `0x8` (`vz` of a null `VECTOR*`).
Both the new per-frame path and `PositionGallery` now fall back to the camera
when the pointer is null.

## Panel: what changed

- **The display confirmation is its own window** (`DrawDisplayConfirmWindow`),
  drawn by `BuildOverlayWidgets` whether or not the panel is open and *after*
  the panel so it is never covered. It is anchored to the bottom-left of the
  applied display size every frame, so it stays on screen after the mode change
  it is asking about. While armed, the cursor is shown and input stays captured
  (`UpdateOverlayInputState`), so the click cannot also reach the game.
- **`(?)` markers moved to the end of the control's label** in every tab:
  `CheckboxWithHelp` for checkboxes, `SliderFloatWithHelp`/`SliderIntWithHelp`
  for the new sliders. The marker stays on the control's line while it fits and
  moves to its own line otherwise, so long labels are not clipped. Prose blocks
  keep the marker on its own line.
- **The Input tab table is now `Action | Binding | Also bound to | Reset`.** The
  conflict note has its own stretch column and wraps inside the table; before,
  it was a trailing `SameLine` in the binding column, which widened that column
  until the table ran off the panel. The capture banner wraps, and the Reset
  buttons have a fixed width.
- **The modern sun is exposed in the Graphics tab** (`Modern sun and look`):
  azimuth, height, intensity, ambient, exposure, shadow volume size, legacy
  light strength and the shadow debug view. New `DeveloperModernMesh_*`
  getters/setters derive the angles from the live direction vector, so the F-key
  controls and the sliders agree; each setter persists
  `developer_modern_mesh.ini`. The `(?)` text on both PGXP checkboxes now states
  that the composite needs both options.

## Executed evidence

- Windows `Release_dev|x64`: 0 failed projects. `-vkpsxtest` reports
  `psx self-test: PASS` (`main depth format 129 stencil=1`).
- Windows `Release_dev_gl|x64`: 0 failed projects; the GL build ran with
  `legacyShadowPass=1 legacyLightPass=1` and no shader-compile error in
  `REDRIVER2.log`, i.e. the new GLSL composite compiles and runs.
- Debug view 6 (reconstructed normal): ground and road read as up-facing, wall
  facades as sideways, with only 1-2 px discontinuities at geometric edges.
- Debug view 10 (sun coverage): the whole visible street, buildings, vehicles and
  trees are covered; the sky is not.
- The display confirmation window was observed at the bottom-left with its
  countdown, and the 15 s expiry reverted the mode without touching
  `developer_graphics.ini` (`fullscreen=0 windowWidth=1920 windowHeight=1080`
  survived).
- The Graphics, Game Debug and Input tabs were captured with the inline markers
  and the new table; the Game Debug capture reads
  `Show legacy in-game stats (?)`.
- `git diff --check` clean.
