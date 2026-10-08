---
type: ImplementationStage
title: R07 - Clustered Forward+ and scalable local lighting
status: planned
created: 2026-10-06
tags: [okf, rendering, stage, vulkan]
---

# R07 - Clustered Forward+ and scalable local lighting

**Status:** pending. **Dependency:** R06 accepted; R05 provides reference material/shadow evaluation.

## Objective

Support a large city light population with a bounded visible lighting workload.

## Steps

1. Replace fixed small light UBOs with a bounded growable registry and visible cluster lists. Define world/view-space bounds, overlap caps and overflow policy before implementation.
2. Feed verified street lamps, headlights, sirens, event/emissive light sources from game-owned snapshots; distinguish emissive appearance from emitted light.
3. Implement compatible opaque, cutout and transparent clustered lighting. Verify references against the simple native Forward result in low-light-count scenes.
4. Add a separately budgeted local shadow atlas/update selection for important lights; account for point-light multi-face rendering. Thousands of registered lights do not require all of them to cast shadows.
5. Measure city, dense-overlap and empty-cluster cases. Retain Deferred as an explicit later alternative only if evidence warrants its extra G-buffer/material complexity.

## Deliverables

Clustered Forward+ lighting, light/cluster overflow diagnostics and finite shadowed-light budgets.

## Acceptance gate

- [ ] Thousands of registered synthetic lights are exercised at recorded budgets; a realistic city test defines active/overlap limits and measured cost.
- [ ] No inherited four/eight-light ceiling or silent overflow; all relevant surface types use correct lists.
- [ ] Headlights/sirens follow original state without graphics changing game timing.
- [ ] Shadowed light count/resolution/update frequency are explicit and bounded.

## Planned validation

VS build; RenderDoc buffer/list/shader inspection; analytic light falloff and reference comparisons; daylight/night dense routes, transparency and pathological overlap tests; native GPU timestamp and memory data.

## Risks and fallback

Many overlapping lights can dominate cost even with clustering. Quality policy must preserve important lights and report overflow rather than hide corruption.

Reference native Forward lighting with conservative caps; scalable mode remains independently selectable until accepted.

## Handoff

Record changed files, actual checks/results, unresolved limits and the next stage
in the [active plan](../implementation-plan.md), current status and recent context.
Every Vulkan change also updates the unscheduled OpenGL compatibility backlog.
Pass the [shared validation gates](../validation-and-completion.md); a planned
checklist is not evidence. Update the matching implemented product surface before
claiming delivery. This stage request does not authorize executing later stages.
