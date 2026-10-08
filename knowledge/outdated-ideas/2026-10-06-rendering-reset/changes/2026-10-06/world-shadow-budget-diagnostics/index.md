---
type: Change
title: Resident world-shadow budget diagnostics and route measurements
created: 2026-10-06
tags: [rendering, vulkan, shadows, streaming, validation]
---

# Resident world-shadow budget diagnostics and route measurements

## Objective and delivered scope

Continue stage 1 of the adopted
[shared-geometry direction](../../../discussions/renderer-modernization/geometry-shadows-and-distance.md):
distinguish missing residency, excluded geometry and capacity before static colour
migration or draw-distance expansion. Parent HEAD `2e93f215`, earlier dirty work
preserved; PsyCross clean at `1f8b5a5`. No assets changed.

`DeveloperWorldShadow.cpp` logs resident/unavailable search cells, unique
policy-excluded objects, unsupported model instances, current/reserved game-vector
bytes, region IDs, failed submissions and mean/peak CPU collection/copy cost.
The residency guard matches `GetFirstPackedCop` and reads owning state without
loading resources or altering gameplay usage. No PsyCross, shader or API edits.

Counts describe the last sample; timing and failed frames cover 120 submissions,
including capacity/rejection paths. The search square is conservative: unavailable
cells are not an exact count of missing sunlight blockers. Exclusions combine
category/material policy and unavailable models; unsupported counts are instances.
Payload bytes describe the game vector, not total process/GPU memory.

## Executed validation

- Visual Studio MCP: verified the open solution and `Release_dev|x64`;
  `build_solution` completed with `FailedProjects=0`.
- RenderDoc MCP captured/replayed six scenes with API validation requested:
  baseline/diagnostics, low sun, extents 30,000/60,000 (frame 430), and original
  `ATTRACT.400` replay (frame 900). Inspected vertex buffers, exported colour/depth
  and fetched GPU-duration counters. Viewed baseline, low sun, stress and route.
- MCP `assert-image --threshold 0 --json`: baseline versus diagnostics had
  zero differing pixels out of 921,600. Both had 773 draws and 4,266 static shadow
  triangles. Low sun retained the same static buffer SHA-256:
  `ea5e778705997b2044139ed3f54ca9eca610f831d3be6adc10b2966db599645e`.
- Native Visual Studio launch without debugger: three 120-sample collection
  windows measured 187.6/193.2/189.5 us, zero unavailable cells/failures.
  No native before/after speedup is claimed from this short sample.
- Installed RenderDoc 1.46 capture and replay of the updated default scene:
  773 draws, matching static-buffer hash, zero debug messages. Replaying a MCP
  1.41 capture through 1.46 succeeded but retained its barrier warnings.
- Temporary sun/range settings restored byte-for-byte (SHA-256 checked);
  debug-start settings untouched; task-owned game processes exited/stopped.
  No shader regeneration or PSX self-test was repeated for this CPU-only edit.

## Measured limits

Default extent 6,991: 762 objects, 4,266 triangles, 729 resident cells, zero
unavailable cells, 176 exclusions, 153,576 payload bytes, 2,359,296 reserved.
Under RenderDoc, collection windows averaged 180.7/179.5 us.

Extent 30,000 with low sun: 3,968 resident and 2,593 unavailable cells,
49,040 triangles/6,379 objects, one unsupported model instance. Collection/copy
averaged 1,613.5/1,624.7 us under RenderDoc.

Extent 60,000: 4,096 resident and 19,313 unavailable cells, 62,633 triangles
from 8,067 objects (95.6% of the 65,536 cap). Payload 2,254,788 bytes;
collection/copy 2,158.9/2,099.1 us. No overflow occurred; actual overflow remains
unvalidated at runtime. The unsupported model identity is not yet isolated.

`ATTRACT.400` logs changed region slots from `198,197,214,213` to
`198,199,214,215`, then `198,199,182,183`. Logged triangles varied from 5,106
down to 2,522, zero failed-frame windows. Frame 900 had 234 draws and 2,504 static
shadow triangles. This tests sampled slot reuse, not every loading interval/city.

Summed GPU replay event durations: diagnostics 1.058 ms, extent 30,000 1.219 ms,
extent 60,000 1.282 ms. Static draws: 0.027/0.210/0.274 ms respectively.
These single instrumented samples have serialization and validation warnings;
they are not native frame latency, an FPS prediction or a minimum-GPU budget.

## MCP and capture-version limitations

GPUInfo MCP responds and supplied the RTX 3060 Ti report-list link for
`maxImageDimension2D`. It supplies links, not API documentation or verified limit
values; web fetching failed. No hardware limit was inferred. Context7 Khronos
documentation was queried for image limits and synchronization access/stage rules.

MCP uses RenderDoc 1.41. Previous 1.46 captures cannot open there. New MCP captures
open but report up to ten HOST_READ/ALL_COMMANDS buffer-barrier warnings, VUID
02818. The game source contains no matching HOST_READ barrier. The 1.41 capture
retains warnings in 1.46; a fresh 1.46 capture/replay of identical game code is
clean. This implicates 1.41 capture instrumentation rather than this CPU edit.
Do not label the MCP captures validation-clean or modify game synchronization to
hide them. Registry layers and MCP installations were not modified.

Ignored raw evidence, scripts, reports, PNGs and settings backup are under
`src_rebuild/build/world-lighting-validation/`, labels `mcp-*` and `native*`.
Upstream release baseline rechecked: `8.0` / `b2d8857`. `git diff --check` passed.

## Next bounded work

Align capture/replay versions for clean MCP validation. Continue the bounded
original-texture static colour slice with shared mesh placement, UV/material
policy and mutual depth. Coordinate broader reach with reusable meshes,
camera/light residency and budgets; a larger shadow map cannot load absent
geometry. Replace whole-batch overflow rejection with measured graceful
degradation before broader reach. This checkpoint does not complete the lighting
or draw-distance roadmaps. OpenGL disposition is unscheduled entry VK-003.
