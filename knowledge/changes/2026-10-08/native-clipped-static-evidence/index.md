---
type: Change
title: Native clipped static surface evidence
created: 2026-10-08
tags: [okf, rendering, vulkan, validation]
---

# Native clipped static surface evidence

Part605 was explicitly excluded from the west road oracle because of negative
clip-w. A new private source oracle clips geometry/attributes against all six
Vulkan planes before perspective sampling and verifies captured culling.

Actual configured RenderDoc1.41 replays of existing normal/wide captures and
offline checks pass212,270 source-colour/ID/D32 samples and1,235 eligible
neighbour-band samples. Another117,029 pixels outside the clipped footprint
within exported crops preserve all three attachments exactly. A copied one-byte
pixel corruption is rejected; original export hashes remain unchanged.

This is validation/documentation work, with no production renderer, resource,
shader, synchronization, build, settings or OpenGL change. No new game run or
GPU fixture is claimed.125 raster/depth-uncertain pixels, vertical ranges, full
region/silhouettes and GPU cost remain pending. Four HIGH replay warnings and
auxiliary baseline failures are retained. R03 stays incomplete/R04 unstarted.

Detailed gates, commands/artifacts and limits:
[source/pixel evidence](../../../rendering/r03-clipped-static-validation.md).
Mandatory OpenGL tracking: VK033.
