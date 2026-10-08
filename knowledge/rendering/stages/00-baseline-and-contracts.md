---
type: ImplementationStage
title: R00 - Baseline, coverage inventory and contracts
status: implemented
created: 2026-10-06
tags: [okf, rendering, stage, vulkan]
---

# R00 - Baseline, coverage inventory and contracts

**Status:** implemented and baseline-validated on Windows, 2026-10-07.
**Dependency:** None; first implementation request.

## Objective

Define a reproducible baseline and the native renderer boundary before adding rendering behaviour.

## Steps

1. Inventory original source geometry and every draw category: roads, scenery, foliage, water, cars/wheels/damage, pedestrians, animated/event objects, particles, decals, sky/horizon, mirrors, HUD, frontend, loading and cutscenes. Assign each a decoder, owner and destination stage.
2. Inspect both Git diffs and the current device/presentation/upload ownership. Specify minimal mesh/material/instance/view/light, snapshot, handle generation and pass contracts, with C-compatible entry points where needed.
3. Record target OS/GPU/driver, resolution, Vulkan feature/format support, colour/depth sample-count intersection, frame-time and memory budgets. GPUInfo provides lookup links; actual device queries/verified reports provide values. Start on Windows Release_dev x64; specify later Linux acceptance.
4. Save repeatable original-city routes and fixed artifact camera/sun/FOV cases, plus playground and analytic fixtures. Preserve INI bytes before tests and restore them afterwards.
5. Audit NTSC/PAL cadence, draw-time mutations and mode coverage. Choose a conservative native camera/depth/material strategy and define requested versus effective coverage statistics.

## Deliverables

A source inventory, concrete interface/ownership decision, measured baseline, test-state recipes and numerical budgets recorded in active rendering notes. No broad engine implementation in this stage.

## Acceptance gate

- [x] Each graphics category has an owner, migration stage and observable producer/path counters. Historical fallback terminology is superseded by explicit native loading/failure/auxiliary states.
- [x] Hardware/feature assumptions and numerical budgets come from executed checks; unknown GPU/peak-memory values remain marked unknown.
- [x] Original-city artifacts have reproducible test states; exact causes remain unconfirmed until a matching frame is inspected.
- [x] The first native scope and one presentation owner are explicit, with no required playground-only prerequisite.

Evidence: [implemented diagnostics](../../product/native-renderer-baseline.md),
[source inventory](../r00-source-inventory.md),
[contracts](../r00-native-contracts.md) and
[measurements/recipes](../r00-measured-baseline.md). VS build,12 invariant/ABI
tests and fresh legacy pixel tests passed. Matched city captures are RGB-identical;
city/wide/replay/playground first replays have zero messages. Five INI files and
VS debugger settings restored byte-for-byte. Native scene/GPU budgets remain
future acceptance; R00 explicitly permits unknown GPU/renderer allocation peaks.

## Planned validation

Use Visual Studio MCP for Release_dev x64 baseline compilation, RenderDoc for a reproducible current frame, configured Vulkan documentation tools, actual hardware queries and existing -vkpsxtest. Record source revisions, effective toggles and validation-layer versions. No claimed performance gain.

## Risks and fallback

Dirty existing work, duplicate RenderDoc layers and uncertain reproduction must be recorded. Save only source/config recipes and owned fixtures; do not commit game assets.

No default rendering change. Keep current backend/settings intact.

## Handoff

Record changed files, actual checks/results, unresolved limits and the next stage
in the [active plan](../implementation-plan.md), current status and recent context.
Every Vulkan change also updates the unscheduled OpenGL compatibility backlog.
Pass the [shared validation gates](../validation-and-completion.md); a planned
checklist is not evidence. Update the matching implemented product surface before
claiming delivery. The 2026-10-07 user goal authorizes sequential R00-R12 work;
next is R01, only after this gate has passed. The umbrella stays planned until R12.
