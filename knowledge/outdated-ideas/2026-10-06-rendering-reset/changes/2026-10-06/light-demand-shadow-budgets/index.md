---
type: Change
title: Light-volume selection and complete shadow range budgets
date: 2026-10-06
tags: [rendering, vulkan, shadows, distance, budgets]
---

# Light-volume selection and complete shadow range budgets

## Outcome and ownership

The resident opaque shadow collector now selects geometry against the actual
near/far light volumes instead of accepting everything in a broad world square.
The sampled default-range image and both depth maps remain identical while the
submitted stream loses about 60% of triangles at spawn and 70% on the recorded
route. The sampled 40k route now fits its complete distant collection, where the
previous implementation exceeded capacity and disabled distant coverage.
This is a partial [world-space lighting](../../../roadmap/planned/world-space-lighting.md)
and [distance](../../../roadmap/planned/draw-distance-streaming.md) milestone;
loading, simulation activation and supported geometry categories are unchanged.

PsyCross exposes a C-compatible CPU query for the exact snapped map matrix and
its world bounds. The query uses current light state and the existing matrix
builder, without reading mapped GPU memory. Scaling the matrix before its generic
inverse avoids falsely rejecting large orthographic volumes as singular. Invalid
or unsupported queries leave output unchanged. Vulkan-disabled and OpenGL paths
return failure. No shader, UBO, atlas, allocation cap or pass changes were needed.
Fork commit `446b96daaf74b0aeb6a68338aac7e82d7fd799c5` was pushed to `origin/master`;
the parent gitlink is staged at that commit.

The game owns cell selection, source decoding and budgets in
`DeveloperWorldShadow.cpp`. It searches padded bounds of both independently
snapped volumes, tests conservative vertical cell columns, actual source-vertex
bounding spheres and finally six clip planes per triangle. A triangle is rejected
only when all vertices lie outside the same plane of each active volume.
Crossing triangles remain whole for GPU clipping. Vulkan near depth is zero,
not negative W; plane tests include a one-world-unit tolerance. Source bounds
are rebuilt per collection, including instanced vertex sources and Q12 yaw
rounding padding; model pointers are not cached across frames. The existing
resident iterator, packed-object deduplication, native-caster ownership exclusion
and whole-model unsupported fallback remain in use.

The initial use of legacy `bounding_sphere` hints removed 27 needed triangles
and changed 7,706 far-depth texels despite unchanged colour. Reading actual
vertices corrected this: final maps and colour match the baseline exactly.
Those hints are not guaranteed 3D bounds and must not gate this collector.

Capacity remains 65,536 triangles. Only overflow retries smaller complete
collections: requested far extent, 75%, 50%, then near only, skipping far tiers
that do not exceed near. A reduced tier stays until 120 successful frames, then
probes one higher tier; another overflow returns to the fitting tier. Control
changes reset the policy. Other query/submission failures clear the batch rather
than disguising an error as capacity pressure. Even near can fail; no partial
collection is published. `DeveloperModernMesh.cpp` publishes the effective far
extent for receivers. F11 reports reduced range, resident/unavailable search
cells, culled triangles, attempts and capacity failures. Missing cells are a
conservative search diagnostic, not an exact count of missing shadow blockers.

## Executed validation

- Visual Studio MCP `Release_dev|x64`: final build succeeded, zero failed
  projects. A premature capture initially locked the executable during linking
  (LNK1104); closing the owned test process and rebuilding resolved it.
  No shader regeneration was run because shaders did not change.
- Fresh installed RenderDoc 1.46 capture and first replay: zero messages.
  Final spawn stream is 8,751 triangles in each tile. Raw atlas and colour have
  zero changed texels/pixels against the previous 21,766-triangle capture.
- RenderDoc MCP `ATTRACT.400`, frame 900, low sun (-0.94,0.12,-0.32), near 6991:
  at 18k, 28,632 becomes 8,535 triangles in each tile with zero near/far depth or
  colour differences; at 40k, 48,477 triangles fit both tiles. The near tile has
  zero changes against the old near fallback; far now contains 1,018,432 written
  depth texels. This proves contribution and selection parity, not ground-truth
  shadow accuracy or acceptance of every city/angle.
- Route logs crossed resident-region changes without failed submissions. The
  optimized 40k route fits, so it did not exercise new in-game tier reductions.
  An isolated MSVC harness using the actual query, plane and policy functions
  passed 40k/60k queries, invalid-output preservation, Vulkan near clipping,
  outside rejection, boundary-crossing retention, complete fallback, 120-frame
  recovery/probe failure and control resets. Collection capacity was mocked in
  that harness; it is not a GPU stress result. Command: `cl /nologo /EHsc /O2 /W4
  BoundaryTests.cpp /Fe:BoundaryTests.exe`, then `BoundaryTests.exe`, with
  `vcvars64.bat` loaded in the ignored validation directory.
- VS-launched low-sun 18k run without RenderDoc: three 120-frame CPU windows,
  collection/copy 644.4-663.4 us (median 651.0). Static warm windows 124.8/130.2 us
  (median 127.5), excluding first resource creation. These scopes do not establish
  isolated GPU cost, FPS gain or a controlled timing comparison with prior runs.
- Newly launched installed-1.46 `-vkpsxtest`: both PASS markers in a newly
  modified `vk_fixture.log` (2026-10-06 20:11 local); PSX blend, stencil,
  persistent/partial-frame and VRAM cases passed.
- Context7 supplied official Khronos Vulkan clip-volume conventions. GPUInfo
  returned RTX 3060 Ti report links, without fetched numeric device limits.
  MCP RenderDoc 1.41 retains ten instrumentation warnings on first reads; duplicate
  1.41/1.46 layers remain. No tool installation or registry changes were made.
- Four runtime INIs were restored byte-exactly and owned game/debug sessions
  closed. Captures, raw maps, helper scripts and harness remain ignored under
  `src_rebuild/build/shadow-demand-validation/`; no game assets are in this record.
  [Validation summary](validation-summary.json) preserves the numeric comparisons.
  Upstream [8.0 / b2d8857](https://github.com/OpenDriver2/REDRIVER2/releases/tag/8.0)
  was rechecked before the Unreleased changelog update.
- Final parent/submodule `git diff --check` passed. Local checks passed for
  whitespace, 91 documentation links, summary JSON and status lengths
  (CURRENT_STATUS 349 lines, RECENT_CONTEXT 336). PsyCross working tree is clean.

## Remaining work

Resident-only selection cannot supply unloaded sunlight blockers. Source/LOD
cache transitions and camera/light residency budgets on repeatable routes are
next; streaming remains separate from gameplay activation. Dynamic adapters,
cutouts, other cities, long routes and native GPU/peak-memory acceptance remain
pending. Near/far maps are still nested orthographic volumes, not camera-fitted
cascades. Existing road-depth/reconstructed-receiver limitations remain. All
affected files and deferred work are recorded as VK-006 in the unscheduled
[OpenGL backlog](../../../../../roadmap/planned/port-vulkan-to-opengl-compatibility.md).
