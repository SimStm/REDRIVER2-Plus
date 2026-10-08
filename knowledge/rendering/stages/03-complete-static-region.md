---
type: ImplementationStage
title: R03 - Complete native static region
status: in-progress
created: 2026-10-06
tags: [okf, rendering, stage, vulkan]
---

# R03 - Complete native static region

**Status:** in progress; picking, explicit failure and camera/settings gates accepted; region/surface visual gates pending. **Dependency:** R02 accepted.

**Execution paused2026-10-08 by user request; goal cancelled.** The
[checkpoint](../r03-pause-checkpoint.md) preserves the exact stopping point:
two-view composition accepted, independent original topology matching preliminary
and still awaiting negative controls. No acceptance checkbox changes at the pause.

## Implemented and measured so far (2026-10-07)

The opt-in `-nativeregionprobe` collects all ordinary source-cell references in
Chicago x[-8192,16384), z[-237568,-212992), independently of camera/PVS selection.
The initial actual city run found144 resident cells,628 references,98 duplicate
references and530 distinct objects; all530 were copied, with no missing model,
unavailable source, decoder rejection or capacity failure. Source shape flags
classified508 opaque/22 cutout/16 animated base objects (animation count overlaps
the surface counts), no water or transparency-flagged objects;3642 triangles and
672 material-part instances. These are inventory observations, not GPU coverage.

The subsequent implementation selects available high-detail source models,
preserves the gameplay usage bitset, assigns logical placement identities, and
adds an incomplete `-nativeregion` draw path using the final camera/billboard
basis. Shared GPU materials verify exact content before sharing and retain
reference ownership through replacement/failure/reset. C ABI v3 adds explicit
back-face/two-sided culling; bounded payload capacities are512 meshes,
4096 part instances,32MiB geometry,256 materials and64MiB RGBA. Actual high-detail
inventory/draw:530 objects/4814 triangles/876 parts, usage bitset unchanged,
legacyWorld0. RenderDoc1.46 frame610:876 D32 draws/14442 indices plus10 D24S8
UI draws/318 indices, no debug messages. GPU culling readback PASS: reverse-facing
two-sided cutouts retain solid texels/holes; back-cull policy still discards them.
Fresh legacy PSX readback PASS; direct Khronos validation loaded/no VUID output.
Original native backdrop is now delivered (see the subsequent milestone below).
Mip chains and logical native picking were delivered in later milestones below.

Executed after these changes: native asset/cache tests1219 PASS; scene/C ABI18,
camera8 and material13 PASS; VS Release_dev x64 build PASS. The asset suite covers
shared-material rollback, last-owner release, replacement and reset. A pending
resource count assertion and an oversized test descriptor were corrected before
the passing reruns. Immutable PNG ownership removes per-frame large copies;
actual assembly mean45.910 ms/121 samples before,3.710 ms/481 after, including
initial upload cost; different windows, no matched benchmark claim. See
[partial product](../../product/native-static-region.md) and
[executed evidence](../r03-validation.md). No region acceptance gate passed at that initial milestone.

## Original backdrop milestone

Subsequent backdrop milestone: original models0..3 each have30 vertices/20
family21 quads. Bounded owned decode copies art bindings before sky GTE work;
C ABI v4 selects a camera-relative layer drawn first with no depth test/write.
RenderDoc1.46 frame500 confirms80 backdrop draws/480 indices,876 world draws/
14442 indices and10 original UI draws. Backdrop D32 probes remain1.0; world
probes become<1.0; no debug messages. Asset1503/scene20/camera12/material13 and
VS build PASS; native GPU readbacks prove order, no depth write, translation,
rotation, UI and retirement; fresh PSX regression PASS. Picking/outside
accounting and controlled region visual/drive acceptance remain pending.

## Native mip milestone

ABI v5 trilinear materials now own complete linear-light alpha-weighted mip
chains with closest attainable cutout coverage and full-chain budget/upload/
view/LOD ranges. Scene20/camera12/material13/mip13 and asset1505 checks PASS;
actual GPU minification reads sRGB188 versus base-only black, preserves cutout
holes without red bleed and verifies submitted-chain retention/fence recovery.
RenderDoc1.46 city frame500 confirms all61 native material chains/views/linear
mip samplers,80 backdrop/876 world/10 UI draws and no messages. Steady city
RGBA49926792 bytes, effective1/OK. The initial runtime filter replacement
exceeded the64MiB cap; ABI v6 per-instance sampling now selects retained
descriptors over one full chain. Scene sampling8 and actual GPU independent
choices/eight switches PASS. Actual city eight toggles/ticks120-1080 retain
all960 native submissions,61 uploads/no additional, fallback0, fixed image and
RGBA bytes. Picking/outside/camera/mod/drive checks remain pending.
No acceptance box is checked by this milestone.

## Objective

Prove a coherent original-world slice, including all static surface types, before extending the whole city.

## Steps

1. Choose a reproducible contiguous region in an original city and migrate roads/kerbs/ground, buildings/props and cutout trees/billboards using native geometry.
2. Handle transparent/static effects present in the selected slice, with explicit category/material rules. Render original sky and painted horizon as backdrop, separate from world lighting/depth. Chicago water is not applicable under the 2026-10-08 scope clarification; retain actual Rio water for future city-specific coverage before R12.
3. Use stable instance transforms for all passes and picking; remove duplicate legacy world draws. Count out-of-slice/missing/unsupported sources explicitly; never recover a native scene by drawing the old world.
4. Verify normal and widened FOV, camera turns, bilinear/mip sampling, enabled texture mods, terrain seams and transparent ordering. Authored coplanar paint must cover its base continuously with matching colour/picking depth, preserve cutout holes and remain occluded by genuinely nearer geometry.
5. Add reliable coverage diagnostics: source/native instances, out-of-slice sources, unsupported categories, missing resources and remaining auxiliary consumers. Distinguish expected source/upload loading from native errors; prove recovery with native pixels/resources.

## Deliverables

One complete original static region in opt-in native mode, with supported cutout/transparent/backdrop handling.

## Acceptance gate

- [ ] The selected region has no accidental missing/duplicate source geometry or legacy 3D fallback.
- [ ] Tree silhouettes and applicable transparency work without filling holes or shading the sky/HUD; absent categories have explicit source-backed not-applicable records, and Chicago water does not block this gate.
- [x] Camera movement/FOV/filtering/mod settings do not deactivate native rendering.
- [x] Picking identifies original logical resources/instances independently of draw batching.
- [x] Native loading/errors never trigger old-world drawing; diagnostics distinguish states, and upload-failure recovery produces new native pixels with safe fence retirement.
- [x] The reproduced coplanar Chicago road-paint defect has source/depth evidence, continuous native interior pixels at normal/wide FOV with HD/original art, and GPU cutout/occlusion/order/picking/retirement checks.
- [x] The reproduced equal-height Chicago terrain conflict has canonical native depth, declared tie ownership and clipped/culling-aware normal/wide pixel/resource evidence; this scoped reproduction does not accept all region seams.
- [x] Reproduced fractional artwork errors pass the unchanged two-byte ideal gate in direct native GPU patterns and fresh normal/wide city captures, with fractional mip/clamp/retirement checks and exact retained source resources; this does not accept other tree views or silhouettes.
- [x] The reproduced mixed-height grass/sidewalk top conflict passes source-world height confirmation, bounded ordered draw ranges, native GPU ownership/retirement and normal/wide raw pixel/resource gates; vertical-side/all-region acceptance remains separate.
- [ ] West ground/backdrop coverage passes normal/wide native pixel/resource checks from a source-confirmed road, with actual captured-camera evidence, source-confirmed sky coverage and reconciled auxiliary consumers. Tested backdrop views including all206 former sky edge exclusions, complete605/22 vertical part-view footprints and car/wheel attribution/count scopes pass; other static surfaces/full region/silhouette/cost remain pending. Retained R06/R10 consumers are migration debt. Region bounds or an empty source-cell diagnosis cannot satisfy this gate.

## Planned validation

Direct MSBuild and RenderDoc1.41 capture/local replay for routine checks; VS MCP for debugger investigations. Validate native colour/ID/depth, cutout and transparent passes; controlled camera/FOV/mod-on/off screenshots; source-instance accounting and original-city drive through the region. Tool/process success cannot override failed acceptance flags.

2026-10-08 user clarification: apparent Chicago water is empty space over the
sky/background, not a water surface; the copied inventory likewise reports no
water objects. Validate backdrop coverage instead of adding water geometry.
Keep applicable transparency distinct from cutouts and from absent water;
record source absence rather than claim a transparent pass from opaque tests.
Rio is not the current priority; its actual water remains pending full-game
category coverage. Other R03 geometry, terrain and silhouette gates stay open.

## Risks and native failure handling

Some distant silhouettes exist only as painted art. Record that distinction; do not invent replacement city geometry.

Region-scoped feature switch; unsupported areas remain clearly outside the accepted slice.
Missing/unsupported world coverage is a diagnostic or explicit failure, never
automatic old-scene replacement. Preserve only declared unmigrated auxiliary
consumers and remove their dependencies as their replacement stages pass.

## Logical picking and failure policy milestones

Native ABI v8 includes the v7 GPU ID/depth/cutout picking contract plus explicit
frame states. CPU picking19/frame-state13, GPU failure/loading/readback/native
recovery tests and real Chicago logical-source clicks PASS. RenderDoc1.46 verifies
world IDs, backdrop0 and IDs unchanged by original UI. See the detailed
[R03 evidence](../r03-validation.md); these two gates do not accept the full region.

The subsequent bounded source/publication ledger reconciles530 selected/copied/
published objects and876 material parts in actual Chicago, with228 original
view-inside/78 outside candidates and zero uncopied/duplicate/invalid counts.
CPU accounting18 and existing suites plus VS build PASS. Outside candidates are
declared omissions, not old-scene recovery. This completes the accounting item;
At this accounting milestone, controlled turns/FOV/mod/driving and complete native
pixel coverage still gated R03; subsequent camera/settings evidence follows.

## Controlled camera/settings and physics-input validation

Actual normal/wide focal256/128, four quarter turns, camera translation, nearest/
trilinear and two mod-off/on cycles retain native956/world0 with no failed/loading
submissions. Restoring each state reproduces world pixels exactly; original UI
rectangles are declared separately. Owned geometry148320 stays fixed; RGBA changes
49926792 ->2174132 ->49926792 with mods. Actual GPU images return to61, no retained
staging/commands/upload fences or retiring materials after restoration.

A finite mapped-pad fixture drives133 ordinary physics steps through acceleration,
left/right steering and braking, with the original follow camera and a wide-FOV/
mods-off interval. Car/camera transforms are never assigned. Native stays READY;
three actual GPU BMPs show the changed street views. This is physics-input evidence,
not proof that desktop keyboard injection works. Detailed checks are in
[R03 evidence](../r03-validation.md). The camera/settings gate now passes.

The straight-drive image exposed alternating strips in near road markings;
the subsequent source/depth diagnosis and fix are recorded below. The widened
view still needs a terrain seam review; complete region coverage remains pending.

## Coplanar road-paint milestone

Actual original tile base292/paint293 share a horizontal source plane. RenderDoc
pixel history proves that independent triangle interpolation put paint one D32
rounding unit behind its base at a failed pixel; equal-depth neighbours passed.
The bounded tile decoder now classifies smaller contained coplanar faces as
authored layers, splitting material parts without moving vertices. ABI v9 retains
that layer in the native mesh; Vulkan applies a small dynamic constant depth bias
to those world parts only. Ordinary faces/backdrop reset bias to zero. Colour and
ID share the same depth/alpha draw; no legacy order-table recovery is involved.

Asset1536, scene22 and remaining CPU/host suites, VS Release_dev x64, native GPU
and fresh PSX GPU regressions PASS. The actual normal-FOV paint interior has45000
pixels:17050 wrong IDs before, zero after; wide FOV/original art has35470 interior
pixels, zero wrong IDs. Original/fixed bound vertex/index/image bytes match exactly;
all captures retain80 backdrop/876 world/10 auxiliary UI draws and no messages.
GPU fixtures prove holes, nearer occlusion, both draw orders, IDs and retirement.
See [terrain evidence](../r03-terrain-validation.md) for exact scope and limitations.
The widened original-art capture still shows a triangular ground seam. Diagnose
that distinct native surface before accepting R03; R04 remains unstarted.

Subsequent source/capture evidence identifies incorrect float matrix cancellation
at large city coordinates. Double CPU view/model-first composition passes12
original-placement coordinates and actual GPU large-origin near-face/order/ID/
retirement checks. Normal/wide paint interiors still have zero wrong IDs.
The grass planes then agreed within captured slope spread8.01e-8, exposing actual
coplanar source-overlap z-fighting. Subsequent canonical horizontal fragment depth
gives equal-height parts identical equations: the last submitted ordinary native
face owns a physical tie, while authored paint keeps bounded D32 bias. GPU11685
pixels per order, holes/nearer occlusion/IDs/retirement PASS. Actual normal/wide
visible overlap masks39558/71120 reduce wrong IDs17677/34474 to0; bound geometry/
all-mip art match before/after. Reproduced stippling passes; source UV boundaries,
sloped coplanar surfaces and full region/silhouette/water/drive coverage do not.
Continue those remaining R03 gates before any R04 implementation.

## Handoff

Latest [original-tree evidence](../r03-cutout-validation.md) uses RenderDoc1.41:
44 two-sided parts have the expected alpha/depth policy; three tree footprints
pass scoped holes/IDs/occlusion checks at normal/wide FOV. Reproduced hardware
filtering errors are corrected by native FP32 decoded-texel interpolation.
Fresh1.41 captures pass all40598 colour samples with the original two-byte gate,
and retain exact bound geometry/all-mip hashes. Fifteen fractional GPU cases and
six fractional-mip/large-UV/last-level cases pass, with zero final ownership;
native/PSX/CPU regressions PASS. Terrain interior ID masks also still pass.
Five initial HIGH replay warnings remain; GPU cost is unaccepted. Fork426c9a3
pushed/gitlink staged; six files hash-restored/all10 VS pairs/Design.
Subsequent [view/boundary tests](../r03-tree-view-validation.md) pass3079701 eligible
colours/87152 neighbour-band comparisons across eleven views; all44 parts have
original-art opaque colour evidence,36 normal/HD. Part751 has only35 original
pixels. Excluded boundaries, eight HD parts and GPU cost remain unaccepted.
Additional eastnorth views expose another grass/sidewalk stippled overlap; west
views expose ground/backdrop coverage needing camera/source diagnosis. Tree
tests do not accept those surfaces. Four1.41 pixel histories identify569/681:
one-unit sidewalk depth failure; its mixed-height part disables canonical depth.
Subsequent [mixed-height evidence](../r03-mixed-height-validation.md) confirms both
tops at worldY17. Bounded consecutive ranges preserve source indices/logical IDs;
11456/2639 normal/wide pixels reduce wrong ownership4462/2111 ->0. Source-colour
56030 and tree regression542851 samples pass; uploads/geometry remain exact.
CPU17 new bounds/fence checks, native/PSX GPU and VS build PASS. Physical world
draws are now1023 for876 parts; four replay warnings and cost stay pending.
Forkae83dd2 pushed/gitlink staged; six files restored/all10 VS pairs/Design.
Direct source reads match530 ordinary slots to530 copies; old west spawn has an
empty resident cell, not loading. Replacement start[-7000,-229300] has actual
normal/wide captured GPU camera[-7726,303,-230042], cross-checked from ten source
placements/MVPs/post-VS outputs. Scoped road1217385 colour/ID/depth and4616
occlusion samples pass; all729600 lower pixels/view have native IDs/D32<1.
Exact source buffers/mips/ranges stay unchanged. Subsequent backdrop3861031 colour/
795828 visible-sky samples pass; no visible sky lacks source coverage. The wide
blue cap matches original1x1 RGB[140,165,206], not a native gap. Negative control
rejects one blue byte+5. Auxiliary14/11 partitions into4/1 fully clipped PGXP
draws (51 vertices/17 triangles, unchanged colour) plus ten screen-space draws/
342 vertices. Exact producer/batching cause,605/edges and four HIGH warnings remain open.
Fresh direct build/native GPU/capture/33s replay and normal810425 pixel rerun pass;
full direct report equals MCP report. [Evidence](../r03-west-road-validation.md).
[Exact attribution](../r03-auxiliary-producer-validation.md) proves Car#11 body39/
wheel3 12 vertices in both views; HD/VRAM state explains4/1 batches. These are R06
migration debt. [Count scopes](../r03-count-contract-validation.md) now separate
matching static inventory from unchanged failed auxiliary/combined checks;
vertical sides, silhouettes/region/cost remain unchecked. Do not advance to R04.

[Part605 source clipping](../r03-clipped-static-validation.md) subsequently passes
212270 horizontal source-colour/ID/depth samples and117029 outside-footprint
crop preservation checks across normal/wide views. A copied colour mutation is
rejected.125 excluded raster/depth pixels and vertical ranges remain pending;
no full west gate or later stage is accepted. Production source is unchanged.

[Vertical face evidence](../r03-vertical-static-validation.md) confirms part605's
side range is back-facing/unchanged in both views. Visible part762 passes175832
source-colour/ID/depth samples and29158 outside-footprint crop checks, with source
buffers/full mips exact and five mutation controls rejected.78 facade raster
pixels, other vertical surfaces and earlier exclusions remain pending.
Ten more visible parts now pass709948 source-colour/ID/depth and12136 occlusion
checks. Opaque within-draw source occlusion/order reversal is verified;504 raster/
65 ambiguous pixels remain excluded. Other surfaces/region/cost still block R04.
[Captured warning provenance](../r03-capture-warning-provenance.md) attributes the
four stored HIGH messages to1.41 instrumentation through structured/source
evidence; exact injected stack is uncollected. Direct validation-enabled native
GPU suite passes without recorded VUID. No message/clean flag is waived, and
this does not accept city pixels, edges, silhouettes or cost.
Subsequent762 fixed-grid/source-artwork checks resolve its78 edge exclusions
across two views, with175892 complete facade samples and no exclusions.
The retained normal779 edge failure is subsequently explained by FP32 viewport
rounding before fixed-grid coverage.22 part/views pass886209 colour/ID/D32 and
185206 outside-footprint samples with0 raster exclusions; source/post footprints
agree. Three sky-pixel corruptions/order controls pass. Subsequent bounded5e-7
depth decisions resolve32 of66 exclusions:886226 source-colour/ID/D32 and12167
occlusion samples PASS.34 ownership-ambiguous samples pass separate attachment
integrity checks but initially lack a unique source winner. Eight copied
corruptions/order controls pass. Subsequent [raster-depth traces](../r03-raster-depth-trace-validation.md)
verify all34 decisions, including three exact ties, using source-covered primitives
and GPU input/shader outputs.22 part/views now pass886246 colour/ID/D32/12181
occlusion samples with0 exclusions. Seven trace/comparison controls reject;
no full surface/region/cost gate or universal offline raster model is accepted.
Subsequent complete605 horizontal checks resolve its125 exclusions:212377 colour/
ID/D32 and117047 outside-footprint samples PASS,0 exclusions.24 raster variants
identify the tested clip/viewport model;74 canonical ties and501/605 equation/
shader hashes match. Seven pixel/four trace controls reject; order reversal passes.
Sky boundaries, other surfaces/full region/silhouette/cost remain pending. See the latest vertical
evidence for this device-scoped arithmetic model and preserved failed receipts.

[Complete tested backdrop pixels](../r03-full-backdrop-validation.md) subsequently
resolve all206 sky edge exclusions:3861830 colour/796034 visible sky and285370
outside-source preservation checks PASS,0 exclusions. Source/post masks match;
seven controls reject,404 raw hashes are intact and actual1.41 attachment/artwork/
state replays match. Other static surfaces/full region/silhouette/cost remain open.

Record changed files, actual checks/results, unresolved limits and the next stage
in the [active plan](../implementation-plan.md), current status and recent context.
Every Vulkan change also updates the unscheduled OpenGL compatibility backlog.
Pass the [shared validation gates](../validation-and-completion.md); a planned
checklist is not evidence. Update the matching implemented product surface before
claiming delivery. The active goal authorizes the sequence; advance only after
every acceptance gate of this stage passes with verifiable native evidence.

The [fixture exit fix](../r03-fixture-exit-validation.md) closes a false-success
test gate: real unavailable Vulkan exits1, fresh native/PSX GPU suites pass and
normal startup retains native resources. This does not accept remaining R03 gates.

## Complete-view attribution and observed sky absences (2026-10-08)

[Complete-view audit](../r03-complete-view-coverage-validation.md) checks all1023
ranges/876 parts/33 world materials per view.3351166 observed world pixels have
source/post geometry attribution,0 footprint differences. All8919 observed
geometry-covered sky absences are source-alpha explained;72 complete traces
resolve58 thresholds with16048 steps and exact backdrop/ID/depth preservation.
Six resource/geometry and20 sky controls reject; originals/settings preserved.
This is not independent nearest-world ownership, complete final colour, filled-
hole detection or all-asset re-decoding. Keep the region/silhouette gate open.
Next: complete-scene source colour/alpha/depth composition and expected winner,
then remaining full region/resource/cost requirements. R03 incomplete/R04 unstarted.

## Source-generated composition and pending decisions (2026-10-08)

[Complete-scene composition](../r03-complete-composition-validation.md) evaluates
every ordered world range before reading final native IDs/depth/colour.4145830
decided pixels pass with0 errors,40 state/pixel controls reject.1370 alpha/depth
decisions, including277 provisional source/observed-ID differences, remain
unaccepted. Bound-resource hashes and actual state match; no production change.

Acceptance requires every pixel's source-selected covering candidates, alpha,
depth competition, winner and final colour to be explained without choosing the
expected owner from observed IDs or hiding uncertainty. Expected holes must
reject filled-hole mutations. Resolve all1370 decisions under unchanged gates,
rerun the whole-scene oracle/controls, then finish source/region/silhouette/
resource/GPU-cost gates. The partial partition does not close this stage.

## Complete tested scene composition (2026-10-08)

[All-candidate references](../r03-composition-trace-validation.md) subsequently
resolve all1370 decisions/277 provisional source-ID differences.2873 explicit
source-selected primitive traces/764548 steps, raw artwork and native depth/
output comparisons explain both complete views:4147200 pixels PASS,0 exclusions;
1365 traced solid winner depths are bit-exact and five sky pixels preserve the
backdrop exactly.42 new controls reject,20 original hashes preserved. Source-
generated coverage and candidate competition precede captured-ID comparison.

The complete composition item passes for these two captures with supplemental
device shader-input/output references. A thin-facade UV failure led to derived
FP32 compatibility bounds, not an exact/universal interpolation model; strict
colour/artwork/alpha/LOD/depth gates remain. Do not equate this scope with all
original-source/region/other-view/silhouette/resource/GPU-cost gates. Evaluate
those remaining requirements next; R03 incomplete/R04 unstarted.
