---
type: Validation
title: R03 native coplanar road-paint evidence
status: partial
updated: 2026-10-07
tags: [okf, rendering, vulkan, validation]
---

# R03 native coplanar road-paint evidence

This accepts reproduced native paint and transform defects, not the entire static region.
R03 remains in progress; R04-R12 are unstarted. Follow the no-old-scene-recovery
policy in [the active plan](implementation-plan.md).

## Environment and reproducible state

Windows11, RTX3060Ti, driver610.88, Vulkan API1.4.341, NTSC VS Release_dev|x64.
Parent HEAD2e93f215 has substantial existing local work; fork baseline
bd5b67540fcfb3028256ec2c1907212596d55185. The fix changes native ABI8 ->9 and
game decoder version1 ->2. Road-paint fork6ba8068 is committed/pushed to the project
origin/master; the parent gitlink is staged.
RenderDoc1.46 capture/replay at1920x1080, frame500. City startup arguments:

```text
-nointro -mission 50 -gametype 1 -level 0 -playercar 0 -startpos 6917 -218984 -startdir 2798 -players 1 -nativeregion
```

Matched baseline/fixed normal camera uses focal256, HD mods, bilinear1/PGXP1.
The fixed widened case uses focal128 with original textures/mods off. Game
physics/camera transforms are not changed by the production fix.

## Source and depth diagnosis

Native logical parts292/293 are the base/paint of the same original tile. Their
source vertices share local Y=0; the smaller paint quads lie inside the base
rectangle x[-517,517], z[-542,542]. Original source order alone did not establish
the cause: actual RenderDoc pixel history at(1500,960) records base D32
0.9874797463417053 versus paint0.9874798059463501. The one-representable-unit
farther paint fails depth, not alpha discard; its shader would output the light
marking. At(1500,940), equal depth passes. This establishes coplanar interpolation
rounding, not missing source art or a proven MODEL.zBias requirement.

## Implementation and targeted checks

- `NativeTerrainLayers.h` classifies bounded convex horizontal source-tile faces;
  smaller contained same-winding faces receive a layer. Non-tile/sloped/partial-
  overlap/equal-area/opposite-facing cases stay ordinary. Candidate/layer budgets
  reject atomically; tests also cover contained converted triangles.
- Decoder parts key material plus layer; the GPU cache validates/copies it before
  reuse/upload. Native owned mesh descriptors validate0..8 and reset the field at
  reclamation. No vertex displacement, artwork rewrite or legacy packet ordering.
- Vulkan loads vkCmdSetDepthBias, enables dynamic constant bias and sets -4 per
  layer on world draws; ordinary world/backdrop set0. Slope/clamp0 needs no optional
  feature. Colour and R32_UINT ID use the same native depth/alpha draw.
- Actual GPU fixture includes an unbiased failed-depth control, transparent paint
  holes, a genuinely nearer occluder, both base/paint orders, matching logical IDs
  and reset/fence retirement to zero owned geometry/material bytes.

Executed final commands/results: `pwsh -NoProfile -File
scripts/run_native_asset_tests.ps1`1536 PASS; `pwsh -NoProfile -File
scripts/run_native_scene_tests.ps1` scene/C ABI22, camera12, material13, mip13,
sampling8, picking19, frame-state13 PASS. Host PSX/Android/Emscripten exclusions
compile, not runtime validation. VS Release_dev|x64 build PASS/zero failed
projects. Fresh `-vknativetest` and `-vkpsxtest` GPU reports PASS/exit0; Khronos
validation loaded with no VUID output. Earlier build failure (missing dynamic
loader entry) and locked captured executable were corrected before final checks.

## Actual native pixels and bound resources

All three captures contain80 native backdrop draws (depth off),876 native world
draws (D32 depth on),10 remaining auxiliary UI draws and zero debug messages.
The fix does not draw an old world as recovery.

- Matched normal full world colour and ID images each change33420 pixels; zero
  colour changes retain the same ID.18238 pixels previously belonging to base292
  now belong to paint293. Four actual paint pixel-history probes pass depth.
- A CPU oracle projects the captured post-VS paint293 triangles and checks pixel
  centres with barycentric weights>0.01, explicitly excluding triangle edges.
  Normal interior45000 pixels:17050 wrong IDs before, zero after. Fixed widened
  original-art interior35470 pixels:zero wrong IDs. Wider and normal frames are
  not used as a matched before/after comparison.
- Every bound native vertex/index buffer and every material mip is hashed.
  Baseline/fixed normal buffers and artwork match exactly per draw. Geometry
  payload148320 bytes remains fixed. Normal61 images/all-mip RGBA49926792 bytes;
  widened original62 images/RGBA2174132 bytes. Mod changes legitimately remap UVs
  from original atlas expansion to PNG clamp; position/normal/colour and indices
  still match. These are bound payloads, not total GPU allocation measurements.
- Fixed states:804 ordinary world draws bias0,72 authored paint draws bias-4,
  80 backdrop draws bias0. Baseline all956 native draws bias0. Source/post-VS
  geometry remains unchanged in the matched paint case.

Private ignored evidence under `src_rebuild/build/native-renderer-r03/`:
`native-road-depth146_frame500.rdc`, `native-road-fixed146_frame500.rdc`,
`native-road-wide-original146_frame500.rdc`, their world PNG/ID/depth readbacks,
`road-history146-report.json`, fixed/wide history reports,
`road-interior-check.json`, `road-wide-original-interior-check.json`,
`road-resource-audit.json`/`road-resource-audit-status.txt`, replay/check helpers,
`road-native-final-fixture.txt`, `road-psx-fixture.txt` and fresh debugger logs.
Original assets/captures remain private and are not committed or redistributed.

Five runtime INIs and `.vcxproj.user` restored hash-exactly from
`road-depth-backup`; all10 VS configuration pairs preserved, Release_dev|x64
reopened and debugger stopped. No gameplay source or runtime data asset changed
for this correction. Root/fork diff whitespace checks PASS before documentation.

## Large-city transform precision milestone

The separate grass seam diagnosis found actual coplanar placements511/514/516
with world Y17 and unchanged local Y0, using the same original64-square grass
image. Their source positions/rotations differ; their height does not. Old GPU
pixel history nevertheless gives different depths, and captured post-VS plane
slopes differ by about1.05e-5 between these three draws. Source ownership and
artwork are present; this is not evidence of a missing texture.

The old recorder formed `(projection * view) * world` entirely in float. Large
city translations around z-219000 cancel after projection, giving different
planes for the same world surface. A12-coordinate CPU regression with actual
Chicago placements/view fails that arithmetic, then passes the corrected helper.
`PsyX_NativeTransform.h` now forms `projection * (view * world)` with both CPU
products in double, narrowing only the final matrix supplied to the float shader.
The supplied view transform is respected, including existing backdrop translation
removal; no camera/gameplay state, public ABI or GPU float64 requirement changes.

Final CPU12 transform coordinates plus scene22/camera12/material13/mip13/
sampling8/picking19/frame-state13 and host guards PASS; asset1536 and VS build
PASS. Actual native GPU local versus z-220000 origins, two mesh origins, a0.004
nearer face, both draw orders, colour/ID602 and retirement PASS. Existing native
failure/recovery/coplanar-paint cases and fresh PSX GPU tests also PASS/exit0;
Khronos loaded with no VUID output. The fixed native normal/HD and wide/original
paint interiors remain45000/35470 pixels with zero wrong IDs. New RenderDoc1.46
world draws remain876/backdrop80/UI10, zero messages. Captured grass plane-slope
spread is8.01e-8 after, versus1.05e-5 before; D32 raster rounding still exists.

Private `native-grass-precision146_frame500.rdc` (wide/original),
`native-grass-precision-normal146_frame500.rdc` (normal/HD),
`grass-live-matrices.json`, `grass-matrix-precision-check.json`, captured histories,
`grass-precision-pixel-check.json`, native/PSX reports and debugger logs retain
this evidence. Five INIs/`.user` restored hash-exactly from `terrain-seam-backup`;
all10 VS pairs preserved, Release_dev|x64 reopened, debugger stopped.

Every bound buffer and all native artwork mip levels match exactly before/after
the precision fix at both settings:148320 geometry bytes, normal61 images/
RGBA49926792 and wide/original62 images/2174132. Private `grass-resource-audit`
JSON/status/helper records the actual PASS. Fork09d74de18e262ce421cc46cdea261b82dd1f3c99
is committed/pushed to project origin/master; parent gitlink staged.

**This is a transform correctness fix, not acceptance of the ground seam.** More
accurate planes reveal native z-fighting between overlapping grass placements,
including the grass/pavement edge. Deterministic ownership of equal-depth source
overlaps still needs a native policy, explicit occlusion/cutout/picking tests and
actual city pixels. Do not claim the visual gate passes because the false depth
separation was removed or because a legacy scene could hide the overlap.

## Canonical horizontal depth and native overlap ownership

The next native correction derives exact horizontal planes from copied mesh
vertices, without public ABI, source geometry or artwork changes. A bounded,
allocation-free double inverse of the frame's projection/view transforms the
world-height plane into a window-depth equation. Equal world heights give
bit-identical coefficients across distinct origins and triangulations. Ordinary
physical ties use `LESS_OR_EQUAL`: the last submitted native face owns colour
and ID. This is explicit native ordering, not PSX order-table recovery or an
artificial per-instance height offset.

The fragment shader replaces depth only with that equation for horizontal world
surfaces; other geometry/backdrop retain native raster depth. Authored horizontal
paint subtracts four positive D32 representable units per layer, including a
zero-depth guard; nonhorizontal layers keep raster bias. Alpha discard precedes
depth/ID writes. Pitched and edge-on planes use ordinary native raster depth.
Singular views and nonfinite coefficients fail explicitly. Plane classification
is copied/cleared with its mesh; frame preparation validates before recording.
The 112-byte push record has checked offset96 alignment and fits Vulkan's core
128-byte minimum; no optional GPU feature or new buffer/image is required.
Depth-attachment dependencies cover both early and late fragment tests.

CPU15 plane ownership/projection/error checks,12 large-city coordinates,
scene22/camera12/material13/mip13/sampling8/picking19/frame-state13 and host
exclusions PASS; asset1536 PASS. The new GPU floor fixture initially failed
because its rotation made the one-sided floor face downward; corrected winding
passes both native orders, each checking11685 interior pixels with zero wrong
colours. Cutout paint/hole, genuinely nearer floor, IDs and zero-owned retirement
pass. Existing native failure/loading/new-image fence recovery remains exercised.

Actual RenderDoc1.46 normal/HD and wide/original city captures retain80 backdrop/
876 world/10 auxiliary UI draws and zero messages. A clipped, culling-aware oracle
compares visible interiors of the eight equal-height parts510..517 against the
native tie policy. Normal:39558 compared,17677 wrong before,0 after; wide:71120
compared,34474 wrong before,0 after. Respectively23926/5357 overlap samples are
excluded as occluded/boundary samples, not declared correct. Captured coefficients
are exactly equal for these eight draws at each setting. Paint has zero wrong
IDs across45191/35601 interiors; these masks include homogeneous clipping and
therefore differ from the earlier un-clipped45000/35470 paint masks.

All bound vertex/index buffers and every artwork mip match before/after:
geometry148320 bytes; normal61 images/RGBA49926792, wide62/2174132. Pixel histories
show equal surviving grass depth instead of triangle-dependent alternation.
Visual review confirms the reproduced stippling is removed. Distinct source
patches still have artwork/UV boundaries; this policy does not rewrite them or
establish continuity of every city texture. No full-region, sloped-plane,
performance or additional platform-runtime acceptance follows from these checks.

Private `canonical-*` captures, histories, pixel/resource audits and fixture/debug
reports live under `build/native-renderer-r03/`; original assets remain private.
Final shader captures `native-canonical-normal-final146_frame500.rdc` and
`native-canonical-wide-final146_frame500.rdc` have byte-identical colour/ID/depth
pixels to the tested canonical images; final buffer/all-mip audit also PASS.
Fresh VS native PID59604 and PSX PID22744 report PASS/exit0, Khronos loaded,
no VUID. Microsoft `wil::ResultException` first-chance messages still appear in
the environment; they are not Vulkan validation results. Shader compilation,
`run_native_scene_tests.ps1` (15 plane checks plus existing suites),
`run_native_asset_tests.ps1` (1536), VS Release_dev|x64 and whitespace checks PASS.
Fork23411087ad867008239f8c42f50ca39e92553b18 committed/pushed to origin/master;
parent gitlink staged. Five INIs/`.user` restored hash-exactly from
`canonical-plane-backup`, all10 VS pairs preserved, debugger stopped/no breakpoints
and no task-owned game/helper processes remain.
See the [depth rule](../rules/native-coplanar-depth.md) and compatibility VK019.

## Unaccepted scope and next action

The reproduced equal-height depth conflict now passes its scoped native ownership
and pixel checks. Continue the original-city drive/surface coverage review;
do not infer full R03 acceptance from this reproduction. Source artwork boundaries,
sloped coplanar surfaces, all tree/water silhouettes and full region coverage
remain unaccepted. Synthetic
nearer-face occlusion is tested; no universal depth tolerance, GPU performance
or additional platform-runtime claim is made. Existing OpenGL remains intact;
its future native depth/coordinate/plane work is tracked as VK017-VK019.
