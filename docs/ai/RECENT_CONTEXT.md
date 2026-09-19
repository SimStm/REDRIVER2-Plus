# Recent Engineering Context

> Updated: 2026-09-18
> Status: active
> Source of truth: current repository state, Git history and Git diff.
> This file preserves durable engineering context across long OpenCode sessions.
> It does not replace inspecting the relevant source code.

## Current objective
- Goal: renderer modernization item 14. Phase 1 (native Vulkan backend for the
  modern fixture, validation-clean) is done. Phase 2 (R7b) is done: the *game*
  renders through Vulkan, it is now the **default** backend, and OpenGL remains
  selectable with `-opengl`.
- Acceptance criteria: the game window presents through Vulkan with the same
  image as OpenGL (scene, HUD, minimap), zero Khronos validation errors,
  `-vkpsxtest` and the inspector suites pass, and no OpenGL/Emscripten/Android/
  PSX regression. All met.

## Architecture and invariants
- Vulkan device/swapchain is owned by PsyCross (`PsyX_Vk.cpp`); the game maps
  the `GR_*` contract onto `PsyX_Vk_Game*`. `psx.vert` converts GL clip space to
  Vulkan (`y = -y`, `z = (z + w) * 0.5`); the viewport and scissor must both map
  GL's bottom-left origin to Vulkan's top-left (`height - (y + h)`).
- VRAM is a `R32G32_SFLOAT` image (R = low byte / 255, G = high byte / 255) over
  the CPU `unsigned short vram[1024*512]` mirror; the PSX shader does the CLUT
  and texture-window lookups. `GR_UpdateVRAM` re-uploads the whole mirror.
- Public API / ABI: PsyCross keeps C-compatible declarations for mixed C/C++
  code, and project-specific PsyCross changes are committed to the project fork
  (`knowledge/rules/psycross-fork.md`); the parent records the gitlink.
- Toolchain: Premake 5 generates the projects, generated output is not tracked,
  dialect is C++11 (`knowledge/rules/generated-build-files.md`).

## Completed changes
- 2026-09-18: Vulkan is now the default game renderer. Parity was proven (image,
  resize, readback, stencil, offscreen, VRAM export, 0 validation errors, 30 FPS
  equality), so `redriver2_psxpc.cpp` defaults `useVulkan = 1` on desktop, adds
  `-opengl` as the OpenGL opt-out, and keeps `-vulkan`. The platform guard
  matches the backend's own, so PSX/Android/Emscripten keep OpenGL. Verified:
  default run initialises Vulkan (29.4-30.7 FPS, 0 validation errors),
  `-opengl` produces no `psyx_vk.log`, `-vulkan` still works.
- 2026-09-18: Vulkan game path - live resize recreation fix. The resize and
  out-of-date-acquire paths called `DestroySwapchain()` alone, which destroys
  only the swapchain handle; framebuffers, swapchain views and the depth image
  for the old extent leaked and the recreated framebuffers sat beside stale
  attachments. Both paths now use `RecreateSwapchain()` (idle -> destroy
  framebuffer/view/depth -> destroy swapchain -> recreate; the readback buffer
  is rebuilt in `CreateSwapchain`). The recreate logs
  `swapchain recreated WxH images=N`. Verified externally with
  `user32!MoveWindow` on the live process: four resizes produced four
  recreations at the matching client extents (1008x729, 784x561, 1584x861,
  1264x681) with 0 validation errors and the frame counter still advancing
  (29.4-29.9 FPS at frames 360/480/600). Fork commit `ea7754f`.
- 2026-09-18: Vulkan game path - PSX texture mip layout chain fix and backend
  performance comparison. `PsyX_Vk_GameCreateTexture` used whole-image barriers
  while generating mips, so mip 0 stayed `TRANSFER_DST`, an image-wide barrier
  forced every level to `TRANSFER_SRC`, the next level was blitted in as
  `TRANSFER_DST` against that layout, and generated mips never reached
  `SHADER_READ_ONLY`. Validation: 30 messages (capped) of `oldLayout-01197`,
  `vkCmdBlitImage-srcImageLayout-00221` and `vkCmdDraw-None-09600` per run.
  Barriers are now per mip level and the final transition uses
  `ImageBarrierLevels(0, levels)`; a game run reports 0 validation errors. The
  same run produced the documented comparison: a shared wall-clock sample in
  `PsyX_EndScene` (opt-in `PSYX_PERF_LOG`, `psyx_perf.log`) shows OpenGL and
  Vulkan both at 30.0 FPS / 33.4 ms with identical vertex and draw counts, i.e.
  the game is PSX-timestep-bound, not GPU-bound. Fork commit `6b7e5bb`.
- 2026-09-18: Vulkan game path phase 3 - VRAM export fix and FMV scope.
  `GR_SaveVRAM` (F10) was OpenGL-only (`#if USE_OPENGL`) and wrote an 18-byte
  header-only TGA on Vulkan; the writer reads the backend-agnostic CPU `vram`
  mirror, so the guard was removed and `-vkpsxtest` gained Case 4 asserting
  1,048,594 bytes. The "FMV shader" phase-3 item is a non-item: the desktop FMV
  (`fmvplay.c` `FMV_main`) and `nplay` rely on `n.EXE`/`nplay.h` absent from
  this tree, and the lens flare's VRAM sample is already fed by
  `GR_VkMirrorFrameToVRAM`. Fork commit `4b1191d`.
- 2026-09-18: Vulkan game path phase 3 - PSX primitive mask bit.
  `GR_SetStencilMode` is implemented instead of stored-and-ignored: the main
  pass depth attachment prefers `D24_UNORM_S8_UINT` (`PickDepthStencilFormat`),
  cleared every frame; each blend mode gains a mask-set pipeline variant
  (`pipelinesStencilWrite`); `VkPsxDraw.stencilMode` is captured and
  `RecordPsxDraws` selects the variant. Matches GL semantics (mask-bit draw
  writes stencil bit 4, other draws pass only where it is clear), which the
  `DrawPrim` minimap/map overlay depends on. `D32_SFLOAT` remains the fallback
  where no combined format exists. Fork commit `1ad5b81`.
- 2026-09-18: Vulkan game path phase 3 - offscreen render-to-VRAM.
  `GR_SetOffscreenState` is no longer a no-op: `PsyX_Vk_GameSetOffscreen` tags
  the queued draws and `PsyX_Vk_GameResolveOffscreen` (from `GR_EndScene`)
  renders each group into a colour-only target, packs 5551 and writes it into
  the VRAM mirror before the frame, closing the Tanner shadow (`dfe=0`). Groups
  are keyed by frame index because the game builds the display list one frame
  ahead. The uniform-grey bug was `RecordPsxDraws` iterating a draw range while
  filtering by frame, so only one draw candidate was considered; it now scans
  all queued draws and filters by `frame` + `offscreen`. Fork commit `98c176c`.
- 2026-09-18: Vulkan game path phase 2. `-vulkan` maps the `GR_*` contract to
  the native backend; the game window renders through Vulkan. Three fidelity
  fixes: the PSX vertex capacity was raised to 65536 (the previous 256 KiB held
  ~5957 vertices against ~11532/frame, truncating the lower half), the game
  texture-slot limit went 64 -> 512, and `RecordPsxDraws` now maps the scissor Y
  to Vulkan's top-left origin, which restored the minimap.
- 2026-09-18: `GR_StoreFrameBuffer` is implemented for Vulkan. The display rect
  is queued and `GR_VkMirrorFrameToVRAM` converts the top-down presented frame
  (via `PsyX_Vk_TakeStoredFrameBuffer`) into 5551 words at `(disp.x, disp.y)`,
  matching the GL CPU-mirror result (top-down, `flip_y = 0`). This closes the
  sun lens flare's `DR_MOVE`/`StoreImage` pipeline.
- 2026-09-18: Submodule now tracks the project fork
  (`git@github.com:SimStm/PsyCross.git`); `patches/psycross/` and
  `scripts/apply_psycross_patches.ps1` were removed. Fork commits `49f9578`,
  `396dd20`, `9281fb7`; parent gitlink at `9281fb7`.

## Current state
- Implemented: Vulkan game rendering (phase 2 complete, now the default backend),
  framebuffer-to-VRAM mirror, offscreen render-to-VRAM target, PSX primitive mask
  bit (stencil), minimap scissor fix, fork migration, SDL3 deferral recorded.
- Pending for phase 3: none blocking. The Tanner shadow was confirmed working in
  an on-foot test by the user (renders correctly, a little smooth at the edges);
  the earlier "no capture" note came from the debug start spawning the player in
  the car, not from a defect. The both-backend performance comparison is done.
  The VRAM export is fixed and the FMV item was dropped as non-existent on the
  desktop path.
- Risks / open questions: the Vulkan frame mirror samples the whole window
  (including the dev overlay) and scales it onto the PSX display rect; the GL
  legacy path's `GR_CopyRGBAFramebufferToVRAM` R/B extraction is suspect, but the
  only consumer (lens flare) is colour-insensitive. `GR_UseVulkan` guards must
  keep Emscripten/Android/PSX untouched.

## Relevant files
- `src_rebuild/PsyCross/src/render/PsyX_Vk.cpp`: Vulkan backend; `RecordPsxDraws`
  scissor fix, `PsyX_Vk_TakeStoredFrameBuffer`, `PsyX_Vk_GameStoreFrameBuffer`.
- `src_rebuild/PsyCross/src/render/PsyX_render.cpp`: `GR_*` dispatch;
  `GR_VkMirrorFrameToVRAM`, `GR_BeginScene`/`GR_StoreFrameBuffer` guards.
- `src_rebuild/PsyCross/include/PsyX/PsyX_vk.h`,
  `include/PsyX/PsyX_render.h`: the new declarations.
- `src_rebuild/Game/C/overmap.c`, `Game/C/sky.c`: minimap draw and lens flare.
- `knowledge/roadmap/planned/vulkan-game-renderer.md`,
  `knowledge/discussions/renderer-modernization/index.md`,
  `knowledge/discussions/sdl-version-migration/index.md`: records.
- `CHANGELOG.md`: `## [Unreleased]` entries.

## Validation
- Executed: VS build, `Release_dev|x64`. Result: 0 failed projects.
- Executed: `REDRIVER2_dev.exe` with no arguments (new default). Result: Vulkan
  initialised (`psyx_vk.log` `instance/surface/physical device`), 29.4-30.7 FPS,
  0 validation errors.
- Executed: `REDRIVER2_dev.exe -opengl`. Result: no `psyx_vk.log`, game runs -
  OpenGL path selected.
- Executed: `REDRIVER2_dev.exe -vulkan`. Result: Vulkan initialised, still
  backward compatible.
- Executed: `REDRIVER2_dev.exe -vulkan` for 20 s, stdout+stderr captured. Result:
  0 `Validation Error` lines (was 30 before the mip-barrier fix).
- Executed: live Vulkan window resized via `user32!MoveWindow` through
  1024x768, 800x600, 1600x900 and 1280x720 while capturing output. Result: 4
  `swapchain recreated` lines at client extents 1008x729, 784x561, 1584x861,
  1264x681 (3 images each), 0 validation errors, and frames 360/480/600
  presented at 29.4-29.9 FPS with draws 1100 -> 1437 -> 1564.
- Executed: `REDRIVER2_dev.exe -vkpsxtest`. Result: PASS - 16-bit worst=0,
  4-bit CLUT worst=0, offscreen ok, `vram export 1048594/1048594 bytes: ok`.
- Executed: `PSYX_PERF_LOG=1 REDRIVER2_dev.exe -vulkan` and without `-vulkan`,
  20 s each, 30 sample windows. Result: both backends 30.0 FPS / 33.37-33.43 ms
  with the same vertex (11298-13476) and draw-split (1295-1595) progression, so
  the game is PSX-timestep-bound rather than GPU-bound.
- Executed: `REDRIVER2_dev.exe -vkfixture -vkcapture 40 -vkshot vk_gallery.bmp`.
  Result: 12 meshes, 12 instances, shadow 2048, ImGui on; 1280x720 capture with
  the 8 textured GLBs and 4 analytic spheres lit correctly.
- Executed: `pwsh -NoProfile -File scripts/run_inspector_tests.ps1`. Result:
  `AssetCatalogTests: 145 checks, 0 failures`;
  `InspectorExportTests: 104 checks passed`.
- Executed: `git diff --check`. Result: exit code 0.
- Still required: macOS/MoltenVK build was never produced (objectives are
  Windows/Linux; MoltenVK code paths exist but are unbuilt). The `D32_SFLOAT`
  stencil-less fallback in `PickDepthStencilFormat` is compiled but never
  exercised, because the RTX 3060 Ti always exposes a combined depth-stencil
  format; it can only be confirmed on a driver that refuses one. Raw uncapped
  GPU throughput was not measured: both backends present at the game's fixed
  30 Hz PSX timestep (30.0 FPS / 33.4 ms), so the equality shows neither is
  struggling, not their maximum frame rate. The Tanner shadow is confirmed
  working in an on-foot test (see Current state).

## Next recommended action
1. Commit the parent working tree (gitlink, docs, fork migration) when the user
   asks; do not commit unprompted. The three open user decisions are: commit the
   parent, confirm the fork contract replaces `patches/psycross/`, and pick the
   next scope.
2. Game-path parity now covers image, mipmapped textures, offscreen, stencil,
   VRAM export, present, resize and readback, all with 0 validation errors and
   measured 30 FPS parity with OpenGL. Remaining verification, none of which is
   blocking: on-foot Tanner shadow capture (now confirmed working by the user),
   macOS/MoltenVK build, the stencil-less `D32_SFLOAT` fallback (unexercised on
   NVIDIA), and raw uncapped GPU throughput (the game is PSX-timestep-bound, so
   both backends present at 30 FPS by design).

## Compact changelog
- 2026-09-18: Vulkan game path - resize recreation fix (framebuffers/views/depth
  now rebuilt, verified over four live resizes at 0 validation errors).
- 2026-09-18: Vulkan game path - PSX mip-chain barrier fix, backend-agnostic perf
  log, measured OpenGL/Vulkan parity.
- 2026-09-18: Vulkan game path phase 2; minimap scissor fix; framebuffer-to-VRAM
  mirror; PsyCross fork migration; SDL3 deferral; validation re-run.
- 2026-09-18: OpenCode context policy configured (`opencode.jsonc`).
