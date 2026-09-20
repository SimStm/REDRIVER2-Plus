---
type: Status
title: Recent engineering context
description: Current renderer correction and validation handoff.
tags: [okf, status, agents]
---

# Recent engineering context

> Updated: 2026-09-20
> Confirm this handoff against source, Git state and actual validation output.

## Objective and status

Address the user's repeat Vulkan UI report: world shadows over the full map,
missing police indicator/cone, faint police flashing and compass/HUD colours,
loading progress and CloseShutters. Corrections are implemented in fork commits `d9d8628` and `647ea0b`, pushed to origin/master,
and the parent gitlink is staged. Controlled visual comparisons are complete
for the reported elements, with small remaining pixel differences. The previous document's
complete claim and explanation that GL writes depth with depth testing off
were incorrect and are superseded here.

## Implemented corrections

- `PsyX_render.cpp` maps the white sentinel to `PSYX_VK_TEX_WHITE`, except pure
  RGBA draws. `psx.frag` decodes the same white 0xffff word as GL (RGB 248 and
  alpha 127); it no longer samples unrelated VRAM for untextured primitives.
- `CreatePsxResources` preserves the stencil capability selected during render
  pass creation instead of erasing it with memset. Pipeline masks/operations
  reproduce GL reference 1, write mask 0xff and separate comparison masks.
  Depth-disabled normal and DrawPrim variants both retain stencil.
- Main and resumed passes store stencil and have matching dependencies covering
  colour and early/late depth/stencil attachment accesses. This fixes the
  render-pass compatibility VUID observed with Khronos validation.
- Game mode prefers UNORM presentation for PSX display-space blending; the
  standalone modern fixture retains its sRGB preference. Constant blend alpha
  is 0.5, matching the actual GL call. Depth writes require depth testing.
- `DeveloperVkFixture.cpp` runs PSX self-test with gameMode enabled and a larger
  report. Tests cover empty-VRAM white primitives in all three PSX texture
  formats, five blend modes, masking through two pass restarts, partial frames,
  existing CLUT/offscreen checks and VRAM export.

## Validation actually executed

- `scripts/compile_vk_shaders.ps1`: succeeded, generated SPIR-V and embedded header.
- Visual Studio MCP `Release_dev|x64` builds: latest succeeded, zero failures.
- `REDRIVER2_dev.exe -vkpsxtest`: log reports PASS. White RGB 248; all five
  blend checks worst error <= 1; stencil protected/outside pixels correct;
  existing texture/offscreen checks pass; VRAM export 1048594/1048594 bytes.
- Khronos layer is installed at `G:\VulkanSDK\1.4.357.0`, registered and loaded.
  The renderer already enables it when available. Before correction the game
  emitted `VUID-vkCmdDraw-renderPass-02684`; the inspected post-fix game output
  had layer-enabled confirmation and no validation error/VUID.
- Live Vulkan captures showed the full map without shadow bleed, white compass,
  green Damage portion, coloured Felony bar, police cones and brighter flash.
  Loading art with a filled red progress bar was observed.
- Controlled HUD captures at the same spawn, CameraCnt=4 and injected cop state
  show matching Damage/Felony, compass and police indicator/flash structure.
  Mean absolute RGB difference was 2.4042/255 in the HUD rectangle and
  2.8266/255 in the minimap rectangle (max 51 and 83 respectively). These are
  visual parity checks, not pixel-identical results; world traffic differs.
- Natural CloseShutters captures at h=96 show both black bands and the same
  loading image on GL and Vulkan. Full-frame mean RGB difference 0.8747/255,
  maximum 4. A prior artificial jump from h=0 to h=96 produced an asymmetric
  Vulkan frame; do not use injected animation jumps as natural-run evidence.
- The shutter captures used temporary source probes (GL before EndScene, Vulkan
  after EndScene). Probes were removed; loadview.c has no content diff. Final
  Release_dev|x64 build after removal succeeded, zero failed projects.
- `git diff --check` and submodule diff check passed before commit. Upstream
  releases page checked: baseline remains 8.0 at b2d8857.
- The partial-presentation test passed for the actual acquired-image sequence;
  it does not guarantee coverage of every swapchain image or establish general
  previous-frame persistence. The renderer still loads individual images.

## Working techniques and environment

- Use Visual Studio MCP to select `Release_dev` (Vulkan) or `Release_dev_gl`,
  pause, assign globals and resume. Vulkan function evaluation
  `PsyX_TakeScreenshot()` works while paused and writes
  `bin/Release_dev/SCREENSHOT.BMP`. The GL function evaluation timed out; use
  the normal-running capture tick for GL and verify file timestamps.
- `DeveloperDebugStart` capture globals also work while rendering gameplay:
  `g_captureDelayMs=1`, `g_captureTaken=0`. Tick occurs before presentation.
- Use System.Drawing to convert BMP to PNG for `view_image`; system Python has
  no Pillow. Captures are local ignored artifacts under
  `src_rebuild/build/ui-parity-20260919/`.
- Native computer control was unavailable (pipe error); the user-authorized
  computer-control MCP worked for running windows. Window tools can hang while
  the debuggee is paused, so prefer renderer screenshot function evaluation.
- For frozen HUD capture set pauseflag=1, CameraCnt to a fixed phase, gShowMap=0,
  player_position_known=1, CopsCanSeePlayer=1, car_data[0].felonyRating=5000,
  PlayerDamageBar.position=2000 and FelonyBar.position=4096. Paused physics does
  not update bar positions automatically. Police sight cones require a cop
  control flag or a live pursuer car; white dot blinks with CameraCnt & 7.
- `-vkpsxtest` command-line dispatch currently ignores the test return status;
  inspect its PASS/FAIL log, not just the process exit code.
- `opencode.jsonc` is an unrelated pre-existing user edit; leave it untouched.

## Enhanced-path validation follow-up

A final launch with modern meshes active exposed two additional errors:
VUID-VkImageMemoryBarrier-image-03320 (depth-only barrier on combined D24/S8)
and VUID-vkCmdDraw-None-09600 (copy destination never transitioned to transfer).
RecordGameModernSceneDepthCopy now transitions both aspects, copies only depth,
and discards/prepares the fully overwritten destination before each copy.
Commit `647ea0b` contains this follow-up. The build passed; the post-fix run
confirmed the validation layer enabled, imported all eight modern fixtures with
modern meshes and shadows enabled, and emitted no validation error/VUID in the
inspected debug output. Modern enabled state was restored to 0 (its prior value),
shadows remained 1, and the debuggee was stopped. Earlier clean-run statements
apply only to those inspected runs.

## Artifacts and remaining limits

- Captures: `gl-hud4.png`, `vk-hud4.png`, `gl-shutters96.png`,
  `vk-shutters-natural96.png`, `vk-loading.png`, `vk-map.png` in the ignored
  capture directory above. The file named gl-loading.bmp is stale from a timed
  out evaluation and is NOT GL evidence.
- No gameplay changes remain. `opencode.jsonc` was not staged or modified.
- Cross-platform builds, the D32-only stencil fallback and a general
  previous-presented-image history mechanism remain outside this validated run.
  Investigate explicit frame history if a natural partial-update sequence
  reproduces corruption; do not claim the current LOAD operation guarantees it.
