---
type: Roadmap
title: Draw distance, LOD and streaming budgets
status: planned
execution_order: 11
tags: [roadmap, rendering, streaming, performance]
---

# Draw distance, LOD and streaming budgets

## Problem

Increasing the draw-distance slider alone does not ensure distant resources are available or that primitive, memory and frame-time budgets are respected.

## Intended behaviour

Improve visible range and transition stability using measured budgets while preserving reconstructed gameplay and timing.

## Scope

Measure culling, LOD, streaming, primitive/ordering-table capacity, memory and CPU/GPU costs. Separate visual range from physics, traffic, AI and mission activation distances.

## Non-goals

Do not increase global limits blindly, disable bounds checks, change AI or collision range without explicit authorization, or require HD mods for the baseline game.

## Dependencies and risks

The user linked this work to [world-space lighting](world-space-lighting.md) on
2026-10-06. Coordinate resident region/LOD budgets for two consumers: camera
visibility and off-camera shadow casters. A light-demand region can matter even
when rejected by camera PVS. Diagnose the resident-only lighting slice first;
extra residency and visible-range changes remain measured follow-up work under
this roadmap. Do not couple either range to simulation activation distances.

Order 11 after rendering stability in [01](../../../../roadmap/done/texture-alpha-semantics.md) and [02](../../../../roadmap/done/texture-flicker-diagnostics.md). Use settings infrastructure from [10](../../../../roadmap/done/runtime-settings-gui.md) where applicable; universal editor support is not a hard prerequisite.

## Suggested execution order

Overall order: **11** in the [planned catalog](../../../../roadmap/planned/index.md).
Execute the following milestones sequentially. A request for one milestone
is not authorization to implement all later milestones or dependent features.

1. Establish repeatable routes across cities, day/night variants and dense scenes, with original and HD textures.
2. Instrument primitive usage, streaming stalls, resident memory, culling/LOD decisions and frame times. Identify the first binding limit.
3. Improve one bottleneck or transition policy at a time, preserving fallback and platform budgets.
4. Expose conservative limits/profiles with explicit resource constraints; test adaptive behavior only after stable fixed settings.
5. Compare visual pop-in and performance against baseline; document maximum validated settings rather than claiming unlimited distance.

## Acceptance criteria

A defined set of scenes gains range or smoother transitions without buffer overruns, missing streamed geometry or gameplay changes. Memory and frame-time budgets are stated, and users can restore baseline behavior.

## Validation plan

Run long routes, rapid camera movement, dense traffic and worst-case visibility. Check primitive caps, loading boundaries, frame-time distribution and low-memory behavior, not only average FPS.

## Starting points

`src_rebuild/Game/C/draw.c`, `tile.c`, `spool.c`, `camera.c`, `system.c`, renderer statistics and the current draw-distance setting.
Paths are relative to the repository root. Inspect current source and local
diffs before editing; these proposals are not proof of current behavior.

## Handoff and completion

For each requested milestone, report changed files, checks actually run,
remaining limitations and the next unblocked milestone. Preserve C++11 and
platform guards; maintain game-specific behavior outside PsyCross and carry
submodule changes in the project patch. Do not include game assets in tests
or commits. Only after all acceptance criteria are met, publish
`knowledge/product/draw-distance-streaming.md`, move this record to `done/`, update
status/date/catalog links, and record the delivered change in Unreleased.

## Coordinated resident-only checkpoint - 2026-10-06

The adopted static-lighting work adds a bounded native camera selector at
10,000-40,000 units (default 18,000), using already resident objects beyond the
old camera/PVS list. It reuses pooled original resources, preserves fallback and
leaves region requests, traffic, physics and missions unchanged. Two nested sun
volumes share this requested reach, but effective far coverage falls back to near
if the caster batch exceeds capacity. F11 reports actual volumes.

Recorded Chicago routes at 18,000 showed added native submissions and changing
region slots without failed caster submissions. At 40,000, distant collection
exceeded 65,536 triangles and the near retry remained usable. Missing light-demand
cells, creation stalls and finite source/material coverage are measured limits;
this does not deliver new streaming or validate all distances/cities. See
[evidence](../../changes/2026-10-06/static-world-lighting-and-range/index.md).
Next: camera/light demand budgeting and LOD/cache transitions on repeatable
routes before changing residency. This record remains planned.

## Coordinated light-demand budget continuation - 2026-10-06

The resident shadow selector now tests actual light volumes rather than a broad
world square. Default-range images/maps remain identical with fewer triangles;
the sampled 40k low-sun route now fits both volumes. Complete far tiers and delayed
recovery expose effective shadow range separately from requested camera range.
F11 reports conservative searched cells and collection pressure. No region loads
or simulation distances changed. See
[evidence](../../changes/2026-10-06/light-demand-shadow-budgets/index.md).
Next: test LOD/cache transitions, identify unavailable camera/light demand and
define bounded residency requests before modifying streaming. Real dense-scene
tier recovery, longer routes/cities and hardware costs remain acceptance work.
This broad roadmap stays planned; resident-only coverage is not full streaming.
