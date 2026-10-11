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
Combo-shaded buildings, sprites and tile/path resources keep their existing
baked colour path. This is a bounded static-world correction, not E03 parity
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

No discussion or roadmap decision changed. The migration roadmap remains in
progress, and full scene, event-specific lighting and PSX blend parity remain
unaccepted.
