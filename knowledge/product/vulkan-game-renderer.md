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
  the progress bar.
- **Clip space.** GL-style matrices are converted per vertex (`y = -y`,
  `z = (z + w) * 0.5`); the viewport and scissor map GL's bottom-left origin to
  Vulkan's top-left.
- **sRGB.** The PSX shader is display-referred and inverse-encodes for the sRGB
  swapchain so the hardware store does not double-encode; the offscreen target is
  UNORM and stays raw.

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
  The `D32_SFLOAT` stencil-less depth fallback is compiled but unexercised on
  NVIDIA hardware that always exposes a combined format.
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
