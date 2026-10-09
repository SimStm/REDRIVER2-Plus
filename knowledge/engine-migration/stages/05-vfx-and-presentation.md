---
type: Plan
title: E05 - Native effects and complete presentation consumers
status: planned
created: 2026-10-08
tags: [engine, diligent, vfx, ui]
---

# E05 - Native effects and complete presentation consumers

## Objective and dependencies

After E03's device/material/depth contracts and E04 audio, migrate every remaining
graphical consumer needed for supported modes through project-owned native
presentation and Diligent. E01 snapshots/events remain authoritative. PopcornFX
is deferred to optional E08 after E07 and only on explicit request; it is not a
dependency of existing effect preservation, full-game acceptance or removal.

## Implementation blocks

- Replace required existing effect producers with focused project-owned native
  implementations using Diligent passes/material/resource ownership. Preserve
  original effect coverage without requiring a new authoring system or PopcornFX
  SDK, and without introducing a competing graphics context/swapchain.
- Publish tire smoke, sparks, explosions, rain/weather and other inventoried
  effects from game events/poses. Map lifetime/intensity and seed semantics;
  isolate visual RNG from gameplay. Cosmetic simulation can use its own clock
  when replay/pause/event behavior remains explicitly defined and tested.
- Migrate decals, trails, flares, shadows, fades and framebuffer/offscreen/map
  consumers through native APIs. Sort transparent effects with native geometry;
  preserve sky/HUD separation and bounded particle/collision budgets.
- Implement native fonts/quads/menus/HUD/loading/pause/map/split-screen and
  developer UI. RmlUi may remain future work; current ImGui must get independent
  vendoring and native rendering/input hooks before PsyCross removal.
- Inventory and replace asset decoding, editor/export or diagnostic tools that
  still depend on VRAM or PsyCross render state. Keep input capture functional.

## Acceptance criteria

- [ ] Required existing VFX categories render through native Diligent with correct depth,
  sorting and world transforms; expected visual changes have specified references.
- [ ] Effects, audio and gameplay events correlate once per tick across frame
  schedules; particle updates do not consume gameplay RNG or alter physics.
- [ ] Every inventoried UI/offscreen/auxiliary consumer has a native replacement
  or an explicitly pending supported-mode gate; no unknown legacy producer remains.
- [ ] UI interaction captures input, stays readable across resolutions and does
  not advance gameplay while menus/loading/pause should hold it.
- [ ] Effect cancel/reload, resize and shutdown preserve synchronization, bounded
  allocations and diagnostics; measured particle/material/CPU/GPU costs fit budgets.

## Validation and limits

Use controlled effect scenes with foreground/background/cutout/transparent
geometry, pause/replay routes, rain/night and relevant original-city cases.
Inspect actual Diligent draws/attachments and resource retirement. Full coverage
must be recorded, not inferred from one explosion or a visible menu. Dedicated
authoring editors, RmlUi and PopcornFX are not required for these basic native
consumers. Verify this stage without any PopcornFX SDK/build/runtime dependency;
its deferral cannot leave a required PsyCross effect consumer behind.
