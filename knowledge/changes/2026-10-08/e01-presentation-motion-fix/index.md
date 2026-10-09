---
type: Change
title: Correct E01 car and camera presentation timing
created: 2026-10-08
tags: [okf, e01, timing, camera, determinism, regression]
---

# Correct E01 car and camera presentation timing

## Scope and outcome

The user reported car flick/jumps across display FPS and authorized testing and
fixing the regression without advancing engine-migration status. Inspected
`feat/new-engine` at `9c353405` with substantial pre-existing E00-E03 local changes.
Those changes and the existing PsyCross fork modifications were retained. E01
remains in progress; no stage acceptance, renderer cutover or replay resampling.

## Verified causes

- `DrawCar` interpolated translation between previous/current simulation ticks,
  while `RenderGame2` called `InitCamera` every presentation frame using current
  tick car state. The follow/head/distance/collision controller consequently
  advanced at display FPS. The mixed sample times produced a car/camera sawtooth.
- Car rotation and `SetShadowPoints` still used authoritative current transforms.
  Translation alone could not smooth turning/bouncing or align shadow corners.
- Snapshot history lacked explicit initialization/reset and paused-pose handling.
  A duplicate tick capture could replace the previous pose while alpha cycled.
- The scheduler rounded NTSC periods to 33333 microseconds and dropped fractional
  phase with bounded backlog, rather than preserving exact 30 Hz counter phase.

## Implementation

- `utils/ProjectClock.*`: counter-count x regional-rate phase; one tick equals
  counter frequency. NTSC stays 30 Hz and PAL 25 Hz even with indivisible counter
  frequencies. Max four catch-up ticks and explicit discarded-tick reporting
  remain; fractional phase survives. Reset clears pending injected elapsed time.
  Existing integer replay tick-period metadata remains 33333/40000, unchanged.
- `utils/VisualSnapshot.*`: car translation/rotation and separate camera snapshots
  per view; unchanged gameplay ticks do not rotate car history. Original camera
  controls execute at scheduled ticks. Paused replay editing remains an explicit
  camera operation at regional service cadence and collapses unchanged-tick history.
  Camera cuts, target/view changes, teleports and slot/session resets delimit history.
- `Game/C/main.c`: seed snapshots/cameras during initialization, advance cameras
  after tick work, apply presentation cameras during drawing, restore current tick
  camera globals before simulation resumes. Drawing an invalid snapshot never
  initializes or advances the player camera controller.
- `utils/PresentationInterpolation.h`: widened translation arithmetic, shortest
  wrapped angles and normalized quaternion rotation blending, with exact matrix
  endpoints and preserved row scale (including PAL camera aspect). All results
  belong to presentation; physics matrices and fixed-point math are unchanged.
- `Game/C/cars.c`, `handling.{h,c}`: local visual car transform for body/wheel
  transforms and shadow corners. Existing PSX tick rendering retains its path.
- `Game/C/camera.c`: desktop startup uses simulation count instead of draw count.
- `tests/PresentationTimingTests.cpp`, `scripts/run_presentation_timing_tests.ps1`:
  direct production-code tests with substituted SDL counter and camera controller.

## Executed validation

Commands are from the repository root unless indicated. Local evidence resides
in ignored `src_rebuild/build/e01-motion-fix/` and does not travel with Git.

- Fresh initial NTSC `Release_dev|x64` build established a runnable before binary;
  an additional before binary contained only the temporary temporal trace probe.
- The stale PAL `build_e00_pal` solution failed with missing `NativeRenderer.h`
  because it predated the existing E03 include/link wiring. Premake regenerated
  an isolated `build_e01_motion_pal` / `bin/e01_motion_pal` with `PAL_VERSION` and
  the current SDL/OpenAL/JPEG dependency paths; that build succeeded. Generated
  JPEG projects were backed up/restored. No source edit was used to bypass E03.
- Final MSBuild commands, both exit 0:

  ```powershell
  & 'C:/Program Files/Microsoft Visual Studio/18/Community/MSBuild/Current/Bin/MSBuild.exe' src_rebuild/build_e00/REDRIVER2.sln /p:Configuration=Release_dev /p:Platform=x64 /m /v:quiet /nologo
  & 'C:/Program Files/Microsoft Visual Studio/18/Community/MSBuild/Current/Bin/MSBuild.exe' src_rebuild/build_e01_motion_pal/REDRIVER2.sln /p:Configuration=Release_dev /p:Platform=x64 /m /v:quiet /nologo
  ```

  Logs: `build-final-ntsc.log`, `build-final-pal.log`. Existing reconstructed-source
  pointer/conversion and JPEG/link runtime warnings remain; successful builds do
  not independently prove presentation correctness.
- `pwsh -NoProfile -File scripts/run_presentation_timing_tests.ps1`: both regions
  pass, 145323 checks and zero failures each. Evidence:
  `src_rebuild/build/presentation-timing-tests/d615e58a42a84492938521549b12c5a4/`.
  Exact 600 NTSC / 500 PAL ticks over 20 seconds at 25/30/60/120/144/1000 FPS and
  irregular intervals, using counter frequency 1000003. Checks cover car/camera
  temporal alignment, unchanged CAR_DATA/PLAYER bytes, restored tick camera,
  pause, cuts, editor service, teleports, slot/session reset, split views, rotation
  shortest arc/size/aspect/endpoints, four-tick catch-up and retained half-tick phase.
- Live game runs used owned `Start-Process` processes (without waiting on the
  foreground executable) after the debugger launch tool failed. Computer-use
  inspection confirmed actual game windows. Each owned process was stopped by PID.
  User INIs stayed intact; uncapped OpenGL used a separate ignored working directory
  with copied INIs and a data junction, not copied/proprietary assets in Git.
- Local controlled moving V1 replay: mission 50, one car, 900 tick records; accelerate
  until 600, right 360-449, left 450-539, brake thereafter. Follow camera fixed as
  the view type. `route-before-state.txt` vs `route-after-state.txt` and vs
  `route-gl-state.txt`: 900 shared ticks, zero authoritative mismatches. OpenGL
  performance log observed approximately 220-410 FPS; Vulkan ran around 30 FPS.
- Temporal probe sampled car centre in camera space after `SetCameraVector` using
  the same alpha as `DrawCar`. Straight moving segment ticks 40-155: maximum
  adjacent-frame camera-relative step 143.13 units before (146 samples), 1.41
  after on Vulkan (149 samples), 1.0 on uncapped OpenGL (1705 samples). This
  specifically measures the reproduced mixed-time sawtooth, not all camera motion
  during turns/collisions. The temporary per-frame CSV probe was removed before
  final builds; fixtures/traces/helpers remain ignored.
- Existing E00 NTSC attract baseline vs `after-attract-state.txt`: 714 shared
  ticks, zero mismatches. Existing E00 PAL baseline vs `pal-after-state.txt`:
  597 shared ticks, zero mismatches. Comparisons used
  `python scripts/engine_migration/e00_compare_state.py <before> <after>`.
- Final binaries without the temporal probe: `route-release-final-state.txt`
  matched all 900 moving-route ticks against the before binary;
  `pal-release-final-state.txt` matched all 597 shared E00 PAL baseline ticks and
  all 1378 ticks against the earlier fixed PAL run. All comparisons had zero
  authoritative mismatches. All owned test game processes were stopped.
- Final `git diff --check` passed. Source search confirmed the temporary
  `VisualSnapshot_TraceView` / `RED2_PRESENTATION_TRACE` probe was absent. Status
  documents remain below 350 lines (CURRENT_STATUS 317, RECENT_CONTEXT 325).
- Before updating Unreleased, GitHub release API confirmed upstream 8.0 released
  [2026-07-02 release](https://github.com/OpenDriver2/REDRIVER2/releases/tag/8.0)
  and `git ls-remote` confirmed tag commit
  `b2d8857426f815007486def9bd23f9a6007a2c64`; no release version/date was changed.

## Limits and next work

Tests substitute the camera controller to isolate scheduling; live comparisons
exercise the actual controller and only the authoritative fields in the existing
dump. A tick-state match alone is not a pixel/GPU or full-game acceptance result.
Low display FPS still limits how many frames a monitor can show. Bounded stalls
retain the adopted discard policy; interpolation cannot recover unseen frames.

Pedestrians, independent wheel spin, effects and complete presentation coverage
remain future E01/E05 scopes. Live split-screen, every camera/mission/cutscene
scenario and complete mission re-record acceptance were not completed here.
Continue only the migration block subsequently requested by the user.
