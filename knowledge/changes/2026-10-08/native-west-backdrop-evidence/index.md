---
type: Change
title: Source-clipped native west backdrop and auxiliary-prefix evidence
created: 2026-10-08
tags: [okf, rendering, validation, tooling]
---

# Source-clipped native west backdrop and auxiliary-prefix evidence

The wide west sky's angular blue cap initially looked like missing backdrop.
Original sky single-UV bindings and captured1x1 RGB[140,165,206] explain it.
Independent six-plane source/MVP clipping, perspective artwork and raw colour
comparisons pass3861031 backdrop/795828 visible-sky samples. Every visible sky
pixel has source coverage; backdrop native IDs0/D32=1 throughout. A separate
negative-control copy with one blue byte+5 is rejected. Preserve the initial
oracle matrix-transpose failure; row interpretation then matches actual post-VS.

Auxiliary14/11 draws partition into4/1 wholly clipped PGXP batches containing
51 vertices/17 triangles, plus ten screen-space draws/342 vertices. Prefix
before/after colour is exactly unchanged. Exact game producer/batching cause
remains pending; the aggregate inspector count gate is not silently changed.

No production code, shader or resource contract changes, and no new game run,
build or direct fixture claim. The native west item/R03 remain incomplete:
attribute the PGXP prefix, reconcile consumers and validate negative-w geometry,
excluded edges, remaining silhouettes/region/cost. Documented rule and VK029
preserve deferred OpenGL independently. Baseline8.0/b2d8857 is reverified against
the official release/commit before updating Unreleased evidence notes.

See [executed evidence](../../../rendering/r03-west-road-validation.md),
[source validation rule](../../../rules/native-clip-and-backdrop-validation.md)
and [active stage](../../../rendering/stages/03-complete-static-region.md).
