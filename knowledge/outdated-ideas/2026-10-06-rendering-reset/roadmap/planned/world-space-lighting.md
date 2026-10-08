---
type: Roadmap
title: World-space static scenery and lighting
status: planned
created: 2026-10-06
tags: [rendering, vulkan, lighting, shadows, streaming]
---

# World-space static scenery and lighting

## Problem

Camera-selected PSX primitives omit off-camera objects and back faces needed by
directional shadows. A finite single map and depth-derived legacy normals limit
coverage and lighting quality. See the
[source review](../../discussions/renderer-modernization/shadow-lighting-foundation.md).

## Adoption and intended behaviour

The user adopted the recommended incremental direction on 2026-10-06. Preserve
original assets and gameplay, retain PsyCross services, and develop Vulkan first.
The first milestone uses source-world static casters in already resident regions.
Modern colour rendering and cascades follow validated geometry/depth contracts.
The user also linked [draw distance and streaming](draw-distance-streaming.md)
as related work; this does not authorize arbitrary global range/limit increases.

## Scope and milestones

1. Diagnose current shadow projection and receiver math. Supply bounded opaque
   static world geometry independently of camera/PVS/backface rejection. Preserve
   gameplay cell-usage state, invalidate model caches on streamed slot reuse and
   level changes, expose an opt-in Vulkan control and measure coverage/cost.
2. Render a bounded static scenery slice from the same source meshes/transforms,
   first unlit with original texture/alpha semantics, then explicit mesh normals
   and controlled lighting. Preserve mutual depth and compatibility effects.
3. Extend resident-region selection with the draw-distance roadmap's measured
   camera and light demand. Keep simulation/traffic/mission activation unchanged.
   State what is missing when light-demand cells are unavailable.
4. Add measured cascaded shadow coverage, stable fitting, filtering, bias and
   transitions; validate low sun, distance, camera turns and region changes.
5. Refine linear lighting/material/environment response on that foundation.

## Non-goals

No whole-engine/PsyCross replacement, gameplay changes, arbitrary full-city
residency, new asset requirement, OpenGL implementation, animated vehicle/ped
conversion, ray tracing or unmeasured unlimited range.

The user's later full-graphics destination is recorded in the
[screenshot/native-renderer discussion](../../discussions/renderer-modernization/geometry-shadows-and-distance.md).
It supersedes the long-term hybrid goal, but this record still represents the
adopted static scope. Bounded rewrite/category stages have not been executed.

## Dependencies and risks

Use the existing Vulkan modern mesh/depth bridge, catalog generations/revisions,
resident cells and developer settings. Camera and shadow visibility are distinct
consumers of shared resident data. Extra light demand can require regions outside
the camera PVS; neither consumer may assume the other's list is sufficient.
Depth offsets in the legacy road path and prelit colours still limit the hybrid
receiver until milestone 2. Alpha-tested or dynamic content needs explicit policy.

## Acceptance criteria

Static source geometry includes faces/instances independent of camera rejection,
without changing reconstructed cell state or rendering unavailable regions.
Bounds and cache lifetimes are explicit; incompatible paths preserve fallback.
Sun-range math is independent of angle. Camera turns and sun sweeps have captured
evidence, with CPU/GPU/memory results and unvalidated categories stated separately.
Later distance expansion must meet the linked streaming roadmap's route/budget
criteria before its completion is claimed.

## Validation plan

Visual Studio MCP `Release_dev|x64`, shader regeneration, Vulkan PSX self-test,
RenderDoc controlled captures, camera/sun changes and original-city inspection.
Compare available caster data and shadow maps; API validation alone does not
establish visual correctness. Update the unscheduled OpenGL compatibility backlog
for every Vulkan modification. Record actual results per completed milestone;
move this record to done only when the adopted scope is delivered with a product
document. Partial milestones remain explicitly partial.

## Delivery checkpoint — 2026-10-06

The resident opaque static caster slice and Vulkan coordinate/receiver fixes are
implemented; see [product evidence](../../product/world-space-lighting.md) and
[validation](../../changes/2026-10-06/world-space-shadow-foundation/index.md).
Full route/region acceptance, GPU cost, colour migration and subsequent milestones
remain pending. This broad record remains planned until its adopted scope is
complete; it is not evidence that the later milestones exist.

## Measurement continuation - 2026-10-06

The user authorized incremental continuation of the shared-geometry direction.
Resident budget diagnostics and recorded-route/low-sun/range stress evidence
are delivered in the [measurement record](../../changes/2026-10-06/world-shadow-budget-diagnostics/index.md).
At increased reach, missing residency and near-capacity triangle counts are now
measured. Static colour migration, streaming budgets and cascades remain pending;
this record remains planned. Clean validation requires consistent capture tooling:
MCP 1.41 captures retain instrumentation warnings; fresh 1.46 capture/replay is clean.

## Static colour baseline continuation - 2026-10-06

One opt-in original-texture unlit building now shares colour/shadow geometry,
with resident/catalog/material lifetime checks, independent frame visibility and
whole-model fallback for unsupported/cutout content. Original nearest textures and
dither are retained; controlled mesh-normal lighting and receiving shadows remain
pending. [Evidence](../../changes/2026-10-06/static-world-unlit-baseline/index.md)
records VS/shader/PSX success, sampled parity, shared-buffer depth state and route
invalidation/culling. Broader categories, native cost, hybrid occlusion, alpha,
streaming and cascades remain acceptance work. This record stays planned.

## Lighting, shared coverage and resident reach - 2026-10-06

Original static diffuse lighting/receiving shadows, eight pooled source keys,
sixteen textures and bounded frame placements are implemented. Resident camera
selection extends beyond the old list without new loads/gameplay changes. Two
nested 2048-square shadow volumes share a 4096x2048 atlas, with near/far blending
and whole-collection near fallback on overflow. These are not camera-fitted CSM.
[Product](../../product/world-space-lighting.md) and
[evidence](../../changes/2026-10-06/static-world-lighting-and-range/index.md)
record actual matrix/receiving tests, low sun, region routes, VS/PSX/API checks
and CPU/allocation bounds. Range 18,000 retained both volumes on the sampled route;
40,000 hit caster capacity. Full streaming/city/category/GPU acceptance remains
pending; this broad record stays planned.

## Light-demand and complete range budget checkpoint - 2026-10-06

Actual snapped near/far volumes now gate resident cells, source-derived bounds and
whole triangles. At 18k, sampled spawn/route images and maps match baseline with
about 60%/70% fewer submitted triangles; the sampled low-sun 40k route fits both
volumes. Complete coverage tiers reduce range on capacity pressure and probe
recovery after 120 successful frames. F11 exposes selection and retries. Build,
PSX, fresh 1.46 replay and isolated boundary/policy checks passed; optimized route
fits, so real tier-recovery stress remains pending. See
[evidence](../../changes/2026-10-06/light-demand-shadow-budgets/index.md).
No expanded residency, dynamic categories or camera-fitted cascades are delivered.
Next: LOD/cache transitions and bounded camera/light residency on repeatable
routes. This record remains planned until its broader acceptance is met.
