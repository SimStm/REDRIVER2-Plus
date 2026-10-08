---
type: Design
title: R00 source coverage and migration ownership
status: inventoried
created: 2026-10-07
tags: [okf, rendering, baseline, coverage]
---

# Source coverage and migration ownership

Inspected parent `2e93f215` with existing local changes and fork PsyCross
`446b96da`. Existing experiments are not the R01 native scene. Paths below
are relative to `src_rebuild/`; retain original data and game responsibilities.

## Coverage owners

- **Roads, terrain, pavement, grass:** `Game/C/tile.c::DrawTILES`, original
  `MODEL`/`PACKED_CELL_OBJECT`, `models.c::ProcessMDSLump`, `spool.c` region
  payloads and LOD tables. Game original-static adapter owns decoding; visual
  scene owns resources/placements. R02 decoding, R03 complete slice, R04 city.
  Counter `road`: one selected tile producer request/emission per view.
- **Buildings and ordinary props:** `draw.c::DrawAllBuildings`, `PlotBuildingModel*`,
  `MODEL::poly_block`, `PolySizes`, `GET_MODEL_DATA`, cell rotation/placement.
  Same static owner/stages. `scenery` counts per selected object; successful
  `DeveloperStaticScene_TryDraw` goes to **experiment**, never native. Extended
  experiment-only submissions are not included in this camera-list count.
- **Foliage, sprite scenery, billboards:** `draw.c::DrawSprites`, sprite model
  flags and original texture/CLUT records. Static adapter plus cutout material
  policy owns this, R02/R03; shadows require R05 silhouette parity. `foliage`
  counts selected billboard bodies. Their ground shadows remain decals/effects.
- **Water and translucent tiles:** water flag on selected `tile.c` model, original
  palette/STP, `texture.c` texture-page mapping. Static decoder owns geometry;
  material adapter owns original blend operations. R02/R03/R05. `water` counts
  tile emission batches; water is not automatically an opaque shadow caster.
- **Car body, LOD and damage:** `models.c::ProcessCarModelLump`, `spool.c` clean/
  damaged/low-detail model loading, `cars.c::DrawCarObject`, `plotNewCarModel`,
  simulation damage buffers and `denting.c`. Car adapter consumes copied component
  geometry and authoritative damage; R06. `car-body-damage` counts body batches.
- **Wheels:** `cars.c::DrawCarWheels`/`DrawWheelObject`, resident wheel model,
  wheel transforms and suspension. Car adapter, R06. `wheels` counts individual
  wheel producer batches, separately from bodies.
- **Pedestrians/Tanner/animation:** `motion_c.c::ProcessMotionLump`, `DrawCiv`,
  `DrawTanner`, `DrawCharacter`, bone/sprite pose data; `pedest.c::DrawAllPedestrians`.
  Pedestrian adapter copies poses without advancing animation. R06. `pedestrians`
  counts one aggregate producer batch per game view, not each bone/person.
- **Animated static props and lamps:** `objanim.c::DrawAllAnimatingObjects`/
  `animate_object`, model animation flags and level-specific animating records;
  CLUT cycling in `ColourCycle`. Original-static adapter preserves identity;
  snapshot/effect adapter owns changing visual state. R03 static shape, R06
  changing geometry, R07 lights, R10 effects. `animated-props` counts aggregate
  selected animating-list batches, including associated original effects.
- **Bridges, ferris wheel, trains, ferry, mission/event objects:** `event.c::DrawEvents`,
  `DrawFerrisWheel`, `DrawRotor`, `draw.c::RenderModel`; simulation owns event
  transforms and collision. Event adapter, R06. `events` counts two event phases
  per view; reflected event geometry is additionally tagged `secondary`.
- **Particles, smoke, debris, flares, lamps, weather, bombs/explosions:**
  `debris.c::HandleDebris`, `DoWeather`, `DrawSmashable_sprites`, `sky.c::DrawLensFlare`,
  `job_fx.c::DrawAllExplosions`/`bomberman.c` producers called by `RenderGame2`.
  Game visual snapshot/effects adapter owns billboard/quads and legacy blends;
  R10, with dynamic component snapshot separation in R06. `particles` counts
  these aggregate phases; nested lamp effects also appear under animated/events.
- **Decals and existing baked ground shadows:** tyre-track producer in
  `shadow.c::DrawTyreTracks`, sprite ground shadows in `draw.c`, car/pedestrian
  shadow helpers in `shadow.c`. Effect adapter owns explicit depth bias/blend;
  R10. `decals` measures tyre-track batches, sprite ground shadows,
  `PlaceShadowForCar`, `RoundShadow` and `TannerShadow` emissions.
- **Sky/horizon, sun and moon:** `sky.c::DrawSkyDome`, `PlotHorizonMDL`,
  `PlotSkyPoly`, sky texture/CLUT tables. Backdrop adapter, R03 classification,
  R10 native presentation. `sky` counts the aggregate dome batch. Painted
  horizon does not imply distant 3D meshes and receives no native world shadows.
- **Reflections/secondary outputs:** horizon reflected polygons in `PlotHorizonMDL`,
  mirrored event transforms in `DrawEvents`, wet-road light quads in
  `debris.c::DisplayLightReflections`. Effects/view adapter, R10. `secondary`
  counts horizon polygons, event reflection batches and wet-road light quads.
  No separate rear-view-camera renderer was found in
  the inspected source; do not invent an implemented mirror view.
- **HUD/minimap/overhead map/pause/text:** `overlay.c::DisplayOverlays`,
  `overmap.c::DrawOverheadMap`/`DrawFullscreenMap`, `pause.c`, `pres.c`,
  mission and driving-game overlays. 2D adapter, R10; texture extraction and
  original VRAM ordering remain explicit contracts. `hud` counts common
  `RenderGame2`/pause phases, persistent mission fades and the full-map phase.
- **Frontend:** `Frontend/FEmain.c::DrawScreen`/`DrawButton`/`FEPrintString`,
  persistent background packets and high-resolution font integration. 2D adapter,
  R10. `frontend` counts seven persistent background packets plus aggregate
  allocated foreground emissions; font direct draws need separate GPU accounting.
- **Loading and fades:** `loadview.c::ShowLoading`/`ShowLoadingScreen`/
  `FadeGameScreen`, original image transfers and persistent quads. 2D adapter,
  R10. `loading` counts direct progress-bar, loading-image, fade and shutter
  packets; persistent packets cannot be inferred from primitive-pointer changes.
- **Cutscenes/replay/director:** `cutscene.c::DrawInGameCutscene`, `camera.c`,
  `replays.c`, original actors and ordinary world producers. Mode/snapshot owner,
  R06 actors, R10 presentation, R11 cadence. `cutscene` counts the cutscene-specific
  producer phase; actual world remains in its geometry categories. A zero overlay
  emission does not mean native cutscene rendering. Desktop FMV remains the
  existing unsupported case, outside new delivery claims.

## Observable counter contract

`utils/DeveloperRenderCoverage.*` exposes C-compatible diagnostics and an F11
**Rendering migration coverage** table. `requested` counts producer calls or
selected objects after original visibility selection. `legacy` counts nonempty
primitive-emission batches, not faces, visible objects, parsed GPU draws or
reasons for missing objects. Existing modern ownership is `experimental`;
`native` is reserved and stays zero before R01. No automatic percentage.

Counters reset at game/frontend/progress frames; two-player game views aggregate
in one game sample. Optional `PSYX_COVERAGE_LOG=1` writes all named counters every
120 completed producer frames, including the first. Diagnostics consume pointers
only during calls and retain no source addresses. Secondary/reflection counts
overlap their parent sky/event batch; **do not sum columns into GPU totals**.
Zero requested/emitted counts can mean the category is absent or culled. Aggregate
particle/pedestrian/event batches include nested direct packets; they establish
an observable fallback, not exhaustive packet accounting. R12 needs GPU accounting.

The inspector's resource/instance/component identities and export contracts are
unchanged. Native picking/export must attach the same catalog identity to owned
triangles/instances in R02/R03/R06/R09, with original and active material views.

## Native material, shadow and inspection policies

Every row currently uses the legacy colour path unless explicitly labelled
experimental. Native picking/export is unimplemented for these adapters; attach
the original catalog identity at the destination stage, never infer it from a
transient pointer. Tested modes are the baseline city/replay/playground plus
frontend/loading counter logs; other mode acceptance remains pending.

Original opaque roads/scenery/car/actor/event surfaces are potential native
casters/receivers after their decoder/dynamic stage; prelit face colour remains
material tint rather than geometric normal data. Foliage/cutouts must use the
same texture-alpha silhouette for colour and mapped shadows. Water, translucent
effects, reflections and decals retain ordered blend/depth-bias policies; no
automatic opaque mapped casting. Sky, HUD, frontend/loading/fades and overlays
neither cast nor receive world shadows. Reflections require separate view/target
ownership when they become true views. Current experimental resident/PGXP casters
do not prove these future policies are implemented.

## Mode and mutation audit

Single-player, split-screen, on-foot, missions, driving games, attract/user replay,
director, in-game cutscenes, loading, frontend, pause and overhead map remain
separate validation cases. They are inventoried, not runtime-accepted by one city.

`main.c::FilterFrameTime` waits two emulated vblanks; `PsyX_main.cpp` selects
NTSC/PAL timestep from `g_vmode`, and `LIBETC.C::VSync` consumes that clock.
`CameraCnt` advances in simulation, `FrameCnt` in `DrawGame`. NTSC 30 Hz and PAL
25 Hz are source-established; this Windows executable is NTSC, not PAL evidence.

Rendering is not read-only today: `DrawMapPSX` changes `goFaster`, tree/alley and
ground-debris selection counters; `debris.c` bird drawing advances position/step/
life, `HandleDebris` maintains particles, and `FadeGameScreen` advances fade state.
Sky/siren/animated colours depend on tick counters and CLUT updates. R06/R10
snapshot extraction must separate these mutations before R11 can draw extra
presentations. R00 instrumentation preserves draw order and does not rerun them.
