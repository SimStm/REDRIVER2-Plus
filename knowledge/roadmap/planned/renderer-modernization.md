---
type: Roadmap
title: Renderer modernization using a dedicated playable playground
status: planned
execution_order: 14
created: 2026-09-15
tags: [roadmap, rendering, psycross, meshes, pbr, lighting]
---

# Renderer modernization using a dedicated playable playground

## Problem

The current PSX primitive path does not submit a complete scene description
with persistent modern meshes, material properties and lights. Replacing OpenGL
alone cannot provide those semantics. Modernization needs an incremental scene
path, preserved legacy compatibility and controlled validation.

## Intended behaviour

Retain useful PsyCross compatibility services and introduce a switchable modern
scene path for static meshes, PBR materials and custom lighting. Establish the
first in-game experiments in the dedicated playground, then validate integration
in original cities. Select scalable lighting and backend portability through
explicit measured decisions rather than assuming particular APIs or pipelines.

## Adoption and related records

The user adopted a playground-first sequence on 2026-09-15 and requested linked
roadmaps. This record promotes the rendering discussion into a staged plan;
no prototype or implementation is delivered by creating it.

- [Renderer discussion](../../discussions/renderer-modernization/index.md)
- [Detailed technical sequence](../../discussions/renderer-modernization/exploration-sequence.md)
- [Playground roadmap](playable-testing-playground.md), milestones P1-P4:
  explicit implementation prerequisite for R2 below.
- [Playground discussion](../../discussions/playable-testing-playground/index.md)
- [Measured graphics improvements](graphics-quality-profiles.md): remains the
  separate plan for bounded legacy-renderer effects and quality profiles.

## Scope

- Source-backed scene submission and resource/backend ownership contracts.
- Hybrid static geometry using shared camera/depth and the existing OpenGL
  backend initially, subject to capability review.
- A bounded static mesh/material asset path and reference Forward PBR rendering.
- Prompt-generated external test assets using the user's configured Meshy MCP
  on the implementation agent, validated and retained as fixed local fixtures.
- Controllable lighting, shadow and AO experiments with declared coverage.
- A measured decision on one initial scalable lighting pipeline.
- Backend portability review and one bounded second-backend prototype after
  target selection, with legacy composition and presentation considered.
- Playground fixtures plus original-city regression/performance scenes.

## Non-goals

No removal of all PsyCross services, automatic PBR conversion of all original
art, full world editor, arbitrary playable cities, traffic/physics rewrites,
animated-character/vehicle-deformation migration, ray tracing, or simultaneous
production implementations of Vulkan, Metal, Forward+ and Deferred.
General content expansion beyond static meshes and full-game rollout of a
second backend require separately bounded follow-up scopes.

## Dependencies and risks

Catalog label 14 follows playground label 13 within this dedicated track.
**No circular dependency:** the playground uses existing rendering; it does
not depend on this roadmap. Source audits, platform feasibility and R1 contract
work can proceed before its handoff, but R2 waits for verified P1-P4 evidence.
Menu polish and deferred playground authoring features do not block rendering.

The playground cannot replace original-city tests: streaming, legacy alpha,
PGXP, offscreen effects, camera variants and real asset complexity need their
own regression checks. Preserve the texture alpha product contract and C++11/
platform guards. Keep generic PsyCross changes as project-owned focused patches.

Shared depth may need a prepass or adapter. Final primitive streams omit normal/
material semantics and may omit off-camera shadow casters. Legacy colours can
already encode lighting. Graphics API selection must account for resource
lifetimes, synchronization, shaders, UI, readback and build/platform support.

## Suggested execution order

All milestones are planned. Each implementation request should name its bounded
milestone(s), and later choices must be recorded with evidence.

1. **R1 - Baseline and contracts.** Set initial desktop/hardware targets and
   measured budgets; trace source geometry, identity, visibility and colour/depth
   handling. Specify minimal camera/mesh/material/light and resource/pass
   contracts. Audit API/platform feasibility early if Apple support is required.
2. **R2 - Unlit hybrid mesh.** After playground P1-P4, draw a synthetic static
   mesh with the same camera and coherent depth as its legacy-rendered floor
   and obstacles. Test mutual occlusion, clipping, state restoration and HUD.
   Repeat the key checks in an original city before proceeding. Keep a toggle
   restoring baseline output; revise the bridge if depth is unreliable.
3. **R3 - Static asset path.** Select a bounded interchange/import contract
   (glTF/GLB is a candidate), validate attributes, resource lifetimes and failure
   cases, and import one owned static asset into the fixture. Define stable
   material identity independently of temporary PSX texture addresses.
   Use the configured Meshy MCP to generate an initial static prop from a prompt,
   request PBR texturing where supported, and retain the exported mesh/maps plus
   provenance. Validate the actual export against the selected import contract.
4. **R4 - Reference PBR.** Add metallic/roughness materials with normals and one
   controllable light using a simple Forward path. Validate linear-light colour
   handling, exposure/output and tangent conventions; preserve legacy shading
   by default. Add environment lighting only with explicit coverage and cost.
   Test the imported Meshy material alongside analytic material fixtures;
   inspect roughness/metallic/normal bindings instead of assuming that a service
   preview proves correct in-game PBR.
5. **R5 - Lighting effects.** Expand light instances and add shadows and AO as
   separately measurable toggles. Define legacy/modern caster and receiver
   coverage, depth/normal validity and off-camera visibility requirements.
6. **R6 - Pipeline decision.** Compare the reference with the relevant Forward+,
   clustered or Deferred candidate using identical scenes and budgets. Record
   one justified initial pipeline choice and deliver its bounded implementation;
   retaining Forward is valid if it meets the target. Do not mandate all options.
7. **R7 - Backend portability slice.** Select an actual second target/API through
   a recorded decision, then port the fixed mesh/material/light scene and the
   required legacy fixture path. Cover lifetime/synchronization, presentation,
   resize, readback and ImGui. Vulkan, native Metal and MoltenVK remain candidates,
   not promised support. A target must be selected/tested before R7 is complete.
8. **R8 - Regression and handoff.** Validate supported configurations in the
   playground and representative original scenes, document limitations and
   costs, and define follow-up scopes for broader content/backend rollout.

## Acceptance criteria

- Verified playground P1-P4 evidence precedes R2; no planned record is treated
  as proof that its dependency is implemented.
- Modern static geometry and legacy geometry mutually occlude correctly with
  coherent camera/depth conventions and documented exceptional passes.
- Static asset import and PBR lighting work with validated resource lifetimes,
  colour spaces and stable identities; legacy shading remains recoverable.
- At least one prompt-generated Meshy asset is imported and tested under the
  playground lighting with verified PBR maps, local immutable fixture files and
  provenance. An unavailable MCP leaves that fixture task pending; do not report
  a synthetic stand-in as a generated Meshy asset.
- Lighting effects state their coverage and measured costs, with independent
  controls and no unsupported full-world claims.
- One initial lighting pipeline decision and a tested second-backend fixture
  are documented; pending choices keep the corresponding milestone incomplete.
- Original-city regression tests complement playground tests and simulation
  behaviour remains unchanged. Supported targets and gaps are explicit.

## Validation plan

Use repeatable playground camera/spawn/object fixtures plus fixed original-city
replays. Compare legacy-only and hybrid images, depth and normal diagnostics,
opaque/cutout/translucent geometry, HUD, day/night and region transitions.
Exercise unload/reload/resize and asset errors, and use GPU validation tools
appropriate to the selected backend. Measure CPU/GPU frame time distribution,
memory and draw/resource costs on recorded hardware. Establish tolerances
before cross-backend comparisons; do not assume bitwise equality across GPUs.
Run applicable generated builds and text checks without modifying game data.

## Meshy MCP test-asset workflow

The user selected Meshy through MCP on 2026-09-15 and reports that it is already
configured on the agent that will implement this work. That configuration has
not been inspected in this documentation task. Discover the available tools and
their schemas on that agent; do not assume API parameter names are MCP tool names.

1. Start with a bounded prompt for one static prop, such as a metal bollard or
   painted concrete barrier. State material intent, approximate shape/scale and
   a simple silhouette; no scene or character generation is needed initially.
2. Request geometry and PBR texturing using the actual tool/model capabilities.
   The official API documents PBR options with model-specific restrictions;
   verify these through the configured MCP at execution time.
3. Export a material-preserving format accepted by R3, preferably GLB if selected,
   and retain all required texture files. Record prompt, exposed generation
   parameters/model version, task ID, date, output hashes and any conversion.
   Keep credentials and expiring signed download URLs out of durable metadata.
4. Validate geometry counts, units, UVs, normals/tangents, texture dimensions,
   colour spaces and metallic/roughness/normal channel conventions. Inspect
   exported material bindings; missing maps need correction or explicit handling.
5. Place the asset at a fixed playground transform and compare it under known
   light/camera/exposure settings. Use a separate simple collision proxy when
   needed; generated geometry does not automatically define drivable collision.
6. Reuse the exact downloaded fixture for regressions. Repeating a prompt is
   not a deterministic substitute for preserving the generated files. Record
   applicable asset-use terms/provenance before sharing the fixture.

Meshy is an authoring tool used during development, not a game runtime or
automated-test network dependency. It does not block playground P1-P4 or R2's
synthetic mesh. It is a required selected asset workflow for R3/R4; later
generation batches can broaden fixture variety without replacing analytic tests.
See the [Meshy discussion](../../discussions/renderer-modernization/index.md#meshy-mcp-for-external-pbr-test-assets)
and [official Text to 3D documentation](https://docs.meshy.ai/en/api/text-to-3d).

## Starting points

Repository-relative source: `src_rebuild/Game/C/draw.c`, `Game/engine/mdl.h`,
`PsyCross/src/gpu/PsyX_GPU.cpp`, `PsyCross/src/render/PsyX_render.cpp`,
`PsyCross/include/PsyX/PsyX_render.h`, `PsyCross/src/gte/`, `utils/DeveloperGraphicsPanel.*`,
`utils/DeveloperGraphicsSettings.*` and the implemented playground contract.
These paths after the first share the `src_rebuild/` prefix. Recheck live code.

## Handoff and completion

Keep partial milestones and their evidence in this planned record. Once R1-R8
and all acceptance criteria pass, add `knowledge/product/renderer-modernization.md`,
move this record to `done/`, set status/date, and update the catalogs, both
discussions, playground consumer link and changelog. If a later decision defers
R7 or another milestone, revise/split the adopted scope explicitly instead of
claiming the original complete feature was delivered.
