---
type: Change
title: Source-bounded native raster-depth trace validation
created: 2026-10-08
tags: [okf, rendering, validation, tooling]
---

# Source-bounded native raster-depth trace validation

Explicit source-selected fragment traces verify34 formerly uncertain decisions:
20 new owners/14 preserved and three exact D32 ties. Reflection mappings,
completed shader outputs, exact D32 bits, source error bounds and actual buffers/
comparison/effective-zero-bias state match. Seven trace/comparison controls reject.
The22-view source/artwork oracle passes886246 colour/ID/D32/12181 occlusions with
zero exclusions. This supplements source geometry with GPU raster-input ownership;
it does not establish an exact independent offline raster model.

No production renderer change, new build/game/capture/GPU fixture. Configured1.41
local replay final phases0.858s/1.706s; initial bias-disabled assertion failed
because zero bias is enabled. Four warnings/earlier failed receipts retained.
Other surfaces, clipped samples/full region/silhouette/cost and R06/R10 consumers
remain pending. R03 incomplete/R04 unstarted. Mandatory tracking VK040.

[Detailed executed evidence](../../../rendering/r03-raster-depth-trace-validation.md).
