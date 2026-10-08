---
type: Rule
title: Separate native sampling diagnosis from accuracy acceptance
created: 2026-10-07
tags: [okf, rendering, vulkan, validation]
---

# Native sampling validation

**When** an independent native colour oracle disagrees with GPU sampling,
**then** retain the failed comparison and isolate the cause before changing
the renderer or accepting a different accuracy bound.

- Check source texels, perspective UVs, vertex colour, filter/LOD, shader output
  and attachment encoding separately. Use the configured RenderDoc version;
  keep diagnostic replacements private and restore/free their resources.
- Constant-UV owned textures can reproduce hardware fractional filtering outside
  city interpolation and replay. Nearest checks, uniformity, filter consistency
  and lifetime are useful independent invariants, not ideal-colour acceptance.
- Coordinate/LOD precision limits and a conformance test's generous diagnostic
  allowance do not alone establish the project's final colour tolerance.
- Record the original tolerance and deviations. Do not widen it merely to pass
  current pixels; justify the applicable accuracy oracle and validation scope.
  A fixture reporting invariant PASS must explicitly retain unaccepted accuracy.
- Count queried draws, positive geometry footprints, opaque colour samples and
  occluded-only checks separately. Report excluded raster/alpha/LOD/depth pixels;
  submission, a bounding box or preserved occluded attachments cannot prove a
  visible silhouette. Keep actual per-part IDs and views in coverage receipts.
- Cross-capture resource IDs and event IDs are local identities. Compare complete
  resource-content records with duplicate multiplicity and ordered draw bindings;
  exclude only verified local identifiers, never hashes, dimensions or mip data.
  Retain an initial failed audit and its diagnosis instead of erasing evidence.

Evidence: [R03 original-tree sampling](../rendering/r03-cutout-validation.md).
The five filtered city differences reproduced directly on RTX3060Ti. Subsequent
native FP32 filtering passes the unchanged strict ideal gate, direct mip/clamp
regressions and40598 scoped city comparisons. Earlier failed evidence is retained;
other views/boundaries and GPU cost remain pending.
