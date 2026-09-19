---
type: Roadmap
title: Vulkan game renderer (R7b)
status: planned
tags: [roadmap, rendering, psycross, vulkan, backend]
---

# Vulkan game renderer (R7b)

Status: planned

## Objective

Render the *game* through Vulkan instead of OpenGL. R7a delivered a Vulkan
backend for the modern fixture gallery (`PsyX_Vk.*`, `-vkfixture`), verified
with the Khronos validation layer. R7b takes the same device/swapchain and
reproduces the emulated PSX GPU path that runs in `PsyX_render.cpp` (OpenGL), so
the game window itself is presented by Vulkan.

**Current state (2026-09-19):** R7b is implemented and Vulkan is the **desktop
default**; OpenGL remains selectable at runtime with `-opengl`. The port is
feature-complete against the `GR_*` seam except for the items listed under
*Open work*. All four user-reported defects from the default flip are fixed and
verified (1 loading screen, 2 sRGB, 3 modern meshes on Vulkan, 4 texture
preview), plus defect 5 (screenshots) found during the work. See
[Reported defects](#reported-defects).

## The seam

The game submits geometry through the `GR_*` API declared in
`PsyCross/include/PsyX/PsyX_render.h`. Everything a backend must implement:

- Geometry: `GR_UpdateVertexBuffer(const GrVertex*, int)`,
  `GR_DrawTriangles(int startVertex, int triangles)`.
- State: `GR_SetTexture(TextureID, TexFormat)`, `GR_SetShader`,
  `GR_SetBlendMode`, `GR_EnableDepth`, `GR_SetScissorState`,
  `GR_SetupClipMode`, `GR_SetOffscreenState`, `GR_SetStencilMode`,
  `GR_SetPolygonOffset`, `GR_SetViewPort`, `GR_SetWireframe`,
  `GR_SetOverrideTextureSize`, `GR_SetOverrideAlphaMode`.
- Matrices: `GR_Perspective3D`, `GR_Ortho2D`.
- VRAM: `GR_UpdateVRAM`, `GR_CopyVRAM`, `GR_ReadVRAM`, `GR_ClearVRAM`,
  `GR_StoreFrameBuffer`, `GR_ReadFramebufferDataToVRAM`, `GR_SaveVRAM`.
- Textures: `GR_CreateRGBATexture`, `GR_CreateRGBATextureMipmapped`,
  `GR_DestroyTexture`, `g_whiteTexture`, `g_vramTexture`.
- Frame: `GR_BeginScene`, `GR_EndScene`, `GR_SwapWindow`, `GR_Shutdown`,
  `GR_InitialiseRender`, `GR_ResetDevice`, `GR_Clear`.

## Facts the port must reproduce (from the OpenGL renderer)

Geometry

- `GrVertex` is packed and, with PGXP enabled, is 44 bytes: floats
  `x,y,page,clut` (0,4,8,12), floats `z,scr_h,ofsX,ofsY` (16,20,24,28), bytes
  `u,v,bright,dither` (32..35), bytes `r,g,b,a` (36..39), signed bytes
  `tcx,tcy,_p0,_p1` (40..43).
- Attributes: 0 `a_position` = 4 floats at 0; 1 `a_zw` = 4 floats at 16;
  2 `a_texcoord` = 4 unnormalised bytes at 32; 3 `a_color` = 4 **normalised**
  bytes at 36; 4 `a_extra` = 4 signed bytes at 40. Pin every location
  explicitly; GL let `a_extra` be assigned by the linker.
- Draws are a flat triangle list: `GR_DrawTriangles(start, tris)` ==
  draw `tris*3` vertices from `start`. One `GR_UpdateVertexBuffer` per frame
  uploads the whole array (up to 65536 vertices), then splits are drawn.
- `a_zw.y` (`scr_h`) selects the path: `> 100` means 3D (GTE position built
  from `Projection3D` with a per-vertex offset matrix), otherwise 2D
  (`Projection * vec4(a_position.xy, 0.5, 1)`).

VRAM and textures

- CPU mirror: `unsigned short vram[1024*512]`, 16-bit PSX pixels, little
  endian; row stride 1024.
- The shader samples VRAM directly and performs CLUT and texture-window
  lookups itself. The GL texture is `GL_RG32F` 1024x512, `GL_RG`, uploaded as
  **bytes**: R = low byte / 255, G = high byte / 255. Filtering is `GL_NEAREST`
  (bilinear is implemented in-shader), wrap defaults to repeat.
- A 256x256 RGBA LUT (`rgLUT`, built by `GR_InitRG8LUT`) decodes the VRAM
  byte pair back into an RGBA colour; it is bound as the second sampler.
- `GR_UpdateVRAM` uploads the whole VRAM whenever `vram_need_update` is set and
  ping-pongs between two textures; the current one is `g_vramTexture`.
- `GR_CopyVRAM` is a CPU row copy (stride 1024; a NULL `src` means "from the
  live framebuffer region" and also raises `framebuffer_need_update`).
- `GR_StoreFrameBuffer` blits the finished window image into VRAM at
  `activeDispEnv.disp` so effects that read the screen work; it runs at the end
  of `PsyX_EndScene` and mirrors the result into the CPU VRAM. The GL path
  resizes `g_fbTexture` to the display rect, blits the window into it and reads
  it back with `glGetTexImage`, which yields a top-down image; `flip_y = 0`
  then writes it top-down into `vram`. The Vulkan equivalent is
  `GR_VkMirrorFrameToVRAM`: it consumes the pending display rect from
  `PsyX_Vk_TakeStoredFrameBuffer`, converts the top-down BGRA/RGBA readback into
  5551 words and re-uploads the CPU mirror.

State

- Blend (`GR_SetBlendMode`): `BM_NONE` disables blending and enables depth;
  every other mode enables blending and disables depth. Factors:
  `BM_AVERAGE` = src_alpha / one_minus_src_alpha, `BM_ADD` = one / one,
  `BM_SUBTRACT` = one / one with **reverse subtract**, `BM_ADD_QUATER_SOURCE`
  = constant_alpha / one with the constant alpha = 0.5. Vulkan has no
  reverse-subtract for the alpha channel, so use
  `VK_BLEND_OP_REVERSE_SUBTRACT` on the colour channel and `VK_BLEND_OP_ADD` on
  alpha, and `vkCmdSetBlendConstants` for the quarter-source mode.
- Depth: `GL_LEQUAL`, enabled only when `BM_NONE` and `g_cfg_pgxpZBuffer`.
- Scissor: `GR_SetupClipMode` maps the PSX clip rect to window space and flips
  Y (`g_windowHeight - clipRectH*h`), including the widescreen aspect factor
  `1/(0.75 * windowWidth/windowHeight)`.
- Offscreen (mirrors): an RGBA render target cleared to (0.5, 0.5, 0.5, 0),
  drawn with the ±0.5 ortho, then blitted into the current VRAM texture with a
  Y flip and downloaded into the CPU VRAM with `flip_y = 1`.
- Stencil: `GR_SetStencilMode` uses
  `GL_ALWAYS/REPLACE,REPLACE,REPLACE` with mask 0x10 for the mask-drawing
  primitive and `GL_NOTEQUAL 1, 0xFF` with `REPLACE,KEEP,KEEP` otherwise.
- Polygon offset on/off with `glPolygonOffset(0, ofs)`, wireframe toggles
  `glPolygonMode`, viewport is the full window unless an offscreen pass is
  active.

Matrices (column-major, uploaded unterminated)

- `GR_Ortho2D` is the standard GL ortho; z maps to -1..1.
- `GR_Perspective3D` is the PSX-style projection: `h = cos(fov/2)/sin(fov/2)`,
  `w = h*height/width`, third column `(0,0,(far+near)/(far-near),1)`, fourth
  `(0,0,-2*far*near/(far-near),0)`. Defaults: fov 0.9265, near 0.25, far 1000.
- Vulkan conversion belongs in the vertex shader after computing the GL clip
  position: `y = -y` and `z = (z + w) * 0.5`, because Vulkan NDC has Y down and
  depth in [0,1] while these matrices are GL-style.

## Phases

1. **Done.** VRAM + LUT + PSX shader on Vulkan: `vk_shaders/psx.vert` and
   `psx.frag` reproduce `GTE_VERTEX_SHADER` and the merged 4/8/16/32-bit
   fragment path (CLUT, texture window, in-shader dither and bilinear filter)
   with the GL-to-Vulkan clip-space conversion. `PsyX_Vk_Game*` models VRAM
   (`R32G32_SFLOAT`, byte-pair packing), the RG8 decode table, the five blend
   pipelines and depth state, and `-vkpsxtest` asserts readback pixels with zero
   Khronos validation errors. The 4-bit case fails if the nibble extraction,
   CLUT row address or RG8 table is wrong, so it is a real decode check rather
   than a smoke test.
2. **Done.** Game geometry: the game window is created with `SDL_WINDOW_VULKAN`,
   reuses the `PsyX_Vk` device/swapchain, maps the `GR_*` state to pipelines and
   draws the game's vertex stream. The in-game scene, HUD, minimap and frontend
   present through Vulkan. Deferred items from this phase (offscreen mirrors,
   framebuffer-to-VRAM store, stencil masking) were subsequently delivered — see
   *Delivered work mapped to R1-R8*.
3. **Partially done.** Parity and rollout: offscreen RT, framebuffer store,
   stencil mode and the save/load VRAM paths are implemented; the FMV shader is
   out of scope on desktop (see *Non-goals*). The regression pass over the
   playground and a representative original city with both backends, and the
   documented performance/limitation comparison, are partial (see *R8*).

## Delivered work mapped to R1-R8

This section inventories what the Vulkan game-renderer work delivered, mapped to
the milestones defined in
[renderer-modernization](renderer-modernization.md). It is the pass-through the
user requested: each R item, its Vulkan-relevant delivery, and its
re-validation state.

### R1 - Baseline and contracts: delivered (for the Vulkan path)

The port consumed and re-validated the R1 contracts on a second API:

- `GrVertex` 44-byte packed layout pinned to explicit Vulkan attribute
  locations (0 `a_position` 4 floats @0, 1 `a_zw` 4 floats @16, 2 `a_texcoord`
  4 unnormalised bytes @32, 3 `a_color` 4 normalised bytes @36, 4 `a_extra`
  4 signed bytes @40), with a static assert on the size.
- GL-to-Vulkan clip-space conversion (`y = -y`, `z = (z + w) * 0.5`) in
  `psx.vert`, reproducing the R1 depth/winding contract.
- State mapped enum-for-enum: `TF_4_BIT=0 .. TF_32_BIT_RGBA=3` ==
  `PSYX_VK_TEX_*`; `BM_NONE=0 .. BM_ADD_QUATER_SOURCE=4` == `PSYX_VK_BLEND_*`.
- Frame double-buffering semantics preserved: the game builds frame N+1's
  display list during frame N's `DrawSync`; draws and offscreen groups are
  tagged with `psx->frameIndex` and filtered per frame in `RecordPsxDraws`.

Status: delivered and validated by the port itself. No re-validation needed.

### R2 - Unlit hybrid mesh: delivered on Vulkan (re-validated)

- The in-game hybrid modern-mesh path now runs on Vulkan: `PsyX_ModernMesh.cpp`
  dispatches every call to `PsyX_Vk_GameModernMesh*` when the backend is active,
  and `DeveloperModernMesh` no longer disables itself (PsyCross `f7a4a0f`).
- The same eight owned GLBs plus the analytic primitives are drawn into the
  shared colour/depth attachments, so they occlude legacy geometry, and the F10
  toggle behaves as on OpenGL.
- The Vulkan *fixture* still renders its own standalone scene (12 meshes /
  12 instances) for the R7a smoke path.

Re-validation outcome: the Vulkan fixture archive is no longer needed to
demonstrate the modern path - it is live in the default build.

### R3 - Static asset path: delivered in-game on Vulkan

- `utils/GltfLoader.cpp` is CPU-side and backend-agnostic; the in-game gallery
  loads the same eight owned GLBs from `assets/modern_fixtures/` with
  `provenance.md` on both backends.

Status: delivered in-game.

### R4 - Reference PBR: delivered in-game on Vulkan

- `vk_shaders/psx_modern.frag` implements metallic/roughness with GGX + Schlick,
  normal mapping, a small light budget and exposure, decoding base colour
  sRGB->linear and re-encoding on output.
- The sRGB output convention (`ToLinear` / `lightInfo[1]`) was validated during
  the defect #2 work and is mirrored by the PSX shader.

Status: delivered in-game.

### R5 - Lighting effects: delivered for casters, receivers on Vulkan

- Modern fixtures cast into the 2048x2048 directional shadow map and the
  composite pass applies the scene-depth legacy-receive term
  (`vk_shaders/psx_composite.frag`), so the OpenGL lighting behaviour has a
  Vulkan equivalent. Shadows and AO default off in `developer_modern_mesh.ini`.
- The runtime sun/exposure keys and persisted settings remain shared developer
  features.

Status: restated for the Vulkan-default configuration; the receive term is in
scope and implemented. Deeper shadow-quality comparison against OpenGL was not
performed.

### R6 - Pipeline decision: restated for the Vulkan-default configuration

- Retain the reference Forward path (small light budget, ample headroom).
- The classic/enhanced switch is a persisted developer Graphics-panel setting
  plus F10. With Vulkan default it now drives the Vulkan modern-mesh path, so
  the switch affects the presented image as it does on OpenGL.

### R7 - Backend portability: R7a delivered, R7b delivered (with open items)

**R7a** (modern-scene Vulkan backend):

- `PsyCross/include/PsyX/PsyX_vk.h` + `PsyCross/src/render/PsyX_Vk.cpp`:
  self-contained Vulkan backend (dynamic loader through SDL, instance/device/
  swapchain, main and shadow render passes, PBR and depth pipelines, per-mesh
  descriptor sets, UBO, textures, resize, RGBA readback, ImGui overlay through
  `imgui_impl_vulkan`).
- `PsyCross/src/render/vk_shaders/*.glsl` + generated `PsyX_Vk_Shaders.h`
  (embedded SPIR-V), regenerated with `scripts/compile_vk_shaders.ps1`.
- `utils/DeveloperVkFixture.*` and the `-vkfixture` / `-vkcapture N` /
  `-vkshot <path>` / `-vknogui` developer entry points.
- Resolved: `CreateRenderPasses()` ran before the swapchain format was
  negotiated, so the main pass had `VK_FORMAT_UNDEFINED` and discarded every
  draw; `QuerySwapchainFormat()` now runs first. `PsyX_Vk_ReadbackRgba` also had
  a stray row flip.

**R7b** (the game itself on Vulkan) — delivered:

- `GR_*` dispatch to a `PsyX_Vk_Game*` module selected at runtime, keeping the
  OpenGL renderer selectable.
- PSX pipeline set: 4/8/16-bit CLUT and 32-bit RGBA paths with the in-shader
  CLUT, texture-window, dither and bilinear filter; five blend pipelines;
  depth state; scissor with the minimap Y fix; viewport.
- VRAM as `R32G32_SFLOAT` 1024x512 with byte-pair packing and the RG8 decode
  LUT; `GR_UpdateVRAM` / `GR_CopyVRAM` / `GR_ReadVRAM` / `GR_ClearVRAM`.
- Offscreen render-to-VRAM (`GR_SetOffscreenState`) consumed by
  `TannerShadow` (`Game/C/motion_c.c:1869`).
- PSX mask-bit stencil (`GR_SetStencilMode`) mapped to a combined
  depth-stencil attachment.
- Framebuffer-to-VRAM mirror (`GR_VkMirrorFrameToVRAM`) so screen-reading
  effects (sun lens flare via `StoreImage`, `Game/C/sky.c:523`) sample the
  frame.
- `GR_SaveVRAM` (F10 VRAM.TGA export) on Vulkan.
- Swapchain resize: `RecreateSwapchain()` fully recreates resources on resize
  and on acquire-out-of-date (previously only destroyed them, leaking
  framebuffers/views/depth).
- Backend-agnostic performance sampling (`PSYX_PERF_LOG=1` ->
  `psyx_perf.log`) and `PSYX_VK_PRESENT_MODE`.
- Default flip: `redriver2_psxpc.cpp` defaults to Vulkan on desktop
  (`#if !defined(PSX) && !defined(__ANDROID__) && !defined(__EMSCRIPTEN__)`),
  `-opengl` forces OpenGL, `-vulkan` kept for compatibility, automatic fallback
  to OpenGL if `PsyX_Vk_Initialise` fails (`PsyX_main.cpp:669`).

Status: delivered. Open items: MoltenVK/macOS prepared in code and premake but
never built on a Mac; the `D32_SFLOAT` depth fallback (no stencil) is compiled
but never exercised because NVIDIA always exposes the combined format; uncapped
GPU throughput was not measured because the game is PSX-timestep-bound.

### R8 - Regression and handoff: partial

Validation executed on Windows `Release_dev` x64 (see
[Validation performed](#validation-performed)). Not profiled: CPU/GPU frame-time
distribution, peak memory, and the Linux/web/Android builds. Defects 1, 2, 3, 4
and 5 are fixed and verified; remaining verification is profiling and a fresh
`-opengl` parity run (see [Reported defects](#reported-defects)).

### Items needing re-validation (consolidated)

| Item | State |
|---|---|
| R2 | Delivered on Vulkan; no longer fixture-only |
| R3 | Delivered in-game |
| R4 | Delivered in-game |
| R5 | Casters and the receive term implemented on Vulkan; quality comparison pending |
| R6 | Restated for the Vulkan-default configuration |
| R8 | Frame-time/memory/other-target profiling still missing |

## Reported defects

Four defects were reported by the user after the default flip. Their current
state and root-cause findings:

### Defect 1 - Loading screen appears black (no progress bar) - FIXED

Symptom: during level load the screen is black and the progress bar does not
appear.

Root cause (reproduced in this session):

- The loading path uses PSX immediate-mode drawing: `ShowLoadingScreen`
  (`Game/C/loadview.c:137`) draws the art every frame, then `ShowLoading`
  (`:22`) draws only the progress bar while the level data is parsed.
- The OpenGL renderer only clears when `activeDrawEnv.isbg` is set
  (`PsyX_main.cpp:889-898`), so the art drawn by `ShowLoadingScreen` persists
  across the `ShowLoading` frames.
- The Vulkan main pass always used `VK_ATTACHMENT_LOAD_OP_CLEAR`
  (`PsyX_Vk.cpp`), and `g_vk.psx.clearRequested` (written by `GR_Clear`) was
  never read - a dead flag. Every `ShowLoading` frame cleared the art to the
  stale clear colour, so only the small bar survived.

Runtime confirmation (VS debugger, `Release_dev` Vulkan):

- Breakpoint in `ShowLoading`; at the break `activeDrawEnv.isbg == 0` and
  `begin_scene_flag == 0`.
- Before the fix the `ShowLoading` frame presented black; after the fix the
  same break shows the loading art still on screen.

Fix (PsyCross fork `41e74b1`):

- The main pass colour attachment now uses `LOAD_OP_LOAD` with a
  `PRESENT_SRC_KHR` initial layout, preserving the previous frame.
- `GR_Clear` takes effect again: when `clearRequested` is set the colour
  attachment is cleared with `vkCmdClearAttachments`, matching `glClear`.
- A swapchain image that has never been drawn is transitioned from its
  undefined layout to `PRESENT_SRC` and cleared once.
- The fixture window, which has no draw environment, still clears every frame.

Validated: `-vkpsxtest` PASS (unchanged numeric assertions), `-vkfixture`
capture renders, in-game Vulkan scene renders without smearing, and the loading
screen keeps its art during `ShowLoading`.

### Defect 2 - Colours wrong (washed out / brighter than OpenGL) - FIXED

Root cause: **sRGB double-encode.** The PSX fragment shader emits
display-referred (already gamma-encoded) values, exactly like the OpenGL shader.
The Vulkan main pass writes into an sRGB-format attachment (`PickSurfaceFormat`
prefers `VK_FORMAT_B8G8R8A8_SRGB`, `PsyX_Vk.cpp:852-866`; the attachment uses it
at `:1460`), so the hardware applied a second linear-to-sRGB encode on store.
The OpenGL renderer never enables `GL_FRAMEBUFFER_SRGB`, so it never double
encodes.

Fix: an `srgbEncode` push constant, set only for main-pass PSX draws
(`draw->srgbEncode = (psx->offscreenActive || !g_vk.srgbOutput) ? 0 : 1`); the
offscreen target is UNORM and stays raw. `vk_shaders/psx.frag` applies
`ToLinear` to the final colour when `pc.srgbEncode != 0`, so the hardware's sRGB
store re-encodes back to the original value.

Evidence: `-vkpsxtest` PASS with
`16-bit 0x001F: got (248,0,0,255) worst=0` and
`4-bit CLUT entry 5: got (0,0,248,255) worst=0`. The numeric proof is the shift
from the previously-asserted encoded value (252) to the correct display-referred
value (248). The self-test assertions were corrected and the now-dead
`SrgbEncodeFloat` helper removed.

Commit: PsyCross fork `837573a` (pushed).

Next step: in-game visual comparison against the OpenGL configuration to confirm
the presented image matches, and confirm the secondary `GR_VkMirrorFrameToVRAM`
path is consistent now that the stored bytes are display-referred again.

### Defect 3 - Dynamic objects not appearing (modern-mesh system on Vulkan) - FIXED (Option A delivered)

Symptom: objects the user expects to be replaced/modernised do not appear.

Findings before the fix:

- There is **no dynamic-object or prop-replacement mechanism in any backend**.
  `utils/DeveloperModernMesh.cpp` does not replace anything: it is a fixed
  gallery of the eight GLBs at hard-coded positions (`:43-54`, `:801-803`) and
  was hard-disabled on Vulkan (`ModernMeshVulkanDisabled`).
- `PsyX_ModernMesh.cpp` compiled only under `USE_OPENGL`; the Vulkan fixture did
  not run the game.
- The game's moving entities (cars, Tanner, animated props) **do** render on
  Vulkan through the normal PSX stream (`GR_*`) delivered by R7b. The
  modern-renderer roadmap lists animated-character/vehicle migration as a
  **non-goal**.

Decision (user): **Option A - port the entire modern-mesh system to Vulkan.**

Delivered (PsyCross fork `f7a4a0f`):

- `vk_shaders/psx_modern.vert/.frag` reproduce the GTE-encoded modern vertex
  path (GL clip-space conversion, xyScale, camera rotation/position) and the
  GGX/Schlick PBR fragment path; `fullscreen.vert` + `psx_composite.frag`
  composite the modern pass over the PSX main pass with the scene-depth shadow
  term.
- `PsyX_Vk_GameModernMesh*` owns a load render pass, UBO, descriptor sets and
  pipelines; `PsyX_ModernMesh.cpp` dispatches every public call to it when the
  Vulkan backend is active; `DeveloperModernMesh` no longer disables itself on
  Vulkan, so F10 toggle and gallery parity work as on OpenGL.
- Root cause of the initial "meshes invisible" state: `psx_modern.vert`
  declared the shared `ModernUBO` without `vec4 ambientExposure`, shifting its
  `cameraPos` onto the wrong std140 slot; the camera transform then clipped
  every mesh away. Adding the field fixed it.

Runtime confirmation (VS debugger + screenshots, Vulkan default): the eight
fixtures import, `modernCalls=8 modernVerts=16143`, and toggling F10 removes and
restores the wooden crate, barriers and panels in the in-game frame.

### Defect 4 - Texture preview in the developer GUI panel not showing - FIXED

Symptom: the HD-texture override preview in the developer graphics panel does
not display the image on Vulkan.

Root cause confirmed:

- The export path is CPU/VRAM and is correct
  (`utils/HdTextureOverrides.cpp:1810+`).
- The preview passes `previewTextureId` to `ImGui::Image`
  (`utils/DeveloperGraphicsPanel.cpp:589`), where the ID comes from
  `PsyX_CreateRGBATexture`. On OpenGL it is a real `GLuint`; on Vulkan
  `GR_CreateRGBATexture` returns `slot + 1` (an index).
- The ImGui Vulkan backend casts `ImTextureID` to a `VkDescriptorSet`
  (`imgui_impl_vulkan.cpp:611`), and no `ImGui_ImplVulkan_AddTexture` bridge
  existed outside the vendored library.

Fix delivered (PsyCross fork `f7a4a0f`):

- `PsyX_GetOverlayTextureId` / `PsyX_GetRGBATextureSize` are backend-aware: on
  Vulkan they dispatch to `PsyX_Vk_GameGetOverlayTextureId` /
  `PsyX_Vk_GameGetTextureSize`, which keep a single cached
  `ImGui_ImplVulkan_AddTexture` descriptor set per slot and drop it when the
  texture is destroyed; on OpenGL the `GLuint` is returned and the size is
  queried with `glGetTexLevelParameteriv`.
- `DeveloperGraphicsPanel.cpp` uses the accessor and the real aspect ratio, and
  shows a disabled note when the backend has no bridge.

Verified on Vulkan: F11 -> 3D Debug -> "Pick visible primitive", then picking a
ground primitive resolves `Texture: GRASS01C | level page 1 | index 5` with
`Override: remaster-textures :: GRASS01C_p1_i5_upscayl_4x_ultrasharp-4x.png` and
the panel renders the override image ("Loaded override preview"). The OpenGL
path is unchanged - the accessor returns the same `GLuint` that
`ImGui::Image` already accepted, so only the Vulkan bridge was new.

### Defect 5 - Screenshots broken on Vulkan - FIXED (found during this work)

`PsyX_TakeScreenshot` used `glReadPixels` unconditionally, so on Vulkan F12
wrote an uninitialised buffer. The Vulkan path now goes through
`PsyX_Vk_ReadbackRgba` (which already returns top-down RGBA, so the rows are not
flipped again and the SDL surface uses matching channel masks); the OpenGL path
is unchanged. Commit: PsyCross fork `55ded1f` (pushed).

## Non-goals

- FMV playback on desktop: `fmvplay.c` `PlayRender` uses `FMV_main` whose
  symbols/headers (`n.EXE`/`nplay.h`) are absent from this tree; the PSX branch
  is `#ifdef PSX` and commented out. Out of scope for the desktop backend.
- Animated-character/vehicle-deformation migration (see
  `renderer-modernization.md` non-goals); moving entities render through the
  normal PSX stream.
- Full-world rollout of a second backend beyond the delivered desktop path.

## Repository state

PsyCross fork (`src_rebuild/PsyCross`, `origin` =
`git@github.com:SimStm/PsyCross.git`, local branch `redriver2-plus` pushed to
`master`, tree clean):

- `49f9578` Vulkan backend for the modern scene (R7a).
- `396dd20` scissor fix; `9281fb7` frame mirror; `98c176c` offscreen
  render-to-VRAM; `1ad5b81` mask-bit stencil.
- `4b1191d` VRAM TGA export fix + self-test Case 4.
- `6b7e5bb` mip-chain per-level barriers (removed 30 validation errors) +
  backend-agnostic perf sampling.
- `ea7754f` resize: fully recreate swapchain resources instead of leaking.
- `837573a` sRGB double-encode fix (defect 2).
- `55ded1f` Vulkan screenshot support (defect 5).
- `f7a4a0f` modern-mesh system on Vulkan (defect 3, Option A) + overlay texture
  accessor (defect 4) + shared modern shaders.
- `41e74b1` preserve the framebuffer between Vulkan frames (defect 1).

Parent (`REDRIVER2-Plus`, `origin` =
`git@github.com:SimStm/REDRIVER2-Plus.git`, branch `master`):

- `3e3aa06e` `feat: native Vulkan game renderer with OpenGL fallback`
  (66 files, +4526/-115822).
- `f98c21f1` `docs: correct the Tanner shadow note and clarify the remaining
  limitations`.
- `11648d3a` `feat: draw the modern mesh system on the Vulkan backend`
  (gitlink `ea7754f3 -> f7a4a0f`).
- `b6e054f4` `fix: keep the Vulkan framebuffer between frames for the loading
  screen` (gitlink `f7a4a0f -> 41e74b1`).
- Fork commits `f7a4a0f` and `41e74b1` are pushed to `origin/master`.

Build configuration (`.vcxproj.user`, git-ignored):

- `Release_dev|x64` = no arguments -> Vulkan default.
- `Release_dev_gl|x64` = `-opengl`.
- Both produce the same binary; the backend is chosen at runtime.

## Validation performed

Windows `Release_dev` x64, NVIDIA GeForce RTX 3060 Ti, Vulkan SDK 1.4.357.0,
validation layer enabled:

- Solution build: 0 failed projects.
- `-vkpsxtest`: PASS - `16-bit 0x001F: got (248,0,0,255) worst=0`,
  `4-bit CLUT entry 5: got (0,0,248,255) worst=0`,
  `offscreen 64,64 32x32: samples ok`,
  `vram export 1048594/1048594 bytes: ok`.
- Present: 29.4-30.7 FPS with zero Khronos validation errors.
- Resize: four resizes exercised via `user32!MoveWindow`; swapchain recreated
  with zero validation errors.
- Cross-backend parity: Vulkan and OpenGL both 30.0 FPS / 33.37-33.43 ms with
  matching vertex (11298->13476) and draw (1295->1595) counts, consistent with
  the game being PSX-timestep-bound rather than GPU-bound.
- `scripts/run_inspector_tests.ps1`: AssetCatalogTests 145 checks / 0 failures,
  InspectorExportTests 104 passed.
- `git diff --check`: exit 0.

Additional validation in the defect-fix session (Windows `Release_dev` x64,
NVIDIA GeForce RTX 3060 Ti; Khronos validation layer not installed on this
machine - no SDK `Releases` layer present):

- Build: 0 failed projects after each of the two fix commits.
- `-vkpsxtest`: PASS with unchanged numbers (`16-bit 0x001F: got (248,0,0,255)
  worst=0`, `4-bit CLUT entry 5: got (0,0,248,255) worst=0`, `offscreen 64,64
  32x32: samples ok`, `vram export 1048594/1048594 bytes: ok`) after the
  clear-semantics change.
- `-vkfixture -vkcapture 10 -vkshot fixture_check.bmp`: exits cleanly and writes
  a 1280x720 frame with the eight GLBs and the analytic spheres.
- In-game modern meshes: `modernCalls=8 modernVerts=16143`; F10 toggling the
  modern path removes and restores the wooden crate, barriers and panels in the
  captured Vulkan frame.
- Loading screen: breakpoint in `ShowLoading` shows `activeDrawEnv.isbg == 0`;
  the presented frame keeps the loading art after the fix (black before it).
- Present: ~29.6-30.2 FPS in game, consistent with the PSX timestep bound.
- Defect 4 preview: on Vulkan, F11 -> 3D Debug -> pick a ground primitive
  resolves a `GRASS01C` texture with a `remaster-textures` override and the panel
  renders the override image.

Not validated: MoltenVK/macOS, the `D32_SFLOAT` fallback, Linux/web/Android
builds, peak memory, uncapped GPU throughput, and a fresh OpenGL (`-opengl`)
comparison run for R8.

## Risks

- The PSX fragment shader is the highest-risk piece: CLUT lookup, texture
  window, dither and the shader-side bilinear filter must match GL exactly.
  Phase 1 exists to catch that early with pixel assertions.
- Colour-space handling is now split between the PSX shader (display-referred,
  inverse-encoded for sRGB targets) and the modern PBR shader (linear, encoded
  for non-sRGB targets). Any new pass must state which convention it follows.
- VRAM feedback (screen-to-VRAM store) is needed by effects that sample the
  framebuffer; getting the Y flip and 5551 packing wrong shows up as mirrored
  or wrong-coloured effects rather than obvious failures.
- Vulkan depth/Y conventions differ from GL; the conversion is per-vertex, so
  the modern mesh path already proves the approach but the PSX z path is
  custom and must be validated separately.
- A second backend doubles the surface that must keep working for Emscripten,
  Android and the PSX toolchain; guard the Vulkan game path so those targets
  are untouched.
- Default flip side effect: the R2-R6 modern-mesh/lighting features are inactive
  in the default configuration because they are OpenGL-only. Treat "the modern
  path works" statements as `-opengl`-only until defect 3 is resolved.
- The `clearRequested` flag in `PsyX_Vk.cpp` is dead code; do not wire it up
  speculatively before defect 1 is reproduced.

## Handoff and completion

Keep partial milestones and their evidence in this planned record. Once R1-R8
and all acceptance criteria pass, add
`knowledge/product/vulkan-game-renderer.md`, move this record to `done/`, set
status/date, and update the catalogs, both discussions, the playground consumer
link and the changelog. If a later decision defers R7 or another milestone,
revise/split the adopted scope explicitly instead of claiming the original
complete feature was delivered.
