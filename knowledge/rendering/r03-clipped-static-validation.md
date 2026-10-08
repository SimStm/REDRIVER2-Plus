---
type: Validation
title: R03 clipped static surface source and pixel evidence
created: 2026-10-08
tags: [okf, rendering, vulkan, validation]
---

# R03 clipped static surface source and pixel evidence

The previous [west road oracle](r03-west-road-validation.md) skipped part605
because one input corner has negative clip-w. That was a declared exclusion,
not proof that the surface is invisible. This check closes the interior scope
of its first horizontal draw range in the existing normal/HD and wide/original
RenderDoc1.41 captures. Vertical ranges and excluded pixels remain pending.

## Source and clipping

At event4193, part605 selects12 source indices/four triangles from its24-vertex
buffer. All selected local heights are17. Independent source-position/MVP
multiplication matches the12 expanded post-VS positions with maximum absolute
errors0.000030637 normal and0.000026081 wide, below the0.0001 gate.

Clip source positions and UV/colour attributes against all six homogeneous
Vulkan planes: `x+w`, `w-x`, `y+w`, `w-y`, `z`, `w-z`. Fan triangulation occurs
after clipping; perspective interpolation occurs after projection. Captured
back-face culling and `frontCCW=true` agree with the negative signed area of
the surviving triangles in a positive-height framebuffer. No triangle is
removed by culling. Normal clipped polygon corner counts are5/3/0/0, yielding
four triangles; wide counts3/3/3/4 yield five. Thus clipping changes coverage
differently in the two views despite identical indexed source geometry.

The independent colour oracle uses the unchanged original UVs, vertex tint and
all captured sRGB mip levels, with linear-light bilinear/trilinear filtering.
Normal has11 mip levels and wide7. It retains the existing two-byte RGB,
LOD +/-0.35, alpha0.005, raster distance>1/128 pixel and depth1e-6/5e-7 gates.
Expected depth comes from source clip geometry, independently of the captured
canonical-plane coefficients. Actual ID605 and D32 must agree for eligible
visible pixels; no legacy image supplies expected pixels.

## Actual results

- Normal:35,639 source-colour/ID/depth pixels and295 eligible neighbour-band
  pixels pass. Maximum compared depth error is3.621e-8.
- Wide:176,631 source-colour/ID/depth pixels and940 eligible neighbour-band
  pixels pass. Maximum compared depth error is6.668e-8.
- Outside the clipped footprint,18,894 normal and98,135 wide pixels inside
  the exported bounding rectangles preserve colour/ID/D32 exactly across this
  draw. No wrong colour, ownership, depth or outside-footprint change is found.
- Excluded raster bands contain16 normal and35 wide pixels. Another74 wide
  pixels are depth-ambiguous under the existing visibility gate. These125
  pixels are not accepted. Neither crop validates pixels elsewhere in the full
  attachment, nor following vertical draws.
- A separate copied normal export changes the red byte at1761,1001 from136
  to0. The same oracle exits1 and reports exactly one wrong colour sample;
  every original normal export file hash remains unchanged.

The local SDK exports complete in0.153s normal and0.110s wide, using the
configured1.41 module and explicit RTX3060Ti replay GPU. Offline NumPy checks
exit0 for both real exports; the deliberate negative control exits1. All
material mip hashes match the existing source export metadata. No new game
launch, capture, build or GPU fixture run is claimed. Previously observed four
HIGH replay warnings and failed auxiliary-baseline/combined fields remain open.

Executed replay entry point: `C:/Python314/python.exe
scripts/run_renderdoc_analysis.py`, with each existing
`native-west-road-{normal,wide}141_frame500.rdc`,
`--script src_rebuild/build/native-renderer-r03/export-clipped-605141.py`,
`--arg label=normal` or `label=wide`, `--gpu "RTX 3060 Ti"` and distinct
`--summary` paths. The exporter refuses to overwrite an existing output.
Executed offline entry point:
`C:/Users/simst/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe`,
first with `check-clipped-605141.py normal wide`, then
`test-clipped-605141.py`; both helper paths are relative to the private build
directory. The negative-control harness exits0 only after proving the nested
oracle's exit1 and original hash preservation.

Python syntax, twelve affected documents/182 local links, status caps349/349,
root/fork diff hygiene and six settings hashes pass. The fork stays clean and
no game process remains. The upstream8.0 tag/release baseline was reverified
against GitHub API (`b2d8857`,2026-07-02) before amending the changelog.

Private assets/helpers stay in ignored `src_rebuild/build/native-renderer-r03/`:
`export-clipped-605141.py`, `check-clipped-605141.py`, `test-clipped-605141.py`,
`clipped-605-{normal,wide,negative}141/`, the two SDK summaries and
`clipped-605-negative-control141.json`. Production source/backend/settings and
forkae83dd2 are unchanged. Compatibility record: VK033.

## Acceptance and next action

Only the first horizontal range's declared interior/source-colour/ID/depth and
outside-footprint crop scope is accepted. The full west gate remains unchecked.
Subsequent [vertical evidence](r03-vertical-static-validation.md) proves expected
part605 side culling and a visible part762 facade's declared interior scope.
Continue within R03 with other vertical ranges and excluded raster/depth pixels,
remaining HD tree silhouettes, complete region coverage, replay warnings and
GPU cost. R06 dynamic and R10 screen-space consumers remain migration debt.
R03 remains incomplete; R04 is unstarted.

## Complete605 footprint and canonical ties (2026-10-08)

The previous125 exclusions are now resolved for these two views:107 additional
source-colour/ID/D32 samples and18 additional outside-footprint preservation
checks. Complete first-horizontal-range footprint:35649 normal/176728 wide,
212377 colour/ID/D32 total,117047 outside-footprint crop checks,1262 neighbour-band
comparisons, zero exclusions and zero wrong samples. Maximum D32 error6.668e-8;
RGB2-byte, LOD+/-0.35, alpha and5e-7 depth-error gates are unchanged.

Initial exact-grid expansion fails three edge pixels: normal1673,1063 and
wide1677,782/1710,850. Preserve `fp32-full-pixel-check.json` in each original
export. Source and actual post-VS both exhibit the same result. A24-variant
coverage audit separates source/post positions, six-plane/depth-only polygon
clipping and FP32 add/multiply, multiply/add and fused viewport arithmetic.
Normal matches native coverage only when XY boundary intersections are deferred
and the viewport pixel domain is preserved. Wide matches only the separate
FP32 multiply-then-add viewport model; fused/add-then-multiply variants still
misclassify two pixels. The combined model predicts both native footprints
exactly from source and post-VS. All22 prior vertical part/view combinations
also pass with multiply/add, preserving their886209 colour/ID/D32 and12152
occlusion counts under their older uncertainty scope; that rerun does not replace
the subsequently completed trace-assisted vertical acceptance.

This is a device/frame-scoped numerical model consistent with guard-band-style
rasterization; actual internal guard-band bounds/clipper were not measured.
It does not prove Vulkan mandates these intermediate operations or exempt any
clip plane. Near/far clipping still precedes division; comparison is restricted
to viewport pixels, so XY visibility is retained. Context7 and the primary
[Khronos vertex post-processing source](https://github.com/KhronosGroup/Vulkan-Docs/blob/main/chapters/vertexpostproc.adoc)
were consulted; they describe the clipping/viewport contract, not this exact
driver implementation. Keep six-plane clipping for the conceptual contract;
a literal XY polygon intersection before quantization need not predict each
observed edge. No production clipping, viewport, shader or pipeline change.

Wide's74 remaining depth samples overlap501. Captured501/605 push coefficients
and complete fragment shader hashes match. Explicit source-selected primitive
traces complete17168 steps, verifying all74 equal shader/pre-D32 values and605
ownership under LESS_OR_EQUAL. Both source-compatible input raster depth and
canonical output depth are bounded; colour/pick/native D32 match. Bias is enabled
with zero effective values. The source/artwork oracle uses those verified tie
decisions only at these74 samples; no approximate offline winner claim.
SDK phases10.177s for traces and0.041s for equation/hash comparison, configured
RenderDoc1.41/RTX3060Ti. Four initial capture HIGH messages remain retained.

Copied formerly excluded covered pixel1845,874 corruptions in colour/ID/D32
are each rejected; normal outside pixel1673,1063 corruptions in those three
attachments are each rejected. Replacing a tied wide pixel1648,753 with its
prior-owner attachments fails source-colour/ID checks. Triangle-order reversal
preserves semantic rows; clipping-corner counts naturally reorder. Original
export hashes are unchanged. Four trace/comparison controls reject strict LESS,
one-ULP output-depth corruption, wrong primitive and a missing sample; original
trace hash is unchanged. No timeout occurred.

Executed NumPy-runtime helpers under ignored `src_rebuild/build/native-renderer-r03/`:
`check-clipped-full-605141.py` initially exits1; `inspect-clipped-raster-605141.py`,
`check-guardband-605141.py`, `check-vertical-mul-add141.py`,
`check-canonical-605-attachments141.py`, `check-complete-605141.py`,
`test-complete-605141.py`, `test-canonical-605-traces141.py` exit0.
Local runner scripts `export-canonical-605-traces141.py` and
`check-canonical-equation141.py` exit0 with distinct SDK summaries.
Receipts: `clipped-605-raster-variants141.json`,
`canonical-605-debug-{points,traces-wide}141.json`,
`canonical-501-605-equation141.json`, `canonical-605-attachment-check141.json`,
`complete-605-pixel-check141.json`, `complete-605-controls141.json`,
`canonical-605-trace-controls141.json` and per-export
`{guardband,traced-complete}-pixel-check.json`. Initial failure receipts retained.

No production/build/game/settings/new capture/GPU fixture change or run.
Forkae83dd2 unchanged. Next: remaining sky boundaries and other source surfaces,
full region/silhouette and measured GPU/resource cost. Four stored warnings and
R06/R10 migration debt remain active. R03 incomplete/R04 unstarted. VK041.

Hygiene: eleven affected documents/214 local links, ten helper syntax checks,
root/fork diff checks,349/349 status caps and six settings hashes PASS. Fork clean;
no game process remains. Upstream8.0/`b2d8857`/2026-07-02 release reverified through
GitHub tag/release API before updating the changelog.
