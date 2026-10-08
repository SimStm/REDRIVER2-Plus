---
type: Change
title: Native renderer bootstrap R01
date: 2026-10-07
tags: [okf, rendering, vulkan, native]
---

# Native renderer bootstrap R01

Implemented an opt-in analytic indexed native world inside the existing Vulkan
owner, with copied C descriptors, bounded generational resources, fence-controlled
upload/retirement, final original-game camera and conventional independent D32.
Original VRAM work, HUD/minimap and ImGui remain composed through their existing
paths; unsupported or incomplete native frames restore the complete legacy world.

The [product](../../../product/native-renderer-bootstrap.md) describes the shipped
surface; [executed evidence](../../../rendering/r01-validation.md) records accepted
Windows gates, recipes, actual tests and the bounded capture-layer limitation.
Source categories remain unmigrated. Original data/installations were not edited.
R02 original resource/material decoding remains pending.

PsyCross fork `99fa6418e03c95f4db71e07c2f09adf554669f51` is committed/pushed and
the parent gitlink staged. Five test INIs and original debugger settings were
restored byte-exactly. Resource extent failures found by actual resize/UI readback
and direct validation produced the
[extent rule](../../../rules/vulkan-resize-extents.md).
The unscheduled [VK-008 backlog](../../../roadmap/planned/port-vulkan-to-opengl-compatibility.md)
records the future GL port; existing GL city fallback was tested.
