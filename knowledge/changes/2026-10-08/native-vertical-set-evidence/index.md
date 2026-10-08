---
type: Change
title: Expanded native static surface evidence
created: 2026-10-08
tags: [okf, rendering, vulkan, validation]
---

# Expanded native static surface evidence

Existing RenderDoc1.41 normal/wide captures now verify ten more visible parts:
709948 source-colour/ID/D32,12136 opaque occlusion and155912 outside-footprint
crop comparisons PASS. Twenty source buffer/index-slice/full-mip bindings match.

Initial815 failures identify missing within-draw occlusion in the independent
oracle. Nearest-depth selection and propagation of the existing uncertain
foreground-edge band correct that oracle; failed receipts are preserved.
Triangle-order reversal gives identical results; matrix/depth corruption fails.
504 raster/65 depth-ambiguous pixels stay excluded. No production code/settings/
build/game/GPU fixture change, full R03 acceptance or R04 advancement is claimed.

[Detailed evidence](../../../rendering/r03-vertical-static-validation.md#additional-ten-part-coverage-2026-10-08).
Mandatory OpenGL tracking: VK035.
