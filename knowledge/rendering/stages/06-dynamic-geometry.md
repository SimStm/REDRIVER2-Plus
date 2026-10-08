---
type: ImplementationStage
title: R06 - Native cars, pedestrians and dynamic world geometry
status: planned
created: 2026-10-06
tags: [okf, rendering, stage, vulkan]
---

# R06 - Native cars, pedestrians and dynamic world geometry

**Status:** pending. **Dependency:** R05 accepted; snapshot contract from R00/R01.

## Objective

Render and shadow every dynamic 3D category without changing gameplay or physics.

## Steps

1. Adapt cars, wheels/components, damage/deformation and palette variants to native resources. Snapshot existing game-owned pose/deformation rather than changing dent/collision algorithms.
2. Adapt pedestrians/Tanner animation and all animated/smashable/event geometry, moving bridges and mission props identified in R00. Decide CPU deformation versus skinning per actual data contract.
3. Ensure colour, shadows, normals/bounds and picking consume the same final pose; update deformed bounds and resource generations correctly.
4. Handle creation/destruction, car switches, respawns, mission transitions and replay/cutscene cameras without draw-time simulation writes.
5. Close per-category coverage counters; test original and modern-compatible component mapping without changing collision or mission identity.

## Deliverables

Native dynamic scene snapshots and adapters, including shadow casting/receiving and instance-aware inspection.

## Acceptance gate

- [ ] All inventory dynamic categories have native colour/shadow/picking coverage with no legacy 3D fallback in accepted modes.
- [ ] Car damage, wheels, pedestrian animation and bridges are synchronized across passes.
- [ ] Simulation-step outcomes, collision, timing and replay behaviour match the original baseline.
- [ ] Resource churn/reload does not leak or reference destroyed instances.

## Planned validation

VS build and RenderDoc pose/mesh/shadow inspection; driving/crashes/damage, traffic/police, on-foot transitions, bridge/events and representative mission/replay/cutscene cases; deterministic per-step comparisons where available.

## Risks and fallback

Draw functions can contain state mutations. Separate presentation extraction without changing the update order/rate; missing animation support must remain visible in coverage status.

Unsupported native dynamic coverage must fail explicitly and remain pending in
the inventory; never recover that scene through original rendering. Preserve
existing separately selected backend/platform paths and unmigrated consumers.

## Handoff

Record changed files, actual checks/results, unresolved limits and the next stage
in the [active plan](../implementation-plan.md), current status and recent context.
Every Vulkan change also updates the unscheduled OpenGL compatibility backlog.
Pass the [shared validation gates](../validation-and-completion.md); a planned
checklist is not evidence. Update the matching implemented product surface before
claiming delivery. This stage request does not authorize executing later stages.
