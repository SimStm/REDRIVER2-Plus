---
type: Design
title: Rendering requirements and documentation review
status: adopted-plan
created: 2026-10-06
tags: [okf, rendering, requirements]
---

# Rendering requirements and documentation review

## Consolidated user requirements

- Replace the complete graphics pipeline, rather than keep adding screen-space
  lighting onto PSX primitives. Preserve Driver 2 content and simulation.
- Load original files directly into native meshes, materials and instances;
  maintain existing file/directory and mod compatibility. Mandatory conversion
  of the installation to GLB is not acceptable.
- Support optional GLB/glTF replacements and metallic/roughness PBR materials.
- Light and shadow every relevant static and dynamic geometry category, including
  cutout foliage; prevent sky/HUD shadows and false facade occlusion.
- Extend visible distance and off-camera shadow coverage together with resource
  availability, LOD and memory budgets. Keep gameplay activation distances intact.
- Support a large local-light population, anti-aliasing, filtering and measured
  subtle quality improvements. Do not inherit the experimental four/eight-light
  limits as the new architecture's ceiling.
- Implement in reviewable stages with Visual Studio MCP builds, Vulkan docs and
  GPU capability checks, and RenderDoc inspection as rendering changes land.
  Use Meshy only when an additional owned test asset is useful.
- Continue Vulkan first; log every Vulkan modification for a future, unscheduled
  OpenGL port. Preserve existing platform fallbacks during migration.

## Review and disposition

The review covered renderer modernization discussions and supporting diagrams;
shared geometry/shadows/distance and shadow-foundation investigations; high-FPS
design; planned renderer, world lighting, draw distance, quality-profile and
model round-trip records; their specific implemented product/done records;
renderer change records from September and October; hybrid projection/composite
rules; current status/handoff; and shared asset, texture/mod, playground and
tooling contracts. Current source guards, resource limits and fork state were
checked to avoid treating an old proposal as implemented behaviour.

The following dedicated old subjects were physically archived, including
supporting images, generators and validation JSON files:

- `discussions/renderer-modernization/` and `discussions/high-fps-timestep/`.
- Planned `renderer-modernization`, `world-space-lighting`,
  `draw-distance-streaming`, `graphics-quality-profiles`, `high-fps-timestep`,
  and `model-export-import-roundtrip`.
- Product/done `vulkan-game-renderer`, `vulkan-ui-image-parity`,
  `legacy-lighting-receptivity`, `legacy-shadow-casters`, `point-light-sources`,
  plus the partial `world-space-lighting` product note.
- Twenty-one dedicated graphics change folders, including the mixed historical
  playground/renderer planning decision; the implemented playground remains active.
- Rules `psyx-modern-mesh-bridge` and `renderer-composite-writes`, which governed
  the hybrid bridge rather than the replacement architecture.

The archive holds **50 files** under
`knowledge/outdated-ideas/2026-10-06-rendering-reset/`. Its manifest gives exact
former/destination paths, original byte counts/hashes and final hashes after
relative-link maintenance. Archived implementation evidence is retained, not
declared false or removed from the code. The active
[current-state note](current-state.md) preserves relevant verified behaviour.
The archive is excluded from normal analysis; this inventory is sufficient
unless the user explicitly requests historical review.

## Active records retained deliberately

- [Vulkan/OpenGL tracking](../roadmap/planned/port-vulkan-to-opengl-compatibility.md):
  the user explicitly requires its existing path and unscheduled role.
- Asset catalog identity and lifetime, model reference metadata, texture alpha,
  palette variants, filtering/export/manifest merge and proportional-alpha
  product/rule records: these describe current file/mod contracts the replacement
  must consume. Keeping them does not prescribe PSX projection for native meshes.
- Inspector selection, export and preview; OpenDriver2Tools interoperability;
  runtime settings and input capture: existing tools and independent plans,
  not competing graphics rewrite roadmaps. Their renderer integration must use
  the new interfaces without changing file identity.
- Playable playground: an implemented fixture that complements original-city
  routes; it is no longer an exclusive prerequisite for native city work.
- Generic Vulkan cached-memory, renderer evidence, build, platform, PsyCross fork
  and world-generation rules: still applicable to their declared systems.
- SDL migration discussion: separate platform investigation; changing SDL is
  not required by this graphics rewrite.
- Root status, handoff and changelog: cross-project facts remain active.

The review intentionally keeps these shared contracts accessible. Moving them
into the excluded archive would hide requirements needed to preserve the user's
existing assets and tools.

## Outcome and decision history

2026-10-06: user requested archiving old graphics directions and a dedicated
rendering plan for a complete replacement. R00-R12 replace the competing hybrid
plans. The new documentation is adopted planning; no rendering code, assets,
settings, tests or current runtime behaviour changed in this task.
