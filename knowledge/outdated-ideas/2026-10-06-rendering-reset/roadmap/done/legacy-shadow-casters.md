---
type: Roadmap
title: Legacy geometry as shadow casters
status: implemented
completed: 2026-10-06
tags: [rendering, shadows, vulkan, performance]
---

# Legacy geometry as shadow casters

Delivered on Vulkan, default off. On 2026-10-06 the user narrowed current
renderer work to Vulkan; the former OpenGL portion is tracked separately in the
[unscheduled compatibility backlog](../../../../roadmap/planned/port-vulkan-to-opengl-compatibility.md).
[Product behaviour](../../product/legacy-shadow-casters.md) and source are the
implementation evidence; this is the delivery record.

## Problem and delivered behaviour

The modern directional map previously contained only imported modern meshes.
Eligible camera-submitted legacy buildings, vehicles and depth-valid geometry
now cast into that same map and its existing legacy receivers. Modern casting
continues; the sun and texel-snapped player-centred volume are shared.

## Scope

- Vulkan depth pipeline over existing queued PGXP vertices, with matching
  projection-offset/world reconstruction and conservative triangle-volume tests.
- Existing PSX/override texture cutouts, excluding blended/offscreen/2D geometry
  and final overlays. Queued-VRAM casters are skipped without reordering uploads.
- Indexed texture-compatible batches, bounded CPU/index storage and complete
  current-frame upload validation. CPU reads use cached vertex/constants copies.
- Persisted `legacyshadowcasters`, Vulkan-only panel control and caster counters.

## Non-goals and remaining limits

- OpenGL implementation/parity, cascaded shadows and per-object switches.
- Resubmitting geometry omitted by camera culling or changing gameplay.
- Casting blended effects or billboards without reliable PGXP depth.
- Guaranteed artifact-free rendering at every camera/sun setting. Camera-facing
  billboards retain their submitted orientation; bias remains experimental.

## Acceptance evidence

- Mission-50 Chicago RenderDoc on/off and rotated-sun frame-430 captures show
  projected street/building shadows and changed vehicle occlusion; all three
  initial replays succeeded without debug messages. Interface remains outside
  the caster draw boundary. Fragment cutouts replace solid tree rectangles.
- Initial scene batching produces 15 caster calls and 690 triangles rather than
  266 calls for the same triangles. Only opaque depth writes are reordered.
- Native 20-window profiling on RTX 3060 Ti: 29.95 FPS/8.735 ms submission off
  versus 29.955 FPS/8.9165 ms on. The 30 Hz gate is retained within sample noise;
  GPU time is not isolated and traffic varies between sequential runs.
- Visual Studio MCP `Release_dev|x64` builds and glslang regeneration passed.
  Final RenderDoc and PSX regression evidence is in the
  [change record](../../changes/2026-10-06/legacy-shadow-casters/index.md).

## History

- 2026-09-21: exploration identified PGXP screen/depth reconstruction and the
  existing shadow-pass integration. Nothing was implemented in that session.
- 2026-10-06: Vulkan-only scope adopted and implemented. The prototype exposed
  cutout and submission-cost issues; shared alpha decoding, indexed batching and
  cached CPU reads resolve them. Future OpenGL work remains unprioritized.
