---
type: Change
title: Orientation-aware native world shading
date: 2026-10-11
tags: [okf, engine, diligent, rendering, shading]
---

# Orientation-aware native world shading

Converted normal-shaded building vertices now preserve their legacy shade bucket
in `_RED2_WORLD_SHADE`. The native instance carries its original yaw group, and
the shader selects the corresponding live plane-colour entry for each vertex.
Combo-shaded buildings and tile/path resources keep their existing baked colour
path. Sprite lighting has a separate bounded correction below. This is not E03 parity
acceptance.

The palette order follows `draw.c`'s 32-entry `f4colourTable` pattern and the
existing packed `0xRRGGBB` to RGB8 conversion used by `LegacyWriteRgb`. Review
found and corrected a red/blue channel reversal in the palette producer. The
mapping is shared with an asymmetric-channel regression test so the runtime
producer and test use the same conversion helper.

The source decoder emits independent polygon-corner vertices, so combo and
normal-shaded primitives do not alias shade metadata in the decoded mesh.
Conversion hashing includes the new shade bytes, and GLB read-back verifies the
attribute survives publication.

## Validation

- `pwsh -NoProfile -File scripts/run_inspector_tests.ps1 -KeepWorkingDirectories`:
  all 18 suites passed. `WorldMeshBuilderTests` passed 358 checks, including the
  RGB channel-order regression.
- `pwsh -NoProfile -File scripts/run_native_renderer_smoke.ps1`: 1249 checks,
  0 failures. The smoke covers eight yaw buckets, independently oriented
  instances sharing one resource, live palette ownership, metadata-free fallback
  and invalid offset rejection.
- Visual Studio `Release_dev_native|x64` and `Release_dev|x64` solution builds:
  both completed with zero failed projects.
- Visual Studio's startup-project debugger launch ran `REDRIVER2_native.exe` with
  its existing mission 1 profile. The current log records level 0/variant 0,
  6,740 placements, 160/160 registered resources, 233 instances submitted,
  186 drawn and zero unresolved resources. This proves the native scene reached
  world rendering; it does not establish the visual correctness of the shading.
- RenderDoc attach was attempted with process PID `73980` as the target identifier.
  That argument is invalid because RenderDoc requires its target-control identifier;
  no new capture was produced and mission-level visual inspection remains pending.
- MSVC test-runner object output is isolated per inspector suite and smoke build
  through `/Fo` directories, avoiding collisions between same-named objects.

## Follow-up: native sprite lighting

Sprite resources now carry per-instance lighting mode through ordinary, LOD and high-detail
placements. The native bridge computes the original camera-light level for camera-lit sprites;
lit sprites and yaw bucket 63 use neutral 128. Sprite conversion captures neutral vertex RGB
because the original sprite draw path ignores its stored face tint. Texture alpha/STP and texture
identity remain intact.

The 21-suite inspector run passed, `NativeRendererSmoke` passed 1285 checks / 0 failures, and
isolated x64 `Release_dev_native` and `Release_dev` builds succeeded. Chicago TAD50 was captured
with RenderDoc 1.41: replay reports 656 draws and zero debug messages; the HUD reported 193
resources, 8,890 placements, 523 selected instances, 14 submitted cars, 56 submitted wheels,
and 1 submitted pedestrian. Perf samples showed 30/31 ticks and zero drops. The PNG shows the
current native scene but there is no matched OpenGL screenshot for sprite-parity comparison.
Other material classes and full lighting parity remain open.

No discussion or roadmap decision changed. The migration roadmap remains in
progress, and full scene, event-specific lighting and PSX blend parity remain
unaccepted.
