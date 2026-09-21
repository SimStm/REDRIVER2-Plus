---
type: Status
title: Recent engineering context
description: Panel follow-up (display confirmation, help markers, Input tab, modern sun controls) and legacy-lighting range/stability fixes.
tags: [okf, status, agents]
---

# Recent engineering context

> Updated: 2026-09-21
> Confirm this handoff against source, Git state and actual validation output.

## Objective and status

Follow-up work after the user tested the two delivered features. Two threads:

1. **Panel (item 4)**: the display Keep/Revert confirmation never appeared when
   a mode change hid or clipped the panel; the `(?)` markers sat below the
   checkboxes; the Input tab's text/table layout was broken.
2. **Legacy lighting (item 3)**: while driving, the lighting looked like a short
   range around the vehicle, slightly distant objects were not lit, and the
   player car's lighting looked wrong. The user tied it to the PGXP options and
   asked for sun height/rotation controls plus the rest of
   `developer_modern_mesh.ini` in the GUI.

Both are implemented and verified. Nothing is committed: the parent tree holds
the knowledge edits plus `utils/*` changes, and the PsyCross fork working tree
holds the renderer changes (no gitlink bump). See
[`changes/2026-09-21/lighting-and-panel-followup/index.md`](changes/2026-09-21/lighting-and-panel-followup/index.md).

## Panel changes

- `DrawDisplayConfirmWindow` is a separate ImGui window anchored to the
  bottom-left of the applied display size, drawn by `BuildOverlayWidgets`
  whether or not the panel is open and *after* it so it is never covered.
  `UpdateOverlayInputState` shows the cursor and captures input while armed.
  Observed live: the window appeared on a fullscreen toggle, counted down, and
  the expiry reverted the mode without writing `developer_graphics.ini`.
- `CheckboxWithHelp` / `SliderFloatWithHelp` / `SliderIntWithHelp` put the `(?)`
  at the end of the control label, moving it to its own line only when the label
  fills the line. Applied across every tab; prose blocks keep their marker on a
  separate line.
- The Input tab table is now `Action | Binding | Also bound to | Reset`, with
  the conflict note in its own stretch column so it wraps inside the table
  instead of widening the binding column past the panel. The capture banner
  wraps and the reset buttons are fixed-width.
- The Graphics tab gained **Modern sun and look**: azimuth, height, intensity,
  ambient, exposure, shadow volume size, legacy light strength and the shadow
  debug view. New `DeveloperModernMesh_*` getters/setters derive the angles from
  the live direction vector (F-keys and sliders agree) and persist
  `developer_modern_mesh.ini`. The PGXP checkboxes now state that the composite
  needs both options on.

## Lighting changes

- The shadow volume and the sun fade centre **follow the player vehicle** (the
  camera when no car is active) instead of a fixed spawn anchor; the set is
  pushed every frame.
- The reconstructed normal is a **five-tap cross** of the neighbour world
  positions with an edge test (`> 0.0015` depth difference keeps the fallback up
  normal), replacing the single-pixel `dFdx/dFdy` normal that flickered on
  moving vehicles.
- The backdrop cutoff moved `0.995 -> 0.999`, and the sun term now fades with
  the reconstructed camera distance between `2x` and `4x` the shadow volume
  half-size, read from `1 / shadowMatrix[0][0]`. Identical logic in the GLSL
  1.30 string and `psx_composite.frag` (SPIR-V regenerated).

## The crash this exposed

Pushing the light set every frame dereferenced `hd.where.t` unconditionally;
the pointer is absent while a level/mission/replay re-initialises, and the game
hit an access violation at offset `0x8`. Both the per-frame path and
`PositionGallery` now fall back to the camera on a null pointer. After the fix
the game ran under the debugger for several minutes with no break.

## Executed evidence

- `Release_dev|x64` and `Release_dev_gl|x64`: 0 failed projects.
- `-vkpsxtest`: `psx self-test: PASS` (`main depth format 129 stencil=1`).
- The GL build ran with `legacyShadowPass=1 legacyLightPass=1` and no
  shader-compile error in `REDRIVER2.log`, i.e. the new GLSL composite compiles
  and runs.
- Debug view 6 (normal): ground/road up-facing, wall facades sideways, 1-2 px
  discontinuities at geometric edges. Debug view 10 (sun coverage): the whole
  visible street, buildings, vehicles and trees are covered, the sky is not.
- `git diff --check` clean.

## Working techniques and environment (updated)

- **Scripted clicks do reach the ImGui panel** once the game window is
  frontmost (`windows-mcp` `App switch` first, then `Click` at
  `image x 1.791667`); the earlier "clicks never arrive" note was a focus
  problem. The tab row is a single merged OCR box, so click by the screenshot's
  measured tab centres (the current layout: Graphics ~1370, Input ~1431,
  Game Debug ~1516 screen x at 3440x1440). A synthetic click on a binding
  button was not observed to start a capture, so that banner stays
  code-verified.
- The display resolution in this environment **flips between 3440x1440 and
  2560x1600 on its own** (Parsec/Meta/SudoMaker virtual display adapters are
  installed; killing a fullscreen game does not restore the desktop mode). Do
  not treat a resolution change as a game bug, and do not try to force a mode
  with `ChangeDisplaySettings` - the target mode may not be enumerable.
- The **GL build has no DLLs in `bin/Release_dev_gl`**; copy
  `SDL2.dll`, `SDL2_2.dll` and `OpenAL32.dll` from `bin/Release_dev` to run it
  (its config/ini files are separate and were stale, so seed
  `developer_modern_mesh.ini` before judging the composite).
- The composite's debug modes remain the fastest diagnosis: 1 depth, 2 frustum,
  3 shadow map, 4 proj/depth, 5 `ndl`, 6 normal, 7 light scale, 8 two-channel
  depth, 9 two-channel distance, 10 sun coverage. Set them in
  `developer_modern_mesh.ini` and relaunch; the panel's slider also sets them
  live. **Reset `shadowdebug=0` afterwards** - the game rewrites the file.
- `-vkpsxtest` writes its PASS/FAIL to `vk_fixture.log` next to the exe, not to
  `REDRIVER2.log`, and ignores its own exit status.
- The self-test, the modern fixtures and the composite all need the game to
  reach the attract mode; expect ~15-25 s after launch before a capture is
  meaningful.

## Next candidate work

- Shadow-centre **texel snapping** (quantise the light-space centre to the
  shadow-map texel grid) to remove any shimmer as the volume follows the car -
  identified, not implemented.
- The R5 shadow receive difference between backends (VK 7722 px vs GL 2264 px,
  IoU 29.2%) and point-light receptivity / legacy casters for the shadow map
  remain open from the earlier workstreams.
- Roadmap planned items 07 (inspector navigation), 08 (OpenDriver2Tools
  interoperability), 09 (model export/re-import), 11 (draw distance/LOD/stream
  budgets), 12 (quality profiles) and 17 (high-FPS timestep, documented only).
