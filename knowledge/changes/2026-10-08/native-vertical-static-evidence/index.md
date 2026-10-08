---
type: Change
title: Native static vertical face and culling validation
created: 2026-10-08
tags: [okf, rendering, vulkan, validation]
---

# Native static vertical face and culling validation

Actual1.41 replays prove part605's side range is back-facing in both tested
views, with151073 crop pixels exactly unchanged. A separate visible vertical
source face, part762, passes175832 source-colour/ID/D32 and29158 outside-
footprint crop preservation checks.78 raster pixels remain excluded.

Source/post-VS arithmetic is checked with per-component FP32 dot-product bounds
after an inappropriate copied absolute threshold rejected the initial attempt.
Pixel/depth tolerances remain unchanged. Five separate copied colour/ID/depth/
matrix/culled-side mutations are rejected; original exports remain hash-identical.
Source vertices, complete indices and all mip hashes match prior bindings.

No production renderer/resource/backend/settings change or new build/game/GPU
fixture is claimed. Other vertical surfaces, excluded edges, full R03 coverage,
HIGH replay warnings and GPU cost stay pending. R04 is unstarted; retained R06/
R10 consumers stay intact. [Detailed evidence](../../../rendering/r03-vertical-static-validation.md).
Mandatory OpenGL compatibility record: VK034.
