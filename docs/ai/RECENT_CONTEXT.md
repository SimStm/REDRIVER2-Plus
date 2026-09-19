# Recent Engineering Context

> Updated: 2026-09-19
> Status: active
> Source of truth: current repository state, Git history and Git diff.
> This file preserves durable engineering context across long OpenCode sessions.
> It does not replace inspecting the relevant source code.

## Current objective
- Goal: fix the visual UI/image problems reported after the Vulkan default flip,
  in this order of attention: the loading-screen progress bar missing, the map
  screen and its navigation icons missing, minimap elements wrong (police
  direction cone, police-colour blinking), the top-left Damage/Felony HUD
  changing colour and tone with the player's position, and the clapperboard
  loading-to-gameplay transition.
- Root cause found for the image-streaming cases: the Vulkan backend records the
  whole frame's PSX draws at present time, while OpenGL executes each `DrawSync`
  flush immediately, so (a) every vertex upload overwrote the previous flush's
  vertices and (b) every draw sampled the frame's final VRAM contents instead of
  the contents at its own flush. Fixed on the fork (see below); the overhead map
  now draws its tiles and labels, but it still does not match the OpenGL
  reference exactly.
- Open: the map's tile sampling is still wrong outside the left column (scattered
  line-work, semi-transparent overlay over the world). The OpenGL reference is
  captured; next step is to compare one map frame's per-draw tile slots between
  backends, starting from `LoadMapTile` (8x32 rect at
  `MapRect.x + (MapSegmentPos[slot].x >> 2)`, sampled through a 32x32 window
  whose `u` is in units of four texels).
- Earlier objective (complete): renderer modernization item 14 phase 2 (R7b),
  the Vulkan game renderer, including post-flip defects 1-5. See
  `knowledge/roadmap/done/vulkan-game-renderer.md` and
  `knowledge/product/vulkan-game-renderer.md`.
- Acceptance criteria for the renderer work: the game window presents through
  Vulkan with the same image as OpenGL (scene, HUD, minimap), zero Khronos
  validation errors, `-vkpsxtest` and the inspector suites pass, and no
  OpenGL/Emscripten/Android/PSX regression. The validation layer is not installed
  on the current machine, so recent runs relied on behaviour and the self-test.


## Architecture and invariants
- Vulkan device/swapchain is owned by PsyCross (`PsyX_Vk.cpp`); the game maps
  the `GR_*` contract onto `PsyX_Vk_Game*`. `psx.vert` converts GL clip space to
  Vulkan (`y = -y`, `z = (z + w) * 0.5`); the viewport and scissor must both map
  GL's bottom-left origin to Vulkan's top-left (`height - (y + h)`).
- VRAM is a `R32G32_SFLOAT` image (R = low byte / 255, G = high byte / 255) over
  the CPU `unsigned short vram[1024*512]` mirror; the PSX shader does the CLUT
  and texture-window lookups. `GR_UpdateVRAM` re-uploads the whole mirror for the
  writers that still use the dirty flag (`GR_ClearVRAM`, the RGBA framebuffer
  copy, the offscreen resolve, the presented-frame mirror).
- The deferred draw list must preserve flush order, because the game rewrites
  state between `DrawSync` flushes and OpenGL executes each flush immediately:
  vertex uploads append (`PsyX_Vk_GameUpdateVertexBuffer` keeps four flushes and
  each draw is offset by its upload's base) and `GR_CopyVRAM` queues its
  rectangle with the pixels (`PsyX_Vk_GameCopyVRAM`), replayed in generation
  order inside the draw list. A transfer cannot be recorded inside a render pass,
  so the pass is closed and reopened with `modernRenderPass` (which loads the
  same colour/depth attachments) around each replay.
- Public API / ABI: PsyCross keeps C-compatible declarations for mixed C/C++
  code, and project-specific PsyCross changes are committed to the project fork
  (`knowledge/rules/psycross-fork.md`); the parent records the gitlink.
- Modern mesh on Vulkan: `PsyX_Vk_GameModernMesh*` mirrors the OpenGL
  `PsyX_ModernMesh` contract. The `ModernUBO` (std140) is declared identically in
  `psx_modern.vert`, `psx_modern.frag` and `psx_composite.frag`; the order is
  proj, projInverse, shadowMatrix, cameraViewInverse, cameraRotation, xyScale,
  shadowParams, lightInfo, ambientExposure, cameraPos, viewport, lights[8]. A
  missing field silently shifts every later member onto the wrong offset.
- The Vulkan main pass colour attachment preserves the previous frame
  (`LOAD_OP_LOAD`, `PRESENT_SRC` initial layout) and clears with
  `vkCmdClearAttachments` only when `GR_Clear` requested it; this matches
  OpenGL's conditionally-cleared framebuffer that the loading screen relies on.
- Toolchain: Premake 5 generates the projects, generated output is not tracked,
  dialect is C++11 (`knowledge/rules/generated-build-files.md`).

## Completed changes
- 2026-09-19: Vulkan preserves flush order for the image-streaming UI (map-screen
  work, in progress). Two defects in the deferred draw list: (1)
  `PsyX_Vk_GameUpdateVertexBuffer` always wrote from offset 0, so a frame with
  more than one `DrawAllSplits` (the overhead map flushes every 16 tiles)
  overwrote every earlier flush's vertices - uploads now append, the buffer keeps
  four flushes, and each draw is offset by its upload's base; (2) `GR_CopyVRAM`
  only set a dirty flag that `GR_UpdateVRAM` turned into a whole-mirror upload at
  the next scene, so every draw sampled the frame's final VRAM state and all
  sixteen recycled map slots held the last batch - writes now queue their
  rectangle and pixels (`PsyX_Vk_GameCopyVRAM`) and are replayed in generation
  order during draw recording, closing and reopening the render pass around each
  replay. Fork commit `8510b31`. Validated: `-vkpsxtest` PASS with exact
  readbacks, and the overhead map now draws tiles/labels instead of scattered
  garbage; it still does not match the OpenGL reference image (see Current
  objective).
- 2026-09-19: Vulkan main pass preserves the framebuffer (defect 1). The PSX
  loading path draws the art once and then only the progress bar; OpenGL keeps
  the art because it clears only when `activeDrawEnv.isbg` is set, while the
  Vulkan pass always used `LOAD_OP_CLEAR` and `clearRequested` was never read. The
  colour attachment now uses `LOAD_OP_LOAD` with a `PRESENT_SRC` initial layout
  and clears with `vkCmdClearAttachments` only when `GR_Clear` was called (or on
  the first use of a swapchain image, which is first moved from `UNDEFINED` to
  `PRESENT_SRC`). Validated: `-vkpsxtest` PASS unchanged, `-vkfixture` capture,
  in-game render without smearing, loading art persists at a `ShowLoading`
  breakpoint. Fork commit `41e74b1`, parent `b6e054f4`.
- 2026-09-19: The in-game modern-mesh system runs on Vulkan (defect 3, Option A).
  `PsyX_ModernMesh.cpp` dispatches to `PsyX_Vk_GameModernMesh*`; new shaders
  `psx_modern.vert/.frag`, `fullscreen.vert`, `psx_composite.frag`; the developer
  gallery no longer disables itself. The shared `ModernUBO` must be declared
  identically in all three shaders (the vertex shader was missing
  `ambientExposure`, which shifted `cameraPos` onto the wrong std140 slot and
  made every mesh invisible). Verified in game: `modernCalls=8`,
  `modernVerts=16143`, F10 toggle removes/restores the fixtures. Also adds the
  backend-aware overlay texture accessor for the developer preview (defect 4).
  Fork commit `f7a4a0f`, parent `11648d3a`.
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
- Implemented: Vulkan game rendering (the default backend), the in-game modern
  mesh system on Vulkan, framebuffer persistence matching OpenGL, the
  framebuffer-to-VRAM mirror, offscreen render-to-VRAM target, PSX primitive mask
  bit (stencil), minimap scissor fix, fork migration, SDL3 deferral recorded.
- Pending: the map-screen parity investigation above; R8 profiling (frame-time
  distribution, peak memory, Linux/web/Android); a fresh OpenGL (`-opengl`)
  comparison; and the R5 shadow-quality comparison.
- Risks / open questions: the Vulkan frame mirror samples the whole window
  (including the dev overlay) and scales it onto the PSX display rect; the GL
  legacy path's `GR_CopyRGBAFramebufferToVRAM` R/B extraction is suspect, but the
  only consumer (lens flare) is colour-insensitive. `GR_UseVulkan` guards must
  keep Emscripten/Android/PSX untouched. The main pass now depends on the
  swapchain image being in `PRESENT_SRC` at entry; any new path that renders to
  the swapchain must keep that layout contract.

## Relevant files
- `src_rebuild/PsyCross/src/render/PsyX_Vk.cpp`: Vulkan backend; modern-mesh
  module, main-pass clear semantics, `RecordPsxDraws` (+ `ApplyVramUploadsUpTo`,
  `ResumeMainPass`), the VRAM upload staging, the frame mirror.
- `src_rebuild/PsyCross/src/render/PsyX_render.cpp`: `GR_*` dispatch;
  `GR_CopyVRAM` (queues a Vulkan VRAM rectangle), `GR_VkMirrorFrameToVRAM`,
  `PsyX_GetOverlayTextureId`, `GR_BeginScene`, `GR_UpdateVertexBuffer`.
- `src_rebuild/PsyCross/src/gpu/PsyX_GPU.cpp`: `DrawAllSplits` (the flush point),
  `ClearSplits` (resets `g_vertexIndex`), `DrawSplit`.
- `src_rebuild/PsyCross/src/render/vk_shaders/psx_modern.vert`,
  `psx_modern.frag`, `fullscreen.vert`, `psx_composite.frag`: the game modern
  pass; regenerate `PsyX_Vk_Shaders.h` with `scripts/compile_vk_shaders.ps1`.
- `src_rebuild/PsyCross/src/render/PsyX_ModernMesh.cpp`: backend dispatch.
- `src_rebuild/utils/DeveloperModernMesh.cpp`, `DeveloperGraphicsPanel.cpp`:
  gallery parity and the overlay preview.
- `src_rebuild/PsyCross/include/PsyX/PsyX_vk.h`,
  `include/PsyX/PsyX_render.h`, `PsyX_public.h`: the new declarations.
- `src_rebuild/Game/C/overmap.c`, `Game/C/sky.c`, `Game/C/loadview.c`: minimap,
  lens flare and the loading screen.
- `knowledge/roadmap/done/vulkan-game-renderer.md`,
  `knowledge/product/vulkan-game-renderer.md`,
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
- 2026-09-19: `REDRIVER2_dev.exe -vkpsxtest` after the main-pass clear change.
  Result: PASS, unchanged numbers (16-bit/4-bit worst=0, offscreen ok,
  `vram export 1048594/1048594 bytes: ok`).
- 2026-09-19: `REDRIVER2_dev.exe -vkfixture -vkcapture 10 -vkshot
  fixture_check.bmp`. Result: exit 0, 1280x720 frame with the 8 GLBs + analytic
  spheres.
- 2026-09-19: VS debugger + screenshots on `Release_dev` (Vulkan). Result: the
  modern fixtures render in game (`modernCalls=8 modernVerts=16143`) and F10
  toggles them; at a `ShowLoading` breakpoint (`activeDrawEnv.isbg == 0`) the
  loading art stays on screen after the clear-semantics fix. Picking a ground
  primitive in the 3D Debug tab resolves a `GRASS01C` override and the panel
  renders the preview image on Vulkan. The Khronos validation layer is not
  installed on this machine, so those runs have no validation output.
- Still required: a fresh `-opengl` comparison; macOS/MoltenVK build; the
  `D32_SFLOAT` fallback; raw uncapped GPU throughput; R5 shadow-quality
  comparison.

## Next recommended action
1. Finish the map-screen parity work: with the map forced open (`gShowMap = 1`
   through the debugger while the game runs), compare one frame's per-draw tile
   slots on Vulkan and OpenGL starting from `LoadMapTile`; the OpenGL reference
   capture is the acceptance image. Then check the loading progress bar
   (`ShowLoading`), the minimap police indicators and the clapperboard transition
   for the same flush-order class of defect.
2. Re-check the remaining renderer items when they are next in scope: a fresh
   `-opengl` parity run and the R5 shadow-quality comparison.

## Compact changelog
- 2026-09-19: Vulkan keeps every PSX vertex flush (uploads append) and replays
  VRAM writes in flush order, so the streaming map-screen image draws instead of
  showing scattered garbage (map parity still open).
- 2026-09-19: modern-mesh system delivered on Vulkan (defect 3) + overlay texture
  accessor (defect 4); Vulkan main pass preserves the framebuffer so the loading
  screen keeps its art (defect 1).
- 2026-09-18: Vulkan game path - resize recreation fix (framebuffers/views/depth
  now rebuilt, verified over four live resizes at 0 validation errors).
- 2026-09-18: Vulkan game path - PSX mip-chain barrier fix, backend-agnostic perf
  log, measured OpenGL/Vulkan parity.
- 2026-09-18: Vulkan game path phase 2; minimap scissor fix; framebuffer-to-VRAM
  mirror; PsyCross fork migration; SDL3 deferral; validation re-run.
- 2026-09-18: OpenCode context policy configured (`opencode.jsonc`).
