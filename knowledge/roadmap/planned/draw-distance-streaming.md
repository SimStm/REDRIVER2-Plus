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

Order 11 after rendering stability in [01](texture-alpha-semantics.md) and [02](texture-flicker-diagnostics.md). Use settings infrastructure from [10](runtime-settings-gui.md) where applicable; universal editor support is not a hard prerequisite.

## Suggested execution order

Overall order: **11** in the [planned catalog](index.md).
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
