---
type: Change
title: Verify the shadow receive parity between backends under a controlled capture
description: The reported Vulkan/OpenGL shadow difference was drifted developer state plus moving-traffic capture noise, not the shadow path.
tags: [rendering, vulkan, opengl, shadows, measurement]
---

# Verify the shadow receive parity between backends under a controlled capture

## Report and cause

The renderer-debt closure recorded a shadow receive difference between backends
("Vulkan 7722 vs OpenGL 2264 changed pixels in the sampled ground band, IoU
29.2 %, with OpenGL additionally darkening the legacy tree canopy") and left it
open. Both composite shaders turned out to be line-for-line ports of each other,
so the difference had to come from the inputs or the measurement.

It came from the measurement:

- **Drifted developer state.** The two installs had different
  `developer_modern_mesh.ini` values (`lightdir` and `legacyLightReceptivity`,
  and `shadowextent` 2500 on OpenGL versus 6991 on Vulkan) and different
  `developer_debug_start.ini` spawns (car 2 at X=13659, Z=-216011 versus car 3
  at X=9046, Z=-217980). A shadow volume 2.8x larger covers ~7.8x the area, so
  the compared regions were never the same.
- **Moving traffic.** The sampled street scene contains driven traffic, so two
  captures of the same build already differ by ~8000 pixels. A per-pixel
  difference metric over that scene is dominated by capture timing, not by the
  renderer.

## Implementation

No renderer change. The fix is to the measurement:

- The OpenGL install's developer state was aligned with the Vulkan install
  (same `lightdir`, `shadowextent` 6991, `legacyLightReceptivity` 0.300, same
  debug-start spawn/car).
- The metric was re-run with a same-backend control capture (to establish the
  noise floor) and in the traffic-free playground (`-playground`), where the
  scene is static and deterministic.

## Evidence

Windows `Release_dev|x64` / `Release_dev_gl|x64` with `-opengl`, one 1280x720
client-area capture per state, shadowed pixels counted as pixels whose
on/off channel sum differs by more than 18:

| Capture pair | Changed pixels | Mean abs |
| --- | --- | --- |
| Vulkan run vs Vulkan run (noise floor) | 2499 | - |
| Vulkan shadows on vs off | 4816 | - |
| OpenGL shadows on vs off | 4823 | - |
| Vulkan vs OpenGL, shadows off | 363585 (39.5 %) | 3.082/255 |
| Vulkan vs OpenGL, shadows on | 363595 (39.5 %) | 3.073/255 |

- Shadowed-pixel sets: intersection 4782, union 4857, **IoU 0.985**; the two
  backend counts differ by 7 pixels, well inside the 2499-pixel noise floor.
- Switching the shadow composite on changes the backend-to-backend difference by
  10 pixels (363585 -> 363595) and by 0.009/255 in the mean, i.e. the shadow
  path contributes no measurable backend difference. The remaining ~3/255 mean
  is the known legacy-scene parity gap.
- The earlier street-scene numbers were re-confirmed as noise-dominated: two
  Vulkan captures that differ only in timing (traffic) differ by 7973 pixels,
  more than the 5124 pixels that had been attributed to the backends.

The two installs keep the aligned developer state; the previous OpenGL values
are recorded in `knowledge/RECENT_CONTEXT.md`.
