---
type: Product
title: Tick-owned desktop effects
description: Regional effect updates and read-only presentation, with replay validation limits.
tags: [okf, presentation, timing, effects, replay]
---

# Tick-owned desktop effects

Desktop effects advance at the active 30 Hz NTSC or 25 Hz PAL simulation cadence.
The display FPS cap controls presentation only. `StepDesktopGame` advances effect
state after each successful gameplay tick, including catch-up/cutscene skipping.
World pause freezes these effects; pause-menu message expiry uses scheduled
regional service ticks while the world is paused.

`StepCarPresentation` emits engine smoke/fire and exhaust once per tick and owns
indicator/siren phases. Handling still emits tyre smoke and collision effects
from its original tick path. `StepDebris` owns smoke/spark/debris/leaf lifetimes,
positions, bird motion and hubcaps. `StepWeather` owns rain/weather progression.
Ground-debris candidates come from a bounded tick-camera resident-cell scan,
rather than a list populated during drawing. Mission-title fade/input unlock
and bomb-flash decay also advance with ticks. Replay controls are drawn on every
presentation frame; their sound control remains in the replay update path.

Drawing uses local interpolated particle/bird/hubcap/rain values and the common
car/camera snapshot alpha, without writing those poses into simulation pools.
New/reused slots invalidate prior history; paused drawing uses the current pose.
Damaged-lamp flicker and splash drawing do not advance a shared RNG. Attached
car lights use the same interpolated car transform as the body. PSX retains the
original render/update path through platform guards.

Night-time palette cycling updates colours at ticks and copies its current
upload packet into each draw's primitive buffer. Catch-up ticks cannot link a
static packet repeatedly and create an ordering-table cycle. Area changes
invalidate palette readiness until the next tick initializes it.

With `-statedump <file>`, `DeterministicBaseline` records field-wise gameplay,
mission timers, replay cursors, an effect-pool digest and indicator/siren phases.
Its optional before/after draw guard reports mutations of these recorded fields.
`e00_compare_state.py --strict` requires identical tick/entity/field inventories;
the default compares shared fields for older formats. Neither the digest nor the
guard covers every game field, the entire RNG state or every presentation system.

Completed OpenGL ATTRACT comparisons cover all 1,378 ticks at NTSC 30/360 FPS
and PAL 25/360 FPS, with zero differences in the recorded fields. The original
route fault remains unresolved: `b2d8857`, with its historical PsyCross revision,
reproduces the same wall collision on the local assets in both x64 and Win32.
The published upstream 8.0 Win32 binary also visually reproduces that collision.
The user confirms NTSC assets and defers further route investigation.
The rainy ATTRACT.401 matches all 2289 ticks at 30/360 FPS after the palette fix.
Schedule equality is not proof of a correct recorded route. Complete
Chase/mission trajectories, hubcap rendering and other platforms still
need targeted runtime acceptance. E00-E03 remain in progress.

See [validation and unresolved replay evidence](../changes/2026-10-09/effect-timing-replay-regression/index.md).
