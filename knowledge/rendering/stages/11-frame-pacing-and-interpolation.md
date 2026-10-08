---
type: ImplementationStage
title: R11 - Independent presentation with preserved simulation
status: planned
created: 2026-10-06
tags: [okf, rendering, stage, vulkan]
---

# R11 - Independent presentation with preserved simulation

**Status:** pending. **Dependency:** R10 accepted and R00 draw-time mutation audit complete.

Execution precedes R08/R09 under the 2026-10-08 adopted order. Interpolate the
accepted original-content world/effects/UI snapshots; do not wait for final AA
or modern imports. Later quality/replacement changes must preserve per-tick
simulation and use the same pose, cut/load resets and presentation scheduling.

## Objective

Allow smoother native presentation without changing Driver 2 simulation, regional timing or replays.

## Steps

1. Separate presentation scheduling from fixed simulation ticks while preserving the actual region cadence: NTSC 30 Hz and PAL 25 Hz unless an explicitly adopted gameplay change says otherwise.
2. Move identified draw-time gameplay/effect updates into their existing-rate owner without changing update order, random calls, timers or replay format. Separate simulation and render counters.
3. Interpolate only presentation snapshots for transforms/camera/animation as appropriate, with resets on teleport/load/cuts. Shadows and colour use the same interpolated pose.
4. Implement bounded catch-up, input sampling, pause/minimize and frame-pacing policy; never mutate game state to match the monitor rate.
5. Expose optional higher presentation rate only after deterministic simulation and mission/replay comparisons pass; retain native-rate fallback.

## Deliverables

An independent render scheduler and presentation interpolation with region-correct simulation.

## Acceptance gate

- [ ] 30/60/high-refresh NTSC presentation and PAL baseline retain identical per-tick gameplay results within the established deterministic contract.
- [ ] Timers, audio/sirens, traffic/AI, damage, particles and replay playback do not accelerate or double update.
- [ ] Camera cuts, stalls, pause, minimize and teleport do not smear old snapshots or create long catch-up loops.
- [ ] The higher-rate setting is opt-in until its acceptance is complete; no arbitrary universal 30 Hz assumption.

## Planned validation

VS build; per-step state/replay comparisons at different presentation rates, long Take a Ride routes followed by missions/director/cutscenes, NTSC/PAL checks, input/pause/stall tests and pacing measurements.

## Risks and fallback

Frame counters and draw mutations are gameplay-sensitive. Do not simply remove FilterFrameTime or alter fixed-point simulation math.

Original cadence/presentation policy remains selectable.

## Handoff

Record changed files, actual checks/results, unresolved limits and the next stage
in the [active plan](../implementation-plan.md), current status and recent context.
Every Vulkan change also updates the unscheduled OpenGL compatibility backlog.
Pass the [shared validation gates](../validation-and-completion.md); a planned
checklist is not evidence. Update the matching implemented product surface before
claiming delivery. This stage request does not authorize executing later stages.
