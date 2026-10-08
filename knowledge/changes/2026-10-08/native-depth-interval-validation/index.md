---
type: Change
title: Bounded native source-depth and attachment validation
created: 2026-10-08
tags: [okf, rendering, validation, tooling]
---

# Bounded native source-depth and attachment validation

Using the existing5e-7 D32 error gate as a conservative visibility interval
resolves32 of66 prior depth exclusions.22 part/views pass886226 source-colour/
ID/D32 and12167 occlusion checks.34 source winners remain ambiguous; their
20 new-owner/14 preserved attachments separately pass integrity checks.
Eight copied colour/ID/one-ULP/visible/occluded corruptions fail exactly once;
triangle reversal is identical and originals stay unchanged.

The first NumPy FP32-array bound calculation quantizes5e-7 to4.768e-7; explicit
FP64 interval arithmetic restores the declared bound. Preserve its initial
receipt and the34 unresolved decisions. No production renderer change, relaxed
pixel gate or full R03 acceptance. Existing1.41 exports suffice; no build/game/
new capture/replay/GPU fixture was run. Next obtain a tighter independent depth
reference and continue other surfaces/region/cost. R06/R10 consumers remain.

[Detailed executed evidence](../../../rendering/r03-vertical-static-validation.md#bounded-depth-decision-and-retained-ownership-ambiguity-2026-10-08).
Mandatory OpenGL tracking: VK039. R03 incomplete; R04 unstarted.
