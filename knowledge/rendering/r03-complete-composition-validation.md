---
type: Validation
title: R03 source-generated complete-scene composition and unresolved pixels
created: 2026-10-08
tags: [okf, rendering, vulkan, validation]
---

# R03 source-generated complete-scene composition and unresolved pixels

The existing normal/wide west-road RenderDoc1.41 captures now have an expected
scene computed from every ordered native world range before reading final IDs,
depth or colour. The confidently decided partition passes4145830 pixels;1370
pixels remain explicitly unaccepted. This is partial composition evidence,
**not** complete-frame or R03 acceptance. R04 has not started.

Subsequent [all-candidate references](r03-composition-trace-validation.md) resolve
these1370 decisions and explain all277 provisional ID differences. Both tested
views now pass4147200 pixels with0 exclusions; the initial source-only partition
and numerical limits below remain historical evidence. Full R03 stays open.

## Source and pipeline contract

Each view exports1023 physical ranges/876 logical parts/33 world materials.
The previous [complete-view resource/geometry audit](r03-complete-view-coverage-validation.md)
is retained: reconstructed full buffers, ordered index partitions and every mip
hash match the retained capture inventory. Bound decoded source positions,
UVs/colours and actual matrices are inputs; independent re-decoding of every
original game asset is still pending.

Direct1.41 replay crosschecks actual push-constant pick/MVP/depth equation/tint/
alpha cutoff and records the fragment shader for every range. Both views share
SHA256 `0a1d6af267c1c4052c2e3a8d8546d9f938a546c3d0022e32dcb88b90bfa0fdec`,
matching the earlier discard traces. Every range has sampling2, explicit sRGB
encoding1, depth test/write and LESS_OR_EQUAL. Both attachments have blending and
logic operations disabled/write mask15; raster samples1, alpha-to-coverage,
alpha-to-one, stencil and depth bounds disabled. The1.41
[Vulkan state](https://raw.githubusercontent.com/baldurk/renderdoc/v1.41/renderdoc/api/replay/vk_pipestate.h)
and [blend fields](https://raw.githubusercontent.com/baldurk/renderdoc/v1.41/renderdoc/api/replay/common_pipestate.h)
were consulted before exporting these fields. Production shader/native pipeline
source was also inspected; neither was changed.

The host oracle checks finite source attributes, source MVP transforms, near/far
clipping, viewport-domain visibility, tested FP32 multiply/add and1/256 top-left
raster coverage/culling. Perspective interpolation uses quantized screen
vertices; raw decoded artwork supplies alpha and linear-light filtered RGB.
The unchanged gates are RGB2 bytes, LOD+/-0.35 and alpha0.005. Ordinary depth is
interpolated from source clip Z/W; each independent depth estimate retains5e-7
uncertainty, requiring competing intervals separated by more than1e-6.

Shared canonical depth equations are grouped by their XYZ coefficients; declared
D32 paint-bias units and native draw order resolve comparisons within that
equation. Each view has14 such equations and104163 shared-equation equal-unit
candidate comparisons across both views. Keep the nearest competitor from a
different equation/primitive; exclude an earlier representative of a newly
winning group. The initial conservative runner implementation was corrected
before the final checks; the accepted pixel partition/results stayed identical.
LESS_OR_EQUAL also permits depth equal to the initial clear value, which remains
uncertain against clear under the conservative depth interval.

Every projected opaque/cutout candidate competes. Uncertain alpha candidates
remain blockers when they could beat the provisional winner. The72 previously
verified, source-bounded discard references supplement their exact candidate
pixels; they do not select final world ownership. Native attachments are loaded
only after the full expected owner/depth/colour/uncertainty arrays are generated.
Expected sky preserves the independently source-validated backdrop from
[full backdrop checks](r03-full-backdrop-validation.md), with exact ID0/D32=1.
This is a tested device-scoped numerical model, not universal raster arithmetic.

## Actual results and rejection controls

- Normal:2072692 decided pixels, comprising1727635 world and345057 sky.
  Zero colour/ID/depth/sky errors; maximum world D32 error1.90940294e-7.
  Pending908 pixels:764 depth and144 alpha decisions.
- Wide:2073138 decided pixels, comprising1622166 world and450972 sky.
  Zero colour/ID/depth/sky errors; maximum world D32 error1.92371376e-7.
  Pending462 pixels:370 depth and92 alpha decisions.
- Total4145830 of4147200 pixels decided:3349801 world/796029 sky.
  Pending1134 depth/236 alpha pixels are explicitly excluded from acceptance.
  Their provisional source IDs differ from observed IDs at148 normal/129 wide
  pixels. These277 differences are unresolved diagnostics, not accepted errors
  or waived failures; evaluate every possible covering candidate next.
- Forty copied controls reject:26 incompatible pipeline-state controls and14
  pixel controls. Per view, wrong owner/depth/colour, removed solid surface,
  filled source-alpha hole and altered sky colour/D32 by one ULP each produce
  exactly one failing pixel. All14 original attachment/oracle/source/state files
  remain hash-identical during the final control run; hashes are in its receipt.

## Executed workflow and retained artifacts

Private ignored artifacts live under `src_rebuild/build/native-renderer-r03/`:

- `export-composition-state141.py`, initial and final
  `composition-state-{normal,wide}[-final]-sdk141.json`, and each complete-world
  `composition-state.json`. Initial SDK phases14.163s/13.605s; final extended
  state checks14.406s/13.768s, exit0 through the public local1.41 runner.
- `check-complete-composition141.py normal wide`, NumPy/Pillow runtime, final
  phases42.656s/31.531s, exit0. Per view: `composition-oracle.npz`,
  `composition-pixel-check.json`, `composition-unresolved.json`; normal also
  retains `composition-initial-conservative-runner.json`. Unresolved inventory
  records source candidate/event/primitive/depth, competing/alpha bounds and
  observed ID only as a subsequent diagnostic.
- `test-complete-composition141.py`, final exit0/40 controls,
  `complete-composition-controls141.json` and its final stdout. A preliminary
  14-pixel-control run and subsequent expanded40-control run also passed.

The added shader crosscheck initially used the state export's field name in the
older trace schema and raised `KeyError: fragmentShaderSHA256` before generating
a new oracle. It was corrected to the trace's actual `shaderSHA256`, then both
full checks and final controls reran. Do not reuse an older PASS as that failed
run's result. No rendering defect is established by this helper error.

Direct local replay and host jobs were observed through the same process handles
across client yields, with no daemon/RPC timeout or duplicate job. Timings measure
analysis throughput, not GPU cost. Four initial HIGH capture diagnostics and
`validationClean=false` remain retained. No production/build/game/new capture/
native GPU fixture was changed or run. Six settings hashes, root/fork whitespace
and helper syntax checks pass; fork stays clean ae83dd2. Upstream release8.0/
b2d8857/2026-07-02 was reverified before updating the changelog.

Next: resolve all1370 source-selected composition decisions, including277
provisional-ID differences, with every covering candidate's alpha/depth evidence;
rerun the whole-frame oracle and corruption controls without relaxing gates.
Then finish original-source/region/silhouette/resource/GPU-cost requirements.
Retained R06/R10 consumers and OpenGL remain preserved; compatibility VK044.
