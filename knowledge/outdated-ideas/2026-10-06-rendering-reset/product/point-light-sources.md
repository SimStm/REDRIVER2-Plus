---
type: Product
title: Point-light sources
description: Game-state point lights published into the modern light set, shading modern meshes and the legacy scene with range falloff.
tags: [product, rendering, psycross, lighting, shadows]
---

# Point-light sources

The modern light set is no longer a single directional sun. The game publishes a
small, distance-ordered set of point lights derived from live state - the player's
headlights and the light bars of nearby police cars - next to the sun, and the
legacy receptivity composite shades them with the same normal reconstruction it
uses for the sun. Modern meshes already consumed the whole light set; the legacy
scene now reacts to a light that is not the sun.

Delivery history and evidence: change record
[`knowledge/changes/2026-09-21/point-light-sources/index.md`](../changes/2026-09-21/point-light-sources/index.md)
and roadmap record
[`knowledge/roadmap/done/point-light-sources.md`](../roadmap/done/point-light-sources.md).

## Controls and persistence

- **Graphics panel, "Point lights"** (`DeveloperGraphicsPanel.cpp`):
  - **Legacy point lights** publishes the sources at all (off by default).
  - **Point light limit** caps the published count, `1` to
    `PSYX_MODERN_COMPOSITE_POINT_LIGHTS` (`4`).
  - **Point light range** is the per-light falloff distance in world units,
    `100` to `8000`.
  - **Point light intensity** scales every published point light, `0` to `2`.
  - **Player headlights** and **Police siren lights** select the sources.
  - **Validation probe light on the player** replaces the game-derived sources
    with one known light, for captures that must not depend on traffic.
- `developer_modern_mesh.ini` keys: `pointlights` (default `0`), `pointlightlimit`
  (default `2`), `pointlightrange` (default `4000.0`), `pointlightintensity`
  (default `1.500`), `headlights` (default `1`), `sirenlights` (default `1`),
  `pointlightprobe` (default `0`). The file keeps its existing hand-rolled
  reader/writer: unlike the other developer settings files it stores floats and a
  vector, so `DeveloperSettingsFile_WriteKeys` (integers only) cannot express it.
- The one-shot `Baseline:` log lines in `REDRIVER2.log` report
  `pointLights=<n>` (published point lights, the sun excluded) and
  `playerCar boxLength=<x>,<y>,<z>` (the player car's collision box, the length
  reference the light offsets are scaled by), so a capture run can confirm both
  what was published and what it was anchored to.
- The point lights need **Legacy geometry receives modern lighting**: the
  composite's light terms are scaled by `legacyLightReceptivity`, so a strength
  of `0` disables the sun and the point lights together.

## Sizing

The offsets and the default range are expressed as fractions of the **player
car's collision box** (`hd.oBox.length`, logged as `playerCar boxLength`; the
capture car measures `139 x 86 x 396` world units), not as fixed distances:
the game's units-per-metre is not documented and a fixed offset either sat
inside the car or lit nothing. The first shipped defaults (`range 1600`,
headlights `400` units ahead) were in that trap and the user could not see the
effect; the current ones are:

| Light | Position | Range |
| --- | --- | --- |
| headlight (x2) | `1.0` box lengths ahead, `0.8` box widths to each side, `0.9` box heights up | `pointlightrange` |
| siren (per police car) | `1.3` box heights up | `0.75 * pointlightrange` |
| validation probe | `1.6` box lengths ahead, `1.6` box heights up | `pointlightrange` |

A fallback box (`150 x 90 x 400`) is used when the collision box is not
populated yet, so the light never collapses onto the car during a transition.

## Sources

- **Headlights** (`AppendHeadlightCandidates`): two lights per player car at the
  front corners, warm white `(1.0, 0.92, 0.72)`. The car's forward/right basis
  comes from `hd.direction` through `RSIN`/`RCOS`, exactly like
  `PositionGallery`, and the offsets are fractions of the car's collision box
  (see Sizing below).
- **Siren lights** (`AppendSirenCandidates`): one light per active police car
  that has a siren (`CarHasSiren(cp->ap.model)`) and is chasing
  (`CONTROL_TYPE_PURSUER_AI` or `CONTROL_FLAG_COP`), on the roof (box height
  `1.3`), red/blue alternated with the phase the drawn light bar already uses
  (`ap.coplife`), range `0.75 * pointlightrange`.
- **Validation probe** (`AppendProbeCandidate`): one warm light `(1.0, 0.85,
  0.55)` in front of and above the player car. It replaces the other sources so
  an acceptance capture has exactly one published point light.

Candidates keep a fixed-size array in ascending distance from the shadow centre
(which follows the player), so the cap always keeps the nearest lights and the
rest are rejected before the light set is built. Publishing happens in
`ApplyLightSet`, which runs every frame while the enhanced renderer is enabled.

## Composite behaviour

- Light 0 is always the directional sun. Point lights occupy indices `1..n` and
  both backends loop the same term:
  `tint *= 1 + legacyScale * N·L * colour * attenuation²`, with
  `attenuation = clamp(1 - distance / range, 0, 1)`. The term is additive to the
  sun term and scales the already-lit legacy colour, like the sun does.
- `N·L` uses the same five-tap reconstructed normal as the sun term, and the
  light direction is the world-space vector from the reconstructed position to
  the light rotated into view space. Distances and the range are in world units.
- Point lights cast no shadow: only the directional shadow map exists, and the
  shadow term is applied before the light terms.
- The loop breaks at the published light count and skips directional entries, so
  a point light beyond its range costs one distance test per pixel.
- Vulkan reads the shared light UBO directly; OpenGL uploads the point lights as
  `u_pointLightPos/Color/Range` arrays sized by
  `PSYX_MODERN_COMPOSITE_POINT_LIGHTS`. Both composites stay line-for-line
  equivalent, and the cap exists so the two backends cannot shade different
  lights.
- Debug view `11` (**Point light coverage**) renders
  `scene * (0.25 + 0.75 * max(N·L * attenuation²))`, i.e. the point-light
  coverage alone, which is the fastest way to see the falloff.

## Measured evidence

Windows `Release_dev|x64`, deterministic mission 50 spawn, 1280x720, strength
`0.300`, one capture per state, same-backend control. The figures below are at
the current defaults (`range 4000`, `intensity 1.5`); the first-tuning numbers
are in the change record.

- Point lights off versus probe on: a bright pool around and behind the car on
  the road, kerb and sidewalk, fading with distance - per 80x80 cell, means up
  to `32` and maxima up to `66`, `0` in the sky and HUD. Four sampled road
  pixels: `(116,106,68) -> (129,116,73)`, `(181,172,162) -> (199,187,171)`,
  `(179,174,168) -> (203,194,180)`, `(152,148,139) -> (177,169,152)`, i.e.
  `+10%` to `+16%` with the light at the capture's strength.
- Headlights (`pointlightprobe=0`, log `pointLights=2`) produce the same pool on
  the road, kerb and markings around the car, visible side by side against the
  lights-off capture.
- **Backend parity, per pixel**: comparing the *added light itself*
  (`on - off` per backend, both builds at the same tuning) gives Vulkan mean
  `+6.203/255` and OpenGL mean `+6.112/255` over the frame, and
  `mean |effect_VK - effect_GL| = 0.311/255` with only **200 pixels (0.02 %)**
  above `8/255`. The two ports apply the same term.
- Whole-frame cross-backend difference: lights off `meanAbs 3.169/255`, lights
  on `3.057/255` with `10.11 %` strong pixels against `10.28 %` off - the lit
  area adds no backend difference beyond the existing one.
- Range `600` versus off: no point-light difference anywhere in the frame - the
  effect is gone once the light's range is exceeded.
- Standalone Vulkan fixture (`-vkfixture -vkpointlight`, which has no legacy
  scene): the published point light changes the modern mesh shading in its
  neighbourhood, exercising the light-count plumbing without game state.
- Cost: `PSYX_PERF_LOG=1`, 1280x720, no point lights versus two published
  headlights at 30.0 FPS: `submit_ms` mean `6.84` both, inside the `6.6-7.4`
  sample spread. Two extra loop iterations in the already-active full-screen
  composite did not measure.

## Limits

- The siren source is implemented but was not exercised in a capture: reaching an
  active police pursuit needs driving, which the capture method cannot inject.
  It shares the candidate/insert/publish path verified for the headlights and is
  reachable through the panel toggle.
- No shadows, per-pixel culling, light bakes or emissive materials: the shadow
  map stays directional and the only per-light rejection is the range test.
- The count is capped at four so both backends shade the same lights; the light
  budget of the modern path allows more.
- The visual choice of sources is a gameplay decision and is kept behind the
  developer toggles; nothing in the shipped (classic) renderer changes.
