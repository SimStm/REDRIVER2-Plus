---
type: Product
title: Native static region inventory and opt-in drawing
status: partial
created: 2026-10-07
tags: [okf, rendering, vulkan, native]
---

# Native static region inventory and opt-in drawing

R03 is in progress. This note describes implemented code and measured output;
it does not establish complete static-region acceptance. R04 has not started.

`-nativeregionprobe` traverses ordinary source cells in Chicago
x[-8192,16384), z[-237568,-212992), before legacy GTE model conversion. Collection
does not use camera/PVS rejection or mutate the gameplay cell usage bitset.
Duplicate cell references are removed by packed source slot; coincident distinct
objects are retained. Geometry/placements are owned, with no recycled game
pointer retained. Available high-detail resources are selected by the original
LOD table. Source animation flags are inventoried; animation is not migrated.

`-nativeregion` is explicitly incomplete opt-in drawing. It submits the same
logical region with the final original camera, quantized placement basis and
camera-facing sprite basis. Each logical object keeps one placement identity
across material parts. Missing cells/models/decodes or payload failure reject
the whole native snapshot, preserving explicit legacy-frame fallback. The
current native-effective path suppresses the legacy world globally; explicit
outside-region policy/accounting remains unfinished. Native picking, world
lighting and dynamic migration are not delivered by this surface.

Shared GPU materials compare exact content/context, then retain reference
ownership across geometry, content replacement and failed creation/reset.
Immutable owned PNG vectors are shared between the adapter and cache, while
the PsyCross C boundary still copies source pixels. Live VRAM originals are
decoded afresh; reuse compares their bytes, not only a revision/hash. C ABI v4
includes back/two-sided culling and backdrop layers; alpha-tested/sprite sources choose two-sided
cutouts. A shape transparency flag alone does not establish blend semantics.
Limits are512 meshes/4096 part instances/32MiB geometry and256 materials/64MiB
RGBA. Water/unverified blend-policy flags reject the whole opt-in view; the
selected region contains neither. These caps are not measured peak GPU allocation.

Actual high-detail city run:144 resident cells/628 references/98 duplicate refs,
530 copied distinct objects, no missing/invalid/rejected source, preserved
gameplay usage bitset.508 opaque/22 cutout/16 animated base objects, no water or
transparency-flagged objects;4814 triangles/876 submitted material parts.
At tick108 native stats show80 resident meshes/33 shared materials,
131040 geometry bytes/36414208 material bytes, nativeDraws876/world0/ready.
RenderDoc1.46 frame610 independently confirms876 D32 indexed world draws,
14442 indices, plus10 D24S8 UI draws/318 indices; no debug messages.

Initial R03 CPU tests: asset/cache1219, scene18/camera8/material13 PASS, including
shared ownership release/replacement/reset and immutable artwork lifetimes;
Android/Emscripten/PSX new region host exclusion checks PASS. VS Release_dev x64
build PASS. GPU culling readback PASS for reversed back-cull and two-sided
cutouts/holes; analytic/resize/reload/upload-failure regressions and fresh PSX
readback PASS. Khronos validation loaded/no VUID output on both direct runs.
CPU preparation diagnostics report cumulative and bounded recent120 mean/p95
separately; neither is GPU time. Before immutable sharing:45.910 ms assembly
mean/121 samples; after:3.710 ms/481 samples, including initial upload cost.
These are different cumulative windows, not an isolated benchmark. Actual
recent120 window at tick600: collection mean0.308/p950.396 ms, material/instance
assembly mean2.983/p953.540 ms. These exclude initial uploads by bounded window;
GPU time/peak allocation are unknown. The inventory/material milestone fork
304c14a was committed/pushed to origin/master. Current backdrop fork83ccdbc
is also committed/pushed; parent gitlink staged at that newer commit.

The original sky/painted horizon now uses four bounded original MODEL decodes
and copied HorizonTextures/UV/page/CLUT bindings captured before sky GTE
projection. No projected screen geometry or borrowed game pointer is retained.
The backend draws backdrop instances before world instances regardless of
snapshot array order, with static depth test/write disabled. Camera translation
is removed; the inverse copied rotation preserves the original view-space
vertical offset, including fixed-point quantization. Original sky/reflection
brightness is copied per frame. Art is decoded from current original VRAM,
independently of the named-world PNG manifest. Changes to layout retire copied
meshes; page/CLUT/pixel/filter changes rebuild materials through exact comparison.
Every sky source must be observed; failure rejects the complete native snapshot.

Actual city source models0..3 each have30 vertices/20 family21 quads. The new
frame submits956 native draws:876 world plus80 backdrop/160 sky triangles.
RenderDoc1.46 frame500 independently confirms80 D32 depth-off draws/480 indices,
876 D32 depth-on draws/14442 indices and10 D24S8 UI draws; zero messages.
Depth probes remain1.0 after backdrop; after world, sky remains1.0 and sampled
world depths become0.985639/0.999395. Latest CPU tests1503/scene20/camera12/
material13 and VS Release_dev x64 PASS. Actual GPU readback PASS for order,
no sky depth write, translation, rotation, UI preservation and retirement;
fresh legacy PSX regression PASS. This is a backdrop milestone, not R03 acceptance.
Frame-local immutable art sharing avoids repeated sky tile decodes across
models while still resolving current VRAM every frame. Recent120 backdrop CPU
mean8.676/p959.135 ms before,4.000/p954.185 ms after; separate windows, not total
frame/GPU timing. Before/after1920x1080 sky-only captures are pixel-exact equal.

See [R03 stage](../rendering/stages/03-complete-static-region.md) and
[partial evidence](../rendering/r03-validation.md).

Native ABI v6 retains explicit trilinear filtering. Original world and backdrop use
it when bilinear filtering is enabled; disabling the setting selects nearest
level-zero sampling. Generic nearest/linear callers keep their level-zero
contract. Trilinear materials own a full floor-halved chain down to1x1, including
thin and NPOT images. Lower levels average linear-light RGB weighted by alpha,
then unpremultiply; transparent RGB cannot bleed into visible texels. Uniform
alpha scaling chooses the closest attainable base cutout coverage while keeping
zero-alpha holes. Tiny/equal-alpha levels cannot represent every source coverage
fraction exactly; this does not guarantee every distant silhouette.

The64MiB native RGBA cap includes all derived levels before pixel traversal.
Vulkan uploads every owned level, transitions the complete subresource range,
views the complete chain and clamps trilinear sampling to its last level. Actual
GPU tests distinguish base-only black aliasing from exact sRGB188 minified gray,
preserve a transparent half and reject red halo bleed. Full-chain submitted-upload
failure retains image/staging/command/fence until actual completion, then retires
to zero ownership. City steady state uses49926792 owned RGBA bytes and956 native
draws. The earlier filter replacement overlapped old/new texture sets and briefly
exceeded the cap. ABI v6 fixes this by copying a per-instance sampling selection.
Original materials keep one full chain regardless of current sampling. Vulkan
prepares nearest/linear/trilinear descriptor sets once over that image, then
binds the selected set for each instance without mutating in-flight descriptors.
Level-zero samplers clamp maxLOD0; trilinear clamps to the last level. A trilinear
override on an incomplete generic material is rejected atomically. Shared images
can sample differently for different instances in the same frame.

New cumulative native-effective/fallback submission and ready-material-upload
counters distinguish stable samples from uninterrupted transitions. Actual city
eight filter changes between ticks120-1080 keep all960 intervening submissions
native, total uploads61/no new uploads, fallback0,956 draws,49926792 RGBA bytes
and the observed image handle unchanged. Analytic GPU pixels independently prove
eight switches and nearest/trilinear instances sharing one image without fallback,
upload or ownership growth. CPU sampling8, existing suites and VS build PASS.
Forkd3e5a40 contains the generic mip/backend milestone, committed/pushed to the
project fork. Sampling fix forkc051051 was committed/pushed and its parent
gitlink staged at that milestone. No full R03 acceptance is claimed; later milestones accept individual gates.

## Native logical picking and explicit scene availability

Current C ABI v8 includes asynchronous native picking and explicit frame states.
The R32_UINT world attachment uses the same transforms/depth/culling/alpha discard
as colour. Backdrop writes0; original UI does not touch it. One copied pixel is
read only after the presenter fence, then mapped through a bounded submitted-frame
identity table. Window scaling, resize, supersession/cancellation and resource/
scene generation validation prevent stale or retargeted selections. The game owns
copied model/placement/material-part/texture metadata with catalog checks; no
borrowed source pointers or old PSX packet picks are used. Native face highlight/
export remain pending; logical picking passes its declared R03 gate.

Automatic old-scene recovery is disabled. Each world frame declares preparation;
explicit source/upload PENDING is expected loading, while missing publication,
unsupported, stale, invalid, budget and upload results are failures. Unavailable
frames clear colour, suppress old-world/shadow/composite draws, retain required
ordered VRAM/UI work and show a reason/resource diagnostic even with F11 closed.
Ready/loading/failed/auxiliary submissions have separate counters. A frontend or
loading-screen frame with no world request/boundary is an unmigrated auxiliary
consumer, never a native success. World boundary without publication is an error.
The producer state is consumed per submission so loading cannot leak into later
frames. Recovery republishes native resources after real synchronization proof.

Existing OpenGL remains selectable independently. Native startup options with a
non-Vulkan backend fail explicitly instead of silently drawing an old scene;
the analytic UI selector is disabled on unsupported backends. Remaining frontend/
HUD/minimap/VRAM/effects consumers belong to R10; dynamic world to R06 and full
city residency to R04. Preserve these dependencies until each consumer migrates.
Actual validation is in [R03 evidence](../rendering/r03-validation.md); only
picking, explicit-failure and controlled camera/settings gates are accepted,
not the complete static region.

## Source and publication coverage accounting

A bounded frame-local ledger uses packed cell slots only as accounting keys;
logical placement identities remain separate. Repeated cell references (including
straddlers) select each source once. Copied part counts must match each published
object exactly, with no duplicate/missing publication. Inconsistency rejects the
native snapshot explicitly. Failure clears publication progress while preserving
source diagnostics; the next frame resets all slot history.

The read-only original DrawMapPSX hook observes candidates after the existing
PVS/frustum test and before queue capacity limits. A candidate is inside when any
selected region cell referenced its slot; otherwise it is outside. Separate
uncopied-inside, duplicate-view and invalid-slot counts expose errors. This is
neither a full resident-city inventory nor a GPU visibility count. No game pointer
is retained, model conversion or gameplay usage state is changed by the hook.

F11 reports producer tick, publication state and counts alongside separately
labelled backend submission statistics. Actual Chicago tick266:530 selected/
copied/published,876 copied/published parts,228 view-inside/78 outside,0 uncopied/
duplicate-view/invalid. The preceding backend submission at tick265 is native
READY with956 native/0 old-world/10 UI draws; owned geometry148320/RGBA49926792
bytes,61 material uploads. The78 outside candidates remain omitted by this bounded
R03 slice; full-city migration belongs to R04. This milestone does not accept
remaining R03 visual/movement gates.

## Controlled camera and runtime settings

Four quarter turns at focal256/128, camera translation and nearest/trilinear
sampling retain native956/world0 without resource growth. Actual HD mod-off/on
uses the normal revision-aware API and native owned images; restored world pixels
are byte-exact. Geometry stays148320 bytes; RGBA returns to49926792 from2174132.
Final native GPU resources contain61 images/allocations and zero pending upload
staging/commands/fences or retiring materials. These are payload/handle observations,
not total GPU allocation or performance measurements.

A133-step mapped-pad fixture exercises ordinary car physics and the original
follow camera, including steering/braking and wide-FOV/mod-off driving. It assigns
only mapped input, never car/camera transforms. Actual GPU street captures and
native counters pass the R03 camera/settings gate. Near road-marking strips found
during driving were subsequently fixed as described below. Terrain seam review,
full region acceptance and R04 remain pending. See
[actual evidence](../rendering/r03-validation.md).

## Authored coplanar terrain layers

The original tile decoder recognizes smaller convex horizontal faces contained
in earlier coplanar faces with the same winding. It assigns bounded depth layers
and splits material parts by material/layer, preserving all positions, indices,
normals, UVs and bounds. Non-tile, sloped, partial-overlap, equal-area or opposite-
facing geometry remains ordinary. Limits are1024 candidate faces and eight layers;
exhaustion rejects the entire decoded resource without partial publication.
Decoder version2 prevents reuse of older part layouts.

Native ABI v9 copies/validates layer0..8 into owned meshes. Vulkan applies constant
depth bias -4 per authored layer to world draws; ordinary geometry/backdrop reset
it to zero, with no slope/clamp or optional device feature. The same colour/ID
draw preserves cutout holes and nearer occlusion; vertices are never displaced.
This addresses captured one-D32-unit interpolation differences between coplanar
road base/paint triangles, without deriving ordering from old PSX packets.

CPU asset1536/scene22 plus existing suites and GPU alpha/occlusion/order/picking/
retirement checks PASS. Actual normal and widened/original-art paint interiors
have zero wrong IDs, and original/fixed bound vertex/index/image bytes match.
See [terrain evidence](../rendering/r03-terrain-validation.md). Cross-model seams,
sloped coplanar layering and full region/silhouette coverage are not accepted by
this bounded fix. Existing OpenGL and unmigrated auxiliary consumers are retained.

Native matrix composition also cancels large city translations in view/model
space before projection, retaining double CPU intermediates until the final float
shader matrix. Actual source-placement coordinate regressions and GPU near-face/
two-origin/order/ID/retirement checks pass; city paint coverage is preserved.
This corrects artificial depth separation of coplanar grass instances. Subsequent
canonical horizontal fragment depth resolves the reproduced equal-height overlaps:
copied mesh geometry supplies a world-height equation shared by all matching
planes. Ordinary ties belong to the last submitted native face; bounded authored
paint keeps its four-D32-unit layer bias. Colour/IDs share alpha/depth, genuinely
nearer geometry still wins, and invalid inverse/coefficients fail explicitly.
Other/pitched/edge-on geometry retains native raster depth; no old-world recovery
or optional GPU feature is added. Early/late depth dependencies preserve resource
synchronization, and retirement clears the derived CPU plane metadata.
CPU/GPU fixtures and actual normal/wide Chicago ownership masks pass, with exact
before/after bound geometry and artwork. Source UV/artwork boundaries, sloped
coplanar surfaces and full static-region/silhouette coverage remain unaccepted.
See [terrain validation](../rendering/r03-terrain-validation.md).

New native capture inspection uses the user-configured RenderDoc1.41 rdc tools.
The version-checked script exports native colour/ID/depth and bound buffer/all-mip
hashes, retaining initial and per-event diagnostics. Actual normal/HD and wide/
original pixels/resources exactly match prior evidence; five HIGH initial replay
barrier messages remain explicit. This is tooling/resource evidence, not full
region acceptance; see [the executed transition](../rendering/renderdoc-141-validation.md).

The inspector also records44 original-tree two-sided parts and alpha/depth policy.
Independent actual-frame source-alpha/ID/depth/occlusion masks pass for three tree
footprints in both settings; the colour oracle still has five normal failures.
No silhouette gate is accepted; [tree evidence](../rendering/r03-cutout-validation.md)
records diagnostic shader replacements and their restored resources.
Subsequent traces match all five shader outputs to framebuffer bytes; texel-centre
sampling matches explicit fetches at the diagnosed pixel. Device capability
tooling reports texel/mip/raster precision. Fractional filtering accuracy remains
unaccepted; neither a CTS allowance nor a changed tolerance establishes it.
The native GPU suite now characterizes five constant-UV 2x2 sRGB patterns across
three filters. Uniformity, nearest texels, linear/trilinear consistency and exact
resource retirement pass; RenderDoc1.41 confirms all ten filtered native readbacks
equal the corresponding city pixels. Strict ideal deviations remain explicit and
unaccepted. This is diagnostic test coverage, not a production sampling change
or completed original-tree gate; native/PSX GPU regressions pass.

Subsequent production correction interpolates decoded sRGB texels and linear
alpha in FP32: four clamped fetches for base-only bilinear, up to eight for
trilinear with the selected implicit LOD. Nearest stays unchanged; large finite
UVs clamp before integer conversion, non-finite UVs discard, and mip indices
stay within owned levels. Draw-uniform sampling mode uses private padding at92;
push size112/depth offset96 and public ABI9 are preserved. Upload ownership,
formats, fences, retirement and backend selection retain existing contracts.
The fractional fixture now gates unchanged two-byte ideal accuracy (ten passing
filtered checks), and six mip/UV/last-level GPU regressions pass exact bytes.
Fresh RenderDoc1.41 captures pass40598 scoped tree colour checks and retain exact
bound geometry/artwork hashes; existing terrain ownership masks still pass.
Native/PSX/CPU suites PASS. Five initial replay warnings and performance cost
remain unaccepted, as do other views/silhouettes/full region. See the subsequent
correction in [tree evidence](../rendering/r03-cutout-validation.md).

Additional actual RenderDoc1.41 heading/exposure views preserve content hashes,
bindings and native draw inventory after excluding capture-local resource IDs.
A tightened independent eleven-view oracle passes3079701 eligible colour pixels
and87152 neighbour-band comparisons. All44 tree parts have original-art opaque
colour evidence,36 normal/HD; part751 has only35 original pixels. Excluded edges,
remaining HD parts, eastnorth terrain stippling and west ground/camera coverage
still prevent full silhouette/region acceptance. Four/five replay warnings remain.
Runtime code is unchanged; see [view validation](../rendering/r03-tree-view-validation.md).

Subsequent mixed-height depth support derives bounded consecutive indexed runs,
so a vertical side in the same material part does not disable the top's canonical
world-height equation. Triangle order/source bytes/logical ID remain unchanged;
sides use raster depth and backdrop remains one draw. Physical frame/mesh ranges
cap at4096, with separate32MiB CPU metadata ownership and existing fence retirement.
Public ABI9/shaders/upload budgets are unchanged. Actual city now has1023 world
draws for876 logical parts,80 backdrop and10 auxiliary draws; GPU geometry148320
and all artwork payloads/hashes are unchanged. Source-confirmed grass569/sidewalk681
share worldY17:11456/2639 normal/wide overlap pixels improve4462/2111 wrong ->0.
56030 independent sidewalk colour and542851 tree regression samples pass;
CPU17 range/bounds/fence checks, native/PSX GPU and VS build PASS. Four replay
warnings, GPU cost, vertical-side city pixels, west camera/ground and remaining
region/silhouette gates remain pending. [Evidence](../rendering/r03-mixed-height-validation.md).

Read-only west diagnosis matches144 cell lists/628 ordinary references/530 slots
to530 copied objects; sampled old west ground cell has no list despite residency.
All876 captured parts/80 geometry pairs are exported using RenderDoc1.41. Copied
source footprints do not cover16 hypothetical points, with billboard/camera
limits retained. Replacement[-7000,-229300] now has normal/wide native camera
[-7726,303,-230042] reconstructed from captured road MVP and cross-checked against
ten source placements/post-VS outputs; this is not a same-frame CPU debugger read.
Scoped1217385 source-colour/ID/depth and4616 occlusion samples pass with unchanged
strict tolerance, exact resources/876 ordered bindings and729600 native lower
pixels/view. Subsequent [part605 homogeneous clipping](../rendering/r03-clipped-static-validation.md)
passes212270 horizontal source-colour/ID/depth and117029 outside-footprint crop
checks. Vertical ranges,125 excluded raster/depth pixels/full silhouettes remain pending.
Subsequent [visible vertical face proof](../rendering/r03-vertical-static-validation.md)
passes175832 part762 source-colour/ID/depth pixels, while part605's back-facing
side range preserves all attachments. Other vertical surfaces/78 facade raster
pixels and previous exclusions remain unaccepted.
Ten additional facade/roof parts pass709948 source-colour/ID/depth/12136 occlusion
checks after independent within-draw oracle correction, with unchanged pixel
gates, exact buffers/mips and order/mutation controls.504 raster/65 ambiguous
pixels remain pending; full region is not accepted. Auxiliary
14/11 against expected10 retains a failed count gate; all run after native world,
write shared colour but leave native IDs/D32 byte-identical. Four HIGH warnings
remain. Direct MSBuild/native GPU and fresh1.41 capture/replay/pixel rerun pass;
direct report equals MCP report, without30s RPC. [Evidence](../rendering/r03-west-road-validation.md).
Subsequent independent six-plane source clipping/artwork oracle passes3861031
backdrop colours/795828 visible-sky samples; every visible sky pixel has source
coverage. Wide cap's1x1 RGB[140,165,206] and projected polygon boundary match;
one blue byte+5 in a private negative control is rejected. Native backdrop IDs0/
D32=1 throughout. Auxiliary14/11 partitions into4/1 fully clipped PGXP draws/
51 vertices/17 triangles plus ten screen-space draws/342 vertices. Prefix colour
is unchanged. [Exact producer/batching evidence](../rendering/r03-auxiliary-producer-validation.md)
now identifies12 wheel3/39 body vertices of Car#11, model1/high/palette3 in both
views; HD wheel RGBA versus VRAM state explains4/1 batches. Retain these consumers
until R06 migration; screen-space consumers await R10. The ten-draw gate stays
failed under the [explicit count scopes](../rendering/r03-count-contract-validation.md):
static inventory matches, auxiliary baseline/combined remain false. Geometry/edges
and full R03 remain open; count success never implies completed migration.

The fixture command-line result now separates unrelated startup, success and
failure. Failed initialization/GPU self-tests exit1; acceptance still requires
a fresh native PASS report and applicable source-pixel/resource evidence.
[Actual negative control and startup checks](../rendering/r03-fixture-exit-validation.md)
pass without changing the scene/backend/fence contracts or accepting full R03.

[Structured warning provenance](../rendering/r03-capture-warning-provenance.md)
shows four stored capture messages and no application buffer barriers in the
two west frames; official1.41 internal readback matches their invalid masks.
Direct native GPU suite passes with validation enabled/no recorded VUID. Exact
injected stack is uncollected, all messages/false clean flag are preserved and
original-city/full R03 coverage remains unaccepted. Production source is unchanged.
Subsequent [facade edge checks](../rendering/r03-vertical-static-validation.md#complete-part762-raster-footprint-and-remaining-edge-failure-2026-10-08)
accept complete762 footprint/source-colour/ID/D32 in two frames, resolving its78
raster exclusions under unchanged strict limits. The broader exact-grid oracle
retains a failing normal779 coverage pixel in its original FP64 receipt. The
subsequent FP32 viewport model explains it without changing production rendering:
22 part/views pass886209 colour/ID/D32/185206 outside-footprint samples with no
raster exclusions. Source/post coverage agrees; three copied sky-pixel corruptions
are detected and order reversal is identical. A subsequent bounded source-depth
interval using the existing5e-7 D32 gate resolves32 of66 exclusions; counts become
886226 source-colour/ID/D32 and12167 occlusion checks.34 ambiguous samples pass
separate native/preserved attachment-integrity checks. Eight mutations/order
controls pass. Subsequent [source-bounded fragment traces](../rendering/r03-raster-depth-trace-validation.md)
verify those34 decisions, including three exact D32 ties, with source-selected
primitives, completed shader outputs and exact native depth bits.22 part/views
pass886246 colour/ID/D32/12181 occlusion samples with0 exclusions; seven trace/
comparison controls reject. Full region/silhouette/cost gates remain open. This
uses a GPU raster-input reference, not a universal exact offline winner model.
The subsequent complete605 horizontal footprint passes212377 colour/ID/D32/
117047 outside-footprint samples with0 exclusions, resolving125 prior exclusions.
24 raster variants establish its device-scoped clip/viewport model;74 canonical
ties and501/605 equation/shader hashes match. Seven pixel/four trace controls
reject/order reversal passes. Remaining sky/other surfaces/full region/cost stay open.

[Complete tested backdrop pixels](../rendering/r03-full-backdrop-validation.md)
now include all206 previous sky edge exclusions:3861830 source-colour/796034
visible sky and285370 outside-source preservation checks PASS,0 exclusions.
Source/post coverage masks match; seven copied corruptions reject and404 original
raw hashes remain intact. Direct1.41 replays match six attachments/28 materials/
196 mips/80 cull-depth states per view; four HIGH messages remain visible.
This accepts the two tested backdrop views, not full R03/other surfaces/cost.

[Complete-view geometry/observed sky audit](../rendering/r03-complete-view-coverage-validation.md)
now covers1023 ranges/876 parts/33 world materials per view and3351166 observed
world pixels with source/post attribution,0 footprint differences.8919 observed
geometry-covered sky absences are source-alpha explained, including58 threshold
pixels/72 completed fragment traces/16048 steps. Exact backdrop/ID/depth policy,
source sampled alpha, six resource/geometry and20 sky controls pass. This checks
bound decoded resources, not independent re-decode of all original assets or
expected nearest ownership/filled-hole detection/all final colour. Full R03 stays open.

[Source-generated complete-scene composition](../rendering/r03-complete-composition-validation.md)
now predicts from every ordered range before reading final attachments.4145830
decided pixels pass colour/ID/depth/sky gates,40 state/pixel mutations reject.
1370 depth/alpha decisions remain unaccepted, with277 provisional source/observed
ID differences pending candidate evidence. Actual native state matches; no
production change. Complete frame/asset-source/region/silhouette/resource/cost
acceptance and retained R06/R10 migrations remain pending.

[Complete tested scene composition](../rendering/r03-composition-trace-validation.md)
subsequently resolves1370 exclusions/277 provisional ID differences via every
covering source candidate:2873 traces/764548 steps and raw artwork references.
Both existing views pass4147200 colour/ID/depth/sky pixels,0 errors/exclusions;
1365 traced solid winner D32 values match bit-exactly/five sky pixels preserve
the backdrop.42 new controls reject/20 original hashes intact. The thin-facade
UV investigation yields conservative FP32 compatibility bounds, not precise or
universal interpolation proof. This accepts two-view composition with device
references; full source/region/other-view/silhouette/resource/cost remains open.
