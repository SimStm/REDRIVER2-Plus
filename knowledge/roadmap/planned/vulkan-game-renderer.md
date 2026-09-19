# Vulkan game renderer (R7b)

Status: planned

## Objective

Render the *game* through Vulkan instead of OpenGL. R7a delivered a Vulkan
backend for the modern fixture gallery (`PsyX_Vk.*`, `-vkfixture`), verified
with the Khronos validation layer. R7b takes the same device/swapchain and
reproduces the emulated PSX GPU path that today runs in `PsyX_render.cpp`
(OpenGL), so the game window itself is presented by Vulkan.

Keep the OpenGL renderer as the default until parity is proven; the Vulkan game
path must be selectable at runtime (proposed `-vulkan`) and must not change
behaviour when disabled.

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
   pipelines and depth state, and `-vkpsxtest` asserts readback pixels:
   16-bit `0x001F` reads back `(252,0,0,255)` and a 4-bit texture whose nibbles
   are all 5 with CLUT entry 5 blue reads back `(0,0,252,255)`, with zero
   Khronos validation errors. The 4-bit case fails if the nibble extraction,
   CLUT row address or RG8 table is wrong, so it is a real decode check rather
   than a smoke test. Not yet covered by this phase: 8-bit textures, the
   texture-window/bilinear paths, offscreen passes and VRAM feedback.
2. Game geometry: `-vulkan` mode that creates the game window with
   `SDL_WINDOW_VULKAN`, reuses the `PsyX_Vk` device/swapchain, maps the `GR_*`
   state to pipelines and draws the game's vertex stream. First target: the
   frontend/menu and the in-game scene with textures, depth and blends.
   Deferred in this phase: offscreen mirrors, the framebuffer-to-VRAM store,
   stencil masking.
3. Parity and rollout: offscreen RT, framebuffer store, stencil mode, save/
   load VRAM paths, FMV shader, then a regression pass over the playground and
   a representative original city with both backends, and a documented
   performance/limitation comparison.

## Implementation progress

Phase 2 (game geometry) is substantially done and the game window presents
through Vulkan with `-vulkan`; OpenGL remains the default. The `GR_*` contract
is mapped, and the in-game scene, HUD, minimap, offscreen state and the
framebuffer-to-VRAM store all run. The store is implemented as
`GR_VkMirrorFrameToVRAM` (top-down conversion, matching the GL CPU-mirror
result) and is consumed at the start of the next scene, so the sun lens flare's
`DR_MOVE`/`StoreImage` pipeline samples the frame. Still open in phase 2/3:
offscreen mirrors (`GR_SetOffscreenState` is a state no-op), stencil masking,
the save/load VRAM export paths and the FMV shader, plus the parity regression
pass and the documented performance comparison on both backends.

## Risks

- The PSX fragment shader is the highest-risk piece: CLUT lookup, texture
  window, dither and the shader-side bilinear filter must match GL exactly.
  Phase 1 exists to catch that early with pixel assertions.
- VRAM feedback (screen-to-VRAM store) is needed by effects that sample the
  framebuffer; getting the Y flip and 5551 packing wrong shows up as mirrored
  or wrong-coloured effects rather than obvious failures.
- Vulkan depth/Y conventions differ from GL; the conversion is per-vertex, so
  the modern mesh path already proves the approach but the PSX z path is
  custom and must be validated separately.
- A second backend doubles the surface that must keep working for Emscripten,
  Android and the PSX toolchain; guard the Vulkan game path so those targets
  are untouched.
