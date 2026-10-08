---
type: Change
title: Original static lighting, shared resources and resident shadow reach
date: 2026-10-06
tags: [rendering, vulkan, lighting, shadows, distance]
---

# Original static lighting, shared resources and resident shadow reach

## Outcome and implementation

Original-texture static meshes now receive diffuse lighting and sun shadows from
actual face normals. Shared resources replace the single-building experiment.
A resident-only camera extension draws eligible objects beyond the old camera/PVS
list. Two nested shadow volumes extend reach without reducing near-map resolution.
These are partial [world-space milestones](../../../roadmap/planned/world-space-lighting.md),
not completion of streaming, dynamic adapters or camera-fitted cascades.

Game ownership remains in `DeveloperStaticScene.*`: model/LOD decoding, copied
placements, palette/yaw keys, catalog handles, resident-slot and VRAM checks.
Eight source resources/four materials each, sixteen pooled original textures,
128 retained placements (default 64) and two new resource attempts/frame bound
migration. Unsupported/over-budget legacy candidates retain their draw;
unsupported extension-only objects do not acquire a new draw. Sprites, cutouts,
water, transparent/animated/smashable objects and road tiles stay legacy.

`draw.c` offers selected LOD models and calls the resident extension after its
building list. The float camera test reads the final basis without writing GTE
or gameplay selection state. Candidates reuse a bounded vector and sort nearest
first. Range defaults to 18,000, clamped 10,000-40,000 world units. Region loading,
traffic, collision and AI are unchanged. No model pointers survive frames.
Catalog/region/VRAM changes release resources; keys unused for 60 frames can be
replaced within the budget. PsyCross copies generic frame instances and replays
each placement/material with shared colour/shadow buffers and transforms. This is
resource reuse, not GPU instanced drawing. Mesh slots rise 16 to 48 for 32 source
materials plus existing-user headroom. Original diffuse mode preserves nearest
texels, modulation/dither and colour conversion; only direct sun is shadowed,
leaving ambient/local lights independent. Strength blends with prelit art.

The game shadow image is a 4096x2048 D32 atlas with two 2048-square tiles. Near/far
volumes share the player centre and independently snapped light bases; far
half-size follows native range. Shared `game_shadow.glsl` clamps PCF taps to each
tile, blends the near boundary, fades the outer edge and scales far bias to keep
approximately the same world-space offset. UBO fields append after existing lights,
preserving the projected legacy caster shader prefix. Projected legacy casters
use the near tile; source-world and modern casters draw in both. The standalone
fixture retains its single 2048-square map. `farshadows` defaults off and requires
static world casters. Overflow retries the near radius and disables the far tile
for that frame. F11 reports effective volume count/extent. Logs count failed
submissions, including distant attempts and near retries, rather than frames.

PsyCross fork commit `c2ab107` was pushed to `origin/master`; the parent
gitlink is staged at that commit.

## Executed validation

VS MCP **Release_dev|x64** builds completed with zero failed projects; shader
regeneration via `pwsh -NoProfile -File scripts/compile_vk_shaders.ps1` passed.
A newly launched `-vkpsxtest` finished with both PASS markers, covering blend,
stencil, partial-frame and VRAM checks. Text checks passed. The upstream baseline
was rechecked at [8.0 / b2d8857](https://github.com/OpenDriver2/REDRIVER2/releases/tag/8.0).

RenderDoc MCP Chicago mission 50/car 0/start 9046,-217980/heading 3300, frame 430:
nearest filtering, both PGXP options, no enabled overrides, lighting strength 1,
near half-size 6991, eight existing imported fixtures, sun (-0.8165,0.4848,-0.3134).
Eight resources/ten native placements/thirteen textures/654 unique vertices;
nine legacy-list replacements plus one extension-only placement. Twenty-five
colour/material draws matched fifty near/far shadow draws in vertex resource,
count and matrix multiplication within 0.0001. Thus placements, not just buffer
IDs, were checked. The resident batch had 21,766 triangles in each tile.

Same-capture shader replacement removing only native receiving changed 83,210
pixels on source facades (x0-1244/y13-353). Removing distant receiving in both
modern/composite shaders changed 80,112 pixels (x632-1279/y26-375). Geometry, depth,
textures and traffic were identical within each comparison; replacements were
restored. Counts establish contribution, not ground-truth shadow accuracy.

Low sun (-0.94,0.12,-0.32) retained byte-identical resident vertices in both tiles.
`ATTRACT.400` replay frame 900 at range 18,000 retained two tiles and 28,632
resident triangles. Logs crossed region sets 198/197/214/213 to 198/199/214/215 to
198/199/182/183 without failed submissions. Last sample: seventeen retained
placements, three legacy-list replacements and fifteen extension submissions.
At 40,000/128 placements, distant collection overflowed during this route;
frame 900 retained 2,150 near triangles. This verifies fallback, not distant
coverage at 40,000. Opt-out frame 430 had no native colour draws, one 4,266-triangle
world batch and only the near tile active.

MCP RenderDoc 1.41 still reports ten capture-instrumentation barrier warnings on
first replay reads; empty subsequent reads are consumed queues. A **fresh installed
1.46 capture and first replay** had zero messages and both 65,298-vertex world
draws. No registry/tool installation changes. GPUInfo returned report links,
without fetching numeric limits; atlas allocation checks real Vulkan device
limits. Context7 supplied official Khronos depth-write/sample synchronization.

## Cost, bounds and remaining limits

A VS-launched run without RenderDoc sampled ten 120-frame CPU windows. Excluding
initial creation, static adapter cost was 120.7-137.1 us (median 126.0);
collection/copy across ten windows was 773.7-828.9 us (median 790.85). These are
CPU scopes under the VS debugger, not isolated GPU time or an FPS improvement.
Route resource-creation peaks reached 8.9 ms; initial spawn creation ~9.8 ms.
GPU counters are replay-only durations. Native GPU time, minimum hardware and
peak process memory remain unmeasured.

Payload bounds: original RGBA pixels 4 MiB, source GPU vertices 576 KiB, texture
CPU snapshots 524,800 bytes plus 32,800-byte refresh scratch, and backend frame
instances 540,672 bytes across 48 slots. The game D32 atlas is **32 MiB even with
far disabled**, versus the previous single map's 16 MiB. World position payload
remains 6.75 MiB across game/backend/GPU copies. Allocator overhead, other renderer
resources and driver memory are excluded. Two active volumes double source/modern
shadow raster work. Default spawn far collection had 265 unavailable search cells;
missing cells are not empty cells and the collector cannot recreate their casters.

Cutouts/transparency, dynamic source adapters, inspector picking, broad VRAM/
frontend invalidation, other cities and long-route acceptance remain open.
Nested volumes are not camera-frustum-fitted CSM. Legacy receivers still depend
on reconstructed depth/normals and road offsets; universal artifact resolution
is not claimed. Greater reach needs measured residency and budget work.

Four INIs were restored byte-exactly; owned game/debug/capture sessions closed.
Game assets and generated configurations were preserved. See
[validation-summary.json](validation-summary.json). Large captures/screenshots
and helpers remain ignored under `src_rebuild/build/static-lighting-validation/`
and `world-lighting-validation/`. Deferred OpenGL work is VK-005.
