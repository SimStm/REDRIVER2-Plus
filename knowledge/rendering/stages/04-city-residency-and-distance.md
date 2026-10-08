---
type: ImplementationStage
title: R04 - Whole-city visual resources, streaming and draw distance
status: planned
created: 2026-10-06
tags: [okf, rendering, stage, vulkan]
---

# R04 - Whole-city visual resources, streaming and draw distance

**Status:** pending. **Dependency:** R03 accepted; decoder coverage may expand in this stage.

## Objective

Make camera-visible and shadow-relevant original city resources available beyond the legacy resident/PVS limits.

## Steps

1. Prototype whole-city CPU visual decoding and measure load time/memory. Compare full preload with bounded sector demand; choose one measured loading policy, not blanket full GPU residency.
2. Implement separate camera/light visual residency demands, asynchronous I/O/decode where justified, render-owned uploads, cancellation and safe retirement. Preserve original gameplay spool/activation rules.
3. Add conservative native bounds, frustum/distance culling, stable LOD transitions and mesh sharing/instancing where measured. Do not submit every loaded object each frame.
4. Extend actual draw range with clear requested/effective settings and memory budgets. Off-camera blockers need light-driven residency even when camera PVS rejects them.
5. Handle fast driving, teleports, city/variant reload and missing/late resources without stale pointers, holes disguised as culling or unbounded allocations.

## Deliverables

A whole-city visual resource policy, measured increased range, independent camera/light demand and bounded loading diagnostics.

## Acceptance gate

- [ ] The validated long routes gain real geometry range without gameplay activation changes.
- [ ] Camera and light unavailable-resource counts can be explained; native range does not silently depend on four gameplay region slots.
- [ ] Budgets cover CPU decoded assets, GPU assets, upload/transient memory and stalls; low-memory behaviour is defined.
- [ ] Reload/teleport/LOD transitions are stable and no incomplete list is reported as complete coverage.

## Planned validation

Long repeatable routes in all four cities and representative variants; warm/cold loads, rapid turns/teleports and memory-pressure tests; native CPU/GPU timings, upload/cull statistics and RenderDoc inspection.

## Risks and fallback

Full preload may cost too much and old files may not contain geometry beyond the painted skyline. Larger distance is bounded by source content and measured resources.

Conservative native residency/range profiles may recover within declared bounds.
Unavailable native coverage is explicit loading/error, never old-scene recovery.

## Handoff

Record changed files, actual checks/results, unresolved limits and the next stage
in the [active plan](../implementation-plan.md), current status and recent context.
Every Vulkan change also updates the unscheduled OpenGL compatibility backlog.
Pass the [shared validation gates](../validation-and-completion.md); a planned
checklist is not evidence. Update the matching implemented product surface before
claiming delivery. This stage request does not authorize executing later stages.
