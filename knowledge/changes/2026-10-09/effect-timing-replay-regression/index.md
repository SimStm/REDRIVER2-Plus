---
type: Change
title: Desktop effect timing and replay regression investigation
description: Tick-owned effects pass FPS comparisons; the existing replay trajectory fault remains open.
tags: [okf, timing, effects, replay, validation]
---

# Desktop effect timing and replay regression investigation

The user reported accelerated smoke, exhaust, lights and blinking UI above NTSC
30 FPS, plus replay cars hitting walls and jumping above 40-50 FPS. Requested
fixtures include Chase 1-4 and `ATTRACT.400`; the user identified upstream
`b2d8857` as a known-good revision from memory. This complementary correction
does not accept or advance E00-E03 or change the adopted migration scope.

## Implemented boundary

- `main.c` separates effect updates from drawing through `StepDesktopGame`;
  skipped cutscenes use the same update seam. Replay labels draw every frame.
- `debris.c/h` separates pool lifecycle and drawing, interpolates local poses,
  handles sparse active slots, invalidates reused history, updates birds/rain at
  ticks, gathers leaf candidates independently of drawing and removes shared
  draw RNG consumption for damaged lamps/splashes.
- `cars.c/h`, `cosmetic.c/h` move engine/exhaust emission and indicator/siren
  phase to ticks; lights consume the interpolated body pose and rear-light
  brightness is render scratch.
- `denting.c/h` separate hubcap simulation from local interpolated drawing.
- `loadview.c/h`, `bomberman.c/h`, `pause.c/h`, `director.c` move title/input
  unlock, bomb flash, message expiry and replay sound control to their update
  owners. Original PSX branches remain guarded.
- `DeterministicBaseline.*` add field-wise effect digest and phase records plus
  a before/after presentation mutation check. Comparator numeric-name parsing
  now includes damage/timer keys such as `d0` and `t0count`; strict mode checks
  complete inventories. `test_compare_state.py` adds eight parser regressions.
- `EffectTimingTests.cpp` links actual debris/title update code into the fresh
  NTSC/PAL timing runner. No production replay input, physics or tape resampling
  change was made to force the observed bad trajectory to match waypoints.
- `objanim.c/h` keep palette cycling data in the tick owner and copy current
  upload packets into per-frame primitive storage; catch-up no longer inserts
  the same static packet twice. `main.c` submits it once per frame from view0.

## Executed validation

Evidence is private/ignored under `src_rebuild/build/fx-replay-ticks/` and
`src_rebuild/build/presentation-timing-tests/`; no original assets are added to Git.
Runtime cases use copied INIs and read-only access through data-directory
junctions. Sequential fire-and-forget game launches are observed by their dumps;
only owned test processes are stopped.

- `pwsh -NoProfile -File scripts/run_presentation_timing_tests.ps1`: PASS in
  fresh directory `75d16d33753541d8b9f8e1204f2372b2`. NTSC: 145354 timing/snapshot,
  21 settings and 2684 effect checks; PAL: 145354, 21 and 2444 respectively.
  Each suite reports zero failures. Effects exercise 15/25/30/60/120/360 FPS,
  sparse slots, lifetime/displacement, local interpolation, reuse, pause and
  title input unlock. Wind/camera/hubcap dependencies are substitutes; native
  rain projection and GPU drawing are outside this unit suite.
- A prior fresh NTSC settings run reported two persistence assertions; a later
  full fresh run passed. The transient file-replacement cause is not established.
- `python scripts/engine_migration/test_compare_state.py`: eight tests PASS.
- MSBuild `build_e00/REDRIVER2.sln` and
  `build_e01_motion_pal/REDRIVER2.sln`, `Release_dev|x64`: both PASS, exit 0.
  Production executables are `bin/Release_dev/REDRIVER2_dev.exe` and
  `bin/e01_motion_pal/REDRIVER2_dev.exe`, explicitly selected with `-opengl`.
  These checks do not use a `Release_dev_gl` configuration or accept Diligent.
- Complete NTSC `final-attract-30/state.txt` versus
  `final-attract-360/state.txt`, comparator `--strict`: 1378/1378 ticks,
  zero tick/entity/field differences, zero recorded presentation mutations.
  The high-FPS run presents 15430 frames for those ticks.
- Complete PAL `final-pal-25/state.txt` versus `final-pal-360/state.txt`,
  comparator `--strict`: 1378/1378 ticks, zero differences, zero recorded
  presentation mutations. Each comparison is within its region.
- Mission 70/gametype3/chase2, final OpenGL 30/360 FPS, `--strict`: all
  296 ticks match, including the recorded effects. The target-escape outcome
  limits this check to a segment, not a complete Chase trajectory.
- A live OpenGL screenshot shows the damaged ATTRACT car/flames rendered.
  A single screenshot does not validate smooth temporal motion or effect speed.

## Additional night/rain regression

`ATTRACT.401` exercises native rain projection and night-time cycling palettes.
The first 30 FPS run froze at tick 857/frame 854. A read-only stack/OT probe located
`ParsePrimitivesLinkedList` traversing a self-linked `cyclecluts[0]` packet.
Several catch-up ticks had inserted that static upload packet into the same OT.
This was a tick/draw ownership defect; the rain draw itself was not the cycle.

After separating palette update/upload, NTSC/PAL production builds pass again
(`build-palette-fix.log`, `build-pal-palette-fix.log`). Final OpenGL
`rain-final-30/state.txt` versus `rain-final-360/state.txt`, `--strict`: 2289/2289
ticks, zero field/inventory differences or recorded draw mutations; both pass
the former stall and complete the recording. A live screenshot of the final
night/rain scene was inspected. The fixed effects suite also checks catch-up
without OT writes, independent packets for repeated draws, unchanged palette
data and invalidation on loading. Final fresh runner directory
`0402adef0c9f48d2a15c8f7309fe2596`: 145354 timing and 21 settings checks per region,
plus 2688 NTSC/2448 PAL effect checks, all zero failures. Intermediate test
compilation attempts failed while adding legacy-header/stub dependencies; those
were corrected before the passing full run. PSX/web/Android builds were not run.

## Replay route investigation: unresolved

The pre-change game, legacy scheduler probe, `b2d8857` game linked with the
current fork, and `b2d8857` linked with its historical PsyCross gitlink
`e56e4cde1c2b8a15e0d4e38b26cdd9202e0d17e6` all produce the same shared gameplay
state for the full 1378-tick ATTRACT recording on the local data. The car still
collides and accelerates into a wall. The original plus historical fork was
also built/run as Win32; its full canonical dump matches the x64 reference
strictly. This rules out those tested timing/fork/architecture differences for
this fixture; it does not establish asset validity or original recording fidelity.

Reference sources are isolated `git archive` snapshots; historical PsyCross was
retrieved from the project fork's history, never substituted in the production
submodule. Diagnostic hooks only observe canonical state and skip their own
CLI arguments. Production source/gitlink and user assets are not replaced.
Win32 probe links only the historical consumers and x86 SDL/OpenAL/JPEG.
Diagnostic builds passed with existing linker warnings about duplicate observer
objects, CRT libraries and import descriptors; no clean-warning claim is made.

A private `FixCarCos` bypass worsened the route and was rejected. Replay cursor
consumption stays tick-owned. V1 waypoint bytes do not prove corruption and are
not substituted for the recording. Direct mission 71-73 launch probes load no
matching local cutscene bundles and record no ticks; they are not valid Chase
acceptance tests. Mission 70/gametype3/chase2 runs stop after 296 ticks because
the stationary player loses the target, so this fixture cannot certify a full
Chase route. The user confirms NTSC files; edition/modification provenance is unverified.

The official upstream 8.0 `REDRIVER2_Win32_Release_dev.zip` was then downloaded
from its GitHub release into ignored evidence. Only executable/PDB/runtime DLL
entries were extracted, with isolated config and the same data junction. Valid
original options `-nointro -nofmv -replay <ATTRACT.400>` launched directly without
diagnostic-only switches. Live screenshots show tyre smoke while the car points
into the building and, at Game Over, the car resting against that wall. This is
visual reproduction in the published binary, not a canonical dump comparison.
The owned process was stopped afterward. Release 8.0/tag b2d8857/2026-07-02 was
rechecked through the official GitHub releases API before changelog edits.

On 2026-10-09 the user deferred replay route investigation and requested final
validation, commit and push of the FPS/effects adjustments only. E00-E03 retain
their in-progress status. No waypoint/physics workaround is included. When the
user resumes routes, obtain a working reference and compare its first divergent
authoritative tick; matching FPS schedules alone does not certify the route.

## Scoped publication validation

The prepared FPS/effects index was exported to ignored `publish-snapshot` with
clean project-fork revision 76d83df. Local input-service, replay-versioning,
E02 converter and E03 Diligent changes remain outside this publication. Fresh
Premake projects rebuilt NTSC and PAL `Release_dev|x64` with MSBuild v145, both
exit 0. Runner `2745e12d5d14494295a4b94a1dd7c55d` passes 145354 timing and 21
settings checks per region, plus 2688 NTSC / 2448 PAL effects checks; eight parser
regressions pass. Its OpenGL 360 FPS ATTRACT.401 dump matches all 2289 ticks of
`rain-final-360/state.txt` strictly, with zero recorded draw mutations. The owned
process was stopped. `git diff --cached --check` and changed-document link checks
pass. Existing linker warnings remain; no other-platform acceptance is implied.
