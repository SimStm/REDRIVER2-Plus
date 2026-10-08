---
type: Roadmap
title: Point-light sources and receptivity
status: implemented
completed: 2026-09-21
tags: [roadmap, rendering, lighting, vulkan, opengl]
---

# Point-light sources and receptivity

Status: implemented 2026-09-21. Product behaviour is documented in
[`knowledge/product/point-light-sources.md`](../../product/point-light-sources.md).
This record is the delivery history; the product document and the source are the
authoritative evidence for current behaviour.

## Delivered

- The game publishes point lights next to the sun from live state: the player's
  two headlights, the roof light of each active police car with a siren, and a
  validation probe that replaces them for captures. Candidates are ordered by
  distance to the shadow centre and capped by a persisted `pointlightlimit`
  (`1..4`, `PSYX_MODERN_COMPOSITE_POINT_LIGHTS`), so the cap always keeps the
  nearest lights.
- The legacy receptivity composite loops the point lights on both backends with
  the same five-tap normal and the sun's data path, adding
  `legacyScale * N·L * colour * clamp(1 - d/range)²` per light, and debug view
  `11` exposes the resulting coverage.
- The Graphics panel gained a **Point lights** group (toggle, limit, range,
  intensity, headlights, sirens, probe); every value persists in
  `developer_modern_mesh.ini` and the `Baseline:` log line reports the published
  count.
- The standalone Vulkan fixture publishes a point light with `-vkpointlight`
  (or the `L` key) for a game-state-free capture of the light plumbing.

## Acceptance

- A published light near the legacy scene brightens it with distance falloff and
  the effect disappears when its range is exceeded (`range 600` versus off is
  empty; `range 3000` produces a radial footprint around the car).
- Modern meshes shade the same set consistently (fixture capture, and the
  light set is shared).
- Vulkan and OpenGL apply the same term: their probe A/B footprints match cell
  for cell, and the cross-backend frame difference with the probe on
  (`meanAbs 3.149/255`) stays at the established `3.07-3.08` backend difference.
- Cost: `submit_ms` at 30.0 FPS is unchanged with two published lights (mean
  `6.84` both runs) inside the sample spread; the loop adds a few operations per
  pixel to an already active full-screen composite.
- Windows `Release_dev|x64` and `Release_dev_gl|x64` built with 0 failed
  projects; `-vkpsxtest` reports `psx self-test: PASS`.

## Not delivered

- Point lights cast no shadow (the shadow map stays directional), there is no
  per-pixel culling beyond the range test, and the cap of four exists so both
  backends shade the same lights.
- The siren source is implemented but not captured (reaching a pursuit needs
  driving input the capture method cannot inject).
- Street lamps and mission script lights were left as candidate sources; only
  the vehicle-derived ones were implemented.
