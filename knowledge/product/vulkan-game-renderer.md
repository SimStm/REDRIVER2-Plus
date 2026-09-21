---
type: Product
title: Vulkan game renderer
description: The native Vulkan backend that presents the game window, its selection, configuration, covered paths and known limits.
tags: [product, rendering, vulkan, psycross, backend]
---

# Vulkan game renderer

The desktop game window is presented by a native Vulkan backend implemented in
PsyCross. It is the **default** on desktop; OpenGL remains a first-class fallback
selected at runtime. This document is the operational product behaviour; the
delivery history and evidence are in
[`knowledge/roadmap/done/vulkan-game-renderer.md`](../roadmap/done/vulkan-game-renderer.md).

## Selecting the backend

- **Default:** desktop builds (`Windows`, `Linux`) initialise Vulkan with no
  arguments. If `PsyX_Vk_Initialise` fails (no usable device), the game falls
  back to OpenGL automatically.
- **OpenGL:** `-opengl` forces the OpenGL renderer.
- **Vulkan explicitly:** `-vulkan` is accepted for compatibility and matches the
  default.
- Both backends are the same binary; the choice is a runtime flag. In the Visual
  Studio solution, `Release_dev|x64` passes no arguments and `Release_dev_gl|x64`
  passes `-opengl` (`.vcxproj.user`, which is git-ignored).
- The emulated/PSX, Android and Emscripten targets keep OpenGL; Vulkan is
  compiled and initialised only for desktop.

## What runs through Vulkan

- The emulated PSX GPU path behind the `GR_*` seam: geometry, textures
  (4/8/16-bit CLUT and 32-bit RGBA with the in-shader CLUT, texture window,
  dither and bilinear filter), the five blend modes, depth, the PSX primitive
  mask bit (stencil), scissor and viewport.
- VRAM as an `R32G32_SFLOAT` image over the CPU mirror, the RG8 decode table,
  offscreen render-to-VRAM, and the framebuffer-to-VRAM mirror that feeds
  frame-reading effects (for example the sky lens flare).
- The in-game modern-mesh system (`PsyX_ModernMesh`), including its PBR shading,
  directional shadow casting and the legacy-geometry shadow receive term.
- The developer overlay (Dear ImGui): the graphics panel and its HD-texture
  override preview.
- The game window's presentation, resize, and the F12 screenshot (`SCREENSHOT.BMP`).

## Behaviour that intentionally mirrors OpenGL

- **Framebuffer persistence.** The main pass preserves the previous frame and
  clears only when the game asks for it (`GR_Clear`, i.e. `activeDrawEnv.isbg`).
  The loading screen depends on this: it draws its art once and then redraws only
  the progress bar. Persistence is **per swapchain image**: each acquired image
  keeps its own previous content, which is not the same as an explicit copy of
  the immediately previous frame. The game's partial-update paths redraw their
  base content on every image before relying on it (the loading art is presented
  for 32-64 frames, more than the swapchain depth), so no corruption has been
  observed; a future path that draws a base frame once and then partially
  updates more frames than the swapchain holds would need an explicit frame
  history. See
  [`changes/2026-09-20/vulkan-renderer-debt`](../changes/2026-09-20/vulkan-renderer-debt/index.md).
- **Clip space.** GL-style matrices are converted per vertex (`y = -y`,
  `z = (z + w) * 0.5`); the viewport and scissor map GL's bottom-left origin to
  Vulkan's top-left.
- **sRGB.** The PSX shader is display-referred and inverse-encodes for the sRGB
  swapchain so the hardware store does not double-encode; the offscreen target is
  UNORM and stays raw.

## Overlays and the modern pass

Single-view gameplay supplies `PsyX_SetModernSceneBoundary(current->ot + 10)`
after updating the modern camera/instances and before submitting the OT.
PsyCross records the exact vertex position, including when it falls inside a
state batch: legacy world first, lighting/shadow composite and modern meshes
second, final overlays last. Bucket 10 includes lens flare; fades at 8 and the
HUD, map and pause menu at 0..1 follow it. World sprites keep their original OT
order and depth policy.

Vulkan records two contiguous PSX draw ranges around the modern pass, retaining
VRAM upload generations across the boundary. The overlay range rebinds PSX
vertices/descriptors and can restart the loaded render pass for a VRAM transfer.
OpenGL composes immediately at the boundary and restores texture units 0..4,
active texture, program, VAO and the other saved draw state. End-of-frame
composition remains the fallback for callers without a boundary (including
the existing split-screen path).

Do not restore the previous always-pass 2D depth workaround. Screen-space
encoding does not identify an overlay, a batch can mix 2D and 3D vertices, and
a translucent panel needs the mesh colour beneath it before blending. The
previous workaround hid that colour and let background/effect rectangles
poison the world depth. See the
[regression record](../changes/2026-09-21/modern-overlay-composition/index.md).

## Configuration

- `developer_graphics.ini` holds the developer graphics settings (bilinear
  filtering, PGXP, VSync, draw distance, field of view, modern renderer, HD
  texture overrides); `developer_modern_mesh.ini` holds the modern-mesh gallery
  state. Both are separate from `config.ini`.
- `PSYX_PERF_LOG=1` writes per-frame samples to `psyx_perf.log`;
  `PSYX_VK_PRESENT_MODE` selects the present mode.
- `developer_debug_start.ini` places the player at a fixed spawn for repeatable
  captures.

## Limits and not validated

- FMV playback is out of scope on desktop (the game-era `nplay` symbols are
  absent from this tree).
- Animated-character/vehicle-deformation migration is a non-goal; moving
  entities render through the normal PSX stream.
- MoltenVK/macOS is wired in code and Premake but has not been built on a Mac.
  The `D32_SFLOAT` stencil-less depth fallback is exercised by setting
  `PSYX_VK_DEPTH_FORMAT=d32`; the self-test then reports
  `main depth format 126 stencil=0`, asserts that the PSX mask bit degrades to a
  no-op, and still passes every other check. The game renders normally under the
  forced fallback with the validation layer enabled and no VUID.
- Legacy shadow reception is implemented on both backends but is not yet
  equivalent: at a low sun in the modern gallery scene, Vulkan's receive term
  changed 7722 pixels of the sampled ground band versus OpenGL's 2264 (2257
  shared, IoU 29.2 %), and OpenGL additionally darkens the legacy tree canopy
  where Vulkan barely does. Recorded as an R5 follow-up, not a blocked feature.
- CPU/GPU frame-time distribution, peak memory and the Linux/web/Android builds
  have not been profiled. At the time of writing both backends present at the
  game's fixed 30 Hz PSX timestep (~30 FPS / 33.4 ms), so that equality shows
  neither backend is struggling, not its maximum throughput.

## Working on it

- Vulkan implementation: `src_rebuild/PsyCross/src/render/PsyX_Vk.cpp`; public
  declarations in `PsyCross/include/PsyX/PsyX_vk.h`.
- Game-modern shaders: `src_rebuild/PsyCross/src/render/vk_shaders/`. Regenerate
  the embedded SPIR-V (`PsyX_Vk_Shaders.h`) with
  `pwsh -NoProfile -File scripts/compile_vk_shaders.ps1`.
- The shared `ModernUBO` layout must be declared identically in
  `psx_modern.vert`, `psx_modern.frag` and `psx_composite.frag`; a missing field
  silently shifts every later member onto the wrong std140 offset and the meshes
  disappear.
- Self-test: `REDRIVER2_dev.exe -vkpsxtest` renders synthetic PSX quads and
  asserts the readback pixels and the VRAM export byte count.
- The modern fixtures are described in
  [`playable-testing-playground`](playable-testing-playground.md) and the
  modern-mesh bridge rule
  [`psyx-modern-mesh-bridge.md`](../rules/psyx-modern-mesh-bridge.md).
