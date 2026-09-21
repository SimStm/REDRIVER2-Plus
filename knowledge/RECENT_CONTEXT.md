---
type: Status
title: Recent engineering context
description: Explicit world/overlay composition replaces the regressed screen-space depth workaround; Vulkan and OpenGL validation.
tags: [okf, status, agents]
---

# Recent engineering context

> Updated: 2026-09-21
> Confirm this handoff against source, Git state and actual validation output.

## Objective and status

Replaced the previous forced-2D-depth workaround after the user's report of
translucent-menu holes, world/effect ordering errors and angle-dependent scene
loss. The final implementation composes the world and modern meshes before
the final gameplay overlays. See
[change record](changes/2026-09-21/modern-overlay-composition/index.md).

PsyCross commit `13705cf` was pushed to the project fork's `origin/master`;
the parent gitlink is staged. Game integration and knowledge/CHANGELOG changes
remain in the parent working tree. VS is back on `Release_dev|x64`, with the
test debug session stopped and temporary breakpoints removed.

## Implementation and evidence

- Game: `DrawGame` updates modern camera/instances before OT submission and
  supplies bucket 10 as the final-overlay boundary (lens flare 10, fades 8,
  HUD/map/menu 0..1). The saved user spawn was not modified.
- PsyCross: the generic boundary records an exact vertex, including inside a
  shared state batch. No first-vertex `scr_h` classification or forced depth
  writes remain. OpenGL restores texture units 0..4/active unit and composes
  once; Vulkan draws contiguous ranges and preserves VRAM transfer ordering.
- Both VS builds passed: `Release_dev|x64`, `Release_dev_gl|x64`.
- Expanded `-vkpsxtest` passed, including all five blend modes across a
  composition boundary with a VRAM transfer. Maximum pixel error 1.
- Vulkan: saved mission 50/car 3/X=9046/Z=-217980/direction=2785 showed correct
  sun occlusion and no rectangular flare lighting holes after the change.
  Paused menu blends correctly. Driving plus four debugger camera orientations
  (0, 1024, 2048, 3072) retained the world/car. The exact original blank-view
  angle was not separately captured before the fix.
- OpenGL: moved the imported bollard to the crate position in debugger memory
  and moved the crate aside; the yellow bollard stayed visible and dimmed under
  the menu's Sfx Volume text. Stopped the session afterwards; no persistent
  fixture or snapshot changes.
- Vulkan game debug output confirmed validation enabled and no VUID.
- Parent/submodule `git diff --check` passed. No shader regeneration required.
- Split-screen retains its prior end-of-frame fallback; it was not tested.
  World sprites retain original ordering; no missing PGXP depth is fabricated.

## Lessons for acceptance

A mesh disappearing behind a translucent panel is a failure, not proof of
correct UI occlusion. Identify an imported mesh unambiguously and check that
its colour survives beneath the blend. Do not infer a whole batch's projection
path from its first vertex: `AddSplit` does not include that path in its key.

The old [depth workaround record](changes/2026-09-21/ui-depth-occlusion/index.md)
is marked superseded. The durable composite rule now explicitly rejects it.

## Environment: scripted keys and captures (read before reproducing input)

- Prefer a temporary debugger breakpoint in `DrawGame` before `DrawPauseMenus`
  and evaluate `EnablePause(PAUSEMODE_PAUSE)`, remove the breakpoint, continue
  and activate the game window. This worked on both backends without rebinding
  config.ini. Give the next frame time to enter the pause menu.
- The bundled Computer Use native pipe was unavailable; configured
  computer-control-mcp still captured and drove the game. WGC can return a
  black/stale image while unfocused; activate the exact `REDRIVER2` window and
  recapture before diagnosing a rendering failure.

- **The desktop-control MCP only delivers keys that carry a scancode.** The
  arrow keys work (`SDL_GetKeyboardState` reports them); `w`, `Return` and
  `Escape` report `0` and never reach the game, which is why ESC could not open
  the pause menu. Instrument `UpdatePadData` with a throttled
  `printWarning("...")` (`driver2.h` maps it to `PsyX_Log_Warning`) when a key is
  in doubt; `eprintwarn` is **not** visible in the game C code (two different
  `platform.h` files).
- To pause for a capture, temporarily rebind `[kbcontrols_game] start=Up` in
  `bin/Release_dev/config.ini` and move `up` to a key that is never injected
  (e.g. `PageDown`) - the check is `paddp == MPAD_START` **exactly**, so a
  second simultaneous binding blocks it. An empty value falls back to the
  compiled default (`SDL_SCANCODE_UP`), so use a real name or `NONE`. Restore
  `config.ini` afterwards.
- Pause confirms with CROSS, which is `Return` in the menu mapping and therefore
  not injectable; to leave the pause, kill and relaunch instead.
- The mission always starts at the same deterministic spawn, so a fresh launch
  is a valid "unpaused" reference frame for pixel comparisons.
- Window capture without the MCP: `capture-window.ps1` in
  `C:\Users\simst\AppData\Local\Temp\opencode` (finds `REDRIVER2_dev`'s main
  window, `CopyFromScreen` on the client rect). The MCP's
  `save_to_downloads` did not write a file.
- The display resolution **flips between 3440x1440 and 2560x1600 on its own**
  (Parsec/Meta/SudoMaker virtual adapters); not a game bug, and
  `ChangeDisplaySettings` may not find the target mode.
- The **GL build has no DLLs in `bin/Release_dev_gl`**; copy `SDL2.dll`,
  `SDL2_2.dll`, `OpenAL32.dll` from `bin/Release_dev` to run it (separate ini
  files there).
- Composite debug modes are the fastest diagnosis: 1 depth, 2 frustum, 3 shadow
  map, 4 proj/depth, 5 `ndl`, 6 normal, 7 light scale, 8 two-channel depth,
  9 two-channel distance, 10 sun coverage. Set them in
  `developer_modern_mesh.ini` and relaunch; **reset `shadowdebug=0` afterwards**
  (the game rewrites the file).
- `-vkpsxtest` writes PASS/FAIL to `vk_fixture.log`, not `REDRIVER2.log`, and
  ignores its exit status. Expect ~15-25 s after launch before a capture is
  meaningful.
- Scripted clicks reach the ImGui panel once the game window is frontmost
  (`windows-mcp App switch` then `Click` at `image x 1.791667`).

## Next candidate work

- Shadow-centre **texel snapping** to remove shimmer as the volume follows the
  car - identified, not implemented.
- The R5 shadow receive difference between backends (VK 7722 px vs GL 2264 px,
  IoU 29.2%) and point-light receptivity/legacy casters for the shadow map
  remain open.
- Roadmap planned items 07 (inspector navigation), 08 (OpenDriver2Tools
  interoperability), 09 (model export/re-import), 11 (draw distance/LOD/stream
  budgets), 12 (quality profiles) and 17 (high-FPS timestep, documented only).
