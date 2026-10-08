---
type: Product
title: Vulkan static world geometry and shadow foundation
description: Resident static lighting and receiving shadows, pooled source meshes and bounded near/far shadow reach.
tags: [rendering, vulkan, shadows, streaming]
---

# Vulkan static world geometry and shadow foundation

The resident shadow foundation and an experimental static colour baseline of
[world-space lighting](../roadmap/planned/world-space-lighting.md) are implemented.
Source-normal lighting, pooled static colour coverage and two nested shadow
volumes are delivered below. Expanded streaming, dynamic adapters and camera-fitted
cascades remain planned.

## Controls and behaviour

Enable **Enhanced renderer**, **Modern shadows** and **Static world shadow
casters (Vulkan)** in the F11 Graphics panel. The last option persists as
`worldshadowcasters=1` in `developer_modern_mesh.ini`, default off. It replaces
the projected legacy caster path in single-player Vulkan while active; imported
modern meshes continue casting. Disabling it restores the legacy caster setting.

`DeveloperWorldShadow.cpp` reads opaque static models from resident map cells
before camera/PVS/backface rejection. It uses its own packed-object deduplication
and does not modify gameplay's cell-usage bitset, region loading or draw distance.
Missing/loading/reused region slots are rejected by the existing cell iterator.
Model pointers and decoded geometry are not retained between frames, avoiding
stale model-slot/level caches. Supported source and converted polygon types are
10/11, 20/21 and 22/23, with the game's quad diagonal and degenerate triangles.
Unsupported models are skipped and counted.

Sprites, alpha/transparent/water models, animated and smashable objects are
excluded. Vehicles, pedestrians and special event-cell lists are outside this
slice. Texture cutouts are not evaluated by this opaque depth pipeline.

The generic C-compatible `PsyX_ModernShadow_SetWorldTriangles` copies a triangle
list into ordinary CPU memory; Vulkan uploads it only after the prior frame fence.
The submission is consumed once, not retained into frontend frames. Capacity is
196,608 vertices (65,536 triangles). Exceeding it rejects that whole collection and logs a warning rather than
presenting an arbitrary partial batch. With distant shadows active, the game
retries smaller complete far volumes, then near only (details below). The game
vector, backend CPU copy and mapped GPU buffer each reserve 2.25 MiB; aggregate
payload capacity is 6.75 MiB plus allocator/driver overhead. This is an allocation
bound, not a measured peak process-memory result. Statistics include
`PsyXModernMeshStats.worldCasterTriangles`; warmed build time is logged every
120 submissions, including the CPU copy but excluding GPU upload/rasterization.

## Coordinate and receiver fixes

Game scenery/cameras use Y down; vehicle physics, glTF local geometry and the
sun UI use Y up. The Vulkan game boundary converts sun/vehicle-light/volume Y,
reflects imported local Y in the instance world transform and grounds fixtures
with the corresponding sign. A genuine zero terrain height is retained when
`sdGetCell` finds a surface. OpenGL's existing convention is preserved.

The Vulkan composite receives the configured shadow centre and half-size
explicitly. Its sunlight fade uses distance from that centre instead of inferring
extent from a rotating matrix element. Both receivers reject light depth below
zero; modern receivers now use nine-tap PCF. Receiver biases remain distinct:
legacy reconstruction is approximate and does not supply reliable mesh normals.

## Validation and limits

The collection log now reports resident/unavailable cells in the conservative
light-volume search columns, unique policy-excluded/outside objects, unsupported model instances,
current/reserved game-vector payload bytes, resident region IDs and failed-frame
count (now failed submissions, since a frame can retry). Counts describe the
last sample; mean/peak CPU cost and failures cover 120 submissions. Missing cells are not empty cells or an exact count of required
sun blockers. Outside-triangle counts and F11 collection attempts/capacity failures
make light selection and range reduction visible. The diagnostic does not request
streaming or alter gameplay state.
See [budget evidence](../changes/2026-10-06/world-shadow-budget-diagnostics/index.md).

Visual Studio MCP `Release_dev|x64` builds, shader regeneration and `-vkpsxtest`
passed. Four RenderDoc captures replayed without debug messages. The stock
Chicago debug start submitted 4,266 triangles from 762 static objects in one draw;
the vertex buffer was byte-identical with high/low sun and a 90-degree change in
spawn/camera heading. The opt-out capture contained no static world draw.
Median collection/copy time was 174.5 microseconds across ten 120-frame windows
(172.3–178.1 microseconds), under RenderDoc on this machine. Native GPU time and other
hardware/cities and event scenes remain unvalidated. Later checkpoints below
add sampled moving-route/region evidence without expanded streaming. See the [change record](../changes/2026-10-06/world-space-shadow-foundation/index.md).

The initial foundation used one 2048-square map; current game coverage uses the
near/far atlas documented below and still depends on already resident regions. Low sun
can require unloaded casters. The legacy depth composite, road depth offsets,
prelit colours and far-depth precision remain limitations. This milestone does
not establish that every artifact from the user's original frame is resolved.
Streaming, dynamic adapters and camera-fitted cascades remain coordinated work.

## Original static colour, lighting and resident range (2026-10-06)

The initial [single-building unlit baseline](../changes/2026-10-06/static-world-unlit-baseline/index.md)
now uses a shared bounded source cache. In F11 enable **Native static scenery
(Vulkan)** (`staticunlit`, default 0), enhanced rendering and both PGXP options.
Requires single player, nearest filtering and no enabled registered texture
overrides. Other paths/materials keep their original renderer.

**Light native static scenery** (`staticlighting`, default 1) uses actual world
face normals for diffuse sun, ambient fill and local lights. Direct sunlight
receives modern mapped shadows; ambient and unrelated lights remain independent.
**Native lighting strength** (`staticlightingstrength`, default 0.75, 0-1) blends
with the textures' existing prelit appearance. Disable lighting for unlit
comparison. Original nearest texels, face modulation, dithering and attachment
colour-space conversion are retained. Imported PBR meshes keep their existing
material response. Source debug 4/5/6/7 shows shadow factor/sun dot product/world
normal/lighting, while legacy diagnostics retain their depth-derived meaning.

The adapter accepts selected original and LOD models with converted types
11/21/23, 8-256 polygons and at most four materials, preserving the quad diagonal.
Whole unsupported models or face UV rectangles containing zero PSX texels stay
legacy. Sprites, alpha/transparency, water, road tiles, animated and smashable
objects are excluded. Vehicles/pedestrians and special event lists are not source
adapters. Alpha-tested source migration is not implemented.

Eight resource keys cover model/LOD slot, original face-colour table/combo/dither
and yaw; sixteen original page/CLUT texture resources are pooled across them.
**Native scenery object limit** (`staticscenelimit`) defaults 64, clamped 1-128.
Mesh/material buffers are reused across copied placements. PsyCross publishes
bounded CPU frame-instance lists and issues a draw per material/placement, rather
than GPU instancing. Colour and shadow use the same buffers/world transforms;
visibility expires after recording. Off-camera retained placements still cast;
the world collector excludes those identities to avoid duplicate geometry.

**Native scenery range** (`staticrange`) defaults 18,000, clamped 10,000-40,000
world units. A separate float camera test and packed-object deduplication add
eligible resident candidates beyond the old camera/PVS list, nearest first.
It does not write GTE/cell-usage state or request region loads. Unsupported or
budget-exhausted legacy-list candidates keep their old draw; extension-only
candidates gain a draw only when accepted. Traffic, physics and missions are
unchanged. This is a partial draw-distance milestone, not expanded streaming.

Catalog generation/revision handles cover original identity, actual LOD and
instanced vertex source. No model pointers are cached. Unloaded/reused region
slots, model handles or VRAM contents invalidate resources. Placements beyond
range+4000 expire; source keys unused for 60 frames can be replaced. At most two
new resource attempts occur per frame. Meshes are destroyed before textures.
Payload caps: GPU original pixels 4 MiB, native vertices 576 KiB, CPU texture
snapshots 524,800 bytes plus refresh scratch. These are bounds, not measured peak
process memory. Resource creation can stall; CPU windows are logged separately.

## Near and distant resident shadows

**Distant resident shadows (Vulkan)** (`farshadows`, default 0) requires enhanced
rendering, modern shadows and static world casters. The distant half-size follows
native scenery range, reduced when capacity requires; **Shadow volume size** remains the near
half-size. Far must exceed near. F11 reports actual active volume count/extent,
including budget fallback. Both modern and depth-derived legacy receivers use
this coverage. Source/modern casters draw in both; projected legacy casters use
near only. Dynamic caster coverage has not been expanded by this setting.

Game Vulkan uses a 4096x2048 D32 atlas with two 2048-square tiles. Each volume
keeps independently snapped placement around the player. Shared receiver code
clamps filtering taps within a tile, blends the near transition and fades at the
far boundary. Bias is scaled to preserve an approximate world-space offset.
These nested volumes are not camera-frustum-fitted cascades. The atlas allocates
32 MiB even with far off (previous single D32 map: 16 MiB); the standalone fixture
retains one map. Two active tiles double source/modern shadow raster work.

Distant collection stays within 65,536 triangles and resident region ownership.
If it overflows, requested far coverage steps through 75%, 50%, then near only,
skipping collapsed tiers. Each retry is a complete collection. Reduced coverage
waits 120 successful frames before probing one higher tier; control changes reset
the policy. Other failures clear the batch; near itself can also fail. No partial
triangle list is published. F11 shows effective extent and attempt/failure counts.
Unloaded sunlight blockers remain missing; low sun can require extra residency.

## Previous lighting/range checkpoint

VS MCP Release_dev x64, shader regeneration and newly launched PSX self-test
passed. Chicago spawn had ten native placements/eight source keys and one
extension-only placement. Twenty-five colour/material draws matched fifty
near/far shadow draws by buffers, vertex counts and transformed matrices.
Same-frame shader replacement proved native receiving (83,210 changed pixels)
and distant contribution (80,112); these are contribution checks, not accuracy
scores. Low sun retained identical caster vertices. A recorded route crossed
region-slot changes with both volumes at 18,000 and no failed submissions.
Before the light-demand optimization below, 40,000 triggered near-only fallback. Opt-out retained 4,266 world triangles and no native colour draws.

A VS-launched run without RenderDoc sampled warm static CPU work at median
126.0 us and resident collection/copy at median 790.85 us. Creation stalls remain;
GPU replay counters do not establish native GPU performance. A fresh installed
RenderDoc 1.46 first replay was clean; MCP 1.41 retains capture-instrumentation
warnings. Four test INIs were restored byte-exactly. See
[lighting/range evidence](../changes/2026-10-06/static-world-lighting-and-range/index.md).

Still pending: dynamic source adapters, cutouts/material expansion, streamed
camera/light residency, inspector picking, broad VRAM/frontend invalidation,
other cities, long routes, quality profiles and native GPU/peak-memory results.
Legacy road depth offsets and reconstructed receivers retain their limitations.
This checkpoint does not claim every original artifact is resolved. OpenGL is
tracked separately without priority or execution order.

## Light-demand selection and current evidence (2026-10-06)

The collector now queries the exact independently snapped near/far matrices from
PsyCross CPU light state. Conservative padded cell columns, per-collection bounds
from actual source vertices and six Vulkan clip planes reject irrelevant objects
and triangles. Legacy bounding-sphere hints are insufficient 3D bounds. Source
bounds include instanced data and Q12 yaw padding; no cross-frame pointers survive.
Only triangles entirely outside a common plane of every active volume are removed;
boundary-crossers remain whole for GPU clipping. Near depth uses Vulkan zero.
Selection is independent of camera/PVS and retains native-caster deduplication.

Final VS MCP Release_dev x64 build and a newly launched PSX self-test passed.
Fresh installed RenderDoc1.46 capture/first replay returned zero messages.
At spawn, 21,766 became 8,751 triangles; on ATTRACT.400 frame900 at 18k, 28,632
became 8,535. Both comparisons have zero changed colour pixels and zero changed
near/far depth texels. At 40k/low sun, 48,477 triangles now fit both volumes;
near depth matches the old near fallback and far contains 1,018,432 written texels.
This is sampled parity/contribution, not universal visual acceptance.

MSVC isolated tests using actual query/plane/range-policy functions passed volume
boundaries, crossing triangles, invalid inputs, complete tier reduction and
120-frame recovery with mocked collection capacity. Real optimized route captures
fit without tier reduction; real dense-scene recovery acceptance remains pending.
Three VS CPU windows at low-sun 18k had collection/copy median651.0 us; two warm
static windows median127.5 us. No isolated GPU/FPS or controlled timing-gain claim.
No shaders changed or were regenerated in this continuation. MCP1.41 retains its
instrumentation warnings. Four test INIs were restored byte-exactly; PsyCross
446b96d is pushed to origin/master with the parent gitlink staged. See the
[current validation record](../changes/2026-10-06/light-demand-shadow-budgets/index.md).

Next: source/LOD cache transitions and camera/light residency budgets on repeatable
routes before new streaming. Dynamic categories, cutouts, other cities, long
routes and native GPU/peak-memory acceptance remain pending. The OpenGL port is
unscheduled; current Vulkan work is not blocked by it.
