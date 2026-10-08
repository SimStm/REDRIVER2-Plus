---
type: Design
title: Native Vulkan rendering architecture
status: planned
created: 2026-10-06
tags: [okf, rendering, architecture, vulkan]
---

# Native Vulkan rendering architecture

## Target data flow

Original game files and optional modern mods feed game-owned decoders. Decoders
produce immutable mesh/material resources and spatial placement records. Game
simulation publishes a read-only snapshot of dynamic transforms, animation,
damage, camera and light state. A visual scene combines these inputs. Camera
visibility and light visibility independently select resident instances. Vulkan
renders those same resources into world colour/depth, shadows and effects,
then composes native UI and presents once.

This describes the proposed architecture. R00 defines concrete contracts and
ownership before new interfaces or files are added. Do not invent an engine
framework or distribute responsibilities through unrelated global helpers.

## Responsibilities and boundaries

1. **Original game/simulation:** owns physics, collision, AI, missions, replay,
   animation decisions, car deformation and original world loading for gameplay.
   Graphics consumes snapshots; it does not change activation distances.
2. **Game-owned asset adapters:** understand Driver 2 archives, model formats,
   texture/CLUT identity, placements, categories and mod precedence. Decode before
   GTE projection/ordering-table emission. Copy owned decoded data; never retain
   pointers into recycled original spool slots.
3. **Visual scene and residency:** owns stable resource/instance handles, bounds,
   LOD, loading demand and presentation state. Camera, sun and local lights are
   separate consumers. Loaded/decoded/uploaded/ready/retiring states are explicit.
4. **Generic Vulkan renderer:** owns buffers/images, descriptors, pipelines,
   upload synchronization, deferred destruction, passes and diagnostics. It sees
   generic meshes/materials/instances/lights/views, not game structs or missions.
5. **Platform/presentation:** exactly one window/device/swapchain/present owner.
   Reuse or extract the existing Vulkan resource and presentation code behind a
   clear boundary; do not create a second independent game swapchain. PsyCross
   input/audio/CD/timing services may remain. Its PSX GPU serves only explicitly
   unmigrated consumers until they are replaced; it cannot recover native scene
   failures. OpenGL backend choice is independent of native scene availability.

Keep public interfaces consumed by C game code C-compatible. Match C++11 and
target guards. Choose small concrete modules when their responsibility is needed;
do not pre-create a generic service/class for every item on the roadmap.

## Native camera and geometry

Use conventional floating-point world/view/projection transforms and native depth,
with explicit units, handedness, winding and Vulkan clip conventions. Retain the
game's camera intent and verify FOV/framing against original scenes. PGXP/GTE
encoding, PSX screen distance, ordering-table limits and legacy depth bands do not
define native meshes. Pick depth format/near/far and precision policy in R00/R01;
reversed depth is an option to measure, not a hidden requirement.

World geometry stores source normals, UVs, indices and material bindings. Colour,
depth, shadows, picking and motion use the same instance transforms and deformation.
Position reconstruction from accurate native depth remains valid for later effects;
it is not a substitute for missing source geometry or material normals.

Classify sky/horizon, opaque, alpha-tested cutout, transparent, additive/subtractive
effects, decals and UI explicitly. Sky and HUD do not receive world shadows.
Cutout colour and shadow passes share the silhouette rule. Painted horizon art
stays a backdrop unless optional authored 3D replacement content is provided.

## City resources and visibility

Prototype decoding a whole city's visual data into a CPU cache to measure size,
load time and correctness. Full preload is allowed if measured budgets support it;
it does not mean drawing every object or placing every texture on the GPU.
Ship bounded asynchronous sector residency where required, with shared meshes,
instancing where useful, conservative bounds, distance/frustum selection and LOD.

Camera range and shadow range have separate requested/effective budgets. A building
behind the camera may need to cast onto the road ahead. Light demand therefore
requests resources beyond the camera list. Retain original simulation streaming
independently. Use stable generations, cancellation and fence-protected retirement
on reload/teleport; never let a worker mutate active gameplay arrays or Vulkan
resources without the owning thread's protocol.

## Lighting and material direction

Start with clear unlit output, then native sun, ambient/environment response and
metallic/roughness PBR. Original art often already contains shading; provide a
documented source-material policy/blend so new lighting does not double darken it.
Do not infer true roughness, metallic or surface shape from old textures alone.

Use camera-fitted **cascaded sun shadows**: several maps distribute detail from
near to far. Fit receiver ranges and include upstream/off-camera blockers, stable
texel alignment, conservative caster bounds, filtering and measured bias. Coverage
means all relevant geometry within a validated visual range, not an infinite map
with unlimited resolution. Skyline artwork cannot act like absent 3D buildings.

Choose **Clustered Forward+** as the initial scalable-light direction: partition
the visible volume and give each part the lights that affect it. It supports a
large city light registry while bounding overlap and per-frame work. Transparent
objects use a compatible light list. Deferred rendering is a later measured
alternative if justified; it cannot fix missing geometry/residency. Shadowed local
lights have a separate atlas/update budget; thousands of lights do not promise
thousands of simultaneous full-resolution shadow maps.

## Pass order and image quality

The initial order is shadow/depth as required, opaque/cutout world, compatible
transparent/effect composition, world post-processing/AA, then UI and presentation.
Define exact resolve/HDR/alpha order for the chosen pipeline before implementation.
Use linear-light shading, declared sRGB texture/output conversions and exposure.
Preserve legacy effect blends through explicit compatibility material operations.

MSAA is a candidate for native geometry edges; query the intersection of supported
colour/depth sample counts and per-format usage. A spatial AA fallback is required
where multisampling is unavailable or too expensive. Temporal AA is optional later
and needs valid motion/history, camera-cut resets and ghosting tests. World AA and
tone mapping must not blur or shadow HUD text. Add fog/AO/subtle effects only with
independent controls, measured costs and correct depth/material semantics.

## Migration and final boundary

Migrate a complete region/category with explicit loading/error and coverage reports.
Unavailable native world coverage never invokes the old scene renderer. Temporary
composition with declared unmigrated auxiliary consumers needs its own tested
camera/depth contract; the old GTE rule applies only to those pending consumers.
Native gameplay can initially be a separate world pass rather than depend on
cross-projection depth. At R12 no PSX GPU world or UI draws remain on the supported
native mode. Native quads/fonts can consume original 2D command semantics without
running PSX GPU emulation; original formats are still accepted.

Reuse proven loader/backend helpers only after inspecting their invariants. A
new rendering API is not a reason to rewrite gameplay or discard non-graphics
PsyCross services. Preserve existing separately selected backends/platform paths
until their supported consumers migrate. Native startup/scene failure is explicit;
any recovery uses validated native resources. OpenGL modernization stays unscheduled.
