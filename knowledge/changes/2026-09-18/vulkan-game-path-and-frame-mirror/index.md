---
type: Change
title: Render the game through Vulkan and mirror the presented frame into PSX VRAM
description: The player-visible PSX GPU path now runs on the native Vulkan backend behind -vulkan, with the minimap scissor fix, the framebuffer-to-VRAM store and the PsyCross fork migration.
tags: [okf, rendering, psycross, vulkan, psx, roadmap-14]
---

# Render the game through Vulkan and mirror the presented frame into PSX VRAM

## Context

Phase 1 of the Vulkan game renderer (R7b) verified the emulated PSX shaders with
pixel assertions. This change covers the phase that puts the actual game image on
the Vulkan backend, selectable with `-vulkan`, while OpenGL remains the default
(`Release_dev_gl` runs it without the flag). It also records the PsyCross
submodule moving to the project fork.

## What shipped

- The `GR_*` state machine is mapped onto `PsyX_Vk_Game*`: one vertex upload per
  frame, per-split state captured into a draw record, and the five blend
  pipelines. The game window is created with `SDL_WINDOW_VULKAN` and presented by
  the backend, with the developer overlay contributed inside the frame.
- Vertex capacity: `PSYX_VK_PSX_VERTEX_CAPACITY` was 256 KiB, i.e. 5,957
  vertices, while the game submits roughly 11,532 per frame. Draws past the
  capacity were truncated, which is what the earlier "mirrored lower half"
  symptom was. Raised to match the OpenGL `MAX_VERTEX_BUFFER_SIZE` (65,536).
- Texture slots: `PSYX_VK_PSX_MAX_TEXTURES` went from 64 to 512, clearing 28
  "out of PSX game textures" warnings.
- Scissor: `GR_SetupClipMode` supplies the clip rectangle in GL's bottom-left
  window space. `RecordPsxDraws` used it unchanged as a Vulkan top-left scissor,
  so every clipped element tested a vertically mirrored region and the minimap's
  63x60 bottom-right clip was discarded. The scissor Y is now mapped like the
  viewport (`height - (y + h)`).
- Framebuffer-to-VRAM store: `GR_StoreFrameBuffer` queues the display rect;
  `GR_VkMirrorFrameToVRAM` consumes it at the start of the next scene through
  `PsyX_Vk_TakeStoredFrameBuffer` and converts the top-down presented frame into
  5551 words at `(disp.x, disp.y)`, then re-uploads the CPU mirror. Orientation
  matches the GL path (the blit plus `glGetTexImage` is also top-down,
  `flip_y = 0`). This closes the sky lens flare's `DR_MOVE`/`StoreImage`
  pipeline, which samples the screen through VRAM.

## Fork migration

The submodule now tracks `git@github.com:SimStm/PsyCross.git` (origin) with
OpenDriver2 kept as `upstream`. `patches/psycross/` and
`scripts/apply_psycross_patches.ps1` were removed and the prepare scripts
initialise the submodule instead. Fork commits: `49f9578` (backend, panel hooks,
modern mesh, ImGui), `396dd20` (scissor fix), `9281fb7` (frame mirror). The parent
gitlink is at `9281fb7`.

## Evidence

- VS build `Release_dev|x64`: `FailedProjects=0`.
- Game launch under the debugger with `-vulkan`: Vulkan device
  `NVIDIA GeForce RTX 3060 Ti` API 1.4.341, swapchain 1280x720 sRGB, zero
  validation errors, log `Vulkan frame mirror: 1280x720 -> VRAM 0,0 320x256
  bgra=1`, and a screenshot with the car, HUD and minimap matching OpenGL.
- `Release_dev_gl` launch: OpenGL image matches Vulkan.
- `pwsh -NoProfile -File scripts/run_inspector_tests.ps1`: `AssetCatalogTests`
  145 checks / 0 failures, `InspectorExportTests` 104 checks passed.
- `git diff --check`: exit code 0.

## Known limits

`GR_SetOffscreenState` (mirrors) is still a state no-op, stencil masking is not
emulated, the save/load VRAM export paths are not ported and the FMV shader is
not verified. The frame mirror samples the whole window, including the developer
overlay, and scales it onto the PSX display rect. No macOS/MoltenVK build exists.
