---
type: Product
title: Vulkan legacy shadow casters
description: Optional directional shadow casting from queued opaque legacy geometry.
tags: [rendering, shadows, vulkan, psycross]
---

# Vulkan legacy shadow casters

Enable **Legacy geometry casts shadows (Vulkan)** in the Graphics panel or set
`legacyshadowcasters=1` in `developer_modern_mesh.ini`. The default is **off**.
The enhanced renderer, modern shadows and both PGXP options must be enabled.
The control is disabled on OpenGL, whose implementation remains deferred in
the [compatibility backlog](../../../roadmap/planned/port-vulkan-to-opengl-compatibility.md).

## Behaviour

The existing 2048x2048 directional shadow map receives eligible legacy triangles
after the modern mesh casters. Buildings and vehicles submitted by the game can
cast onto the existing legacy receivers; modern mesh casting stays active.
The sun, player-centred texel-snapped volume and receiver bias are shared.

`PsyX_Vk.cpp` reconstructs camera/world positions from PGXP screen coordinates,
depth, projection offsets and the same matrices used by the receiver. A triangle
is rejected if a vertex has unreliable depth, non-finite reconstruction, or all
three vertices lie beyond the same light-volume plane. Large triangles crossing
the volume remain eligible even when their vertices are outside it.

The pass excludes blended effects, offscreen draws, depth-disabled draws and
draws after the world/overlay boundary. Without a valid boundary, camera,
projection or complete current-frame vertex copy, it skips casting.
`psx_shadow.vert` supplies the existing texture coordinates and page/CLUT data;
the `PSYX_SHADOW_PASS` variant of `psx.frag` preserves the main renderer's PSX
and override-texture cutout rules without a colour output.

Opaque caster draws are sorted by texture state and submitted as indexed
batches over the original GPU vertices. A bounded host-coherent index buffer
is reused after the existing frame fence. CPU selection uses an ordinary RAM
copy made during enabled uploads: reading the Vulkan mapping can be expensive
because host-coherent memory need not be CPU-cached. Incomplete copies are
rejected, including the first frame after a mid-frame enable.
Allocated capacity is 11 MiB for the CPU vertex copy and 1 MiB for shadow indices;
disabled frames perform no caster copy/selection/draw work.

`PsyXModernLightSet.legacyShadowCastersEnabled` enables the pass. The appended
`PsyXModernMeshStats.legacyCasterDrawCalls` and `legacyCasterTriangles` fields
report its accepted work, also included in the game's baseline log.

## Limits

- User feedback (2026-10-06) reports sun-angle artifacts and near-only useful
  coverage. The sampled validation does not close these visual issues; the
  [source review and proposed approach](../discussions/renderer-modernization/shadow-lighting-foundation.md)
  are analysis only, without a new fix or adopted implementation scope.
- Only camera-submitted geometry is available. Objects omitted by game culling
  cannot cast; the pass does not resubmit the level from the light's viewpoint.
- Blended effects do not cast. Depth-valid billboards retain their texture
  silhouettes, but their geometry still comes from the game's camera-facing
  representation.
- VRAM-backed draws are skipped while VRAM uploads are queued. Stable RGBA and
  white-texture draws remain eligible. The main pass keeps its original upload
  generations and offscreen order.
- Bias is a fixed raster depth bias (constant 1.25, slope 1.5) plus the existing
  receiver bias. This is an experimental single directional map, not cascaded
  shadows or a guarantee of artifact-free results in every city/camera.
- Windows Vulkan validation is recorded in the
  [change record](../changes/2026-10-06/legacy-shadow-casters/index.md).
  OpenGL and other platform builds have not been validated for this feature.
