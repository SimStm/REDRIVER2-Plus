---
type: Validation
title: R03 source-bounded native raster-depth trace validation
created: 2026-10-08
tags: [okf, rendering, vulkan, validation]
---

# R03 source-bounded native raster-depth trace validation

This closes the34 ownership decisions left by the [bounded source-depth oracle](r03-vertical-static-validation.md#bounded-depth-decision-and-retained-ownership-ambiguity-2026-10-08)
for the existing normal/wide west-road RenderDoc1.41 captures on RTX3060Ti.
R03 remains incomplete/R04 unstarted. No production renderer change, new build,
game launch, capture or GPU fixture; these are local capture replays and host checks.

## Reference and scope

Independent source geometry/MVP and verified post-VS already predict identical
fixed-grid/top-left footprints for22 part/view cases. All34 uncertain pixels
have exactly one source-covered primitive in their draw; select its explicit
primitive ID rather than letting the debugger choose a random fragment.
`prepare-depth-debug141.py` reconstructs those candidates from source geometry
and reproduces the previous point receipt byte-for-byte.

Use the installed1.41 API `DebugPixel`, `ContinueDebug` and exception-safe
`FreeTrace` through the local runner. Resolve inputs/outputs through source-variable
mappings and reflection signatures: Position/gl_FragCoord, DepthOutput/gl_FragDepth,
outPick and outColor. Never hardcode compiled SPIR-V variable IDs or identify
gl_FragCoord solely because its values look like a pixel coordinate.

Capture input coordinates must equal the pixel centre. Shader output depth must
match input raster depth bit-for-bit and remain within the unchanged5e-7 source-
interpolation bound. Source/artwork/MVP and bound buffer checks stay independent;
this adds a GPU raster-input reference for ownership, not a universal exact-FP64
raster algorithm or independent offline winner proof for these34 samples.

Actual pipeline state is depth test/write with LESS_OR_EQUAL, raster depthPlane
[0,0,0,-1], matching logical pick and source vertex/index buffer hashes.
Depth-bias enable is true, but constant/slope/clamp values are all zero. An initial
assertion incorrectly required bias disabled and exits1 before collecting traces;
replace it with checks of the effective zero values. No pipeline/source change.

Continue each fragment trace to completion:7648 total steps (normal4444/wide3204).
Compute the expected owner from traced shader depth <= the original before-D32
sample, then compare the original native attachments.20 new owners match their
logical ID, exact traced D32 bits and shader colour within the existing2-byte
limit.14 rejected owners preserve colour/ID/D32 exactly. Three exact D32 ties
belong to the new draw, as LESS_OR_EQUAL requires. Maximum traced/source depth
error is7.655e-8 normal/1.593e-7 wide. These checks do not use Pixel History's
previously disputed ownership report.

## Complete declared pixel scope

`check-traced-depth141.py` uses the verified trace decision only at those34
coordinates; all source coverage/UV/artwork/colour/alpha/LOD and D32 error gates
stay unchanged.22 part/views pass886246 source-colour/ID/D32,12181 occlusion and
185206 outside-footprint crop preservation samples.886246+12181=898427, the full
source footprint. Zero raster/depth/alpha exclusions and zero wrong samples in
this scope. Previous failed/ambiguous receipts remain intact; this does not close
other city surfaces, clipped605/sky edges, region/silhouette or GPU cost.

Seven copied/in-memory controls fail: corrupted raster input depth, shader pick,
primitive ID, missing sample, shader colour, one-ULP increase of a tied depth
(with input/output consistent), and substituting strict LESS for LESS_OR_EQUAL.
The latter rejects the exact ties, proving the ownership comparison matters.
Original trace hashes are unchanged. Prior source/pixel/order/resource mutation
controls remain applicable; do not describe them as rerun in this milestone.

## Executed workflow and artifacts

Ignored root: `src_rebuild/build/native-renderer-r03/`.
Existing captures: `native-west-road-{normal,wide}141_frame500.rdc`.
Use `C:/Python314/python.exe scripts/run_renderdoc_analysis.py <capture>` with
`--script <root>/export-depth-debug-traces141.py --arg label=<normal|wide>`
`--gpu "RTX 3060 Ti" --summary <fresh-summary>`.
Final SDK analysis phases0.858s normal/1.706s wide. Initial successful normal
phase2.654s preceded additional buffer/state receipts; two earlier probe phases
0.339s/0.100s inspect input/mapping data. These are phase timings, not GPU cost.

NumPy-runtime `prepare-depth-debug141.py`, `check-depth-debug-attachments141.py`,
`check-traced-depth141.py` and `test-depth-debug141.py` exit0. Receipts:
`depth-debug-all-primitives141.json`, `depth-debug-traces-{normal,wide}141.json`,
`depth-debug-{normal-final,wide}-sdk141.json`,
`depth-debug-attachment-check141.json`, `depth-debug-controls141.json`,
`traced-depth-pixel-check141.json` and per-part `traced-depth-pixel-check.json`.
Four initial HIGH capture messages per replay are retained; no clean-replay claim,
message waiver or timeout occurred. The runner frees traces/controllers/capture.

Next: remaining clipped source605/sky samples and other region surfaces, followed
by full-region/silhouette and measured resource/GPU cost. R06/R10 dependencies
remain active. Mandatory OpenGL tracking VK040; existing GL preserved.

Hygiene: twelve affected documents/211 local links, five helper syntax checks,
root/fork diff checks,349/349 status caps and six settings hashes PASS. Fork clean;
no game process remains. Upstream8.0/`b2d8857`/2026-07-02 release reverified through
GitHub tag/release API before updating the changelog.
