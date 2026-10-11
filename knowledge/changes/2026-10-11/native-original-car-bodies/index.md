---
type: Change
title: Resident original car body submission and source texture links
date: 2026-10-11
tags: [okf, engine, diligent, rendering, assets]
---

# Resident original car body submission and source texture links

The native resident-world renderer now submits clean original car body meshes with live per-car
scene transforms. The source asset catalog also extracts texture keys for declared PL10/PL11
polygon links through the same helper used by production classification. The texture fix improves
source identity coverage; it does not resolve every unknown material classification.

## Validation

- All 20 inspector suites passed, including the production-linked PL10/PL11 texture-key
  regression, `NativeCarMeshTests` and `NativePedMeshTests`. `NativeRendererSmoke` passed 1,274
  checks with zero failures. Isolated x64 `Release_dev_native` and reference `Release_dev` builds
  succeeded. The separate reference OpenGL startup reached asset loading and was stopped cleanly.
- Chicago mission 50 launched with `-mission 50 -drawregion -nointro -nofmv`. Logs showed the
  player car (`playerCar=0`) and 14/14 cars and 56/56 wheels ready/submitted. The world reported
  193 resources and 8,890 placements. At capture the HUD showed 12 active pedestrians, 2 ready,
  1 submitted, 1 waiting and 0 failed; four `CIVILIAN` meshes reached ready over the session.
- RenderDoc 1.41 capture `native-ped-chicago-tad50-20261011.rdc` replayed 719 draws with zero
  debug messages. The inspected PNG shows Chicago streets, the player car and a civilian at the
  left sidewalk. Steady samples reported 30 ticks and zero interval drops, while loading recorded
  five catch-up ticks dropped in total. The reference OpenGL startup reached asset loading.
- `-mission 1 -gametype 0` loaded `CUT1.R`; its visible type-0 actor had `pad=-2`, so that story
  capture does not prove player control or Tanner on foot. Mission 50 proves only a bounded
  pedestrian sample, not complete pedestrian/city coverage, input behavior, damage, lighting or
  parity. The inspected PNG and logs remain ignored under `build_e00/native-performance/`.
- Game processes were stopped. `config.ini`, `developer_graphics.ini`, and the corrected
  `.vcxproj.user` profile match pre-run SHA-256 values.

This is bounded Chicago TAD evidence, not full city or engine-migration acceptance. Complete
pedestrian coverage, damage, story missions, on-foot views, full city/streamed-route coverage and
scene parity remain open.

## Follow-up: dynamic wheels, pedestrians, damage updates, and diagnostic HUD

The text-only `RED2_NATIVE_VISUAL_DEBUG=1` HUD runs on the existing overlay independently of F9
and does not capture input. Wheel positions update from current actor transforms; source snapshots
are owned CPU data, workers do not dereference live models, and generation/revision changes reject
stale work. Dynamic uploads validate finite vertices/counts before bounds are committed. Legacy
constant-black wheel quads are preserved and participate in source cache identity.

Pedestrian presentation preserves CPU pose evaluation and `head_pos`; owned component and sprite
data is converted off-thread, then dynamic XYZ/UV is updated before shared instance publication.
It is bounded to 28 actor slots, 24 components each, and 4,096 expanded corners. Legacy animation
ownership and interpolation remain outside the native system.

Per-car, per-LOD bodies now update expanded vertex positions and packed UVs from the original dent
arrays before the shared scene submission. Owned corner bindings preserve polygon identity; cold
conversion builds bounded page/CLUT atlases and stable frames update dynamic XYZ/UV. Focused tests
cover packet carry, LOD choice, point/bilinear atlas edges, bounds and atomic rejection. The original
car damage event was not triggered during Chicago or Havana capture, so visible damage is not yet
verified. Legacy car-body lighting remains open.

The corrected mission 50 capture shows player-car identity and a submitted civilian, with limited
counts and image evidence. The source-supported alternate TAD sample is mission 52 (`-mission 52`):
the first `-level 1 -mission 50` attempt loaded city 0 because mission 50's header replaces the
requested level. Mission 52 reported level 1 and 430 models / 452 textures; its RenderDoc PNG shows
Havana streets, buildings and vehicles. This is an additional bounded city sample, not complete
Havana or streamed-route coverage.

The latest validation passed all 21 inspector suites and 1,285 GPU-smoke checks; isolated native and
reference x64 builds succeeded. The first MSBuild attempt failed MSB8052; final commands used
v145/v180 CLI overrides. Chicago damage-run capture `native-car-damage-chicago-20261011.rdc`
replayed 703 draws; Havana mission 52 capture `native-car-damage-havana52-20261011.rdc` replayed
959. Both had zero RenderDoc debug messages, and neither emitted a damage-change event.

The user reports entering the car and moving successfully (2026-10-11), but did not specify backend
or confirm controlled Tanner visibility on foot. Full pedestrian coverage, player-exit/on-foot proof,
visual damage, lighting and broader city coverage remain outstanding.
