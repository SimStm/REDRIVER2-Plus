---
type: Discussion
title: Shadow correctness and the world-space lighting foundation
status: adopted
created: 2026-10-06
updated: 2026-10-06
tags: [rendering, lighting, shadows, vulkan, architecture]
---

# Shadow correctness and the world-space lighting foundation

## Request and decision status

The initial request was an analysis-only review of sun-angle artifacts and
near-only shading. The user subsequently adopted the incremental direction on
2026-10-06, authorizing continuation and linking draw-distance/streaming. The
[world-space roadmap](../../roadmap/planned/world-space-lighting.md) records that
scope; its [product note](../../product/world-space-lighting.md) describes the
delivered resident static shadow slice. Vulkan remains selected; OpenGL is deferred.

Recommendation: retain PsyCross compatibility services, but feed original model
geometry and world transforms to a scene path before camera projection and
visibility rejection. Use independent camera/light visibility, then extend
distance coverage with cascaded shadows. Migrating colour rendering to the same
world geometry supplies a firmer lighting foundation than the depth composite.

The [plain-language follow-up](geometry-shadows-and-distance.md) explains shared
geometry, dynamic categories and coordinated distance budgets. Dynamic conversion
is recommended future scope, not part of the adopted static roadmap.

## Historical source review before the first milestone

Source inspected on 2026-10-06: parent HEAD `2e93f215`, PsyCross HEAD `c564607e`.
The parent has existing uncommitted game/tooling/documentation work and a staged
fork gitlink. PsyCross is clean. These are source findings, not reproduction of
the user's exact faulty frame. No game launch, capture, build or renderer edit
was performed for this analysis. All source paths below are relative to this
discussion folder.

These findings describe the pre-milestone `c564607e` snapshot. The delivered
checkpoint below supersedes the fade, lower depth-bound and modern filtering
defects; the incomplete legacy colour path and finite coverage remain relevant.

### Camera geometry is an incomplete shadow scene

- [draw.c](../../../../../src_rebuild/Game/C/draw.c), `DrawMapPSX`, rejects cells
  through camera planes/PVS and objects through `FrustrumCheck16` before drawing.
  `PlotBuildingModel` projects original vertices through GTE and emits polygons
  only after camera-facing `gte_nclip` rejection (`opz > 0`). A visible building
  can therefore supply only part of its original surface to the backend.
- [PsyX_Vk.cpp](../../../../../src_rebuild/PsyCross/src/render/PsyX_Vk.cpp),
  `RecordGameLegacyShadowPass`, reuses queued main-camera draws. It excludes
  blended, offscreen, depth-disabled and queued-VRAM draws and rejects triangles
  without valid PGXP depth. These safeguards preserve compatibility, but leave
  holes in caster coverage.
- [psx_shadow.vert](../../../../../src_rebuild/PsyCross/src/render/vk_shaders/psx_shadow.vert)
  recovers world positions from projected PGXP vertices and camera/projection
  matrices. Consistent projection can be inverted; inversion cannot recreate
  surfaces or instances that were never submitted. Camera-facing billboards
  also retain their camera-facing geometry in the light pass.
- Modern meshes use their original positions/world transforms in
  `RecordGameModernShadowPass`, but both caster paths write the same map. A bad
  legacy caster can therefore affect a correctly positioned modern receiver.
  The modern pass also checks `mesh->visible`; a future scene needs separate
  camera visibility and shadow eligibility rather than one ambiguous flag.

Camera-dependent caster loss is proven by source. It explains unstable/missing
coverage; it does not by itself establish the cause of every extra dark patch.
An apparently invisible blocker needs identification in the offending capture.

### One bounded map limits distance and sun-angle coverage

`kShadowSize = 2048` and `BuildShadowMatrix` define one orthographic directional
map. [DeveloperModernMesh.cpp](../../../../../src_rebuild/utils/DeveloperModernMesh.cpp),
`ApplyLightSet`, supplies `shadowExtent` (default 2500) and the player position
as centre, falling back to the camera during transitions. The volume has lateral
half-size `extent` and light-space far distance `4 * extent`.

Consequences: receivers outside that light volume receive no mapped shadow;
low sun needs distant casters and long projections; increasing extent spreads
the same texels across more world area. Increasing map size/extent alone cannot
recover camera-culled geometry. Texel snapping already exists in
`PsyX_ModernShadowSnapCentre`; stabilization is not wholly absent. Its alternate
up vector near vertical sun is another boundary to inspect in angle sweeps.

### Receiver lighting and local defects are separate concerns

- [psx_composite.frag](../../../../../src_rebuild/PsyCross/src/render/vk_shaders/psx_composite.frag)
  reconstructs visible positions and a five-tap normal from depth, gates some
  depth bands/edges, and multiplies the already shaded legacy image. It has no
  authored surface/material normal. Silhouettes, thin surfaces, billboards and
  legacy baked lighting remain approximate; shadowing the tint also attenuates
  existing colour and unrelated added light instead of isolating the sun term.
- The same shader obtains sun fade extent with
  `1 / max(shadowMatrix[0][0], 1e-6)`. That matrix element includes the rotated
  light basis, not just projection scale. It therefore cannot reliably recover
  the configured extent as the sun rotates. Fade distance is measured from the
  **camera**, whereas the map is normally centred on the **player**. Earlier
  documentation conflated these centres. This is a verified calculation flaw
  in legacy lighting range, not a proven cause of the reported model shadow.
- [psx_modern.frag](../../../../../src_rebuild/PsyCross/src/render/vk_shaders/psx_modern.frag),
  `ShadowFactor`, uses one depth sample and an angle-dependent receiver bias;
  the legacy composite uses nine PCF samples and a different fixed bias. Legacy
  caster rasterization adds constant 1.25 / slope 1.5 depth bias. These policies
  need evaluation at grazing light angles for self-shadowing and detached edges.
- Both Vulkan receiver checks omit the lower `proj.z >= 0` light-volume bound.
  This is an incomplete clip test to audit, not evidence that it creates the
  reported phantom occluder.

The previous build/self-test and RenderDoc replays establish operation in sampled
states and API correctness, not geometric correctness across sun angles, camera
turns, distance or city streaming. The user report leaves visual acceptance open.

## Initial proposed sequence, subsequently adopted for static scope

1. **Establish a reproducible diagnosis.** Freeze a problematic object/camera,
   sweep sun azimuth/elevation and compare modern-only versus combined casters.
   Inspect the shadow depth map and offending draw/vertices. Separate wrong
   caster placement, missing surfaces, volume clipping, self-shadowing and
   legacy illumination range. Any bounded math/bias fixes follow that evidence.
2. **Supply source-world casters.** Start with static city models, instances and
   loaded cells before PVS/frustum/backface rejection. Cache reusable GPU meshes
   with explicit invalidation on resource/level changes; submit instance world
   transforms and alpha/UV semantics. Cull for the light separately, including
   off-camera casters that can reach visible receivers. Region streaming must
   provide those cells: an early hook after camera/PVS rejection is insufficient.
   This shadow-only bridge can retain legacy colour temporarily, but does not
   solve its depth-derived normals or material response.
3. **Render a bounded static scene through the same geometry.** Reuse original
   models/textures; establish unlit appearance, depth/occlusion and real or
   derived mesh normals, then add controlled direct and ambient lighting. Colour
   and shadow passes share world transforms and geometry. Treat existing vertex
   shading/prelit textures deliberately to avoid double lighting. Keep HUD,
   sky and compatibility effects on their established path initially. Cars,
   deformation and pedestrians need later adapters with their own invariants.
4. **Extend shadow coverage.** Evaluate two to four cascades, receiver-distance
   splits, off-camera caster volumes, fitted depth bounds, texel stabilization,
   transition blending and consistent PCF/bias in world-scale terms. Cascades
   improve coverage/resolution distribution; they still need complete casters.
   Measure GPU shadow/main times and memory before choosing final settings.
5. **Develop material quality on that foundation.** Linear lighting/output,
   ambient or environment response and authored roughness/metallic/normal maps
   can follow. The current Forward mesh path is a starting point; Deferred,
   ray tracing, replacement assets and a new full engine are not prerequisites.

Game-specific source decoding, identities and streaming stay with the game;
generic GPU resource/pass handling stays behind the renderer interface. No
physics, mission, traffic or simulation change is required by this direction.
Original low-poly art can remain while its submission and lighting become modern.

## Acceptance criteria for a future bounded scope

- A building outside the camera still casts onto a visible road within the
  supported shadow distance; rotating the camera does not change a static shadow.
- Rotate sun across low, overhead and side angles without unexplained blockers,
  large self-shadow bands or sudden light-range changes.
- Drive through declared near/far shadow ranges and region boundaries with
  controlled transitions; verify low-sun casters beyond the visible footprint.
- Check thin/cutout geometry, LOD changes and modern/legacy mutual occlusion.
- Preserve original geometry, gameplay, UI/effects and asset files. Declare the
  unresolved categories and measure GPU/CPU/frame-time/memory rather than using
  the 30 Hz game ceiling as proof of rendering cost.

## Open decisions

The user adopted the static-city world-space direction; its first resident
shadow slice is delivered. Full static colour migration, cascades, route/region
validation and hardware budgets remain open. Desired validated distance, original
art lighting versus later authored PBR materials, and dynamic-category expansion
are open decisions. The latest recommendation and category-specific acceptance
examples are in the [supporting synthesis](geometry-shadows-and-distance.md).

## External references

- [Microsoft: cascaded shadow maps](https://learn.microsoft.com/en-us/windows/win32/dxtecharts/cascaded-shadow-maps):
  frustum partitioning, resolution distribution, caster depth bounds, cascade
  blending, PCF and bias tradeoffs. Its Direct3D sample is conceptual evidence,
  not a proposed dependency or a Vulkan implementation ready for this game.
- [Khronos: Vulkan rasterization](https://docs.vulkan.org/spec/latest/chapters/primsrast.html):
  constant/slope depth bias semantics. Also consulted through configured Context7
  (`/khronosgroup/vulkan-docs`), including host-coherent write/fence/submit
  visibility for the new upload. GPUInfo remains absent; the later `rdc-tools`
  connection exposes tools but `doctor` and `status` timed out. Installed
  RenderDoc supplied actual capture/replay evidence for the delivered slice.

## History

- 2026-10-06: recorded the user's shadow correctness report, source-backed
  limitations, receiver calculation findings, validation limits and proposed
  incremental direction. Documentation only; no new scope adopted.

### Adopted foundation and captured findings (2026-10-06)

Source model/camera coordinates use Y down, whereas vehicle physics, imported
local geometry and UI sun elevation use Y up. The first new capture showed
inverted fixtures; the Vulkan game boundary now converts the relevant transform,
placement, volume and light inputs. Valid zero-height terrain no longer falls
back to camera height on Vulkan. This explains a concrete observed inconsistency,
but cannot attribute every artifact in the user's uncaptured original frame.

The source-world static pass includes resident opaque faces before camera/PVS/
backface selection; four replays have no debug messages. High/low sun and a
90-degree spawn/camera change retain an identical 4,266-triangle source buffer.
The composite now uses explicit configured centre/extent, and receiver bounds/
modern PCF are fixed. Legacy bias/normal reconstruction and the finite map remain
limitations. Dynamic/cutout/event geometry and moving/streaming routes are pending.
[Change evidence](../../changes/2026-10-06/world-space-shadow-foundation/index.md).

The linked draw-distance plan shares resident data but must distinguish camera
and light demand. This milestone changes neither region loading nor gameplay
activation. Broader reach follows measured streaming budgets and later cascades.

- 2026-10-06 follow-up: user adopted the roadmap; delivered the resident static
  shadow slice and coordinate/receiver fixes. The remaining adopted milestones
  stay planned, with product evidence kept separate from planning metadata.
- 2026-10-06 further discussion: explained the shared-scene recommendation,
  coordinated camera/light residency and a proposed dynamic-category extension.
  Marked pre-milestone defects as historical; no additional runtime changes.
