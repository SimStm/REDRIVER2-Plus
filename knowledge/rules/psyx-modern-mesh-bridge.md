---
type: Rule
title: Share the legacy projection and depth when adding a modern mesh path
description: How the PSX 3D vertex encoding maps to clip space, and how to add an OpenGL mesh that occludes and is occluded by legacy geometry.
tags: [okf, rendering, psycross, psygpu, depth, meshes]
---

# Share the legacy projection and depth when adding a modern mesh path

**When** adding a non-PSX (modern) mesh draw to PsyCross, **then** reproduce the
legacy 3D vertex encoding exactly and draw into the same framebuffer/depth
buffer. A standard world/view/projection matrix computed independently does not
land in the same clip space and will be clipped or depth-rejected.

## How the legacy 3D vertex stream reaches clip space

Verified in `PsyX_GPU.cpp` and `PsyX_GTE.cpp` with PGXP enabled (the desktop
default):

- The GTE writes `PGXPVData.px/py/pz = fMAC1/2/3 / (512*1024)`. Because the GTE
  MACs are Q12 (`4096 == 1.0`), this is `camera_axis / 128` in world units. So
  `pz = camera_z / 128` and `px/py = camera_x/y / 128`.
- `PGXPVData.scr_h = C2_H`, the GTE screen distance (`SetGeomScreen(scr_z)`,
  256 by default). `MakeVertex*` copies `x=px, y=py, z=pz, scr_h=scr_h` into
  `GrVertex`.
- `ScreenCoordsToEmulator` then divides `x` by the display width and `y` by the
  display height (`activeDispEnv.disp.w/h`, typically 320x240) and subtracts
  0.5.
- The vertex shader detects 3D with `a_zw.y > 100` and computes
  `gl_Position = Projection3D * vec4((a_position.xy + 0.5) * vec2(1,-1) * a_zw.y, a_zw.x, 1)`.
  So the clip position is
  `Projection3D * vec4(cx*H/(128*dispW), -cy*H/(128*dispH), cz/128, 1)`.
- `Projection3D` is set in `GR_SetOffscreenState` (`GR_Perspective3D`,
  `fov=0.9265`, `near=0.25`, `far=1000`, aspect folded into the matrix).

## Rules for a shared-depth modern mesh

1. Capture the exact `persp[16]` from `GR_Perspective3D` and reuse it; do not
   build an independent perspective matrix.
2. Build the view transform from the game's own camera basis
   (`inv_camera_matrix.m / 4096`, translation `-R*camera_position`) so it matches
   `Apply_InvCameraMatrix*`.
3. Scale the vertex before the projection the same way the shader does:
   `x*(C2_H/(128*dispW))`, `y*-(C2_H/(128*dispH))`, `z*(1/128)`.
4. Draw after the legacy world but before final overlays, while framebuffer 0
   still owns the legacy colour and depth. Single-view gameplay supplies the
   exact OT boundary through `PsyX_SetModernSceneBoundary`; `GR_EndScene` is
   only the fallback for callers without a boundary.
   Enable `GL_DEPTH_TEST`/`GL_LEQUAL`/depth write and disable culling/blend for
   an opaque unlit mesh.
5. **Restore the caller's render state, especially the bound VAO and
   `GL_ARRAY_BUFFER`.** Binding VAO 0 at the end of mesh creation corrupted later
   legacy draws (access violation on the next frames); save and restore
   `GL_VERTEX_ARRAY_BINDING`/`GL_ARRAY_BUFFER_BINDING` instead.
6. Create the mesh only once the camera is valid. `StepGame` can run while
   `camera_position` and `inv_camera_matrix` are still zero; positioning then
   pins the mesh to the world origin and it never appears.
7. Keep the public API C-compatible and generic in PsyCross; keep placement,
   the scene gate and the toggle in `src_rebuild`. Do not edit the gitlink.
8. **Anchoring must use the frame's final camera.** Update the instances after
   the legacy scene camera is built (`DrawGame`, after `RenderGame2`, before
   `SwapDrawBuffers` submits the OT), not at the
   start of `StepGame`. The game recomputes `camera_position`/`inv_camera_matrix`
   during the scene render, so an earlier read uses the previous frame and the
   mesh visibly swims when the camera moves.
9. **Static fixtures are placed once, in world space.** Ground each imported
   glTF by its own bounding box (`origin.y = MapHeight(x,z) - minY`) and derive
   the layout basis from the spawn pose only; never re-derive positions or
   orientations from the live camera.
10. **Every instance needs a world matrix, not only a view matrix.** The
    shadow-map pass renders casters as `lightMatrix * world * position`; a mesh
    that only supplies the view matrix is rendered at the world origin in the
    shadow map and casts no shadow where it stands
    (`PsyX_ModernMesh_SetInstanceWorld`).

## Projecting the modern shadow map onto legacy scenery

The shadow map can also darken legacy pixels (road, walls, trees) so modern
objects interact with the existing scene. Verified in `PsyX_ModernMesh.cpp`:

- Copy the legacy scene depth into a sampleable texture with `glBlitFramebuffer`
  from framebuffer 0. The window depth attachment is depth-stencil, so the copy
  target must be `GL_DEPTH24_STENCIL8` and attached as
  `GL_DEPTH_STENCIL_ATTACHMENT`; a `GL_DEPTH_COMPONENT24` target rejects the
  blit with `GL_INVALID_OPERATION` and silently produces no shadows.
- Reconstruct the world position from the sampled depth with
  `inverse(Projection3D)` and `inverse(cameraView)`. Do **not** transpose the
  camera rotation: the game's matrix can carry an aspect scale and is not
  orthonormal.
- Disable `GL_SCISSOR_TEST` for the pass; the legacy renderer toggles scissor
  per draw and a leftover rect clips the composite.
- The legacy 2D path (HUD, overlays) writes a constant `0.5` depth; skip pixels
  whose depth is within `1e-5` of `0.5` so the HUD is not darkened.
- Run the composite after the legacy scene and before the modern meshes (they
  shade their own shadows, so running it after would double-darken them).
- `PsyX_ModernMesh_SetShadowDebug` plus key `7` cycle diagnostics: `0` normal,
  `1` scene depth, `2` shadow-volume membership, `3` shadow map, `4` receiver
  vs caster depth. Keep them as the first stop when shadows disappear.
- Shadow position depends on the sun direction: with the light behind the
  objects relative to the camera the cast shadow is hidden behind the caster.
  Rotate the sun (`[`/`]`, `;`/`'`) before concluding that shadows are broken.

## Building the experimental Vulkan backend

The native-Vulkan slice (`PsyX_Vk.cpp`, `PsyX_vk.h`, `vk_shaders/`,
`PsyX_Vk_Shaders.h`, `utils/DeveloperVkFixture.*`) adds requirements that are
easy to get wrong:

- The build needs **Vulkan headers only**, vendored under
  `src_rebuild/dependencies/vulkan/include` (`VULKAN_DIR` overrides the path).
  The loader is resolved at runtime through SDL (`SDL_Vulkan_LoadLibrary`), so
  there is no `vulkan-1.lib` and no SDK; do not add a link dependency.
- `PsyX_Vk.cpp` and `imgui_impl_vulkan.cpp` compile with `VK_NO_PROTOTYPES` /
  `IMGUI_IMPL_VULKAN_NO_PROTOTYPES`. Global commands (`vkCreateInstance`,
  `vkEnumerateInstance*`) must be fetched with a NULL instance *before* the
  instance exists; fetching everything after creation crashes on the first
  call. ImGui needs `ImGui_ImplVulkan_LoadFunctions` plus
  `ImGui_ImplVulkan_NewFrame` and `ImGui_ImplSDL2_NewFrame` before
  `ImGui::NewFrame` (the renderer hook builds the font atlas, and without it
  `GetDefaultFont()` dereferences an empty font list).
- Vulkan conventions differ from the OpenGL path: NDC Y is flipped (negate
  `proj[1][1]`) and depth is [0,1] (`far/(near-far)`,
  `near*far/(near-far)`, `-1` in the last row), including the shadow ortho
  (`1/(near-far)`, `near/(near-far)`) and the shadow lookup
  (`vec3(sc.xy/sc.w*0.5+0.5, sc.z/sc.w)` - do not bias z by 0.5).
- A mesh's descriptor set is allocated once at initialisation; `CreateMesh`
  resets the mesh struct, so preserve that handle across the reset (wiping it
  crashes the first descriptor write).
- **Render passes must be created after the surface format is negotiated.**
  The main pass declares its colour attachment with `g_vk.swapchainFormat`; if
  that is still `VK_FORMAT_UNDEFINED` the framebuffer mismatches and *every*
  draw in the pass is silently discarded (no validation-free symptom other than
  a cleared frame). `QuerySwapchainFormat()` therefore runs before
  `CreateRenderPasses()`. Enable the Khronos validation layer
  (`VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation`, `VK_LAYER_PATH=<SDK>\Bin`)
  when touching the Vulkan path - it names this class of bug directly.
- `PsyX_Vk_ReadbackRgba` returns tightly packed **top-down** RGBA8 (row 0 is
  the top of the presented image, matching the swapchain image and SDL
  surfaces). Do not flip rows: `SDL_SaveBMP` already writes a bottom-up BMP
  and flips the flat data, so an extra flip produces an upside-down file.
- Regenerate SPIR-V with `pwsh -NoProfile -File scripts/compile_vk_shaders.ps1`
  after editing `vk_shaders/*.glsl`; the script needs a glslang build
  (Vulkan SDK `glslangValidator` or the standalone `glslang.exe` in
  `dependencies/vulkan-tools`).

## Porting the emulated PSX GPU path to Vulkan

The PSX path in `PsyX_Vk.cpp` (VRAM image, RG8 table, `psx.vert`/`psx.frag`,
`PsyX_Vk_Game*`) reproduces the OpenGL PSX shaders, so the details below are
load-bearing:

- The CPU VRAM mirror is 1024x512 little-endian `unsigned short` with row stride
  1024. GL uploads those bytes as `GL_RG`/`GL_UNSIGNED_BYTE` into an `RG32F`
  texture, so the Vulkan image is `R32G32_SFLOAT` with
  `R = (word & 0xFF)/255`, `G = (word >> 8)/255`. Uploading the 16-bit word any
  other way breaks every CLUT lookup without an obvious failure.
- **The RG8 table sampler must clamp, not wrap.** The shader samples
  `rg - c_LUTTexel * 0.0001`, which is slightly negative for a zero byte; with
  `VK_SAMPLER_ADDRESS_MODE_REPEAT` a zero high byte samples the *last* row
  (e.g. a red PSX word comes out magenta). GL sets `GL_CLAMP_TO_EDGE` on the same
  table. The VRAM sampler stays `REPEAT` + `NEAREST`, matching GL's defaults.
- `discard` is compiled with `--target-env vulkan1.1` so glslang emits `OpKill`
  instead of `OpDemoteToHelperInvocation`; otherwise the SPIR-V requires the
  `shaderDemoteToHelperInvocation` feature, which is not enabled and shows up as
  `VUID-VkShaderModuleCreateInfo-pCode-08740` under the validation layer. The
  PSX shaders compute their own filtering with `fract`/`floor` and use no
  derivatives, so the two have the same meaning here.
- The blend modes map to fixed pipeline state: `BM_NONE` has blending off
  (depth on for PGXP-Z), `BM_AVERAGE` is src-alpha/one-minus-src-alpha,
  `BM_ADD` is one/one, `BM_SUBTRACT` is `VK_BLEND_OP_REVERSE_SUBTRACT` on colour
  only (Vulkan has no reverse subtract for alpha), and `BM_ADD_QUATER_SOURCE`
  uses `CONSTANT_ALPHA` with `vkCmdSetBlendConstants(0.5,...)`, which is why
  `VK_DYNAMIC_STATE_BLEND_CONSTANTS` is in the PSX dynamic state list.
- Verify with `-vkpsxtest`, which writes synthetic VRAM patterns and asserts the
  readback pixels (`Assets/.../REDRIVER2_dev.exe -vkpsxtest`). It catches decode
  regressions that a screenshot would not: the 4-bit case puts a distinct colour
  in CLUT entry 5 so a wrong nibble index or CLUT row fails instead of looking
  plausible.

See [Track project PsyCross changes in the fork](psycross-fork.md) and the
[renderer modernization discussion](../discussions/renderer-modernization/index.md).
