---
type: Validation
title: R03 complete two-view scene composition with all-candidate references
created: 2026-10-08
tags: [okf, rendering, vulkan, validation]
---

# R03 complete two-view scene composition with all-candidate references

The1370 decisions excluded by the [initial whole-scene oracle](r03-complete-composition-validation.md)
are now explained by **every source-covered candidate**, raw source artwork and
explicit primitive shader-input/output references. Both existing west-road1.41
views pass4147200 pixels with0 colour/ID/depth/sky errors and0 exclusions. This
accepts complete composition for these two captured views; it does not accept
all original assets, the entire R03 region, universal arithmetic or GPU cost.
R03 remains incomplete/R04 unstarted. Production code/settings are unchanged.

## Source-selected candidate and reference contract

Re-evaluate every ordered range/primitive's source MVP, near/far clipping,
device-scoped quantized viewport/top-left/culling model. At the908 normal/462
wide pending pixels, enumerate1902/971 covering candidates, respectively.
No final native ID is consulted to select candidates or a winner. Full index
partitions/resource/mip hashes and source/post geometry remain checked by the
[complete-view audit](r03-complete-view-coverage-validation.md); actual shader/
constants/depth/blend/sample state by the initial composition export.

For all2873 candidates, actual1.41 DebugPixel uses sample0 and an explicit
source-selected primitive. Reflected names identify fragment coordinates/UVs,
depth/colour/pick outputs; sparse instruction-local mappings identify sampled
texel/LOD and executed Kill instructions. Each draw rechecks actual culling,
depth test/write/LESS_OR_EQUAL/bias, push constants and bound geometry hashes.
Each candidate completes its trace and frees it; all trace references use the
same previously verified native fragment shader. No captured attachment ID
chooses shader inputs or the expected owner.

Discarded candidates must have source-compatible sampled alpha below the actual
cutoff, Kill reached and output initialization only. Surviving candidates must
write all three outputs, have the correct logical pick and source-compatible
depth within5e-7. Ordinary shader depth is bit-exactly its native raster input;
canonical shader output supplies the supplemental reference. Every candidate
competes in actual range/primitive order under LESS_OR_EQUAL, starting from
verified clear D32=1. Later exact ties win;75 normal/44 wide candidate comparisons
have different logical IDs at identical D32 depth. These are119 comparisons,
not119 necessarily distinct pixels. All1365 traced winning solid pixels match
final native D32 **bit for bit**; the five sky pixels preserve ID0/D32=1/backdrop
exactly. All277 previous provisional source-ID differences are explained, with
148 normal/129 wide expected IDs changed before attachment comparison.

Raw mips independently resample alpha at referenced shader UV/LOD within5e-6;
RGB at those inputs agrees within the existing two sRGB-byte gate. Separately,
source quantized UV/tinted vertex colour predicts shader output colour at the
bounded shader LOD under the same two-byte gate. Actual LOD stays within the
existing0.35 source gate and sampled alpha within the initial source LOD/alpha
envelope under the unchanged0.005 gate. This checks source/artwork effects, not
only shader self-consistency. Neither pixel limits nor depth/cutoff rules change.

## UV arithmetic investigation and limits

The new helper initially applied the prior tree-case fixed5e-7 UV agreement gate
to all surfaces. It failed at normal1814,522, part76/event969/primitive1:
source quantized UV `[0.3876456606191534,0.8487022294071694]`, shader input
`[0.3876454830169678,0.8487001657485962]`, maximum difference2.064e-6.
Source and post-vertex quantized predictions agree to about1.4e-10 here;
unquantized prediction differs by4.075e-5. The projected triangle is narrow:
about21 pixels wide/3 pixels high. FP32 absolute-coordinate interpolant terms
cancel; tested FP32 evaluation orders yield different errors. No exact device
interpolator implementation is established.

The candidate enumerator now derives a **source-compatibility envelope** from
absolute interpolant coefficient/coordinate operands and gamma7 FP32 rounding
(`u=2^-24`, `gamma7=7u/(1-7u)`), propagating numerator/denominator intervals
through positive reciprocal W. It never fits a margin to shader values or
changes the existing colour/alpha/LOD/depth gates. All2873 UV references fit this
envelope; maximum source/shader difference5.4723e-6 normal/7.3725e-7 wide.
The largest envelope widths0.008907/0.005497 are explicitly conservative because
of cancellation. They do not prove precise UV equality, mandated coefficient
setup or universal interpolation precision. Source RGB, raw-art sampling and
native winner/depth/pixels remain separately tested. The old fixed-gate failure
is retained; this is supplemental device-reference evidence, not an exact
source-only arithmetic oracle. Do not retroactively reinterpret older5e-7 tests.

## Executed results and controls

- Normal:1902 candidates/521698 steps;88 discard/1663 ordinary/151 canonical
  surviving depth candidates.908 pending pixels resolved;2073600 full-view
  pixels pass,1728540 world/345060 sky,0 exclusions. Maximum raw alpha error
  2.2174e-6; raw RGB0.224563/source-shader RGB0.224590 byte.
- Wide:971 candidates/242850 steps;33 discard/867 ordinary/71 canonical surviving
  depth candidates.462 pending pixels resolved;2073600 full-view pixels pass,
  1622626 world/450974 sky,0 exclusions. Maximum raw alpha error5.344e-7;
  raw RGB0.251210/source-shader RGB0.251202 byte.
- Total2873 traces/764548 steps and4147200 colour/ID/depth/sky pixels,
  including3351166 world/796034 sky. No errors; maximum untraced source D32
  discrepancy remains1.924e-7 and traced winner-bit discrepancy0.
- Forty-two new controls reject:28 incomplete/incorrect trace controls and14 copied
  formerly excluded pixel controls. Reject missing candidates, wrong primitive/
  shader/coordinates, UV outside the derived interval, LOD/alpha/cutoff/Kill,
  depth value/bits/pick/colour and raw texel RGB. Pixel controls cover wrong
  owner/depth/colour, removed solid, filled source-alpha hole and sky colour/
  one-ULP D32 changes; exactly one pixel fails per control. Twenty original raw/
  source/state/oracle/trace files remain hash-identical during controls. Prior
  forty state/pixel controls and their14 original hashes also remain valid.

## Reproducibility and next gate

Private ignored artifacts under `src_rebuild/build/native-renderer-r03/`:
`enumerate-composition-candidates141.py`, `export-composition-traces141.py`,
`check-composition-traces141.py`, `test-composition-traces141.py`.
Per complete-world view: `composition-candidates.json`,
`composition-candidate-traces.json`, `composition-traced-oracle.npz`,
`composition-traced-pixel-check.json`, `composition-depth-decisions.json`.
SDK receipts `composition-traces-{normal,wide}-bounded-sdk141.json`:
163.973s/84.143s script phases, exit0 through the public local1.41 runner.
Observe the same process through client yields; neither RPC timeout nor duplicate
replay occurred. Host checks for each view and the42-control suite exit0.
`composition-traces-controls141.json` retains control details/original hashes.

Initial enumerator bounds used an inclusive end for an exclusive raster box and
raised IndexError; corrected before its2.8/3.1s successful runs. The first SDK
assertion compared integer culling with a serialized string; corrected before
tracing. The subsequent fixed-UV gate failure is preserved in normal
`composition-first-uv-failure.json`; its early-failure texel/LOD fields came from
the preceding candidate and are not evidence for the failed candidate. Reset
diagnostic locals before each trace. Failed SDK calls produce no success summary.

Four initial HIGH capture diagnostics/false clean flags are retained. No
production/build/game/new capture/native GPU fixture change/run. Six settings
hashes, fork cleanliness ae83dd2, helper syntax/root-fork whitespace and scoped
documentation links pass; both status files stay under350 lines. Upstream8.0/
b2d8857/2026-07-02 baseline was reverified before changelog editing.

Next: evaluate the remaining R03 original-source/region/other-view/silhouette/
resource/GPU-cost requirements against these newly completed two-view pixels.
Do not start R04 or treat these captures as full-stage acceptance. Retained R06/
R10 consumers remain pending; preserve OpenGL, compatibility tracking VK045.
