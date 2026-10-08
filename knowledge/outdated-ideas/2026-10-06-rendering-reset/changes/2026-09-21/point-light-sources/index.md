---
type: Change
title: Point-light sources and legacy receptivity
description: Publish headlights, police siren lights and a validation probe into the modern light set, and shade them on legacy geometry in the receptivity composite on both backends.
tags: [rendering, lighting, psycross, vulkan, opengl]
---

# Point-light sources and legacy receptivity

Implements [`roadmap/planned/point-light-sources.md`](../../../roadmap/done/point-light-sources.md).
The modern light set carried a directional sun only (`DeveloperModernMesh.cpp`
set `count = 1`), and the legacy receptivity composite read `lights[0]`, so no
light that was not the sun could reach the legacy scene. Product behaviour is
documented in [`product/point-light-sources.md`](../../../product/point-light-sources.md).

## What changed

- **Game layer** (`utils/DeveloperModernMesh.cpp`): candidate point lights are
  collected from live state each frame and published after the sun, nearest to
  the shadow centre first, capped at `PSYX_MODERN_COMPOSITE_POINT_LIGHTS`.
  Sources are the player's two headlights, the roof light of each active police
  car with a siren (red/blue in the game's own flash phase) and a validation
  probe on the player car that replaces the others. New persisted settings
  `pointlights`, `pointlightlimit`, `pointlightrange`, `pointlightintensity`,
  `headlights`, `sirenlights` and `pointlightprobe`; the `Baseline:` log line now
  reports the published point-light count.
- **Panel** (`utils/DeveloperGraphicsPanel.cpp`): a **Point lights** group with
  the toggle, limit, range, intensity, both sources and the probe.
- **Composites**: `vk_shaders/psx_composite.frag` and the GLSL string in
  `PsyX_ModernMesh.cpp` loop `lights[1..n]` with
  `tint *= 1 + legacyScale * N·L * colour * clamp(1 - d/range)²`, reusing the
  five-tap reconstructed normal. Debug view `11` shows the point-light coverage.
  `PSYX_MODERN_COMPOSITE_POINT_LIGHTS` (public header) is the shared cap, and the
  OpenGL composite uploads `u_pointLightPos/Color/Range` arrays of that size.
- **Vulkan gate** (`PsyX_Vk.cpp`): the composite no longer requires light 0 to be
  directional, and debug view 11 joins the documented range.
- **Fixture** (`utils/DeveloperVkFixture.cpp`): `-vkpointlight` (and the `L` key)
  publishes a point light in the standalone Vulkan fixture, so the multi-light
  plumbing can be captured without game state.

## The defect this exposed

The point-light block was introduced without setting `PsyXModernLight.intensity`.
The light set is zero-initialised and both backends multiply the colour by
`intensity`, so every published point light had a black colour: the coverage
diagnostic (`legacyPointNdl`, which does not use the colour) showed the light
correctly while the shaded frame showed nothing. Found by an A/B that kept the
sun at zero intensity and the point intensity at the maximum and still produced
identical frames; fixed by publishing `color` and `intensity` separately like the
sun does. The fixture, which already set `intensity = 1.0`, was the first place
the light worked.

## Executed evidence

- Windows `Release_dev|x64` and `Release_dev_gl|x64`: 0 failed projects.
- Mission 50 deterministic spawn, 1280x720, probe light, strength `0.300`,
  range `3000`, one capture per state, same-backend control:
  - off versus on: radial footprint around the player car (per 80x80 cell, means
    up to `17.9`, maxima up to `31` on road and sidewalk; `0` in sky/HUD).
  - range `600` versus off: no point-light difference anywhere.
  - headlights (`pointLights=2` in the log) versus off: same footprint, means up
    to `42`, maxima up to `80`.
  - OpenGL probe A/B: the per-cell means and maxima match the Vulkan grid cell for
    cell.
  - cross-backend with the probe on: `meanAbs 3.149/255` (established backend
    difference is `3.07-3.08`).
- `-vkfixture -vkpointlight` versus without: the point light changes the modern
  mesh shading near it (per-cell maxima `15`), with no legacy scene involved.
- `PSYX_PERF_LOG=1`, 30.0 FPS in both runs: `submit_ms` mean `6.84` without and
  with two published lights (medians `6.78` vs `6.85`, sample spread `6.6-7.4`).
- `REDRIVER2_dev.exe -vkpsxtest`: `psx self-test: PASS`, including the stencil
  pass restart.
- Panel capture: the **Point lights** group renders and applies live (F11).
- `git diff --check` clean in both repositories.

## Retune after user feedback

The user could not see the effect in game. The mechanism was working - the A/B
captures showed it - but the *tuning* was wrong: the offsets and the range were
fixed distances picked by eye, and the game's units-per-metre is not documented,
so the two headlights sat inside the car body and the default range lit only a
couple of car lengths. The frame differences were `+5..16/255` on the road,
which the eye reads as nothing in a busy street scene.

What changed:

- **The offsets are now fractions of the player car's collision box**
  (`hd.oBox.length`, logged once as `Baseline: playerCar boxLength`; the capture
  car measures `139 x 86 x 396`, so ~4.5 m long at the legal-car scale). The
  headlights sit at `1.0` box lengths ahead, `0.8` box widths to each side and
  `0.9` box heights up; the siren at `1.3` box heights; the probe at `1.6` box
  lengths and `1.6` box heights. A fallback box is used while the collision box
  is not populated.
- **Defaults raised**: `pointlightrange` `1600 -> 4000`, `pointlightintensity`
  `1.0 -> 1.5`. `legacyLightReceptivity` still scales whatever is published, so
  the panel sliders remain the authority.
- Both changes are in the game layer only; the composite term is untouched, so
  the earlier per-backend port parity still applies.

Re-measured at the new defaults (same method):

- Probe versus off: a pool around and behind the car, per cell means up to `32`
  and maxima `66` (was `17.9`/`31`); four road pixels `+10%` to `+16%`.
- Headlights versus off: the same pool on the road, kerb and markings, visible
  in a side-by-side crop.
- Backend parity, per pixel: the added light averages `+6.203/255` (Vulkan) and
  `+6.112/255` (OpenGL) over the frame, and `|effect_VK - effect_GL|` averages
  `0.311/255` with `200` pixels (`0.02 %`) above `8/255` (both builds at the
  same tuning; an earlier pass compared a retuned Vulkan build against a
  pre-retune OpenGL one and read `4.188/255` at whole-frame level, which is what
  prompted rebuilding both).
- Whole-frame cross-backend difference: `3.169/255` lights off, `3.057/255`
  lights on, i.e. the lit area adds no backend difference.
- `submit_ms` and the 30.0 FPS gate are unchanged; the loop cost did not move.

## Not measured

- The siren source was not captured: reaching an active police pursuit needs
  driving input, which the capture method cannot inject. It shares the verified
  candidate and publish path.
- No minimum-GPU-class numbers; the cost claim is the 30 Hz-gated `submit_ms`
  comparison above.
