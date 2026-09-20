---
type: Change
title: Restore Vulkan UI primitive and stencil state
description: Follow-up after repeat tests disproved the previous parity claim.
tags: [rendering, vulkan, ui, validation]
---

# Restore Vulkan UI primitive and stencil state

Fork commit `d9d8628` fixes white primitive selection, decoded colour/alpha,
stencil initialization and masks, depth-disabled stencil pipelines, resumed
render-pass compatibility and PSX display-space blend state. The game self-test
now exercises these paths through the actual GR texture bridge.

The SDK validation layer was already installed and automatically enabled.
Its incompatible-render-pass error was real and corrected. The inspected
subsequent game run had no validation error/VUID. Windows builds and the expanded
PSX self-test passed; controlled HUD and natural shutter captures confirm the
reported elements, with small pixel differences rather than exact equality.

The earlier claim that disabled GL depth testing still writes depth was wrong.
Product rules and cumulative/handoff records are corrected. LOAD still preserves
individual swapchain images and does not establish arbitrary prior-frame history.
No game drawing code or gameplay changes remain; temporary capture probes were
removed. See [the product contract](../../../product/vulkan-ui-image-parity.md)
and [handoff](../../../RECENT_CONTEXT.md) for exact validation and limitations.

A subsequent enhanced-path run exposed depth/stencil barrier VUID 03320 and
missing destination layout VUID 09600. Follow-up commit `647ea0b` fixes combined
aspect transitions and destination preparation. Its Windows build and inspected
modern-mesh/shadow run passed with the layer active and no validation error/VUID.
Both commits were pushed to the fork; the parent gitlink points to 647ea0b.
