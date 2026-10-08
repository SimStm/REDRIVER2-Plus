---
type: Evidence
title: R03 original tree cutout validation
status: partial
created: 2026-10-07
tags: [okf, rendering, vulkan, validation]
---

# R03 original tree cutout validation

R03 remains incomplete; the tree/surface acceptance gate is unchecked. This
milestone inspects the existing fork2341108 build using the user-configured
RenderDoc1.41 MCP/rdc tools. No runtime shader, gameplay, backend or asset changed.
It replays the two [previously captured frames](renderdoc-141-validation.md),
not newly launched game runs: normal focal256/HD and wide focal128/original art,
1920x1080, trilinear, fixed Chicago slice, frame500.

## Bound policy and resources

`scripts/inspect_native_capture.py` now records two-sided world parts, fragment
alpha cutoff, logical pick ID and depth-test state. Fresh-session normal/wide
runs both find44 such parts (22 trees, canopy/trunk), all cutoff0.5, depth test
enabled and world depth writes enabled. IDs708..751 map to the copied placements.
This is pipeline inventory, not pixel acceptance. Opaque materials also have an
alpha threshold, so threshold alone cannot classify tree draws. The first
inspector extension counted876 draws using that incorrect classifier; corrected
source-backed two-sided classification and fresh replays give44.

There are four bound tree artworks, complete mip views/linear filters/clamp-edge,
with seven/ten/seven/eleven levels normal versus seven/six/seven/seven wide.
Both full scenes retain80 backdrop/876 world/10 auxiliary draws, geometry148320
bytes, RGBA49926792/2174132 and unchanged colour/ID/depth hashes. Fresh-session
reports retain five HIGH initial barrier warnings and `validationClean=false`.
Reusing a previously stepped session clears these diagnostics; close/open before
inspection. An initial attempted open while active was rejected; final reports
were regenerated on the correct freshly opened captures.

## Independent source-alpha and depth comparison

Private exports retain original bound vertices/indices/artwork mips, actual
post-VS clips and before/after draw colour/R32_UINT/D32 crops. Six visible
canopy/trunk bounding boxes belong to three source trees, picks708/709,
734/735 and736/737. One tree is behind buildings; zero surviving opaque pixels
there is an occlusion observation, not a visible-colour pass. The other38 parts
are outside this camera's tested footprint and are not counted as pixel passes.

The offline oracle uses perspective source UV interpolation, bilinear/trilinear
source alpha, pixel centres and interior barycentrics>0.01. It excludes uncertain
edges/alpha thresholds/depth ties: alpha margin0.1 around cutoff0.5, LOD uncertainty
plus/minus0.35 (including crossed mip integers), potential visibility depth
margin1e-6 and surviving depth tolerance5e-7. These margins bound this test;
they do not establish every silhouette boundary or minified tree.

- Normal:82811 potentially visible holes retain colour/IDs/depth byte-exactly;
  486087 opaque visible pixels have the expected tree ID/depth;1984 opaque
  occluded pixels preserve all attachments. Zero mismatches in these masks.
- Wide:69769 holes,162460 opaque visible pixels and463 opaque occluded pixels,
  with the same checks and zero mismatches.

## Colour comparison remains unresolved

An additional independent 4x4-grid sample checks decoded source RGB filtered in
linear light, perspective vertex colour and shader sRGB encoding. The same LOD
envelope and fixed two-byte per-channel tolerance are used. Normal has five
failures among30458 samples; wide has zero among10140. The combined oracle exits1.
Do not widen tolerance to turn this into a pass.

The failures are on canopy708 at (276,472), (212,576), (236,576), (336,776),
(348,788). At the first pixel, ideal RGB is approximately(31.35,33.11,18.51),
captured RGB(30,31,17). RenderDoc shader debugging agrees with independently
interpolated UV within5.4e-7 and shows sampled linear RGB
(0.013313293,0.013595581,0.005752563). Original shader encoding yields the captured
RGB. A replay-only `textureLod(...,0)` probe returns that same sample, excluding
implicit mip selection as its cause. A four-`texelFetch`/FP32-mix probe returns
(0.013988212,0.015308824,0.006295873), close to the ideal source computation.
Eight-bit coordinate-weight quantization alone did not explain all five failures.

These observations locate the difference in filtered texture sampling for this
pixel; they do not prove a driver defect or a sufficient portable error bound.
The probes are diagnostic replacements, not renderer fixes or accepted frames.
Both replacement resources were restored/freed; the repeated restore reports0/0.
No manual-filter implementation was shipped. Next: establish the actual device
filtering/conversion error bounds and compare the remaining failures, then cover
the other tree placements/camera views before accepting the silhouette gate.

### Sampling diagnosis continued

`scripts/query_vulkan_baseline.cpp/.ps1` now report actual texel/mip/raster
fractional precision through `VkPhysicalDeviceLimits`; existing schema1 fields
remain intact. Two compiled probe runs exit0 on the NVIDIA GeForce RTX3060Ti:
subTexelPrecisionBits8, mipmapPrecisionBits8, subPixelPrecisionBits8; loader log
empty. Report/build output is under ignored `sampling-capabilities/`.
The [official limit definitions](https://docs.vulkan.org/refpages/latest/refpages/source/VkPhysicalDeviceLimits.html)
describe coordinate/LOD divisions, not a universal final sRGB byte tolerance.

RenderDoc1.41 original-shader traces now cover all five failed pixels. For each,
shader RGB converted to framebuffer bytes equals the captured RGB. Maximum
linear sample deviations from the independent source calculation are0.001702,
0.002617,0.002792,0.001418 and0.001546 respectively. No wrong vertex colour,
post-sample encoding or framebuffer value was found at these points.

A new replay-only probe samples the four neighbouring texel centres with
`textureLod` and mixes them in FP32. At (276,472), all four centre samples match
the previous `texelFetch` values exactly, and their mixed result matches the
previous FP32 probe exactly. Thus this point's remaining difference occurs in
hardware filtering between texel centres, rather than LOD selection or a
different conversion at those centres. The new shader was restored/freed1/1;
the subsequent restore returns0/0. No diagnostic shader remains installed.

The [Khronos image sampling test](https://github.com/KhronosGroup/VK-GL-CTS/blob/main/external/vulkancts/modules/vulkan/pipeline/vktPipelineImageSamplingInstance.cpp)
adds4/255 for sRGB conversion to a format-dependent linear-space threshold. Its
comment explicitly says these tests use low precision because accuracy is not
their objective. The five deviations fit that added allowance alone, but this
is diagnostic context, **not** proof of a normative portable filtering bound or
our visual acceptance. The original strict oracle still exits1; no threshold
was changed. Next: isolate fractional filtering in a controlled native GPU
fixture, establish a suitable accuracy oracle, then cover remaining tree views.

Private `tree-five-original-debug141.json`, `tree-five-filter-diagnosis141.json`,
`diagnose-filter-weights141.py`, `tree-centre-debug141.json` and
`native-centre-probe141.frag` retain the new receipts. This capability tooling
extension is tracked separately in VK022; it changes no renderer contract.

### Direct native fractional-filter fixture

`PsyX_VkNativeTests.inl` now adds five owned 2x2 patterns to `-vknativetest`,
using the four source texels and fractional weights from each failed point.
All vertices have constant UVs (zero derivatives, mip0), white colour and tint.
Each pattern runs nearest, bilinear and trilinear through ordinary native
publication, submission and readback. No city geometry or replay replacement
shader participates. The developer report buffer is enlarged to 32KiB.
Tests ran on Windows/RTX3060Ti, native1280x720 linear UNORM output; preceding
device probe driverVersionRaw2559967232. Parent HEAD2e93f215 retains the active
goal's uncommitted implementation and unrelated local edits; the tested fork
content is subsequently committed as7616e0d. No clean parent revision is claimed.

The first run failed because the new resource assertion incorrectly expected
20 material bytes for all filters. Existing base-only nearest/bilinear own16;
trilinear also owns the 1x1 mip, totalling20. The assertion now follows that
contract and prints observed/expected ownership. No resource implementation
was changed to make the test pass.

Final VS `Release_dev|x64` build PASS (two succeeded, zero failed; existing
linker warnings). Fresh native GPU suite PASS/exit0, including fifteen uniform
32x32 interior masks, nearest source-texel checks, exact bilinear/trilinear
agreement, one resident mesh/material,216 geometry bytes,16/20 material bytes,
no pending/retiring resources and final zero ownership. Fresh `-vkpsxtest`
PASS/exit0. Both IDE debug outputs confirm Khronos validation enabled and no
VUID/validation error/warning. Readback forces alpha255; the fixture's alpha
check is a readback-contract check, not independent attachment-alpha evidence.

The configured RenderDoc1.41 MCP executes `compare-fractional-native141.py`
against original normal capture EID4768. All ten comparisons (five points,
bilinear and trilinear) match direct native RGB exactly: (30,31,17), (55,63,37),
(46,52,28), (33,39,24), (24,27,16). This reproduces every observed difference
outside replay and city interpolation. It does **not** establish a portable
accuracy bound: all ten filtered fixture results still exceed the unchanged
two-byte ideal tolerance in at least one channel. The fixture explicitly reports
`strict2 outside=10, accuracy unaccepted`; its invariant PASS does not accept
the original tree colour oracle, which remains failed.

Fork7616e0d committed/pushed to project origin/master; parent gitlink staged.
Private `fractional-native-pass.log`, `fractional-psx-pass.log`, matching debug
receipts, `fractional-native-vs-capture141.json` and comparison script retain
the checks. Six test files (five INIs and generated `.user`) hash-restored from
`fractional-backup`; all10 VS configuration pairs retained, debugger Design,
owned native/PSX processes exited. Compatibility: VK023. Next: establish an
accuracy oracle justified for hardware sRGB filtering, then finish tree views,
silhouette/minification and the remaining original-region surface gates.
Follow [the durable sampling rule](../rules/native-sampling-validation.md).

### Native FP32 filtering correction

The subsequent production correction addresses the reproduced error without
relaxing the two-byte ideal tolerance. Native bilinear sampling fetches four
clamped sRGB texels, then interpolates decoded RGB and linear alpha in FP32.
Trilinear uses the selected implicit LOD and mixes adjacent levels in FP32;
nearest and base-only bilinear semantics are retained. The LOD query precedes
per-fragment discard in draw-uniform control flow. Clamp UVs before integer
conversion, bound levels to the retained chain, and skip the next-level fetch
at an integer LOD. Non-finite UVs discard explicitly.

`NativeDrawPush` uses its former padding word at offset92 for the effective
sampling mode. Its size112 and depthPlane offset96 stay asserted; public ABI9,
resource formats, upload/fence/retirement and backend selection are unchanged.
Only native shaders and their generated SPIR-V/header change. Filtered reads
cost four fetches per level, up to eight for fractional trilinear sampling.

Before implementation, replay-only FP32 probes on both original1.41 captures
reduce the original oracle to zero errors. Each replacement is restored/freed
once before closing its owned session. Production tests then corroborate this:

- Shader compilation and VS Release_dev x64 PASS (two succeeded/zero failed;
  existing linker warnings). CPU scene suites PASS:15 planes,12 coordinates,
  22 ownership/C ABI,12 camera,13 material,13 mip,8 sampling,19 picking,13 state.
  Asset integrity1536 PASS. Both scripts exit0.
- Fresh native GPU suite PASS/exit0 after the production fix, then PASS/exit0
  after adding six mip/clamp regressions. Fresh PSX GPU regression PASS/exit0.
  IDE output confirms Khronos validation enabled and zero VUID/errors/warnings.
- All fifteen fractional pattern/filter cases retain uniform32x32 masks,
  source-nearest controls, exact bilinear/trilinear mip0 agreement and bounded
  ownership. Ten filtered ideal checks now pass the unchanged two-byte gate;
  RGBs31,33,18 /55,65,39 /46,54,30 /35,40,25 /26,29,17. Previously all ten failed.
- Six additional GPU checks pass with exact bytes: minified nearest/bilinear
  retain base red; fractional mip1/mip2 gives225,137,137; finite UVs +/-1e20
  clamp to red/white; huge derivatives clamp to last mip188,188,188. Final
  ownership and retiring mesh/material counters are zero.

Fresh actual frame500 captures through configured RenderDoc1.41 use the same
fixed Chicago spawn, trilinear,1920x1080: normal focal256/HD on and wide focal128/
original art. Each retains80 backdrop/876 world/10 auxiliary UI draws and44
two-sided cutout parts with passing alpha/depth policy. Bound geometry148320
bytes, all buffer/artwork-mip hashes, formats and per-draw resource bindings
match preceding captures exactly. Normal61 images/49926792 material bytes;
wide62/2174132. No legacy world draw is used to satisfy native colour evidence.

The unchanged independent oracle on these fresh captures exits0: normal30458
and wide10140 colour samples have zero errors; holes82811/69769, solids486087/
162460 and occluded1984/463 also have zero wrong ID/depth/occlusion pixels.
These are still only three visible trees/six parts;38 inventoried parts and
silhouette/threshold boundaries are unaccepted. The earlier failed capture and
its results remain retained. Full attachment hashes change; better filtered
alpha can affect cutout boundaries, so exact whole-image equivalence is not claimed.

Existing clipped/culling-aware terrain masks against the fresh ID attachments
also pass: paint45191/35601, visible coplanar overlaps39558/71120, zero wrong IDs.
This reuses the original interior masks and exclusions, not a new tolerance.
Both new world PNGs are inspected; visual inspection does not accept all edges.
Each fresh replay retains the same five initial HIGH barrier messages and
`validationClean=false`. Direct IDE validation does not clear those diagnostics.
One normal replay timestamp experiment sums956 native draws as2.810,2.705,
2.751,0.291ms. Variation makes performance inconclusive; no FPS/budget claim.

Private evidence: `native-precise-normal141_frame500.rdc`, corresponding wide
capture, `precise-normal141/`, `precise-wide141/`, `cutout-precisenormal141/`,
`cutout-precisewide141/`, `precise-city-oracle.log`, `precise-resource-comparison.json`,
`precise-terrain-pixel-check.json`, native/PSX/CPU logs and debug receipts.
Fork426c9a3 committed/pushed to project origin/master; parent gitlink staged.
Six test files hash-restored from `precise-filter-backup`, verified again after
VS reopened; all10 configuration pairs preserved, debugger Design, no breakpoints
or owned game processes remain. Compatibility: VK024. R03 stays incomplete.

## Reproduction and limits

Subsequent [view/boundary evidence](r03-tree-view-validation.md) compares3079701
eligible colours and87152 boundary-band pixels across eleven views with zero errors.
All44 parts have original-art opaque colour evidence;36 have normal/HD evidence.
Part751 has only35 eligible original pixels; excluded boundaries, remaining HD
parts and terrain/camera problems in additional views still gate full silhouettes
and region acceptance. Production fork426c9a3 remains unchanged.

Ignored local artifacts below `src_rebuild/build/native-renderer-r03/` include
`export-cutouts141.py`, `export-cutout-pixels141.py`, `check-cutout-pixels141.py`,
`inspect-cutout-colour-failures.py`, `cutout-normal141/`, `cutout-wide141/`,
`cutout-shader-debug141.json` and the two replay-only shader probes. They contain
original local artwork and must not be redistributed. Run the export scripts
through `rdc script ... --arg output=ABS_DIRECTORY --json` on the matching capture;
run the offline oracle with the bundled NumPy Python and arguments `normal wide`.
Final inspector outputs are `normal141-cutout-policy/` and `wide141-cutout-policy/`.

The earlier inspection-only milestones did not rebuild C++; the direct fixture
and subsequent production FP32 correction did. Error/loading/native recovery,
fence retirement and OpenGL selection remain unchanged. Full source geometry, boundary/minification,
remaining original-city drive coverage and replay-warning isolation still gate
R03; R04 is unstarted. Compatibility tracking: VK021-VK026.
