---
type: Rule
title: Reproduce gameplay with debug start snapshots
description: Start a Debug or Release_dev build deterministically at a known mission, vehicle and position for repeated comparisons.
tags: [okf, debugging, testing, rendering]
---

# Reproduce gameplay with debug start snapshots

**When** verifying a rendering or gameplay change that must be compared between
runs, **then** start the game directly in a known session instead of navigating
the frontend, and prefer the most deterministic entry point available.

- `-replay <file.d2rp>` (any build) plays a recorded attract or user replay and
  is the most deterministic option: the car, camera, and frame sequence repeat
  across runs. Attract replays live in `data/DRIVER2/REPLAYS/ATTRACT.*`.
- `-mission`, `-gametype`, `-level`, `-playercar`, `-startpos`, `-startdir`,
  `-players`, and `-chase` exist only in `Debug`/`Release_dev` builds, which
  define `DEBUG_OPTIONS`. Always include `-gametype` and `-level`:
  `GAME_TAKEADRIVE` recomputes `gCurrentMissionNumber` from `GameLevel`, so
  `-mission` alone does not select the map. Include `-startdir` (12-bit angle)
  so the vehicle heading matches the saved state.
- Any command-line argument already skips the intro; `-nointro`/`-nofmv` state
  it explicitly.
- The Developer Graphics Panel's **Game Debug** tab generates the matching
  command line and persists it to `developer_debug_start.ini`. Keep the file
  disabled when not testing; it is applied only when `enabled=1` and no
  `-mission`/`-replay` argument is present. An enabled snapshot also skips the
  intro movie, so no separate `-nointro` is needed.
- A snapshot captured during a replay is not applied at startup: replay files
  are loaded by name, not as a mission, so the panel emits a `-replay` command
  for that case. `DeveloperDebugStart_ShouldSkipIntro` and
  `DeveloperDebugStart_TryApply` both ignore snapshots with `mission >= 400`.

For automated captures, do not inject F12: window focus is unreliable in
headless or scripted sessions. Set `[game] captureAfterSeconds=<seconds>` so the
game calls `PsyX_TakeScreenshot()` itself once it is in gameplay. Use
`[render] textureOverrides=0|1` to compare the override path against the
original VRAM path in the same scene. `scripts/run_debug_start.ps1 -Capture`
does both runs and stores the results under `src_rebuild/build/`.

`glReadPixels` returns bottom-up rows, so `PsyX_TakeScreenshot()` flips the
image before `SDL_SaveBMP`; keep that flip when changing the capture path or
screenshots become vertically mirrored.

**When** changing generated `.vcxproj.user` debugger arguments/environment on
disk, **then** close/reopen the solution and verify the launched process command
line. Visual Studio can retain old values and run a city instead of a requested
fixture. Check fresh `vk_fixture.log` timestamps/last run for `-vkpsxtest`;
`REDRIVER2.log` is not its result log, and appended earlier PASS lines are not
evidence for the new build. Fixture screenshots are BMP even with a `.png` suffix.

Back up all test INI bytes and debugger settings before modifying them; restore
and verify hashes after stopping owned sessions. An off-screen saved ImGui panel
position can make F11 appear ineffective at a smaller resolution: test with a
temporary visible position and restore it, without claiming an input defect.

**When** validating multiple native states in one paused session, **then** arm
the existing normal capture tick and resume through the owner breakpoint; wait
for confirmed Break mode before querying results. Do not execute GPU screenshot
readback through debugger function evaluation: an early R03 evaluation saved a
BMP but that session subsequently exited0, with cause unconfirmed. The normal
capture path subsequently completed all camera/mod/physics tests. Label source
versus completed-backend ticks separately. If a desktop key pulse is not consumed,
do not claim driving from it; a finite mapped-pad fixture may test normal physics,
but record the injection boundary and never assign car/camera transforms for that
claim. Save pixel/resource evidence and remove only the test-owned breakpoints.
