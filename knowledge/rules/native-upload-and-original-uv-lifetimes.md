---
type: Rule
title: Preserve submitted upload resources and original atlas UV coverage
created: 2026-10-07
tags: [okf, rendering, vulkan, assets]
---

# Submitted uploads

**When** a native upload has been submitted and its wait fails, **then** keep
its command, staging buffer/memory, destination image and fence owned until
completion or device loss is confirmed. A prior ordinary frame fence cannot
prove completion of an upload submitted after that frame. Keep retirement
ownership charged while completion is unknown; never clear live handles to
make an error path look clean. Check shutdown device-idle results as well.

Evidence: R02's real-submission conservative wait-failure GPU test confirms
legacy-world pixels/draws, retained handles, actual fence recovery and zero
remaining owned bytes. Successful upload cycles alone did not expose the bug.

**When** a scene reset interrupts resource creation, **then** reject the whole
transaction and retire any new handles it created. Do not infer that reset
already retired handles created after the reset. Ordinary cached entries from
the old scene may be dropped after reset; interrupted transactions must still
validate their individual generational handles during rollback. A debugger-
forced reset at `CreateMaterial` exposed two orphan road parts; the CPU regression
and repeated actual Vulkan run restore four resources and the original budgets.

# Original texture UVs

**When** adapting original MODEL textures, **then** preserve the UV byte range
even if it exceeds the named inspector texture rectangle. Expand the original
VRAM crop to contain every referenced texel. Active PNG overrides keep the
declared texture rectangle and explicit clamp sampling, matching the existing
override path. Do not silently clamp original atlas neighbours or reject valid
complete geometry because inspector metadata was treated as a UV bound.

Evidence: Chicago model1180, page0/index28 declares `(208,128,24,62)` but its
source UV centres extend to239.5, requiring32 original atlas texels. The active
PNG is384x992. Decoder/cache tests cover expansion, invalid UV atomic rejection
and override overscan without mutation of source UVs. Model1180 is a traffic-light
resource as observed visually; polygon count is not a category classifier.
