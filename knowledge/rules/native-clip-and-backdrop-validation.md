---
type: Rule
title: Validate native backdrop against clipped source geometry and artwork
created: 2026-10-08
tags: [okf, rendering, vulkan, validation]
---

# Validate native backdrop against clipped source geometry and artwork

**When** native sky appears empty/uniform, or a triangle has negative-w corners,
**then** inspect source indexed geometry, UV/material bindings and actual MVP/
post-VS, and clip against all six homogeneous Vulkan planes before classifying
coverage. One negative-w vertex does not prove the whole triangle is invisible.

- Source UVs can intentionally sample one texel: Chicago's blue cap uses original
  single-UV sky geometry. Distinguish source colour from native clear pixels;
  never add replacement art/geometry merely from an apparent screenshot gap.
- RenderDoc constant matrices expose rows; packed CPU instance matrices may use
  columns. Confirm source-matrix multiplication against actual post-VS outputs
  before trusting raster/colour results. Reject a failed matrix gate explicitly.
- Validate perspective artwork/tint and native colour independently, preserving
  the strict declared colour/LOD/raster scope. Require ID0/D32=1 for backdrop and
  visible sky source coverage after world occlusion. Keep excluded edges pending.
- Use a separate copied export for deliberate pixel corruption; require the
  oracle to reject it and leave original capture/export bytes untouched.
- Auxiliary submission counts depend on batching and can include wholly clipped
  PGXP triangles. Check source/post-VS, before/after colour and native ID/depth;
  do not call all auxiliary draws HUD or accept an unknown game producer. Record
  exact attribution and migrate each retained consumer before retiring it.
- Match actual expanded vertex indices and float32/UV/RGBA payloads to registered
  producer identity; reject missing/ambiguous matches. Check actual texture state
  before inferring batch totals. Restore temporary probes and force recompilation
  if copy restoration preserves an older source timestamp.
- Keep fixed native inventory/index-partition checks separate from historical
  auxiliary batch baselines. Preserve existing combined-field semantics for
  report consumers; neither result accepts pixels, producer identity or migration.
- A culled surface with zero opaque comparisons only proves expected absence in
  that view. Test a source-confirmed visible face before accepting shading.
- Bound source/post-VS FP32 dot-product differences per component from actual
  operand magnitudes and arithmetic operations; do not generalize a small-model
  absolute threshold to larger coordinates. Keep pixel/depth gates unchanged
  and require a source-matrix mutation control to fail the arithmetic gate.
- Model occlusion inside an opaque multi-face draw using nearest source depth;
  triangle order alone cannot choose a hidden facade/roof's expected shading.
  Propagate nearer uncertain raster-edge coverage instead of accepting a farther
  face there. Retain the declared band, count exclusions and verify order reversal
  plus mutation controls. Confirm opacity before using this simplified oracle.

Evidence: [west source-road/backdrop validation](../rendering/r03-west-road-validation.md).
Also [vertical face/culling evidence](../rendering/r03-vertical-static-validation.md).

When exact edge coverage differs while source/post-VS positions agree, check
perspective division and viewport arithmetic before blaming the renderer. A
one-subpixel rounding change can reverse coverage. Verify a device-scoped model
against independent source/post footprints and native attachments; never treat
subPixelPrecisionBits alone as proof of FP32 intermediates or discard the pixel.
Require preservation corruption controls and retain previous failed receipts.
[Executed viewport evidence](../rendering/r03-vertical-static-validation.md#fp32-viewport-model-resolves779-coverage-2026-10-08).

For depth-uncertain source samples, separate uniquely bounded winner acceptance
from candidate attachment integrity. A valid new/preserved attachment does not
prove a unique source winner. Use the existing D32 error bound for conservative
visibility intervals; cast stored FP32 depth to FP64 before adding scalar bounds
so the declared interval is not silently quantized. Retain rounded-threshold
receipts, count unresolved ownership, and verify one-ULP/pixel corruption controls.
[Executed depth-interval evidence](../rendering/r03-vertical-static-validation.md#bounded-depth-decision-and-retained-ownership-ambiguity-2026-10-08).

When a source-depth interval cannot choose an owner, an explicit GPU fragment
trace can validate the actual native comparison: select every source-covered
primitive, resolve Position/DepthOutput through reflection/source mappings,
finish the trace, bound raster input against source, and compare exact output
D32/pick/colour or prior preservation. Test exact ties and corrupted trace inputs.
Do not present this GPU reference as an exact independent offline raster oracle.
Bias enable alone does not establish effective bias; check constant/slope/clamp.
[Executed trace evidence](../rendering/r03-raster-depth-trace-validation.md).

For quantized clipped-edge coverage, test viewport multiplication/addition order
and whether generating XY boundary vertices changes the observed footprint.
Keep the six-plane visibility contract; a tested guard-band-style numerical model
may defer XY polygon intersections while retaining the viewport pixel domain and
near/far clipping before division. Confirm source/post/native footprints and prior
cases; record unmeasured internal clipper/guard-band limits. Do not generalize a
matching device model as a mandated implementation or change runtime clipping
just to make an oracle pass. [Executed605 evidence](../rendering/r03-clipped-static-validation.md#complete605-footprint-and-canonical-ties-2026-10-08).

For complete backdrop checks, verify both windings when actual culling is off,
recompute source/post-VS coverage and retain near/far clipping plus viewport
visibility. Compare source-compatible artwork including former edge exclusions,
all backdrop ID/depth policy and exact outside-source pre-image preservation.
Uniform art cannot establish exact primitive ownership by colour alone. Require
copied formerly excluded colour/ID/depth and outside-source corruption controls,
original hashes, and actual replay texture/attachment/state equivalence.
[Executed two-view evidence](../rendering/r03-full-backdrop-validation.md).

For whole-view audits, export every ordered index range and verify reconstructed
full-buffer/mip hashes against the retained capture inventory. Observed ID
attribution can prove source-compatible geometry but not expected visibility or
nearest ownership; geometry-covered observed sky can still require source-alpha
and explicit discard traces. Inspect every covering primitive. Resolve instruction
locals through1.41 `trace.instInfo` lower-bound mappings, not shader-state fields;
use quantized source UV interpolation where verified and compare effective
clamped LOD. Validate sampled alpha from raw artwork and exact background/ID/depth
preservation. Keep source-selected final composition/filled-hole detection open.
[Executed complete-view evidence](../rendering/r03-complete-view-coverage-validation.md).

For complete-scene composition, generate owner/depth/artwork/alpha decisions from
all ordered source candidates before reading final native IDs. Keep competing
equations/primitive groups distinct and resolve canonical shared-equation bias/
order from actual shader/push state; a group's earlier representative must not
become its own competitor. Record uncertain alpha/depth as unaccepted pixels,
including provisional source/observed-ID differences. Require copied wrong
owner/colour/depth, missing-solid, filled-hole, exact sky and pipeline-state
controls; retain original hashes. A matching decided partition cannot accept
excluded pixels, original-source re-decoding, a whole region or GPU cost.
[Executed composition evidence](../rendering/r03-complete-composition-validation.md).

For excluded composition decisions, enumerate every source-covered primitive,
then trace explicit primitives rather than allowing DebugPixel or captured IDs
to choose a winner. Check reflection/instruction mappings, raw sampled artwork,
source colour/LOD/alpha/depth and all surviving/kill outputs. Compute expected
winners in actual depth/order before final attachments; require traced winner
depth bits and formerly excluded pixel/reference corruptions to fail correctly.
Do not generalize a tree-case UV tolerance to thin faces: investigate cancellation
and record source-compatibility bounds separately from exact/device interpolation
claims. Conservative bounds must not replace strict independent artwork/pixel
checks. Reset diagnostics per trace; retain initial failures and actual success
receipts. [Executed full two-view composition](../rendering/r03-composition-trace-validation.md).
