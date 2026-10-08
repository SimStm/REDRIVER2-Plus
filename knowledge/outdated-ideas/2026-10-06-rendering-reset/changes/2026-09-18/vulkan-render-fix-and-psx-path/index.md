---
type: Change
title: Fix the Vulkan fixture's discarded draws and start the emulated PSX game path on Vulkan
description: Root cause and fix for the native-Vulkan backend rendering nothing in its main render pass, plus the first verified slice of the game's PSX GPU path (VRAM, RG8 table, PSX shaders, blend pipelines and a pixel-assertion test).
tags: [okf, rendering, psycross, vulkan, psx, roadmap-14]
---

# Fix the Vulkan fixture's discarded draws and start the emulated PSX game path on Vulkan

## Context

Roadmap item 14 (renderer modernization) had a native-Vulkan backend for the
modern scene that initialised, presented, resized, read back and drew its ImGui
overlay, but nothing recorded in the main render pass rasterised: the render
pass clear and the framebuffer readback worked, so the window showed only the
clear colour. The user installed the Vulkan SDK so the Khronos validation layer
could be used to diagnose it. The same objective also asks for the *game* to be
rendered by Vulkan instead of OpenGL, keeping the OpenGL path working until
parity is proven.

## Root cause of the missing draws

The validation layer named it immediately:

```
vkCreateRenderPass(): pCreateInfo->pAttachments[0].format is VK_FORMAT_UNDEFINED
```

`CreateRenderPasses()` ran before `CreateSwapchain()`, so the main pass
captured `g_vk.swapchainFormat` while it was still undefined. A pass with an
undefined colour attachment still accepts the framebuffer created afterwards
(with a format mismatch warning), records every draw, and then discards all of
them - exactly the reported symptom. `QuerySwapchainFormat()` now negotiates the
surface format before `CreateRenderPasses()`, and `CreateSwapchain()` reuses it.

A second defect surfaced while verifying the first: `PsyX_Vk_ReadbackRgba`
flipped the rows even though it documents a top-left origin, so `-vkshot`
produced upside-down BMPs. The flip was removed; the ImGui backend already
submits a positive viewport, so the framebuffer was always upright.

## Decision: build the game path in three phases

The game drives the emulated PSX GPU through the `GR_*` seam
(`PsyX_render.h`). The port is planned as (1) VRAM, RG8 table, PSX shaders and
pipelines verified by pixel assertions; (2) the `GR_*` state machine and the
game window; (3) offscreen render targets, the framebuffer-to-VRAM store,
stencil, save/load VRAM and the FMV shader. Phase 1 avoids running the game
while the decode maths is the highest-risk part.

## What shipped in phase 1

- `vk_shaders/psx.vert` reproduces the GTE vertex path: packed `GrVertex` with
  PGXP, `a_zw.y > 100` selecting the 3D path with the per-vertex offset matrix,
  `v_z`, and the GL-to-Vulkan clip conversion (`y = -y`,
  `z = (z + w) * 0.5`).
- `vk_shaders/psx.frag` merges the 4/8/16/32-bit fragment programs with the
  CLUT lookup, texture window, in-shader dither and bilinear filter.
  `scripts/compile_vk_shaders.ps1` takes a per-shader target environment and
  compiles this one for `vulkan1.1` so `discard` becomes `OpKill` rather than
  requiring `shaderDemoteToHelperInvocation`.
- `PsyX_Vk_Game*` (`PsyX_Vk.cpp`, `PsyX_vk.h`) models VRAM as
  `R32G32_SFLOAT` with `R = low byte / 255`, `G = high byte / 255` - the same
  mirror GL uploads through `GL_RG`/`GL_UNSIGNED_BYTE` into an `RG32F` texture -
  builds the 256x256 RG8 table locally, and creates five blend pipelines plus a
  no-depth variant. `BM_SUBTRACT` is reverse subtract on colour only and
  `BM_ADD_QUATER_SOURCE` uses `CONSTANT_ALPHA` with
  `VK_DYNAMIC_STATE_BLEND_CONSTANTS`.
- `REDRIVER2_dev.exe -vkpsxtest` renders synthetic PSX quads and asserts the
  readback pixels.

## Evidence

- `-vkpsxtest` with `VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation`: a 16-bit
  `0x001F` word reads back `(252,0,0,255)` and a 4-bit texture whose nibbles are
  all 5 with CLUT entry 5 blue reads back `(0,0,252,255)`; report `PASS`, zero
  validation errors. Both values are the sRGB-encoded `248` the OpenGL shader
  maths produces. The 4-bit case fails if the nibble extraction, CLUT row
  address or RG8 row is wrong.
- The modern Vulkan fixture still renders 12 meshes / 12 draws with zero
  validation errors.
- The OpenGL game regression still reports
  `Baseline: legacyVertices=2040 legacyDrawSplits=31 modernMeshes=12
  modernVerts=18915 modernCalls=12 legacyShadowPass=1`.
- Build `Release_dev|x64`: `FailedProjects=0`.

## Known limits

Phase 1 covers the 16-bit and 4-bit paths only; 8-bit textures, the texture
window, bilinear filtering, offscreen passes, VRAM feedback and the game window
itself are later phases. The game still renders through OpenGL.
