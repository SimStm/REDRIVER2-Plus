---
type: Change
title: Optional Vulkan legacy shadow casters
description: Cast queued opaque PGXP geometry into the modern directional shadow map.
tags: [rendering, shadows, vulkan, performance]
---

# Optional Vulkan legacy shadow casters

PsyCross fork commit: `c564607`, pushed to `origin/master`; the parent gitlink
is staged at this revision. Existing parent point-light changes remain intact.

The user approved Vulkan-only continuation on 2026-10-06. The September
exploration identified the vertex reconstruction; implementation uses the
backend's current deferred draw list and upload offsets rather than stale GPU
globals. See [product behaviour](../../../product/legacy-shadow-casters.md)
and [deferred OpenGL work](../../../../../roadmap/planned/port-vulkan-to-opengl-compatibility.md).

## Implementation

- `PsyX_Vk.cpp`: shadow pipeline/layout, conservative triangle selection,
  texture-compatible indexed batches, current-frame CPU upload copy and bounded
  mapped index storage. Existing frame synchronization protects reuse.
- `psx_shadow.vert` and the `PSYX_SHADOW_PASS` variant of `psx.frag` share the
  main renderer's texture cutout decoder. Embedded and binary SPIR-V are generated
  by `scripts/compile_vk_shaders.ps1`, which now accepts per-variant defines/output.
- `PsyX_public.h`: appended C-compatible light-set toggle and caster counters.
  `DeveloperModernMesh.*` persists `legacyshadowcasters` and publishes it;
  `DeveloperGraphicsPanel.cpp` exposes the Vulkan-only, default-off control.
- Composite descriptors are updated before command recording, since updating
  a descriptor set already bound by the shadow commands would invalidate them.

## Executed validation

- Shader regeneration passed with the local glslang tool.
- Visual Studio MCP built `Release_dev|x64` successfully after pipeline,
  fragment-cutout, texture-batching and cached-vertex changes.
- RenderDoc 1.46 local Python API captured mission 50, Chicago, at frame 430,
  1280x720, with point lights disabled, shadow extent 6991 and sun
  `(0.6,0.4,-0.6)`. On/off captures retain the same player/camera state; traffic
  can still vary. A third capture uses sun `(-0.6,0.4,-0.6)` and visibly changes
  the projected street/building shadows and car occlusion.
- The initial cutout pipeline replay has 283 shadow draws versus 8 modern-only
  draws with casting off; all three replays returned no debug messages.
  Batching reduces the initial scene baseline to 15 caster draws for the same
  690 accepted triangles (previously 266). This draw-count evidence is separate
  from the earlier frame-430 capture.
- Temporary scripts, captures and profiling logs live under the ignored
  `src_rebuild/build/legacy-shadow-validation/`. RenderDoc/GPUInfo MCP servers
  were absent; Visual Studio and desktop-control MCPs were used. RenderDoc was
  driven locally and official Vulkan documentation was consulted.
- Final frame-430 replay succeeds without debug messages: 24 total shadow draws
  (8 modern, 16 legacy), versus 283 before batching, with identical 30,729 total
  submitted indices. Exported colour and shadow PNGs have identical SHA-256
  hashes to the initial cutout capture, confirming unchanged output after
  batching and cached-copy changes.
- `REDRIVER2_dev.exe -vkpsxtest` reports `psx self-test: PASS` and
  `VkFixture: psx self-test PASS`, including 4/16-bit sampling, blend/stencil,
  partial presentation and VRAM export. This covers the shared main PSX shader
  path; it is distinct from the caster runtime captures.
- The original runtime INI was restored byte-for-byte (matching SHA-256), all
  task-owned game/helper processes exited, and temporary VS breakpoints were
  removed. Parent/submodule `git diff --check` passed.

## Validation boundaries

The on/off images are a runtime check, not a cross-backend parity test. GPU time
is not isolated. Objects absent from the camera draw list and queued-VRAM
casters are explicit limits; no new scene submission or gameplay change was
introduced.

Native `PSYX_PERF_LOG=1` runs on the NVIDIA RTX 3060 Ti, with the validation
layer enabled, used the first 20 one-second windows after the four loading
windows. Off: mean 29.95 FPS, 8.735 ms submission (8.02-9.47 ms). Final on:
29.955 FPS, 8.9165 ms (8.40-9.42 ms), an increase of 0.1815 ms. This includes
command recording/submission and presentation; it does not isolate GPU time.
These are sequential city runs with moving traffic, not a benchmark guarantee.
Profiling launched task-owned processes without waiting on their handles,
temporarily inheriting `PSYX_PERF_LOG=1`, then restored the environment.
