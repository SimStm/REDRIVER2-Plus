---
type: Change
title: Source-confirmed FP32 viewport edge validation
created: 2026-10-08
tags: [okf, rendering, validation, tooling]
---

# Source-confirmed FP32 viewport edge validation

The prior779 failure came from the oracle's FP64 viewport calculation. FP32
intermediates change one vertex's snapped x by one subpixel, correctly predicting
preserved sky at1125,397.22 part/views pass886209 source-colour/ID/D32 and185206
outside-footprint samples with no raster exclusions. Source/post coverage agrees;
three copied sky-pixel corruptions are detected, order reversal is identical and
original hashes stay intact. Prior failed receipts remain available.

No production renderer change or universal arithmetic claim.66 depth ambiguities,
other surfaces/full region/cost and R06/R10 migration remain pending. Existing1.41
exports suffice; no build/game/new capture/replay/GPU fixture was run. Routine
checks use direct commands/local analysis; VS remains available for debugging.

[Detailed executed evidence](../../../rendering/r03-vertical-static-validation.md#fp32-viewport-model-resolves779-coverage-2026-10-08).
Mandatory OpenGL tracking: VK038. R03 incomplete; R04 unstarted.
