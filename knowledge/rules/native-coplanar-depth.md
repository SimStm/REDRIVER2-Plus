---
type: Rule
title: Preserve native coordinate precision and prove coplanar ownership
created: 2026-10-07
tags: [okf, rendering, native, depth]
---

# Preserve native coordinate precision and prove coplanar ownership

**When** composing native city transforms or resolving overlapping surfaces,
**then** preserve world-coordinate precision and establish ownership from actual
source geometry and GPU depth/pixel evidence.

- Cancel large model/camera translations in view/model space before projection;
  retain double CPU intermediates until the final float shader matrix. Do not
  use false per-instance depth separation as an ordering policy. Respect supplied
  view matrices and the explicit translation-free backdrop contract.
- Distinguish authored within-model paint layers from cross-model source overlaps.
  Layer classification must be bounded and geometry-based, with atomic rejection;
  neither a texture name nor old PSX ordering proves native depth ownership.
- Validate shared source-world coordinates, actual post-VS/depth planes, colour/
  ID agreement, holes, nearer occlusion, both draw orders and resource retirement.
  Correct matrix algebra or a passing fixture does not accept visible city seams.
- If accurate coplanar planes reveal source-overlap z-fighting, keep that surface
  gate pending until a native ownership policy passes its actual pixels. Do not
  recover an image through old drawing or conceal it with inaccurate transforms.
- Exact horizontal source planes use one frame-wide inverse clip transform and
  world height to derive fragment depth. Equal heights must produce identical
  coefficients across origins/triangulation; ordinary physical ties belong to the
  last submitted native face. Authored paint retains its bounded D32 layer bias.
  Preserve alpha, physical nearer occlusion and logical IDs in the same draw.
  Invalid inversion/coefficients are explicit native errors; pitched/nonplanar
  geometry and edge-on planes retain ordinary native raster depth.
- Classify horizontal planes from referenced indexed triangles, including tops
  in mixed-height/material meshes. Preserve consecutive triangle order and one
  logical ID; bound physical draw counts and owned metadata before allocation/
  publication. Retire metadata with its mesh fence. A top/side split is not proof
  of vertical-side pixels or all city surfaces.
- Pixel oracles must clip triangles to Vulkan's homogeneous volume and respect
  actual culling before predicting ownership. Exclude declared boundary/occlusion
  samples, publish the compared count, and inspect unchanged source artwork/UVs
  separately from depth artifacts. Never accept the entire region from one mask.

Evidence: [R03 native terrain validation](../rendering/r03-terrain-validation.md)
and [mixed-height validation](../rendering/r03-mixed-height-validation.md), with
source transforms, CPU bounds/fences, GPU fixtures and actual city pixels.
