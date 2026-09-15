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
- `-mission`, `-gametype`, `-level`, `-playercar`, `-startpos`, `-players`, and
  `-chase` exist only in `Debug`/`Release_dev` builds, which define
  `DEBUG_OPTIONS`. Always include `-gametype` and `-level`: `GAME_TAKEADRIVE`
  recomputes `gCurrentMissionNumber` from `GameLevel`, so `-mission` alone does
  not select the map.
- Any command-line argument already skips the intro; `-nointro`/`-nofmv` state
  it explicitly.
- The Developer Graphics Panel's **Game Debug** tab generates the matching
  command line and persists it to `developer_debug_start.ini`. Keep the file
  disabled when not testing; it is applied only when `enabled=1` and no
  `-mission`/`-replay` argument is present.

For automated captures, do not inject F12: window focus is unreliable in
headless or scripted sessions. Set `[game] captureAfterSeconds=<seconds>` so the
game calls `PsyX_TakeScreenshot()` itself once it is in gameplay. Use
`[render] textureOverrides=0|1` to compare the override path against the
original VRAM path in the same scene. `scripts/run_debug_start.ps1 -Capture`
does both runs and stores the results under `src_rebuild/build/`.

`glReadPixels` returns bottom-up rows, so `PsyX_TakeScreenshot()` flips the
image before `SDL_SaveBMP`; keep that flip when changing the capture path or
screenshots become vertically mirrored.
