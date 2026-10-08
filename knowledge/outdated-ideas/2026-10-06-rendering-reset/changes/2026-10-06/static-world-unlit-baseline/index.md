---
type: Change
title: Bounded Vulkan original-texture static colour baseline
description: One resident original building shares geometry between colour and shadow passes.
tags: [rendering, vulkan, geometry, shadows, validation]
---

# Bounded static colour baseline - 2026-10-06

The user authorized the next incremental shared-geometry milestone. This delivery
adds an opt-in unlit baseline for one opaque original building; it does not finish
the broad lighting or draw-distance roadmap. Unlit preserves the existing art
before introducing a new lighting response, so texture/projection issues can be
distinguished from light calculation issues.

## Implementation and ownership

- Game: `DeveloperStaticScene.*` decodes supported original polygons/CLUT textures,
  retains copied identity/catalog handles, rejects unsupported whole models and
  checks residency/material validity. `draw.c` substitutes only its successful
  high-detail submission; `main.c` publishes before drawing. Existing game cell
  usage, simulation, LOD selection and streaming remain intact.
- `DeveloperModernMesh.*` persists `staticunlit` (default off), owns shutdown and
  processes keyboard toggles before geometry choice. `DeveloperGraphicsPanel.cpp`
  exposes requirements; `DeveloperWorldShadow.cpp` excludes the converted instance.
- PsyCross: generic unlit descriptor fields and one-frame colour/shadow visibility;
  Vulkan resource state, shaders, SPIR-V and embedded header. Original mesh buffer
  and transform are shared across colour and shadow. Visibility expires after
  recording, because game submissions precede BeginFrame. Existing lit fixtures
  are unaffected by zero-initialized descriptor fields. Unsupported GL unlit
  creation/visibility APIs return failure so callers preserve legacy fallback.
  Fork commit `5ebd3b2` is pushed to `SimStm/PsyCross` origin/master; the parent
  gitlink is staged to that commit.

Bounds: one instance, four materials, 256 source polygons, two decode attempts per
frame; nearest opaque 4-bit pages only. Any zero texel in a face UV bounding rectangle
rejects the entire model. Maximum GPU payload is 1 MiB texture data + 73,728 vertex
bytes; this excludes allocations/driver storage and is not measured process memory.
Source normals are decoded but controlled lighting/receiving shadows is deferred.
See the [product contract](../../../product/world-space-lighting.md).

## Actual validation

Ignored evidence resides in `src_rebuild/build/world-lighting-validation/`.

- `scripts/compile_vk_shaders.ps1` passed; Visual Studio MCP Release_dev x64 builds
  passed, final FailedProjects=0. `-vkpsxtest` launched through installed RenderDoc
  1.46; the newly appended fixture log ended with both self-test PASS messages.
  Parent/PsyCross `git diff --check` passed.
- `static-reference_frame430.rdc` versus `static-final-unlit_frame430.rdc`: nearest
  filtering, no new shadows/legacy lighting. Model 273, position (895,0,-214016),
  yaw 48, two materials/40 triangles. Original modulation and shader dithering
  reduced the difference to 202 of 921,600 pixels (0.022%), bounds x1216..1279,
  y1..359; the remainder of the image matched. Maximum channel difference 78.
  Selected corresponding post-VS vertices differed by roughly 0.004..0.013 pixels,
  consistent with original fixed-point versus GPU float transform precision.
  That is an inference, not proof of every differing pixel's cause or bit parity.
- Final `static-final-frame_frame430.rdc`: MCP replay reports 117 draws, world
  batch 4,226 triangles, plus native source 40 triangles. Events 83/579 share the
  30-vertex buffer/hash; events 86/583 share the 90-vertex buffer/hash. Both passes
  have depth test/write enabled. Compared with the original 4,266 triangle resident
  batch, precisely the selected native geometry is removed. Screenshot inspection
  and a separate turned-heading capture covered the source object; these do not
  establish exhaustive hybrid occlusion acceptance.
- Final recorded route `static-frame-route_frame900.rdc`, command
  `-replay G:/_Projetos/Pessoal/REDRIVER2-Plus/data/DRIVER2/REPLAYS/ATTRACT.400`:
  58 draws, 2,464 world triangles, selected model 275/40 triangles across three
  materials (12/18/90 vertices), shadow-only with no native source colour draws.
  Route logs show successive selected models and invalidation/fallback, alongside
  region IDs changing through 198/199/214/215 to 198/199/182/183. Log invalidations
  do not identify whether distance, material or region validity caused each one.
- Final `static-frame146_frame430.rdc` and fresh `replay-static-frame146-report.json`:
  115 draws, 4,226 world triangles + 40 native source triangles, **zero** debug
  messages. MCP 1.41 initial reads retain ten HOST_READ/ALL_COMMANDS barrier VUID
  02818 instrumentation warnings. Later empty message reads are consumed queues,
  not evidence of a clean capture. No registry/server changes were made.
- GPUInfo MCP was queried for RTX 3060 Ti sampler limits; it returns a hardware
  report URL, not a fetched verified limit or Vulkan registry documentation.
  Context7 supplied official Khronos synchronization/lifetime documentation.

Generated project regeneration initially removed local ignored GL/playground
configurations. They were restored from the existing project backup, with source
and filter entries added for WorldShadow/StaticScene. VS reopened the solution
and confirmed the ten configuration/platform pairs; the existing Premake utils
glob covers the new source. No generated project or Premake change is committed.
Four test INIs were restored byte-for-byte and their hashes compared successfully.
The restored opt-out capture `static-restored_frame430.rdc` reproduced the previous
`mcp-baseline.png` with zero differing pixels and the original 4,266-triangle world
batch/hash. It has no converted static meshes. Existing enhanced settings remain
as the user had them; only the new experiment defaults off. Existing CP1252 roadmap
records touched here were normalized to UTF-8 for reliable tooling.

## Remaining acceptance

Controlled lighting on source normals, receiver shadows, alpha-tested materials,
larger resident caches/budgets, native CPU/GPU profiling, mesh inspector picking,
all generation/VRAM invalidation paths and frontend transitions remain pending.
Dynamic geometry and expanded distance/cascades are following scopes. OpenGL work
is recorded in VK-004 without priority or deadline. Default opt-out keeps the
original renderer; this experiment is not a claim that all reported shadow bugs
are fixed.
