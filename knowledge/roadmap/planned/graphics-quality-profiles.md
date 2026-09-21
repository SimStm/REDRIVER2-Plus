---
type: Roadmap
title: Measured graphics improvements and quality profiles
status: planned
execution_order: 12
tags: [roadmap, graphics, performance]
---

# Measured graphics improvements and quality profiles

## Problem

Further graphics improvements need explicit visual targets and performance budgets; generic 'better graphics' work can introduce regressions or make the original presentation unrecoverable.

## Intended behaviour

Deliver optional, measurable improvements through small independently testable milestones with a faithful baseline.

## Scope

Candidate areas include sampling/anisotropy after order 02, lighting refinements, edge stability and other effects explicitly selected with the user. Define quality profiles only for implemented and validated settings.

## Non-goals

This record does not authorize arbitrary PBR conversion, ray tracing, new art, major renderer replacement or gameplay changes. Do not implement every candidate effect at once.

## Dependencies and risks

The broader [renderer modernization roadmap](renderer-modernization.md) owns
modern scene submission, PBR and backend migration experiments and links its
[playground prerequisite](../done/playable-testing-playground.md). This record remains
focused on bounded improvements and profiles for implemented settings.

Order 12 after the relevant correctness and measurement work in [01](../done/texture-alpha-semantics.md), [02](../done/texture-flicker-diagnostics.md), [10](../done/runtime-settings-gui.md) and [11](draw-distance-streaming.md). Smaller independent improvements need only their actual prerequisites.

## Suggested execution order

Overall order: **12** in the [planned catalog](index.md).
Execute the following milestones sequentially. A request for one milestone
is not authorization to implement all later milestones or dependent features.

1. Agree on one concrete visual target and representative scenes; record original output and CPU/GPU/memory budgets.
2. Choose the smallest supported technique and define shader/extension capability fallbacks.
3. Implement one feature behind an independent setting, preserving the original render path.
4. Validate interaction with alpha, depth, PGXP, day/night lighting, texture overrides and distant geometry.
5. Add measured quality presets, documentation and before/after evidence; request direction before selecting the next major effect.

## Acceptance criteria

Each delivered effect has a toggle, supported-platform statement, reproducible visual benefit and measured cost. Disabling enhancements restores the baseline. Profiles never enable unavailable or unvalidated features silently.

## Validation plan

Use matched-camera captures, controlled performance runs, low-end configurations and extended gameplay. Review artifacts at transparent edges and transitions, not just attractive still images.

## Starting points

`src_rebuild/PsyCross/src/render/`, game lighting paths, developer settings, and results from the preceding diagnostics/performance stages.
Paths are relative to the repository root. Inspect current source and local
diffs before editing; these proposals are not proof of current behavior.

## Handoff and completion

For each requested milestone, report changed files, checks actually run,
remaining limitations and the next unblocked milestone. Preserve C++11 and
platform guards; maintain game-specific behavior outside PsyCross and carry
submodule changes in the project patch. Do not include game assets in tests
or commits. Only after all acceptance criteria are met, publish
`knowledge/product/graphics-quality-profiles.md`, move this record to `done/`, update
status/date/catalog links, and record the delivered change in Unreleased.
