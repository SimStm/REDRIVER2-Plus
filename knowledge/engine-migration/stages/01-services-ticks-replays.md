---
type: Plan
title: E01 - Project services, fixed ticks and replay independence
status: planned
created: 2026-10-08
tags: [engine, timing, replays, platform]
---

# E01 - Project services, fixed ticks and replay independence

## Objective and dependencies

After [E00](00-dependency-baseline.md), move required non-rendering contracts into
focused project-owned services and decouple simulation from presentation.
Extraction may retain small compatible entry points while callers migrate;
copied code must have provenance and no hidden PsyCross include/link dependency.

## Implementation blocks

1. Extract required math/GTE behavior, pad/input, clocks, file/CD/stream callbacks
   and diagnostics according to the inventory. Keep reusable SDL/platform services
   direct; preserve fixed-point overflow/rounding and callback order where relied on.
2. Introduce monotonic elapsed-time scheduling and integer tick IDs, separate from
   presentation IDs. Preserve native NTSC/PAL simulation cadence/semantics initially.
   An engine-owned tick no longer depends on emulated vblank or display refresh;
   the chosen simulation frequency remains part of content/replay compatibility.
3. Sample/record inputs once per tick with deterministic catch-up behavior. Audit
   frame counters, timers, fast-forward, pause and original spooling dependencies.
   Rendering/effects must not advance game RNG or authoritative state per display.
4. Preserve existing replay run-length semantics as tick repeats. Define a
   versioned new replay header/region/rate/content contract without breaking old
   tapes or silently converting PAL tapes to 30Hz. State checksum fields explicitly.
5. Publish previous/current visual snapshots and interpolate without feeding
   floats back into physics. Reset at discontinuities; events trigger once per tick.

## Acceptance criteria

- [ ] Extracted contracts match E00 fixtures, including math edge cases, input
  mapping, callback completion/cancellation and resource shutdown.
- [ ] Tick counts, game state, mission/AI/damage/timer outcomes and replay cursors
  match approved baselines with identical tick inputs across presentation schedules.
- [ ] Existing NTSC/PAL tapes play and re-record with documented compatibility;
  corrupt/version/region mismatches diagnose explicitly.
- [ ] 25/30/60/120/144 FPS, uncapped and irregular schedules do not change
  authoritative state; interpolation adds visible intermediate poses only.
- [ ] Slow frames, stalls, pause/minimize and clock jumps follow a bounded explicit
  backlog policy; no silent discarded gameplay ticks or uncontrolled acceleration.
- [ ] Remaining PsyCross render/audio dependencies are inventoried, not falsely
  claimed removed by this stage. Rendering twice leaves authoritative state equal.

## Validation and limits

Run deterministic scheduler/replay/math fixtures and original-game integration
routes under injected clocks, plus live high-FPS presentation. Exercise analog
input, controller disconnect and pause/resume. Record both tick-state and elapsed
time evidence. Physics-rate changes or a new solver are future versioned work,
not part of removing the display clock dependency. E02/E03 consume this boundary.
