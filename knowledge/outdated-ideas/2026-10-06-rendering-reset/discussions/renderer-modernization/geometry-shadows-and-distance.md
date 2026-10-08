---
type: Discussion
title: Shared geometry, complete shadow coverage and measured draw distance
status: exploring
created: 2026-10-06
updated: 2026-10-06
tags: [rendering, geometry, shadows, streaming, vulkan]
---

# Shared geometry, complete shadow coverage and measured draw distance

## Decision status and purpose

Supporting synthesis for the existing [lighting discussion](shadow-lighting-foundation.md).
The user requested a plain-language recommendation covering static and dynamic
geometry, PsyCross boundaries and draw distance. The user authorized incremental
execution on 2026-10-06, beginning with measurements. This note does not establish
delivered behaviour.
The adopted [static roadmap](../../roadmap/planned/world-space-lighting.md) and
[distance roadmap](../../roadmap/planned/draw-distance-streaming.md) retain their
scopes. Vehicle/pedestrian conversion remains outside the adopted static scope.

**Latest user direction (2026-10-06, after screenshot testing):** the desired
destination is replacement of the entire graphics pipeline, retaining Driver 2
content, mechanics, physics and simulation. This supersedes incremental hybrid
enhancement as the long-term goal. The current request is analysis and a staged
recommendation, not execution of that broader rewrite. Existing static milestones
remain partial evidence; dynamic/full-native scopes need bounded roadmap adoption.
The updated proposal and screenshot findings below govern the next discussion.

## Historical starting point (before the delivered static continuations)

Inspected on 2026-10-06: parent HEAD `2e93f215`, dirty parent work preserved;
PsyCross HEAD `1f8b5a513d8733e2168cff17601dd9f82b1bf131`, clean submodule.
[DeveloperWorldShadow.cpp](../../../../../src_rebuild/utils/DeveloperWorldShadow.cpp)
collects resident opaque static models independently of camera selection.
It excludes sprites, alpha/transparent/water, animated/smashable models and
multiplayer. Cars, pedestrians and event-object lists are not supplied by it.
The batch is bounded; overflow rejects the whole batch. Collection/copy happens
each frame, rather than through a persistent reusable source-mesh cache.

[draw.c](../../../../../src_rebuild/Game/C/draw.c), `DrawMapPSX`, still uses camera/PVS
selection and the reconstructed PSX colour path. The delivered slice changes
shadow input, not static colour submission, streaming or draw distance.
[Product evidence](../../product/world-space-lighting.md) records actual builds
and sampled captures. Full visual acceptance, native GPU time, moving region
routes and other cities remain unvalidated. No build/capture was run for this
documentation-only follow-up.

## Recommendation in ordinary terms

Give the renderer a shared description of the scene before projecting it onto
the screen. Each object supplies a model, its position/orientation, current
shape and material policy. The colour pass and shadow pass consume that same
description, with different visibility decisions. A building behind the camera
can still block sunlight reaching a visible street.

Retain original assets, low-poly appearance and reconstructed gameplay. This
architecture improves submission and lighting; it does not add geometric detail
that the assets do not contain. Higher-detail replacement assets are optional
later work, not a requirement to fix shadows.

## Small glossary, with application to this game

- **Mesh:** the vertices and triangles describing a model, such as one lamp post.
- **Instance and transform:** one placement of that mesh, including position,
  rotation and scale. Fifty identical lamp posts can share one mesh with fifty
  placements. Cache static meshes; update moving placements each frame.
- **World space:** positions in the city before camera projection. Both camera
  and sunlight can inspect the same building from their own viewpoints.
- **Normal:** a direction pointing out of a surface, used to distinguish how
  sunlight hits a floor versus a wall. Use source or derived face normals rather
  than estimating every surface direction from the completed screen image.
- **Depth buffer:** per-pixel depth used to decide what is in front. Shared depth
  rules are necessary for a modern car to disappear correctly behind a wall.
- **Culling:** skipping objects irrelevant to one rendering pass. Being outside
  the camera view does not necessarily make an object irrelevant to sunlight.
- **LOD (level of detail):** a simpler distant representation. It reduces work,
  but changes must not cause obvious object or shadow jumps.
- **Streaming/residency:** loading/unloading city resources and tracking which
  are available now. A shadow cannot use an unloaded building without a policy
  to request or substitute the required resource.
- **Shadow map:** a depth image from the light's viewpoint, used to determine
  which surfaces the light can reach. Its resolution and coverage are finite.
- **Cascades:** several sun-shadow maps for distance bands. Near shadows receive
  more detail; farther shadows use coarser coverage with controlled transitions.
- **Alpha test/cutout:** texture holes, such as gaps between fence bars. Colour
  and shadow must discard the same holes; glass/smoke need a different policy.

## Earlier incremental sequence and observable completion criteria

### 1. Establish measurements and repeatable scenes

Continue the existing resident slice's acceptance work before expanding range.
Use fixed scenes and routes: low/overhead sun, camera turns, dense streets and
region boundaries. Expose missing geometry, rejected categories, triangle caps,
resident memory, loading stalls and CPU/GPU frame costs. FPS at the 30 Hz game
ceiling alone is insufficient. Completion means the first limiting resource and
remaining visual defects are identified with captures and measured budgets.

### 2. Render a bounded static scene from shared source geometry

Create reusable mesh resources with explicit generation/revision invalidation
for streamed slot reuse, level changes and asset overrides. Submit instance
transforms, original textures, UVs and alpha semantics. First verify an unlit
appearance baseline, then normals and controlled light. Unlit means showing
the texture/colour without the new lighting calculation, not a black image.
Preserve mutual legacy/modern depth, avoid rendering each converted object twice,
and retain per-category fallback during migration. Completion means the same
building geometry and placement produce its visible image and shadow, including
off-camera blockers, without texture/occlusion regressions.

### 3. Coordinate camera/light residency and LOD budgets

Work with the existing draw-distance record rather than duplicating it. Track
resources needed for visible colour and resources needed to cast onto visible
receivers. Low sun can require blockers farther away in the light direction.
Measure a bounded request/prefetch policy and stable LOD transitions; do not load
the entire city or increase every global limit. Define graceful quality/range
reduction when a budget is exhausted instead of arbitrary holes or an unexplained
whole-batch disappearance. Completion means repeatable routes gain validated
range without missing regions, unbounded memory or changes to traffic, collision,
missions and simulation activation.

### 4. Add dynamic geometry in explicit categories

Recommended order: rigid vehicles, vehicle damage/deformation and moving parts,
pedestrians, then event/movable/breakable props. Each adapter reads the owning
game system's current placement and shape; colour and shadow use the same frame's
state. Inspect the actual animation representation before choosing a technique;
do not assume all original models use a modern skeleton. A damaged car's shadow
must follow its damaged shape, not an intact cached mesh.

Define casting and receiving separately. A car casts onto the road and receives
a building's shadow; a cutout fence preserves holes. Camera-facing sprites need
an explicit shadow representation and validation, not automatic reuse from the
camera view. Smoke, water and glass should not automatically cast opaque black
silhouettes. Completion is category-specific: movement, deformation, animation,
mutual shadows and fallback validated. This extension requires a separately
adopted bounded roadmap scope before implementation.

### 5. Expand and stabilize sun-shadow coverage

Evaluate two to four cascades using measured hardware budgets, fitted caster
bounds, stable placement and blended transitions. Cascades depend on adequate
resident geometry; they do not recreate missing models. Tune filtering and depth
bias after coordinate/geometry correctness. Filtering smooths edges; bias is a
small comparison offset that can reduce self-shadow speckles but can detach a
shadow if excessive. Completion means declared near/far ranges work through sun
sweeps, motion and region transitions with documented CPU/GPU/memory cost.

### 6. Refine lighting and quality profiles

Separate direct sunlight, ambient fill and local lights. Blocking the sun should
not automatically suppress all unrelated light. Preserve the original textures'
existing shading deliberately to avoid double lighting. Finish linear-colour
handling and measured quality profiles on the shared scene foundation. Current
point lights add illumination but do not cast their own mapped shadows; selected
headlight/local-light shadows would be a separate later scope. Do not promise
shadows from every light by default. PBR material authoring and replacement assets
can follow if desired; ray tracing or a new engine is not required.

## Ownership and alternatives

The game owns city decoding, object identities, animation/deformation, residency
decisions and original texture semantics. PsyCross owns generic mesh resources,
uploads, draw passes, depth, shadow maps and Vulkan synchronization. Extend the
existing public renderer boundary without teaching PsyCross game-specific car
or mission rules. Keep UI, audio, input and compatibility services intact.

A larger single shadow map is a useful measured tuning option, but cannot repair
missing geometry or unlimited coverage. Further screen-image lighting patches
can bridge migration, but retain uncertain surface normals and material response.
A complete engine replacement introduces broader compatibility work. The earlier
incremental recommendation preserved useful working systems and permitted
category-level comparisons. The user's new full-graphics goal now calls for
native world rendering as the destination, rather than permanent hybrid lighting.

## Documentation contract and open decisions

Keep this explanation and decisions in the existing discussion folder. Use the
static and draw-distance roadmaps for adopted work; add linked bounded dynamic
records only after adoption. Every milestone should state: player-visible
problem/example, geometry categories, changed ownership, preserved behaviour,
budgets, fallback, acceptance scenes, actual evidence and remaining limitations.
Product/current-status records describe only delivered behaviour; change records
hold actual validation. Every Vulkan implementation still updates the
unscheduled [OpenGL backlog](../../../../roadmap/planned/port-vulkan-to-opengl-compatibility.md).

Open: reference hardware and target render cost, desired validated distance,
original-art lighting strength, supported sprite/transparent shadow policies,
and adoption/order of dynamic categories. Begin measurement on the available
machine; do not invent universal range or minimum-GPU performance claims.

## Screenshot review and full graphics replacement proposal (2026-10-06)

### Inspected state and evidence boundaries

Parent HEAD is still `2e93f215` with extensive existing local implementation and
documentation changes. PsyCross is clean at `446b96d`. This continuation reads
source, saved settings, the latest game log and the three supplied screenshots.
No new build, live capture, shader replacement or rendering fix was executed.
Screenshot observations are not GPU pixel-history evidence. Saved settings/logs
may reflect a later state than an individual screenshot.

The last [budget milestone](../../changes/2026-10-06/light-demand-shadow-budgets/index.md)
proved less unnecessary geometry, sampled baseline image/depth parity and a
fitting 40k resident route. It did not prove physically correct shadows under
all FOV/sun/material states. The user's new artifacts keep visual acceptance open.

- **Image 1:** dark scenery-shaped regions appear over the sky with distant
  shadows and altered FOV. In [psx_composite.frag](../../../../../src_rebuild/PsyCross/src/render/vk_shaders/psx_composite.frag), `worldPixel` accepts depths
  below 0.999999 except approximately 0.5. The separate `sunPixel` sky threshold
  only gates added legacy lighting; `GameShadowLit` still runs on `worldPixel`.
  Thus backdrop depths documented in that shader can receive mapped shadows if
  their reconstructed positions enter the enlarged volume. This is a verified
  receiver-classification weakness and a strong explanation, not confirmation
  of the exact screenshot's depth/matrix. Check FOV/projection consistency and
  originating draws with RenderDoc before attributing every shape to that cause.
- **Images 2/3:** facade patterns can come from legacy position reconstruction,
  quantized/offset depth, inferred normals, self-shadow bias and original prelit
  textures. Several effects can overlap. A shadow on the apparent opposite side
  does not by itself prove a reversed sun vector; compare the same pixel with
  mapped receiving, legacy lighting and original colour independently disabled.
  True native receivers already use source normals, but most scene categories
  still use the compatibility path.
- **Image 2:** distant shadows are visibly unchecked, active volumes = 1,
  distant half-size = 0, collection attempts = 1, capacity failures = 0, and
  world triangles = 939 / 65536. Capacity did not trigger range reduction in
  that frame. Native scenery range 22706 does not enable a distant shadow volume.
  The on-screen cutoff is consistent with finite world coverage projected into
  the image; it is not evidence of a Vulkan half-screen restriction.
- **Saved settings/latest log:** bilinearFiltering=1, native setting enabled,
  staticlightingstrength=0.5 and farshadows=0. `PolicyAllowsMigration` in
  [DeveloperStaticScene.cpp](../../../../../src_rebuild/utils/DeveloperStaticScene.cpp) requires nearest filtering and rejects enabled
  registered overrides. The latest baseline log shows only eight imported
  modern meshes and no native static adapter resources. The requested native
  checkbox is not an effective-coverage indicator; the saved bilinear state
  prevents migration. Do not assert identical settings for all supplied images.
- Static native colour migration still has eight resource keys, sixteen textures,
  up to128 placements (default64), format/material eligibility and unsupported
  category fallback. World casters include only resident supported opaque
  statics; sprites, cutouts, animation, damage and event/dynamic paths are absent.
  Some apparent distant buildings may belong to painted skyline/horizon assets
  (`DrawSkyDome` / `PlotHorizonMDL`), rather than real city placements. Identify
  their source before promising they can cast physical shadows.

### Recommended destination and PsyCross boundary

Build a native Vulkan world renderer consuming original-file geometry/materials
and copied game state before GTE projection, ordering tables and PSX primitive
conversion. One authoritative world-space representation supplies camera draws,
shadow casters and explicit normals/materials. Use a normal perspective camera
and native depth; compose sky, alpha cutouts, transparency, effects and HUD with
explicit policies instead of identifying surfaces from a completed legacy image.

Original models still need decoding: compressed/paletted textures, face UVs,
colours, source instancing, LODs and region placements do not become Vulkan-ready
by removing PsyCross. A file decoder can build indexed meshes, GPU textures and
stable instance identities once into an owned cache. Conversion to GLB files is
not required. Read original data without rewriting or redistributing game assets.
Native GPU geometry need not replace gameplay's collision or fixed-point storage.

PsyCross is not intrinsically required for native 3D. It currently also provides
input, audio, CD compatibility, timing, GTE helpers and platform services; deleting
all of it would add unrelated replacement work. Reuse/extract useful Vulkan
device/upload/synchronization code behind a clear renderer boundary. Keep one
device/presentation owner, not competing swapchains for the same game window.
Retain needed non-graphics services during migration. Full library removal can
be evaluated after callers are inventoried, without blocking graphics replacement.

### Original-city loading and visibility

Loading an entire city's decoded visual data is a reasonable desktop diagnostic
prototype if measured memory permits. It does not imply submitting every object
every frame. Keep camera visibility and per-light caster selection separate;
an off-camera building can shadow a visible road. Real occlusion, LOD and finite
GPU budgets remain useful in a modern renderer.

`UpdateSpoolPC` in [spool.c](../../../../../src_rebuild/Game/C/spool.c) already caches a fixed 16 MiB spool byte block on desktop. That
does not establish fully decoded city residency: `LoadRegionData`, area model
slots, texture pages and gameplay cell ownership still determine usable data.
A separate visual decoder/cache must establish region/model/texture provenance
and lifetime instead of globally inflating or repurposing gameplay arrays.
Start with a complete native region, measure full-city decoded memory/load time,
then choose eager residency or bounded asynchronous sector streaming. Camera and
light demands request visual resources independently of AI/collision/mission
activation. Preserve simulation timing; higher visual refresh must not accelerate
physics. Do not promise a universal memory budget before asset inventory.

### Sun shadows and many local lights

Fit stable cascaded shadow maps to camera distance bands with an extended caster
search toward the sun, including off-camera blockers. Each band needs actual
resident geometry, consistent native depth, source normals, slope/normal-aware
bias and matched colour/shadow cutout rules. Cascades allocate detail; they do not
create missing geometry. Preserve a bounded quality policy and test low sun,
wide FOV, different cities and region transitions. Replacing the hybrid receiver
removes a major source of approximation, not every shadow-map limitation.

Deferred rendering writes actual material/normal/depth data to a G-buffer before
lighting. Clustered Forward+ groups lights in screen/depth volumes and evaluates
only the lights relevant to a pixel, while shading native materials directly.
Both can support many local lights with spatial selection. Recommend native
geometry/camera/depth first, then clustered Forward+ as the simpler starting
lighting path for this small custom renderer and its trees/glass/effects. Keep
scene/material interfaces independent of that choice. Add/choose deferred for
opaque materials if measured post-processing/lighting needs justify its bandwidth
and transparency complexity. Deferred alone does not fix loading or shadow input.

Thousands of registered street/vehicle lights are a target workload, not a claim
that thousands may illuminate every pixel or cast simultaneous free shadows.
Shadowed lights need their own budget, priorities, atlas allocation and update
frequency; point maps typically require six directions. The current eight-light
UBO/four composite-point-light caps are software choices, not Vulkan's universal
light limit. Use scalable light buffers and cluster lists with explicit overflow.

### Proposed bounded execution stages

1. Record the three artifact cases with actual effective settings and pixel
   history. Add acceptance scenes for sky exclusion, facade self-shadow, FOV,
   far range and alpha materials. This is baseline diagnosis, not a commitment
   to repeatedly expanding legacy image-space fixes.
2. Implement a read-only original-city visual decoder and stable resource cache.
   Inventory static models, roads, horizon assets, palettes, instances and LODs;
   prove one complete region can exist independently of gameplay resident slots.
3. Render that region natively with its own conventional camera/depth and original
   art, including roads, trees/cutouts, sky and transparent materials. Compare
   appearance/occlusion in an unlit mode before adding light. Expose effective
   category coverage and fallback reasons. No PSX 3D triangles for accepted region.
4. Expand to full city/visual streaming using measured CPU/GPU/memory/load budgets,
   stable LOD transitions and separate camera/light demand. Preserve collisions,
   traffic, missions and original simulation activation.
5. Add native cascaded sun shadows and controlled lighting on the complete static
   scene. Capture sun sweeps, camera/FOV sweeps, off-camera blockers and long routes;
   validate alpha and near/far transitions, not only API cleanliness or old parity.
6. Migrate dynamic categories: vehicles with wheels/damage, pedestrians/animation,
   smashables and bridges/event scenery. Read live game-owned shapes/transforms;
   matching colour/shadow geometry is required without replacing simulation.
7. Add clustered local lighting and selective local shadows, then HDR/exposure,
   AO/fog/reflections and materials as measured needs. Retire PSX world drawing
   when category coverage and effects are complete; HUD/frontend compatibility
   may remain independently. Retire non-graphics PsyCross only if separately useful.

This sequence is a proposal. The user stated the broad graphics destination;
implementation of all stages has not been requested in this analysis turn.
Before execution, adopt one bounded native-region roadmap scope and revise links
to the existing static/distance plans. OpenGL implementation remains deferred,
and every actual Vulkan change still belongs in the unscheduled compatibility log.

Open decisions: target desktop hardware/memory/frame-time budget; initial city
and acceptance route; original stylized/prelit art versus physically relit material
policy; treatment of horizon paintings, damage/alpha/water; graphics interpolation
with preserved simulation; and whether full-city eager residency fits measurements.
Use original assets as baseline; replacement assets are optional later work.

Primary architectural references:
[Khronos Forward/Forward+/Deferred guide](https://docs.vulkan.org/tutorial/latest/Building_a_Simple_Engine/Advanced_Topics/Forward_ForwardPlus_Deferred.html),
[Khronos GPU draw/culling sample](https://docs.vulkan.org/samples/latest/samples/performance/multi_draw_indirect/README.html).
GPUInfo supplied an RTX 3060 Ti storage-buffer report lookup link only; no numeric
limits were verified. Context7 spec excerpts were supplementary; the primary
Khronos tutorial supports the lighting-path comparison.

## References and history

- [Khronos instancing sample](https://docs.vulkan.org/samples/latest/samples/api/instancing/README.html):
  reusable geometry with per-instance data; conceptual reference, not a drop-in adapter.
- [Microsoft cascaded shadow maps](https://learn.microsoft.com/en-us/windows/win32/dxtecharts/cascaded-shadow-maps):
  distance partitions, resolution allocation, stabilization and transition costs.
- 2026-10-06: documented a plain-language shared-scene recommendation, dynamic
  category extension and coordinated distance budgets. Documentation only;
  existing static adoption retained and new dynamic scope remains a proposal.
- 2026-10-06 continuation: user authorized incremental execution. Added resident
  budget diagnostics and tested fixed sun states, distance stress and a recorded
  route through region-slot changes. See the
  [measurement record](../../changes/2026-10-06/world-shadow-budget-diagnostics/index.md).
  Static colour migration and dynamic adapters remain future milestones.
- 2026-10-06 next checkpoint: one opt-in unlit original building uses shared
  colour/shadow geometry, original textures/modulation/dither, explicit lifetime
  checks and separate frame visibility. Captured depth state, a 202-pixel sampled
  parity difference and route shadow-only visibility establish a first colour
  baseline, not full-city acceptance. See
  [evidence](../../changes/2026-10-06/static-world-unlit-baseline/index.md).
  Next: controlled source-normal lighting/receiving shadows, material/category/
  cache budgets and native profiling before expanded distance/cascades.

- 2026-10-06 lighting/reach checkpoint: source-normal diffuse lighting and native
  shadow receiving are captured; shared static resources replace the one-building
  baseline. Resident visual selection and two nested shadow volumes extend reach
  without gameplay/load changes. Same-capture replacements isolate receiving and
  far contribution. Range 18,000 held both volumes on the sampled route; 40,000
  exceeded capacity and preserved near shadows. See
  [evidence](../../changes/2026-10-06/static-world-lighting-and-range/index.md).
  This is bounded static delivery, not dynamic/streaming or camera-fitted CSM
  completion. Next: explicit camera/light demand budgets, material/LOD coverage
  and route transitions; isolated GPU timing and minimum hardware remain open.

- 2026-10-06 light-demand checkpoint: exact snapped CPU light volumes now select
  resident cells, actual source bounds and conservative whole triangles. Legacy
  bounding-sphere hints proved insufficient; real vertex bounds restored exact
  sampled near/far parity. Default spawn/route maps and image are unchanged with
  about 60%/70% fewer triangles; the sampled 40k route retains distant shadows.
  Complete range tiers provide bounded reduction/recovery, verified in isolated
  actual-function tests with mocked capacity; real dense-scene recovery is open.
  [Evidence](../../changes/2026-10-06/light-demand-shadow-budgets/index.md).
  Next: LOD/cache transitions and camera/light residency policy before streaming.
  Dynamic category adoption, long-route/city and native GPU acceptance remain open.
- 2026-10-06 screenshot review: user reports sky silhouettes, facade artifacts
  and missing distant shadows, then explicitly requests full graphics replacement
  preserving content/gameplay/physics. Source audit identifies permissive sky
  shadow receiving and bilinear-gated native migration; exact frame causes remain
  unconfirmed. Updated recommendation targets complete native city rendering and
  separate visual residency, retaining useful PsyCross services. Clustered
  Forward+ is the proposed initial many-light path; deferred remains an option.
  Documentation only; new bounded implementation scope awaits adoption.
