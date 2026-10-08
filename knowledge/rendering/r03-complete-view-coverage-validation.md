---
type: Validation
title: R03 complete-view geometry attribution and observed sky absences
created: 2026-10-08
tags: [okf, rendering, vulkan, validation]
---

# R03 complete-view geometry attribution and observed sky absences

Audit every native world range in the existing west-road normal/wide1.41
captures, rather than selecting more isolated visible parts. All observed native
world IDs have compatible projected geometry, and all observed geometry-covered
sky absences have source-alpha or fragment-discard evidence. This is **not** a
complete scene-composition, silhouette or stage acceptance. R03 remains incomplete;
R04 unstarted. Production source/backend/synchronization/settings are unchanged.

## Complete range and geometry audit

Direct configured1.41/RTX3060Ti exports1023 indexed ranges/876 logical parts and
33 world materials in each view. Selected slices, full vertex/index hashes and
all mip bytes match the retained capture inventory. Host verification repacks
the JSON vertex floats, concatenates ranges in order and rechecks full-buffer/
mip hashes and exact index partitions. This checks bound decoded geometry; it
does not independently re-decode every original game asset.

The source oracle multiplies bound local positions by the actual row-exposed
MVP, clips near/far before division and evaluates FP32 viewport multiply/add,
nearest1/256 grid, top-left edges and actual two-sided/back-cull policy. Viewport
pixels enforce XY visibility. The previous device-scoped numerical model is
retained; driver clipper/guard-band internals remain unmeasured. Every source/
post-VS component passes the operand-scaled gamma7 FP32 dot-product bound:
maximum error0.00103736/budget0.01244597, maximum budget fraction0.3749563.

- Normal:337 visible logical parts/1728540 native world pixels;
  source geometry union1730554 pixels.
- Wide:380 visible logical parts/1622626 native world pixels;
  source geometry union1629531 pixels.
- All3351166 observed native world pixels lie inside their own logical part's
  projected geometry in both source and post-VS models. No attributed pixel is
  missing. Every one of1023 range footprints per view has0 source/post differences.
- Native IDs select observed attribution only. Matching geometry does not prove
  correct alpha, nearest ownership, colour, or absence of missing/duplicate parts.

Both geometry audits exit0, host phases18.422s/18.828s. Direct export SDK phases
41.262s/32.849s exit0 without the daemon's30-second RPC limit. These timings are
analysis throughput, not native GPU cost. Six resource/attribution controls
reject: vertex bytes, range offset and an ID1 pixel outside its source geometry
in each view. The ID controls detect exactly one pixel each;522 raw hashes stay
unchanged. No count or geometry-only result accepts the full scene.

## Observed geometry-covered sky

The geometry unions contain2014 normal/6905 wide pixels where final native ID0
leaves sky visible. Evaluate all source candidates there, perspective UVs and
decoded alpha with the unchanged LOD+/-0.35 and alpha margin0.005.
1981 normal/6880 wide pixels are confident source holes; no confidently opaque
candidate explains a sky error. The remaining33/25 pixels require72 explicit
primitive traces, covering every candidate, not just one convenient fragment.

The completed1.41 fragment traces verify the actual input centre/quantized source
UV, sampling2/cutoff0.5, buffers, cull/depth state and shader identity. Instruction
source mappings identify local texel/LOD values and the executed `Kill()` path;
outputs only have initial allocations, with no depth/colour/pick assignment.
Each candidate preserves its before/after RGBA/ID0/D32=1 pixel. All72 candidates
discard. Offline texture-alpha interpolation at the traced UV/LOD independently
matches sampled alpha: maximum error0.00000191139 against5e-6, UV error at most
0.000000176526 against5e-7; effective LOD error at most0.024243 against0.35.

All8919 observed sky absences are now explained,0 unresolved in that scope;
their final RGBA equals pre-world backdrop exactly and ID0/D32=1 remains intact.
42 normal/30 wide traces complete16048 shader steps. Final SDK phases2.609s/
2.627s exit0. Four initial HIGH messages remain recorded with a false clean flag.
The GPU fragment reference supplements source/artwork; it is not an exact
universal offline sampling algorithm or proof that no other holes were filled.

Initial private-helper attempts fail and are retained in the evidence record:
1.41 has instruction-local mappings in `trace.instInfo`, not `state.sourceVars`;
unquantized source UV at normal[1294,499] differs by0.0000245916, while the
quantized interpolation model differs by less than8e-8. Wide[1551,397] compares
raw source LOD-2.0593 to shader LOD0 incorrectly; both sample the clamped base
level. Correct effective-LOD comparison preserves the original0.35 gate.
A dictionary-key iteration error in the offline summary is also corrected
before passing reruns. None of these helper corrections changes native rendering.

## Controls, artifacts and remaining work

`test-world-sky-gaps141.py` exits0: seven trace mutations per view (kill flag,
alpha exactly at cutoff, LOD, UV, missing candidate, primitive, sampled alpha)
all reject. Copied colour/ID/one-ULP D32 mutations at a traced hole each detect
one error, six attachment controls total. All528 original raw/trace hashes remain
unchanged. Geometry/resource controls are separate from these20 controls.

Private helpers/receipts stay in ignored `src_rebuild/build/native-renderer-r03/`:
`export-complete-world141.py`, `audit-complete-world-coverage141.py`,
`test-complete-world-coverage141.py`, `audit-world-sky-gaps141.py`,
`export-sky-gap-traces141.py`, `check-world-sky-gaps141.py`,
`test-world-sky-gaps141.py`; `complete-world-{normal,wide}141/` holds draws,
bindings, geometry audit/masks, source-alpha candidates, discard traces and
verified sky absence. SDK summaries, two control receipts, the retained probe
and `complete-world-initial-failures141.json` accompany them. The latter
reconstructs the initial UV failure from the retained probe; failed SDK calls
do not generate successful summaries. Official1.41
[instruction mapping definitions](https://raw.githubusercontent.com/baldurk/renderdoc/v1.41/renderdoc/api/replay/shader_types.h)
were consulted for lower-bound lookup and disassembly-line mapping.

No build/game/new capture/native GPU fixture was run. Six settings hashes,
root/fork diff checks and helper syntax checks pass; fork remains clean ae83dd2.
Next: independent complete-scene colour/alpha/depth composition and expected
winner, including potentially filled holes and remaining static surface types,
then full region/source/silhouette/resource/cost gates. Do not substitute observed
ID attribution for an independent expected winner. Retained R06/R10 consumers
remain pending migration; OpenGL compatibility tracking VK043.
