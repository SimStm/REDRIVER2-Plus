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

The two backends schedule work differently; queued data and pipeline state both
need explicit parity checks:

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
4. **Depth writes require depth testing.** Disabling GL_DEPTH_TEST disables
   depth writes even when glDepthMask remains true. Vulkan uses depthEnable
   together with the presence of a depth attachment. Earlier documentation
   claiming otherwise was incorrect; UI protection is provided by stencil.
5. **Stencil must match the GL state and survive pass splits.** Preserve the
   selected stencil capability through resource initialization. DrawPrim writes
   reference 1 with writeMask 0xff and ALWAYS comparison; other draws compare
   NOT_EQUAL with mask 0xff, KEEP on pass and REPLACE on stencil failure.
   Depth-disabled draws still need stencil. Both main and resumed passes store
   depth/stencil, clear them at frame start, and use compatible dependencies.
6. **White primitives are independent of VRAM.** The white texture sentinel is
   mapped to PSYX_VK_TEX_WHITE for PSX formats. Its fragment value mirrors GL's
   decoded 0xffff word: RGB 248/255, alpha 127/255. Pure RGBA keeps its path.
7. **Blend in display space.** Game mode prefers a UNORM swapchain format.
   Inverse gamma in the shader cannot undo destination decoding by an sRGB
   blend attachment. Match the GL blend constants, including constant alpha 0.5.
8. **Preservation has an explicit limitation.** LOAD_OP_LOAD preserves the
   acquired swapchain image, not necessarily the last presented frame. Current
   code does not copy the preceding frame. The partial-frame self-test checks
   its actual acquisition sequence only. Do not infer complete loading or
   transition parity from this test without runtime image evidence.

## Verification

`REDRIVER2_dev.exe -vkpsxtest` checks PSX textures/CLUT, offscreen resolve,
white primitives in all three PSX formats, all five blend modes, stencil through
two pass restarts, partial presentations and VRAM export. Inspect the log's
PASS/FAIL result: the current command-line dispatcher does not propagate failure
as a process exit code.

Khronos validation is installed and automatically enabled when discovered. The
resumed-pass compatibility error observed in this investigation was corrected;
the subsequently inspected game run had no validation error/VUID.

Compare Vulkan and OpenGL at the same spawn and state. For police flashing use
player_position_known=1 and felony above the threshold; CopsCanSeePlayer alone
does not trigger it. Freeze CameraCnt to compare the same blink phase, and set
bar positions explicitly if pauseflag freezes their updates. A breakpoint hit
in CloseShutters proves execution, not that its pixels rendered correctly.

The follow-up in fork commit d9d8628 has synthetic test evidence and controlled
GL/Vulkan HUD and natural CloseShutters comparisons. All reported elements
render; the images are not pixel-identical. See [the current handoff](../RECENT_CONTEXT.md)
for metrics, captures and persistence limitations. Historical completion claims
remain superseded by these narrower, measured results.
