---
type: Rule
title: Use the new drawable extent across Vulkan resize
description: Render targets and validation producers must agree on the next swapchain size.
tags: [okf, rendering, vulkan, validation]
---

# Use the new drawable extent across Vulkan resize

**When** adding size-dependent resources to `CreateSwapchain`, **then** pass its
resolved new width/height into allocation helpers. The owner's cached dimensions
are assigned later and still describe the previous swapchain during allocation.
Recreate attachments/framebuffers only after the owner's completion wait.

**When** a GPU fixture emits legacy viewport/scissor state before `RenderFrame`
processes SDL resize events, **then** sample the current drawable dimensions
for that producer. Bottom-left legacy scissors are converted using the new
height during recording; using the old height can produce a negative Vulkan Y.

Validate native world **and UI** pixels after shrinking and restoring, and inspect
actual Khronos validation messages separately from RenderDoc instrumentation.
Readback PASS alone can hide an invalid scissor. R01's800→1280 tests exposed
both defects; see [executed evidence](../rendering/r01-validation.md).
