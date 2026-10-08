---
type: ImplementationStage
title: R08 - Anti-aliasing and subtle image quality
status: planned
created: 2026-10-06
tags: [okf, rendering, stage, vulkan]
---

# R08 - Anti-aliasing and subtle image quality

**Status:** pending. **Dependency:** R11 accepted (including R07/R10); AA feasibility/attachment contracts are fixed earlier in R00/R01.

Execution follows native effects/UI and independent presentation under the
2026-10-08 adopted order, and precedes R09. Reuse their accepted scenes and
multiple-view contracts when changing samples, resolves, colour and history.

## Objective

Reduce jagged/flickering edges and improve image consistency without obscuring original art or UI.

## Steps

1. Implement the selected native-world AA baseline: capability-checked MSAA for geometry where viable, with an explicit spatial AA fallback. Define HDR/resolve/transparent ordering and sample-compatible pipelines.
2. Add validated mipmaps, anisotropic filtering and cutout-edge coverage handling; prevent atlas bleed and distant texture flicker.
3. Establish consistent colour spaces/exposure/tone mapping. Add conservative fog and AO only where correct world depth/normals and measured value justify them.
4. Keep features independently switchable and provide measured quality profiles. Do not expose controls for features not implemented.
5. Treat temporal AA/upscaling as optional follow-up requiring motion vectors, history invalidation and foliage/transparency ghosting tests; it is not a prerequisite for first complete native mode.

## Deliverables

A working AA mode/fallback, filtering and colour policy, plus conservative measured quality presets.

## Acceptance gate

- [ ] World edges improve at validated resolutions and camera motion without blurring/shadowing HUD or causing tree halos.
- [ ] Sample-count changes, unsupported hardware and resize recover safely.
- [ ] Quality profiles state measured frame-time/memory costs and supported feature combinations.
- [ ] AO/fog/tone effects do not hide geometry/shadow defects or double baked lighting.
- [ ] Accepted R10 effects/transparency/UI, auxiliary targets and multiple views remain correct at R11 presentation rates; quality changes preserve per-tick simulation and reset behaviour.

## Planned validation

VS build, RenderDoc attachment/sample/resolve checks; screenshots and driving clips at contrasting edges/cutouts/transparent surfaces; multiple resolutions, preset switches, capability fallback and GPU timings.

## Risks and fallback

MSAA does not solve all shader/specular aliasing; spatial filters can soften detail. Choose defaults from comparative evidence, not feature count.

AA/effects off and conservative sampling profile remain available.

## Handoff

Record changed files, actual checks/results, unresolved limits and the next stage
in the [active plan](../implementation-plan.md), current status and recent context.
Every Vulkan change also updates the unscheduled OpenGL compatibility backlog.
Pass the [shared validation gates](../validation-and-completion.md); a planned
checklist is not evidence. Update the matching implemented product surface before
claiming delivery. This stage request does not authorize executing later stages.
