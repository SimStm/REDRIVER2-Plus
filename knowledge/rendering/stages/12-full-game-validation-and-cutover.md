---
type: ImplementationStage
title: R12 - Full-game acceptance and native renderer cutover
status: planned
created: 2026-10-06
tags: [okf, rendering, stage, vulkan]
---

# R12 - Full-game acceptance and native renderer cutover

**Status:** pending. **Dependency:** R00-R11 accepted; optional temporal AA/ray tracing/OpenGL work are not blockers.

R12 remains last in the adopted order R00-R07 -> R10 -> R11 -> R08 -> R09 -> R12.
Chicago's absent water category is not a R03 blocker; actual Rio water remains
part of city-specific category acceptance here. No earlier scoped not-applicable
record establishes water coverage in a city that contains it.

## Objective

Make the native Vulkan renderer the validated default for its supported desktop targets.

## Steps

1. Complete the category/mode matrix across Chicago, Havana, Las Vegas and Rio, representative day/night/weather/variants, dense traffic, long routes, missions, replays and split-screen.
2. Run resource churn/reload/resize/minimize, cold/warm loads, malformed mods and low-memory/capability profiles. Record CPU/GPU p50/p95/p99, memory, stalls and visual acceptance on named hardware.
3. Verify zero native-mode PSX GPU world and UI draws, with original-format content and simulation intact. Audit leftover hidden gates on PGXP, legacy PVS/draw caps, filtering and override state.
4. Select native default only after coverage, visual, simulation and performance gates pass. Preserve existing non-Vulkan backend/platform choice separately; validate Linux native support before including it in the completed target list. Native failure must never automatically select the old scene.
5. Publish operational product documentation, migrate the umbrella roadmap to done, update status/handoff/changelog and retain the unscheduled OpenGL compatibility backlog.

## Deliverables

A supported native-default renderer, completed acceptance evidence, current product documentation and an explicit platform/feature support list.

## Acceptance gate

- [ ] All currently functional original graphics categories/modes in the declared target set use the native renderer, with zero old-world/UI GPU draws and no automatic old-scene recovery. Expected loading/errors are explicit; preserved auxiliary consumers cannot be counted as native success.
- [ ] Original file structure/mod contracts, collision/physics/AI/missions/replays remain compatible.
- [ ] Reported user artifact scenarios, distant/off-camera/dynamic/cutout shadows and increased range pass the defined tests.
- [ ] Performance/memory budgets and Vulkan lifetime/synchronization checks pass; outstanding optional/platform work is documented without claiming it shipped.

## Planned validation

Run the full validation-and-completion matrix, appropriate generated VS/Linux builds, affected standalone tests and native self-tests. Capture final reference frames and hardware/settings metadata. No completed claim based solely on a roadmap checkbox.

## Risks and fallback

100% refers to complete original graphics coverage on explicitly supported native desktop modes, not invented skyline geometry, unlimited distance/lights or Vulkan on unsupported PSX/web targets.

Native startup/scene failure is explicit; no automatic legacy scene recovery.
Conservative presets may recover within native capabilities. Preserve OpenGL and
useful PsyCross platform services as separate compatibility choices. Remove old
graphics dependencies after their last consumers pass native acceptance.

## Handoff

Record changed files, actual checks/results, unresolved limits and the next stage
in the [active plan](../implementation-plan.md), current status and recent context.
Every Vulkan change also updates the unscheduled OpenGL compatibility backlog.
Pass the [shared validation gates](../validation-and-completion.md); a planned
checklist is not evidence. Update the matching implemented product surface before
claiming delivery. This stage request does not authorize executing later stages.
