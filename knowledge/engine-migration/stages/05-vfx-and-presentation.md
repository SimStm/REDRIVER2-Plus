---
type: Plan
title: E05 - PopcornFX and complete presentation consumers
status: planned
created: 2026-10-08
tags: [engine, popcornfx, vfx, ui]
---

# E05 - PopcornFX and complete presentation consumers

## Objective and dependencies

After E03's device/material/depth contracts, integrate PopcornFX and migrate every
remaining graphical consumer needed for supported modes. E01 snapshots/events
remain authoritative; E04 audio enables combined effect/event verification.
The user confirms PopcornFX permitted use; pin SDK/bake/tool versions explicitly.

## Implementation blocks

- Establish effect authoring/baking and packaged dependencies with reproducible
  manifests. Integrate render helpers into Diligent passes/material/resource
  ownership, rather than introducing a competing graphics context/swapchain.
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

- [ ] Adopted VFX categories render through PopcornFX/Diligent with correct depth,
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
authoring editors and RmlUi are not required for these basic native consumers.
