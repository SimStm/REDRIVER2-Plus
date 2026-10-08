---
type: Design
title: R00 native scene and presentation contracts
status: adopted-for-implementation
created: 2026-10-07
tags: [okf, rendering, architecture, ownership]
---

# Native scene and presentation contracts

These are concrete implementation decisions for R01 onward, not an existing
native API. R00 diagnostics are described in [the inventory](r00-source-inventory.md).

## One device and one presentation owner

Keep `PsyCross/src/render/PsyX_Vk.cpp::g_vk` as the sole Vulkan instance, physical/
logical device, graphics/present queue, SDL surface, swapchain and presentation
owner. `CreateInstance`, `PickPhysicalDevice`, `CreateDevice`, swapchain creation,
`PsyX_Vk_RenderFrame` acquisition/submission/presentation and `frameFence` are
existing source seams. `PsyX_EndScene`/`GR_SwapWindow` dispatch exactly once.
Native scene recording is a pass contributor on that owner's command buffer;
it creates no second window, logical device or independent present loop.

The current device enables only `VK_KHR_swapchain` and no optional core features.
Supported physical features are not automatically enabled. R01/R08/R07 must
explicitly negotiate anything they use. The R00 query creates a separate temporary
**query instance only**, no device, surface, swapchain or queue.

Render thread owns Vulkan mutations and handle tables. Source adapters copy data
on the game thread before publishing. Future workers own only immutable file/
decoded payloads and return requests through the scene owner; they never mutate
live spool/model/game arrays or execute Vulkan operations directly.

## Minimal C boundary

Use a size/version field on descriptors, POD records, fixed-width integer fields,
pointer/count pairs consumed during the call and integer result codes. C++11
implementation may use owned vectors internally. Do not expose STL, exceptions,
Vulkan handles or Driver 2 structs through generic public interfaces.

- `Handle = { uint32_t slot, uint32_t generation }`; generation zero is invalid.
  Distinct mesh/material/instance handle types; resource catalog IDs stay separate.
  A slot's generation advances on reuse; an exhausted generation retires the slot
  rather than accepting a stale handle. Validate all references and count rejection.
- `MeshDesc`: float3 positions/source normals, float2 UVs, optional tangent4 and
  RGBA tint, uint32 indices, vertex/index counts, submesh material ranges and local
  conservative bounds. Validate sizes/ranges/finite values before copying/upload.
  Return a handle only after ownership is established; failed creates leave no
  partially usable handle. Source pointers may be released after the call.
- `MaterialDesc`: texture resource handles; base/emissive factors; explicit
  original-unlit, original-lit or metallic/roughness shading; opaque/cutout/
  transparent/additive/subtractive blend; cutoff; double-sided; sampler/colour
  spaces; CLUT/STP/source face modulation identity and revision. Original mod
  precedence remains game-owned. No guessed PBR properties or PGXP/filter gating.
- `InstanceDesc`: mesh/material bindings, affine local-to-world matrix, local and
  world bounds, catalog resource/instance identity, caster/receiver/pick flags and
  transform/deformation revision. Camera and shadow visibility remain independent.
  Instance changes do not change collision or authoritative simulation transforms.
- `ViewDesc`: column-major float view/projection, world camera position, near/far,
  pixel viewport, FOV/aspect, view ID and camera-cut revision. One record per
  split-screen or secondary view, with explicit target/depth ownership.
- `LightDesc`: directional/point/spot, world transform, linear RGB/intensity,
  range/cone, stable light identity and optional shadow request. Ambient/exposure
  are scene parameters. R07 owns clustered lists and bounded local shadow budgets.
- `SnapshotDesc`: simulation tick, scene generation, immutable array spans for
  views/instances/poses/lights/effects, and camera-cut/discontinuity flags. Copy or
  take explicit owned payloads at publish; no borrowing recycled game memory.
  Simulation publishes once per tick. R11 interpolates only visual copies.
- Entry points: create/update/destroy resources, publish snapshot, record native
  world contribution, query requested/effective/rejected/legacy coverage and error
  reason. Return unsupported/pending/stale/out-of-budget distinctly; no silent drop.
  Implement the concrete narrow functions needed in R01, not an engine framework.

## Upload and retirement protocol

States are CPU decoded -> upload pending -> ready -> retiring. Upload commands and
staging payload remain owned until the associated submission fence completes.
Use render-thread scheduling, a monotonically increasing submission serial and
the existing frame fence initially; no timeline-semaphore feature assumption.
Destroy invalidates the public handle immediately but frees underlying allocations
only after the last referencing serial completes. Reset/reload advances scene
generation, cancels stale queued work and retires old resources. No recycled slot
pointers or GPU-mapped reads for CPU visibility/picking.

Existing PSX rectangle upload generations in `PsyX_Vk_GameUpdateVram`/
`ApplyVramUploadsUpTo` retain their ordered fallback semantics. Texture resources
outlive referencing meshes/materials. Resize recreates only size-dependent targets
after completion, preserving persistent scene resources; minimized/out-of-date
acquisition must not reset a fence with no submission to signal it.

## Camera, depth and first native scope

R01 starts with an opt-in isolated analytic **native world** using the final game
camera snapshot in an original Chicago scene or the owned playground. Neither
playground construction nor original world decoder completion is a prerequisite.
The original-city scenario is mandatory evidence. The existing world is an
explicit selectable fallback; do not mix its GTE/PGXP depth with native depth.
Native analytic mode suppresses legacy world colour and composes original UI at
the known boundary, with separate native world depth. Default selection stays
legacy until the validated R12 cutover.

Keep original integer world units. Adapter conversion to Y-up uses `(x,-y,z)`;
view construction must convert camera basis/translation consistently. This reflects
handedness: reverse triangle winding once in the adapter and validate culling with
an asymmetric analytic mesh. Do not claim a metres scale. Projection is right-
handed, looking along camera -Z, Vulkan depth [0,1], positive viewport height with
one projection Y flip. Declare the resulting front-face choice and test it in R01;
never flip both viewport and projection or reuse PSX ordering-table depth bands.

Start with conventional D32_SFLOAT native depth, clear=1, compare=LESS_OR_EQUAL,
near=10 and far=125000 original units. This is an R01 precision acceptance choice,
not a promise that it suffices for R04 city range. Measure close intersections and
far coplanar surfaces before changing range; reversed-Z is a measured follow-up.
For matched framing derive focal length from final `scr_z`, logical viewport and
the game's aspect correction; do not equate `fieldOfView=256` to 256 degrees.

Initial material is original-unlit/analytic unlit, explicit tint and linear/sRGB
conversion policy; no hybrid depth-derived normals, AO or shadows in acceptance.
Classify backdrop/opaque/cutout/transparency/effects/UI at the source. R03 provides
cutout/transparent completeness; R05 applies lighting/shadows to source normals.

## Pass contract

Native depth/world colour -> opaque/cutout -> ordered compatibility transparency/
effects -> world post/AA resolve -> UI/debug overlay -> one present. Sky/background
does not write receiver depth; HUD has no world depth/shadow participation. R01
uses single-sample SDR targets and keeps colour transforms explicit; R05/R08
introduce HDR/tone mapping/MSAA only with tested resolves and UI exclusion.
Secondary and split-screen view targets do not share accidental depth history.

Requested/effective statistics must distinguish native mode request, active native
view, resident/ready assets, selected/submitted geometry, explicit legacy fallback,
unsupported/rejected records and budget reductions. R00 producer batches are only
a baseline diagnostic; R01 must add actual native draw counts and R12 must measure
all legacy GPU world/UI draws, including direct draws and reused persistent packets.
