---
type: Change
title: Complete clipped605 source-pixel and canonical-tie validation
created: 2026-10-08
tags: [okf, rendering, validation, tooling]
---

# Complete clipped605 source-pixel and canonical-tie validation

Complete605 horizontal footprint passes212377 source-colour/ID/D32 and117047
outside-footprint samples with0 exclusions, resolving125 prior exclusions.
Initial three edge failures are preserved;24 source/post/clip/viewport variants
establish the tested device-scoped numerical model, consistent with guard-band-
style rasterization. Actual driver clipper/bounds remain unmeasured.

74 coplanar501/605 ties match captured plane coefficients/shader hashes and
17168 completed shader steps. Seven pixel/four trace controls reject; triangle
order reversal preserves semantic rows and original hashes stay intact.
22 prior vertical cases also pass separate multiply/add viewport arithmetic.
Existing1.41 captures suffice; no runtime/build/game/settings/new capture/GPU
fixture change/run. SDK phases10.177s/0.041s are analysis timings, not GPU cost.

[Executed evidence](../../../rendering/r03-clipped-static-validation.md#complete605-footprint-and-canonical-ties-2026-10-08).
Sky/other surfaces/full region/silhouette/cost and R06/R10 consumers remain
pending. R03 incomplete/R04 unstarted. Mandatory OpenGL tracking VK041.
