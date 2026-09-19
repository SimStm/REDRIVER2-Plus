---
type: Status
title: Recent engineering context
description: Session-to-session handoff - what the last interactions changed and what to do next.
tags: [okf, status, agents]
---

# Recent engineering context

> Updated: 2026-09-19
> Role: **handoff for the last interactions.** Everything implemented in the
> project lives in [`CURRENT_STATUS.md`](CURRENT_STATUS.md); read that first for
> the cumulative picture and use this file for what just happened.
> Source of truth: the current source, `git status`, `git diff` and Git history.
> Never claim validation that was not executed, and do not record secrets,
> credentials, tokens or long logs.

## Last interaction (2026-09-19)

**Objective (complete):** fix the visual UI/image problems reported after the
Vulkan default flip - the loading progress bar (1.1), the map screen and its
navigation icons (1.2), the minimap police direction cone / police-colour
blinking (1.3), the top-left Damage/Felony colour and tone (1.4) and the
clapperboard loading-to-gameplay transition (1.5).

**Root cause:** the Vulkan backend records a whole frame's PSX draws at present
time, while OpenGL executes every `DrawSync` flush immediately. Four defects
followed, all fixed in the fork:

| Defect | Fix | Commit |
| --- | --- | --- |
| Vertex uploads always wrote from offset 0, so a frame with several `DrawAllSplits` (the map flushes every 16 tiles) overwrote earlier flushes | uploads append; each draw is offset by its upload's base | `8510b31` |
| `GR_CopyVRAM` only set a dirty flag, so every draw sampled the frame's final VRAM and all sixteen recycled map slots held the last batch | `PsyX_Vk_GameCopyVRAM` queues the rect plus pixels and replays them in generation order, closing/reopening the main pass around each transfer | `8510b31` |
| Depth writes were tied to the depth test, while OpenGL only toggles `GL_DEPTH_TEST` and never `glDepthMask` | `depthWriteEnable` follows the pass's depth attachment | `da6d693` |
| The depth/stencil attachment used `DONT_CARE`, so a mid-frame pass split (the replayed VRAM write) lost depth and later 3D geometry drew over the 2D UI | both store depth and stencil; still cleared every frame | `bddec0c` |

Parent commits: `7cf6484c`, `37a4121d`, `1e77232f`, `2a6ca6af`. The verification
table per reported item is in
[`roadmap/done/vulkan-ui-image-parity.md`](roadmap/done/vulkan-ui-image-parity.md);
the resulting behaviour rules are in
[`product/vulkan-ui-image-parity.md`](product/vulkan-ui-image-parity.md).

### Evidence gathered

- 1.1: the frontend boot load shows the art, "Is Loading" and the bar
  (`fastLoadingScreens=0` delays it enough to capture).
- 1.2: the fullscreen map matches the OpenGL reference - tiles, roads, compass,
  district labels and the "Rotation / Move / Skip cutscene" icon row.
- 1.3: with `CopsCanSeePlayer = 1` and `car_data[0].felonyRating = 5000` set
  through the debugger, the map draws the police flame marker and its white
  direction cone.
- 1.4: the Felony bar draws as a solid police-yellow bar over the scene instead
  of taking the colour of what is behind it.
- 1.5: `CloseShutters` runs at level start (breakpoint hit) with `h` advancing
  16 -> 32 -> 80, and the captured frame during `h = 80` shows the loading art,
  the bar and the closing black bands.
- Regression: gameplay, minimap and frontend unchanged; `-vkpsxtest` PASS
  (16-bit/4-bit `worst=0`, offscreen ok, `vram export 1048594/1048594`).
- The four fixes are Vulkan-only, so OpenGL/Emscripten/Android/PSX are untouched
  and no game logic changed.

## Techniques that worked

- Capturing a UI state deterministically: run the `Release_dev|x64` build under
  the VS debugger, `debugger_break`, write the globals with
  `debugger_evaluate` (`gShowMap = 1`, `CopsCanSeePlayer = 1`,
  `car_data[0].felonyRating = 5000`), `debugger_continue`, then screenshot the
  game window with the exact title regex `^REDRIVER2$`.
- Freezing an animation mid-flight: put a breakpoint inside the loop
  (`loadview.c` `CloseShutters`) and continue repeatedly, reading the loop
  variable in between, so the last presented frame holds a partial state.
- `debugger_evaluate` cannot call functions; assign to globals instead.
- Compare against `-opengl` (same binary, `Release_dev_gl` config or the flag)
  whenever a 2D-over-3D result is in doubt.
- Local settings that matter for captures: `config.ini` `fastLoadingScreens`,
  and `developer_debug_start.ini` `enabled` (also settable with the `-mission`,
  `-level`, `-gametype`, `-startdir` arguments). Restore them after capturing.

## Environment notes

- The Khronos validation layer is **not installed**, so runs cannot report
  validation errors here; rely on captures and `-vkpsxtest`.
- A breakpoint hit brings Visual Studio to the foreground; the window screenshot
  tool then captures VS. Bring the game back with the window-activate tool, or
  minimise VS first.
- `windows-mcp` window calls (`App mode=switch`, `MoveWindow`) can time out while
  the debugged process is paused.
- Synthetic menu input is unreliable in the frontend and in-game pause menu
  (keys need to be held across frames, and some states ignore input entirely),
  which is why the debugger-driven state injection above is preferred.

## Next recommended action

1. Fix the bugs the user reported in the new session. Read
   [`CURRENT_STATUS.md`](CURRENT_STATUS.md) for the implemented surface,
   [`product/vulkan-ui-image-parity.md`](product/vulkan-ui-image-parity.md) for
   the UI/depth rules, and
   [`roadmap/done/vulkan-ui-image-parity.md`](roadmap/done/vulkan-ui-image-parity.md)
   for the previous round's evidence.
2. Keep `knowledge/CURRENT_STATUS.md` updated when a change lands, and record
   this session's work here.
3. Renderer items still open: `D32_SFLOAT` depth fallback, macOS/MoltenVK build,
   R5 shadow-quality comparison, a fresh `-opengl` parity run, raw uncapped GPU
   throughput.
