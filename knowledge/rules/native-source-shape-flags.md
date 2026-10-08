---
type: Rule
title: Derive native blending from the original draw policy
created: 2026-10-07
tags: [okf, rendering, native, assets]
---

# Derive native blending from the original draw policy

When adapting an original shape flag, derive blending from the actual draw
producer, not the flag's name.

`SHAPE_FLAG_TRANS` in `Game/engine/mdl.h` is not sufficient evidence for raster
transparency. `Game/C/bcollide.c` uses it during collision/smash sound processing. The native adapter
must inspect the actual producer/primitive policy: `Game/C/draw.c` uses the
separate `PLOT_TRANSPARENT` context to set the semitransparent primitive bit in
that path. Likewise, source STP/PNG alpha alone does not enable
original PSX blending; consult the active texture alpha rule.

Count unclassified source flags explicitly and report unsupported native policy
as an explicit failure until implemented/tested; never recover through old-world
drawing. Do not silently treat
their name as an alpha blend rule. R03's chosen region contains no such flags or
water; absence is an inventory fact, not proof of general transparency support.

Evidence: R03 source inspection and actual Chicago inventory on2026-10-07.
