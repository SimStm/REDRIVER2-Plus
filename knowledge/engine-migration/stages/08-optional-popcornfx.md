---
type: Plan
title: E08 - Optional PopcornFX integration
status: planned
created: 2026-10-08
tags: [engine, popcornfx, vfx, optional]
---

# E08 - Optional PopcornFX integration

## Objective and dependencies

This is the last, optional extension, only after accepted E00-E07 and a separate
explicit user request. It is not part of required migration completion. The game
must already work completely through Diligent, FMOD Core and project-owned native
effects, without PsyCross or PopcornFX. Do not acquire or integrate this SDK now.
When commissioned, create a separate extension lifecycle record with bounded scope.

## Implementation blocks

- Pin SDK/bake/tool versions and supported targets; record the user-confirmed
  permitted-use constraints and keep proprietary SDKs out of public commits.
- Establish reproducible effect baking and versioned package manifests. Integrate
  render helpers with existing Diligent passes, depth, sorting and resource ownership.
- Migrate explicitly selected native effect categories using stable game events,
  poses and IDs; define lifetime, intensity, cosmetic RNG and pause/replay clocks.
- Preserve existing effect coverage and explicit diagnostics during package/load
  failures. Keep simulation authority, synchronization and bounded cleanup intact.

## Acceptance criteria

- [ ] A separate explicit user request and accepted E00-E07 evidence are recorded.
- [ ] Selected effects render through PopcornFX/Diligent with verified native pixels,
  depth, transparency sorting and transforms; unselected effects still work.
- [ ] Effect/audio events correlate across tick/display schedules, pause and replay;
  cosmetic updates never mutate gameplay physics or consume gameplay RNG.
- [ ] Bake/package/load failures produce useful diagnostics; cancel/reload, resize
  and shutdown preserve synchronization, resource retirement and bounded allocations.
- [ ] Measured CPU/GPU/memory costs meet declared budgets, and full-game regressions
  pass without reintroducing PsyCross or a competing device/swapchain owner.

## Validation and limits

Use controlled foreground/background/cutout/transparent effect scenes, rain/night,
pause/replay and full-game regression routes. Inspect actual Diligent draws and
resources; verify package corruption and repeated load/unload/shutdown. No SDK,
effect integration or runtime validation is implemented by this planning change.
