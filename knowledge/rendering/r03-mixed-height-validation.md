---
type: ValidationRecord
title: R03 mixed-height native terrain depth
status: partial
created: 2026-10-08
tags: [okf, rendering, vulkan, depth, validation]
---

# R03 mixed-height native terrain depth

The reproduced eastnorth grass/sidewalk conflict passes scoped native pixel and
resource checks. R03 remains incomplete; R04 is unstarted. This extends the
[canonical terrain policy](r03-terrain-validation.md), following the
[tree-exposure finding](r03-tree-view-validation.md). No old-world recovery,
OpenGL removal, source geometry movement or artwork modification is involved.

## Objective and source confirmation

Resolve stippling between original logical parts569 and681, while preserving
triangle order, vertical sides, logical picking, payload bounds and fence-owned
retirement. Establish their actual source-world heights before changing depth.

A Visual Studio breakpoint at `DeveloperNativeRegion.cpp:417`, FrameCnt1, reads
copied source/draw scalar fields without function evaluation. Grass569 is model76,
part0, game position[14848,-17,-220672], yaw61; its native translation is
[14848,17,-220672], local topY0. Sidewalk681 is model159, part0, position
[14082,0,-220321], yaw59; native translation[14082,0,-220321], local topY17.
Both worldY bases are[0,1,0]: both tops have exact worldY17. The sidewalk also
contains a vertical side down to localY0. Earlier whole-mesh classification
therefore incorrectly withheld canonical depth from its horizontal top.
Private receipt: `mixed-height-source-confirmation.json`.

## Implementation and invariants

PsyCross commit `ae83dd2` is committed/pushed to the project fork's `origin/master`;
the parent gitlink records it. A mesh owns consecutive indexed-triangle ranges.
Only consecutive horizontal triangles at an exactly equal localY merge; adjacent
nonhorizontal triangles merge separately. Unreferenced vertices do not disable
an indexed top. Vertices, indices, material, triangle order and logical mesh/ID
remain unchanged. Vulkan binds the same source buffers and selects `firstIndex`
and `indexCount` per range. Horizontal ranges use the existing frame inverse/
world-height depth equation; sides retain native raster depth. Backdrop remains
one whole-mesh draw. Colour, alpha, physical depth and logical ID share each draw.

Bounds are4096 ranges per mesh and4096 physical draws per published frame,
including backdrops. Range capacity has a separate32MiB CPU metadata budget;
the existing32MiB geometry and64MiB artwork budgets/metrics are unchanged.
Widened counts and validation precede allocation/publication; temporary vectors
publish atomically and allocation failures remain explicit. Physical-frame
rejection preserves the prior snapshot. Metadata remains owned until the mesh's
existing submission fence permits reclamation, then its capacity is uncharged.
Public C ABI9,112-byte push layout, shaders, synchronization and upload resources
are unchanged. Authored paint keeps its existing bounded layer bias.

Files: PsyCross `PsyX_NativeScene.{h,cpp}`, `PsyX_VkNative.inl`,
`PsyX_VkNativeTests.inl`; parent `NativeSceneTests.cpp` and
`scripts/inspect_native_capture.py`. The inspector distinguishes876 logical
world parts from physical ranges and verifies contiguous exact index coverage
and identical per-part buffer/material bindings. Vulkan draw semantics were
checked through configured context7 against the
[indexed draw specification](https://docs.vulkan.org/spec/latest/chapters/drawing.html#vkCmdDrawIndexed).
OpenGL compatibility work is recorded as VK027; its existing backend is preserved.

## Executed validation

- `pwsh -NoProfile -File scripts/run_native_scene_tests.ps1`: exit0, including
  17 new range/source-byte/budget/atomic-publication/fence checks,15 plane checks,
  12 large-coordinate checks and the existing scene/C ABI22, camera12, material13,
  mip13, sampling8, picking19 and frame-state13 checks. Existing excluded-platform
  host guards pass; this is not a full Android/web/PSX target build.
- `pwsh -NoProfile -File scripts/run_native_asset_tests.ps1`: exit0,1536 checks.
- Visual Studio MCP Release_dev/x64 builds: both complete, zero failed projects.
  The first GPU run rejects the old test's logical draw-count expectation before
  reaching its pixel checks. Its failed log is retained; no passing pixels are
  attributed to that run. The test now accounts for physical ranges and the
  mixed-height fixture independently requires exactly three physical draws for
  two logical meshes. The production implementation is unchanged by that repair.
- Fresh `-vknativetest`: PASS,11,685 top-interior pixels in each submission order,
  zero wrong colours; picking, paint/holes/nearer occlusion and zero final range/
  geometry/material ownership pass. Existing filtering/mip/clamp, explicit
  failure/loading/native-resource recovery and native regressions also pass.
  The appended side exercises a real second draw, not a separate logical mesh.
  A dedicated city-side colour acceptance gate is not claimed.
- Fresh `-vkpsxtest`: PASS; debugger reports exit0. Khronos validation loads;
  no VUID is observed in the direct test output. Logs:
  `mixed-height-native-gpu-first.log`, `mixed-height-native-gpu.log`,
  `mixed-height-psx-gpu.log`.

## Actual RenderDoc1.41 city pixels and resources

Configured rdc-tools captures frame500 at1920x1080, mission50/Chicago,
start[14000,-220800], heading0. Normal focal256/HD enabled and widened focal128/
original art use trilinear sampling. Captures are
`native-mixed-eastnorth-{normal,wide}141_frame500.rdc`; matched pre-fix captures
are `native-tree-eastnorth-{normal,wide}141_frame500.rdc`. Inspector and private
terrain exports are under `mixed-eastnorth-{normal,wide}141/`.

Both frames contain80 backdrop/1023 physical world/10 auxiliary draws,876 logical
world parts,44 two-sided tree parts,148320 GPU geometry bytes. Exact index-range
coverage passes for every part; all source geometry/artwork record multisets
and every logical part's buffer/material hashes match the pre-fix capture.
Only capture-local resource ID keys are excluded; content, duplicate multiplicity
and bindings are retained. HD has61 images/49926792 bytes; original has62 images/
2174132 bytes. These are unchanged upload payloads, not a whole-process memory
or GPU performance claim. The147 extra world draws need cost measurement.

Grass569 and sidewalk681 top now have exactly identical captured depth equations
in each view. Sidewalk ranges are[0,6) top and[6,12) side; the side has
`depthPlane.w=-1`. Independent post-VS masks clip to all six Vulkan homogeneous
planes, respect actual culling and exclude barycentric boundaries of0.01.
The visible comparison excludes other occluders and ambiguous boundaries;
it admits actual old/new owners569/681 only after constructing the source mask.
Ordinary native order predicts681 as the last coplanar top.

- Normal:11456 compared overlap pixels,4462 wrong pre-fix, zero after the top
  draw and in the final frame;1175 occluded/boundary pixels excluded.
- Wide:2639 compared overlap pixels,2111 wrong pre-fix, zero after/final;
  281 excluded.
- In both masks, pre/post-top D32 bits are identical. Every eligible colour
  changes from grass to sidewalk, and matches the final world colour exactly.
  Independent source-UV/mip/sRGB interpolation compares44827 normal and11203
  wide sidewalk colour samples, zero errors under the unchanged two-byte gate.
  The known equal-height tie with569 is admitted explicitly; alpha margin0.005,
  triangle distance>1/128 pixel, LOD±0.35 and depth margins are retained.
- Tree regression:430758 normal/112093 wide eligible colour samples, zero
  colour/ID/depth/hole/occlusion failures;14107/7661 neighbour-band comparisons.
  Their declared threshold/edge/LOD exclusions are not silhouette acceptance.

At normal pixel[1400,795], raw post-top/final ID is681 and D32 is
0.991083562374115, equal to grass. RenderDoc1.41 Pixel History nevertheless marks
the top as `depthTestFailed=true`, despite reporting the same shader/pre/post
depth. Adjacent[1401,795] reports a passing tie. Preserve this tool discrepancy;
do not use the failed-history flag as the actual attachment result or claim that
Pixel History is fully validated. The independent raw attachments are the pixel
gate. These coordinates are outside the same parts in the wide view.

Private reproducible helpers: `export-mixed-terrain141.py`,
`check-mixed-terrain141.py`, `export-kerb-art141.py`, `check-kerb-colour141.py` and
existing tree exports/oracles. Reports: `mixed-terrain-pixel-check141.json`,
`cutout-kerb{normal,wide}141/kerb-colour-check.json`,
`cutout-mixed{normal,wide}141/boundary-pixel-check.json`.
All capture/export phases are below30s. Both world PNGs are visually inspected.
Four initial HIGH replay barrier warnings remain per capture:
`validationClean=false`. Pixel/resource passes do not clear them.

## Preservation and remaining acceptance

Capture targets/replays close. Five runtime INIs and generated`.user` restore
byte/hash-exactly from `mixed-height-backup`; all10 VS configuration/platform
pairs, Release_dev/x64, Design, game startup project and zero breakpoints are
verified. Reopening VS rewrites cached debugger history, so the`.user` is
byte-restored once more afterward and all six files reverified. The failed
startup-name attempts launch FontTool, not the native suite; no native validation
is attributed to them. A failed capture with an incorrect executable basename
is retried only with the verified `REDRIVER2_dev.exe` and succeeds.
Root/fork `git diff --check`, documentation whitespace/status-length checks,
new evidence/rule file-link checks and inspector Python syntax checks pass.
Current status348 lines and recent context349 remain below the350-line limit.

This accepts the reproduced mixed-height top conflict only. Vertical-side city
pixels, sloped coplanar surfaces, source artwork seams, west camera/ground/
backdrop coverage, remaining HD/silhouette/uncertain-boundary checks, complete
region driving/coverage, replay warnings and GPU cost remain pending. Diagnose
the west source/camera coverage next, within R03. R04 cannot advance.

## Subsequent read-only west diagnosis

Existing1.41 west normal/wide captures have no native world ID and D32=1 for
all729600 pixels at y>=700. Five captured tree MVPs/four distinct copied source
placements reconstruct the view; actual post-VS versus matrix output agrees
within0.000968 clip units. Their source point matrix is ill-conditioned
(condition number26220089.5), so the result is an estimate needing direct debugger
confirmation, not a new camera validation gate. Both captured views estimate
native camera[-6990.003,278.999,-232745.999], above assumed groundY0/17.

Intersecting captured-view rays with those two hypothetical ground planes finds
all729600 empty normal pixels inside the declared region bounds. Wide finds
729256 forY0 or729599 forY17. Examples such as[960,900] project inside the region
near[-6845,0,-231975] (normal/Y0). This makes a simple out-of-region or below-plane
explanation insufficient on the measured approximation; it does not prove that
an original source surface exists there. Establish actual camera/car transforms,
original cells/source mesh footprints and copied/drawn coverage before deciding
whether geometry is missing or the selected spawn lacks original ground.

Private helpers/receipts: `infer-west-camera141.py`, `west-camera-inference141.json`,
`check-west-ground-rays141.py`, `west-ground-rays141.json`. No runtime/settings
change or additional capture/build is made for this diagnosis. West coverage
remains explicitly unaccepted; source confirmation is the next R03 item.

## Direct west source investigation (2026-10-08)

VS Release_dev/x64 pauses in `DeveloperNativeRegion_Update:417`, using Chicago
mission50, start[-6500,-231500], heading512, FOV256/HD1 at1920x1080. Frame1
original camera[-6512,-317,-232519]; frame262 camera[-7011,-245,-232706], car0
translation[-6608,46,-231630]. Native camera is therefore above Y0 at both
observed frames. These are direct scalar reads, not confirmation of the earlier
frame500 estimate: the camera/car evolve, and exact captured-frame parity is
still pending. `events.camera=0`, `events.draw=15`; both frames use the ordinary
list rather than event topology. Map cell2048/region32, grid512x768, world half
extents524288/786432 are read directly from runtime.

Cell[252,270], covering the normal estimated point near[-6845,-231975], has
`cell_ptrs[1500]=0xffff`; its bank1 is resident (`loading_region=-1`,
`RoadMapRegions=135`). This denotes no object list, not pending streaming. A
read-only bounded copy of all144 selected resident cell lists and530 owned
instances at frame262 finds628 ordinary references/530 unique source slots,
exactly matching the owned set, with no missing/extra slot. The private offline
oracle initially omitted terminal objects: cell.c returns the object carrying
0x8000 before ending iteration. Correcting that oracle yields exact equality;
production collection is unchanged. An invalid exploratory debugger expression
indexed `cells[65535]`; its value is retained as unusable, never source evidence.

The existing normal capture is replayed with configured RenderDoc1.41. All876
world parts/80 bound geometry pairs are exported in11.917s. Their indexed source
geometry contains4814 triangles. Applying copied placement matrices finds no
source-placement XZ footprint at the16 earlier hypothetical ground points
(four pixels, two plane assumptions, two views). This is a diagnosis, not a
full-screen raster oracle: billboard matrices here are original placement, not
the captured camera-facing draw basis, and rays remain frame500 estimates.
At X=-6845 the nearest geometry begins Z=-230911, well beyond the sampled
normal[-6845,0,-231975]. An empty source cell alone would not prove this because
neighbouring placements may straddle its bounds.

A proposed replacement start[-7000,-229300] lies inside a captured Y0 triangle
of model54, logical part309, source[-7168,0,-229376]. That source test passes;
the replacement is not launched or visually accepted in this investigation.
Next: capture a west normal/wide pair from this source-confirmed road, confirm
the actual captured camera and native ID/colour/depth/resources, then check
ground/backdrop coverage. Retain the old captures as unsupported-spawn evidence;
do not mark their729600 empty pixels as a native renderer pass or remove the
west gate. No production code, shader, build or GPU self-test changes/reruns.

Private receipts/helpers: `west-source-debugger.json`, `west-source-memory.json`,
`west-all-meshes141.json`, `west-source-footprint-check.json`,
`read-west-sources.py`, `export-west-all-meshes141.py`,
`check-west-source-footprints.py`. The owned debugger target/replay closes;
all six temporary files restore byte/hash-exactly from `west-source-backup`,
including after reopening VS. All10 configuration/platform pairs, Release_dev/x64,
Design, game startup and zero breakpoints are verified. R03 remains incomplete.
Root/fork `git diff --check`, private helper Python syntax and the16-document
precision/status checker pass. Current status/recent context are349 lines each.
