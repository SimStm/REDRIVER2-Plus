# Vulkan source-world shadow foundation — 2026-10-06

The user adopted the incremental world-space approach and linked the existing
draw-distance/streaming roadmap. This delivery covers the opaque resident static
shadow slice and coordinate/receiver fixes; the broader roadmap remains planned.

## Implementation

- Game-owned resident selection/triangulation: `DeveloperWorldShadow.*`.
- Persistence, coordinate conversion, placement, light publishing and baseline
  statistics: `DeveloperModernMesh.*`; opt-in UI: `DeveloperGraphicsPanel.cpp`.
- Generic copied one-frame triangle API/statistics: `PsyX_public.h`, `PsyX_vk.h`,
  `PsyX_ModernMesh.cpp`; packed-position depth pipeline, bounded allocation,
  fence-ordered upload, cleanup and explicit UBO volume: `PsyX_Vk.cpp`.
- Vulkan receiver bounds, fade and PCF: `psx_composite.frag`, `psx_modern.frag`,
  `psx_modern.vert`, their SPIR-V files and regenerated `PsyX_Vk_Shaders.h`.

The first capture exposed inverted imported fixtures and source scenery with
negative upper Y. `camera.c` distinguishes camera/scenery Y from vehicle physics
Y (`cameraPos.vy = -where.t[1]`); source polygons and camera transforms retain
that convention. Vulkan now converts UI/physics/glTF at the game boundary.
The final high-sun image shows upright fixtures and preserved zero-height grounding.
This is corroborated source/capture evidence, not attribution of every reported
artifact to one cause.

## Executed validation

1. `pwsh -NoProfile -File scripts/compile_vk_shaders.ps1`: passed.
2. Premake VS2022 regeneration with installed dependency paths. Generated
   solution/projects/debugger settings were backed up; existing GL/playground
   configurations were restored. Visual Studio MCP confirmed all configuration
   pairs. Release_dev x64 runtime/optimization/dependency properties match the
   backups. No tracked Premake configuration change was needed.
3. Visual Studio MCP builds: initial failure from a missing `mission.h`
   declaration of `NumPlayers`; fixed by including the owning header. Subsequent
   builds passed, including the final profiling/fallback changes. Existing compiler
   and link warnings remain; this was not a warning-clean build.
4. RenderDoc 1.46 ExecuteAndInject/replay fallback: final high sun, low sun,
   90-degree spawn/camera heading and static-caster opt-out captures all replayed
   successfully with zero debug messages. High/low frames had 773 draws; changed
   heading 689; opt-out 788 (legacy casters restored). Static batches each had
   12,798 vertices / 4,266 triangles in one packed-xyz draw; 762 objects and zero
   skipped models were logged. Identical static-buffer SHA-256 across high/low/
   camera captures:
   `ea5e778705997b2044139ed3f54ca9eca610f831d3be6adc10b2966db599645e`.
   This demonstrates selection independence in these resident scenes; changing
   spawn heading also rotates the imported gallery, so whole shadow maps need
   not be identical.
5. Collection plus CPU copy: median 174.5 microseconds, ten 120-submission
   windows, range 172.3–178.1 microseconds under RenderDoc. Not isolated GPU time
   or a controlled native frame-time comparison.
6. `-vkpsxtest`: PASS, including VRAM export 1048594/1048594 bytes.
7. Runtime `developer_modern_mesh.ini` and `developer_debug_start.ini` restored
   byte-for-byte; task-launched game processes stopped/exited.

Ignored evidence is under `src_rebuild/build/world-lighting-validation/`: RDCs,
replay reports/PNGs, source-position binary/hash, game/performance/self-test logs,
helper scripts and configuration backups. No game assets were modified.

## MCP availability and remaining work

Visual Studio and Context7 were available. After the user corrected the Python
environment, `rdc-tools` appeared, but both `rdc doctor --json` and `rdc status`
timed out after 60 seconds. GPUInfo tools were still absent. Installed RenderDoc
provided capture/replay; this does not count as a successful RenderDoc MCP test.

Full route/region/level-transition testing, event/dynamic/cutout coverage,
native/GPU cost, shared static colour geometry and cascades remain pending.
The finite map and resident-only policy cannot promise full-city reach.
OpenGL disposition is recorded in unscheduled backlog entry VK-002.

PsyCross backend/shader/API commit `1f8b5a5` was pushed to fork `origin/master`;
the parent gitlink is staged at that commit. Parent source/documentation remains
local alongside the pre-existing work; no unrelated parent changes were committed.
Parent and PsyCross `git diff --check` passed (line-ending warnings only); status
files are under 350 lines. Upstream release `8.0` / `b2d8857` was rechecked against
[the release list](https://github.com/OpenDriver2/REDRIVER2/releases) before the
Unreleased entries were added.
