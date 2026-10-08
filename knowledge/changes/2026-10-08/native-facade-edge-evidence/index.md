---
type: Change
title: Complete facade raster footprint and preserved edge failure
created: 2026-10-08
tags: [okf, rendering, validation]
---

# Complete facade raster footprint and preserved edge failure

Independent fixed-grid/top-left source prediction matches part762's complete
normal/wide native footprint.175892 source-colour/ID/D32 and29176 outside-
footprint crop checks PASS, with no exclusions and unchanged strict limits.
The previous78 facade raster exclusions are resolved for these two frames.
Shared-edge/order controls pass; four copied formerly excluded pixel corruptions
are rejected and original export hashes stay unchanged.

The broader ten-part/two-view exact-grid oracle exits1 for normal779 at1125,397:
source/post-VS snapped coordinates agree, but native attachments remain sky where
the model predicts coverage. Preserve the failure; it does not establish a
renderer defect until raster arithmetic is understood. No tolerance widening,
production change or R03/full-set acceptance. Other exclusions remain pending.

[Detailed results and next diagnosis](../../../rendering/r03-vertical-static-validation.md#complete-part762-raster-footprint-and-remaining-edge-failure-2026-10-08).
Mandatory OpenGL tracking: VK037.
