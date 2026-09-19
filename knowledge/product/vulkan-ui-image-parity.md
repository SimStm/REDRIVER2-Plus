---
type: Product
title: Vulkan UI and image parity
description: How the Vulkan backend reproduces OpenGL's flush-order and depth behaviour for PSX UI and image drawing.
tags: [product, rendering, vulkan, ui]
---

# Vulkan UI and image parity

This document describes the behaviour the Vulkan backend must reproduce for the
PSX UI and image primitives, and the rules a change to that path must keep. The
record of how it was achieved and verified is
[`knowledge/roadmap/done/vulkan-ui-image-parity.md`](../roadmap/done/vulkan-ui-image-parity.md).

## The contract

The OpenGL renderer is the reference implementation. The Vulkan backend may
differ in mechanism, never in result: the same frame must produce the same image
for every UI and image element, including those that stream textures mid-frame
(the overhead map being the extreme case).

The two backends schedule work differently, and that difference is the source of
every defect in this area:

- OpenGL executes each `DrawSync` flush **immediately**, so it renders against
  the VRAM and vertex data as they were at that moment.
- Vulkan records the whole frame's PSX draws at present time, so anything the
  game rewrites between flushes must be captured and replayed in order.

## Rules

1. **Vertex uploads append.** `PsyX_Vk_GameUpdateVertexBuffer` must never write
   from offset 0; a frame can flush several times (the overhead map flushes
   every 16 tiles) and each earlier flush's vertices must survive. The buffer
   keeps four flushes and every draw's `firstVertex` is its upload's base plus
   its own offset.
2. **VRAM writes are ordered, not flagged.** `GR_CopyVRAM` queues its rectangle
   together with the pixels (`PsyX_Vk_GameCopyVRAM`) and `RecordPsxDraws`
   replays them through `ApplyVramUploadsUpTo` before the first draw that must
   see them. Each draw captures a `vramGeneration`, so a draw sees exactly the
   writes that preceded its flush. The dirty-flag path (`GR_UpdateVRAM`) is for
   the whole-mirror writers only: `GR_ClearVRAM`, the RGBA framebuffer copy,
   the offscreen resolve and the presented-frame mirror.
3. **A transfer cannot be recorded inside a render pass.** A replayed write
   closes the main pass and reopens it with the same colour/depth attachments
   (`ResumeMainPass` -> `modernRenderPass`). Everything that survives that
   boundary must be declared to survive it - see rule 5.
4. **Depth writes do not follow the depth test.** OpenGL only toggles
   `GL_DEPTH_TEST` and never touches `glDepthMask`, so a PSX draw with the test
   disabled still **writes** depth. `depthWriteEnable` therefore follows the
   render pass's depth attachment, not `depthEnable`. This is what lets the 2D
   UI drawn early in the frame occlude the 3D scene drawn after it.
5. **Depth and stencil must store, not discard.** Both attachments use
   `VK_ATTACHMENT_STORE_OP_STORE`. With `DONT_CARE` the reopened pass loaded
   undefined depth, so the world painted over the overhead map and the HUD took
   the colour of whatever was behind it. Both are still **cleared** at the start
   of every frame, so the PSX mask bit cannot leak between frames.
6. **Framebuffer persistence is intentional.** The main pass colour attachment
   uses `LOAD_OP_LOAD` with a `PRESENT_SRC` initial layout and clears only via
   `vkCmdClearAttachments` when `GR_Clear` was requested. The loading path draws
   its art once and then only the progress bar, and `CloseShutters` accumulates
   bars across frames, so both depend on the previous frame surviving.

## Failure signatures

Use these to recognise a regression of the same class:

| Symptom | Likely rule |
| --- | --- |
| Streaming image (overhead map) shows scattered or stale content | 1, 2 |
| 2D UI disappears under the 3D scene; HUD colour shifts with the background | 4, 5 |
| Animated transition jumps instead of accumulating | 6 |
| Content vanishes only on frames that both stream VRAM and draw 3D geometry | 3, 5 |

## Verification

- `REDRIVER2_dev.exe -vkpsxtest` - Vulkan PSX readback (16-bit and 4-bit CLUT),
  offscreen resolve and VRAM export.
- `-opengl` vs default comparison on the same scene: the OpenGL capture is the
  acceptance image for any UI or image element under change.
- Force the state under test with the debugger
  (`gShowMap = 1`, `CopsCanSeePlayer = 1`,
  `car_data[0].felonyRating = 5000`) and capture the game window; see
  [`knowledge/RECENT_CONTEXT.md`](../RECENT_CONTEXT.md) for the working recipe
  and the environment caveats.

All of the above is Vulkan-only. OpenGL, Emscripten, Android and PSX are not
affected by these rules and must not be changed to satisfy them.
