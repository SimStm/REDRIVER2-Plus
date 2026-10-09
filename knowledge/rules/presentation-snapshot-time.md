---
type: Rule
title: Present cars and cameras at the same snapshot time
description: Preserve regional simulation while eliminating mixed-time visual motion.
tags: [okf, timing, camera, determinism, presentation]
---

# Present cars and cameras at the same snapshot time

**When** presentation interpolates a moving car, **then** interpolate its camera
over the same previous/current tick interval and alpha. Advance original follow,
head and collision controls at regional tick boundaries, including catch-up ticks,
not from a draw callback. Explicit paused replay-editor camera operations retain
regional service cadence and collapse history at the unchanged simulation tick.

**When** visual camera globals temporarily replace tick globals, **then** restore
the current tick camera before simulation, audio or other authoritative consumers
run. Seed camera snapshots at game initialization. An invalid draw snapshot must
not initialize/advance player camera state. Car translation/rotation and derived
body/wheel/shadow transforms are local presentation values, never written to physics.

**When** sessions reset, slots are replaced, cars teleport or cameras cut/change
target/view, **then** invalidate or collapse previous history. Repeated captures
at an unchanged gameplay tick must not rotate history. Paused poses use the
current endpoint; they must not cycle with the real clock's alpha.

**When** deriving 30 Hz NTSC or 25 Hz PAL ticks from a monotonic counter, **then**
use exact regional phase rather than rounded microsecond periods. Bounded
catch-up discards whole overdue ticks explicitly and preserves fractional phase.
Existing rounded replay metadata does not define the scheduler's arithmetic.

**When** presentation FPS is independent of ticks, **then** effect emission,
pool lifecycle, light phases and UI/input-gating timers need a tick owner too.
Draw callbacks consume local interpolated values; they must not age pools or
advance shared RNG. Validate sparse active slots and invalidate reused history.
Use strict tick/entity/field inventories for same-format replay comparisons.
Matching two FPS schedules alone does not establish a correct recorded route;
also compare against a demonstrably working reference with matching assets.

Validate production timing/snapshot code across display rates and irregular
counter intervals, then compare authoritative live replay state by tick. Gameplay
equality alone does not establish smooth motion; measure the camera-relative car
pose across tick boundaries on an actual moving route. See
[the regression record](../changes/2026-10-08/e01-presentation-motion-fix/index.md).
