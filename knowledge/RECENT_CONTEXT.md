---
type: Status
title: Recent engineering context
description: Vulkan minimize access violation fixed in the swapchain recreation path; the earlier world/overlay composition remains validated.
tags: [okf, status, agents]
---

# Recent engineering context

> Updated: 2026-09-21
> Confirm this handoff against source, Git state and actual validation output.

## Objective and status

Latest session (2026-09-21c): items 3 and 4. The reported shadow-receive
difference between backends was **measured away, not fixed**: it came from
drifted developer state between the two installs and from moving-traffic
capture noise; under aligned settings in the traffic-free playground the two
backends' shadowed-pixel sets reach IoU 0.985. The remaining item-3 scope
(legacy casters, point lights) is planned, not implemented. Item 4 added
`submit_ms` to the perf log and established that the 30 FPS ceiling is the
game's emulated PSX vblank, with ~6.4 ms of Vulkan CPU per frame for the
submission path. See
[parity](changes/2026-09-21/shadow-receive-parity/index.md) and
[profiling](changes/2026-09-21/frame-submission-profiling/index.md) records.

Previous session (2026-09-21b): item 1 and 2. The shadow volume centre is
snapped to the light-space texel grid on both backends, the OpenGL minimize
path was re-tested, the display-mode countdown decision is recorded (it keeps
counting while minimized; one frame can spend at most `0.25 s`), and a
pre-existing Vulkan frame-fence deadlock on the minimize/restore path was fixed.
See [shadow snap](changes/2026-09-21/shadow-volume-texel-snap/index.md),
[fence deadlock](changes/2026-09-21/vulkan-frame-fence-deadlock/index.md) and
[minimize crash](changes/2026-09-21/vulkan-minimize-swapchain-crash/index.md).

Earlier: the Vulkan access violation on minimize was fixed, and before that the
forced-2D-depth workaround was replaced by explicit world/overlay composition
([record](changes/2026-09-21/modern-overlay-composition/index.md)).

Git state: the perf instrumentation is uncommitted in `src_rebuild/PsyCross`
(`src/PsyX_main.cpp`) and the parent reports the submodule dirty with the new
knowledge records. The overlay composition is fork commit `13705cf`; the texel
snap and fence fix are fork commit `e803dd6`, both pushed to `origin/master`.
VS is back on `Release_dev|x64` and no debug session is active.

## Item 3 and 4: implementation and evidence

- **Shadow receive parity.** No renderer change. The two composites are
  line-for-line ports; the earlier `7722 vs 2264 (IoU 29.2 %)` compared
  different configurations: Vulkan had `lightdir=-0.2240,0.8480,-0.4803`,
  `shadowextent=6991`, spawn car 3 / X=9046 / Z=-217980, while OpenGL had
  `lightdir=-0.2500,0.7200,-0.3800`, `shadowextent=2500`, spawn car 2 /
  X=13659 / Z=-216011. The OpenGL install's `developer_modern_mesh.ini` and
  `developer_debug_start.ini` were aligned with the Vulkan ones (backups of the
  previous OpenGL files are in
  `C:\Users\simst\AppData\Local\Temp\opencode\{gl_modern_mesh,gl_debug_start}.ini.bak`).
  Controlled result, `-playground` (traffic-free), 1280x720, threshold 6 per
  channel: VK run-vs-run noise 2499 px; VK shadowed 4816, GL shadowed 4823,
  intersection 4782, union 4857, **IoU 0.985**; VK-vs-GL with shadows off
  `meanAbs 3.082/255`, with shadows on `3.073/255`, i.e. the shadow path adds
  no backend difference. In the street scene the metric is noise-dominated: two
  Vulkan captures of the same build differ by 7973 px just from traffic timing.
- **Item 4 profiling.** `submit_ms` added to the perf sample. Vulkan FIFO
  30.0 FPS / `submit_ms` 6.2-6.7 ms; Vulkan `PSYX_VK_PRESENT_MODE=immediate`
  identical (30.0 FPS) - so the ceiling is the game's `VSync(0)` ->
  `PsyX_WaitForTimestep` vblank wait, not vsync or GPU; OpenGL with `vsync=0`
  30.0 FPS / `submit_ms` 0.17-0.20 ms. Fixture window (`-vkfixture -vknogui`,
  immediate) ~660-900 FPS / 1.1-1.5 ms per frame as the raw pipeline reference.
  The game frame's own GPU time is still not isolated (would need Vulkan
  timestamp queries).
- **Method notes.** Capture procedure: `run_and_capture.ps1` in the temp dir
  (launch, foreground, `capture-window.ps1` client-area grab, kill) with the
  comparison helpers `diff_captures.ps1` and `shadow_mask_iou.ps1`; always run a
  same-backend control capture, because animated content puts the noise floor
  around 2500-8000 pixels at 1280x720.
- **Not implemented (planned records).**
  [`roadmap/planned/legacy-shadow-casters`](roadmap/planned/legacy-shadow-casters.md)
  (legacy geometry casting into the modern map; the Vulkan `submit_ms` is the
  cost baseline) and
  [`roadmap/planned/point-light-sources`](roadmap/planned/point-light-sources.md)
  (the game publishes only one directional sun, so there is nothing for a
  point-light receptivity term to consume yet).

## Item 1 and 2: implementation and evidence

- **Shadow texel snap.** `PsyX_ModernShadowSnapCentre()` in the shared prologue
  of `PsyX_ModernMesh.cpp` (declared in `PsyX_ModernMesh.h`) snaps the centre in
  the light plane only, in double, and returns the basis up vector; both shadow
  matrix builders call it. Evidence: while driving, a temporary log of the
  fractional texel position swept `rawFrac=(0.38,0.05) -> (0.63,0.75) ->
  (0.06,0.25) -> (0.78,0.37)` with `snapFrac=(0.0000,0.0000)` every sample and
  `|deltaTexels| <= 0.42`; a live capture was unchanged; `-vkpsxtest` passed.
- **Vulkan frame-fence deadlock (new finding, pre-existing bug).** The fence
  was reset at the top of the frame, so the out-of-date acquire path
  (`RecreateSwapchain(); return 1;`) left it unsignaled and the next
  `vkWaitForFences(..., UINT64_MAX)` blocked forever. Reproduced debugger-free
  with a temporary acquire log: `acquire: leave f=1926 result=-1000001004`
  (`VK_ERROR_OUT_OF_DATE_KHR`), `swapchain recreated`, then no further frame and
  `responding=False`. Fixed by resetting the fence immediately before the
  submit (and signalling it with an empty submit if that submit fails).
  Re-verified: 46 s and 60+ s iconized intervals, restore re-created the
  swapchain, frames continued past `f=10246`, `responding=True`, capture normal.
- **OpenGL minimize.** `Release_dev_gl|x64` built; the window was iconized
  (verified), the process stayed alive, and the restored frame rendered the
  world, fixtures, HUD and minimap. No crash before or after.
- **Display-mode countdown.** Decision: keep counting while minimized (safety
  timer; the guarantee "an unconfirmed mode always reverts" must hold whatever
  the user does) and clamp one frame's contribution to `0.25 s`. Evidence: the
  first frame after a long debugger stop logs `dt=0.2500`; while the window was
  iconized the timer held its remaining value (Vulkan stops drawing there) and
  resumed on restore.

## Minimize crash: implementation and evidence

- Cause: a minimized window reports a `0x0` surface, so out-of-date
  acquire/present set `resizePending`; `RecreateSwapchain()` destroyed the
  swapchain and `CreateSwapchain()` then failed on the zero extent, leaving
  `g_vk.swapchain == VK_NULL_HANDLE`; the next acquire crashed inside
  `nvoglv64.dll` (`0xC0000005`, break at `PsyX_Vk.cpp:5399`, Debug output:
  `vkAcquireNextImageKHR-swapchain-parameter`).
- Fix: `ResolveSwapchainExtent()` (now used by `CreateSwapchain`) validates the
  extent; `RecreateSwapchain()` checks it before destroying anything;
  `PsyX_Vk_RenderFrame()` skips frames while `SDL_WINDOW_MINIMIZED` and
  re-creates a null swapchain before acquiring.
- Verified: `Release_dev|x64` built; launched under the debugger, minimized
  and restored repeatedly - the debugger stayed in Run with no exception, the
  session log held no validation error, `psyx_vk.log` recorded
  `swapchain recreated 1280x720 images=3`, and the restored window rendered the
  normal world/HUD. `-vkpsxtest` passed afterwards. OpenGL was not re-tested
  (the defect is Vulkan-specific).

## Overlay composition: implementation and evidence

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
- Minimizing/restoring the game for a test: `Add-Type` a `user32.dll`
  `ShowWindow` shim and call it on `(Get-Process REDRIVER2_dev).MainWindowHandle`
  with 6 (`SW_MINIMIZE`) and 9 (`SW_RESTORE`); `IsIconic` confirms the state.
  Keep each step in its own shell call - a `foreach` loop with `Start-Sleep` can
  wedge the shell tool past its timeout even though the game is unaffected.

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
- Minimize/restore for a test: a `user32.dll` `ShowWindow` shim on
  `(Get-Process REDRIVER2_dev).MainWindowHandle`, 6 (`SW_MINIMIZE`) and
  9 (`SW_RESTORE`), with `IsIconic` to confirm; one step per shell call (a
  `foreach` + `Start-Sleep` loop wedged the shell tool). `F11` never opened the
  panel through the MCP - only scancode keys (arrows) reach the game - so arm
  panel state with the debugger or an instrumented path instead of a key.
- On this machine SDL does **not** report `SDL_WINDOW_MINIMIZED` for an iconized
  window (frames kept rendering with `minimized=0` while `IsIconic` was true),
  so do not rely on that flag to detect a minimized window. The Vulkan path
  still stops producing frames once present/acquire reports the surface
  out-of-date, which is what freezes everything until the window is restored.
- To tell whether frames advance, read the `perf:` lines or a frame counter in
  `psyx_vk.log` rather than CPU: the interrupt thread spins in
  `Util_GetHPCTime` (100 % of one core) even when healthy, and
  `Process.Responding` is `False` while the game is inside a debugger pause.

## Next candidate work

- The R5 shadow receive difference between backends (VK 7722 px vs GL 2264 px,
  IoU 29.2%) and point-light receptivity/legacy casters for the shadow map
  remain open.
- Roadmap planned items 07 (inspector navigation), 08 (OpenDriver2Tools
  interoperability), 09 (model export/re-import), 11 (draw distance/LOD/stream
  budgets), 12 (quality profiles) and 17 (high-FPS timestep, documented only).
