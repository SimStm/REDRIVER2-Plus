---
type: Rule
title: Use configured MCP servers for docs, debugging, and game runs
description: Check for context7, visual-studio-ide-mcp, and desktop-control MCPs before documentation or debugging work, and never block on a terminal game launch.
tags: [okf, tooling, debugging, workflow]
---

# Use configured MCP servers for docs, debugging, and game runs

**When** a task needs library documentation, a build/debug cycle, or a running
game, **then** check which MCP servers the session exposes and use them instead
of falling back to memory, terminal builds, or blind input injection.

- **context7** answers third-party API questions with current documentation
  (SDL2, Dear ImGui, Vulkan, OpenAL, libjpeg). Prefer it over recalling API
  details when editing or debugging those integrations.
- **visual-studio-ide-mcp** drives the generated solution
  (`src_rebuild/build/REDRIVER2.sln`) that is open in Visual Studio: build
  configuration selection, build, launch or debug, breakpoints, call stack,
  locals, and the error list. `Release_dev_gl` is the OpenGL renderer and
  `Release_dev` is the default Vulkan renderer.
- **computer-control-mcp**, **windows-mcp**, or another desktop-control MCP
  interacts with the running game window: navigating frontend menus, driving,
  key presses, screenshots, and OCR of on-screen text. Scripted input injection
  is not reliable because it requires a focused window.

## Terminal launches can look like hangs

The game is an interactive foreground application; a normal terminal invocation
does not return until the window is closed, so it blocks the agent. Launch the
game through the Visual Studio MCP path, and when a terminal launch is truly
unavoidable:

- start the process fire-and-forget (`Start-Process` without `-Wait`);
- wait with `Start-Sleep` or a bounded timeout loop, never on the process
  handle;
- read the game log (`<executable>.log` next to the executable, plus
  `psyx_perf.log` when `PSYX_PERF_LOG` is set) and screenshots to learn the
  state.

## Capture and input techniques that were actually verified (2026-09-20)

- `windows-mcp` `Shortcut` reaches the game for special keys: `F10` toggles the
  modern renderer (logged), `F12` writes `SCREENSHOT.BMP`. Character keys
  (`0`, `;`) did not change the polled `SDL_GetKeyboardState` state, and
  `computer-control-mcp` `key_down`/`key_up` did not reach the game at all in
  this environment. For a settings A/B, edit the ini and restart instead of
  injecting a character key.
- The timed capture tick (`captureAfterSeconds`) runs inside `DrawGame` before
  `GR_EndScene`; on OpenGL that is before `PsyX_ModernMesh_RenderFrame`, so a
  GL tick capture omits modern meshes and shadows. Use `F12` for GL modern-path
  frames and the tick for Vulkan.
- For phase-precise captures, set a breakpoint on the game function that draws
  the frame (for example `ShowLoading`) through the Visual Studio MCP, evaluate
  `PsyX_TakeScreenshot()` while paused, copy `SCREENSHOT.BMP`, then continue.
  This works on Vulkan; the GL function evaluation previously timed out.
- Convert the BMPs with `System.Drawing` and compare numerically (full frame and
  fixed rectangles, mean/max channel difference and a per-pixel threshold count)
  instead of relying on visual inspection alone.

## Scripted UI interaction limits (2026-09-20, revised 2026-09-21)

- Scripted mouse clicks **do** reach the ImGui panel once the game window is
  really frontmost: activate it (`windows-mcp` `App switch`) before clicking and
  convert screenshot coordinates with the reported scale (`screen = image x
  1.791667` at 3440x1440). The earlier "clicks never arrive" observation was a
  focus problem - when another window (terminal, IDE) is in front, the click
  lands on that window instead.
- Some widgets still resist a synthetic click: a click on a binding-capture
  button was not observed to start a capture even with the window frontmost, so
  validate such a control through the function it calls (a temporary probe can
  call it directly) or by forcing a tab with `ImGuiTabItemFlags_SetSelected`.
- The tab bar is a single merged OCR box; click by the tab centres measured from
  the screenshot, not by OCR text boxes.
- F-keys only work when the game window is really in front. `windows-mcp`
  `Shortcut` sends keys to whatever is focused, and Visual Studio reacts to
  `F11` by toggling full screen and to an edited `.vcxproj` by showing a modal
  "modified outside the environment" dialog that then swallows every later
  input. Activate the game window first - and when the IDE keeps stealing focus,
  minimize it (`ShowWindow(hwnd, SW_MINIMIZE)` on the `devenv` process) and
  restore it afterwards.
- With the panel visible, the timed `captureAfterSeconds` tick did not produce
  `SCREENSHOT.BMP` in this environment; a desktop capture with the panel forced
  visible (`g_visible = true` in a temporary probe) was the reliable way to
  photograph the panel itself. Plain desktop captures of the running game are
  enough for renderer evidence.
- Remove a temporary probe by replacing its **exact text** (the `edit` tool) and
  then checking the resulting line count. A pattern-matched line range once
  deleted 220 lines of a panel file because the end-of-block heuristic matched a
  later closing brace; the removal has to be verified, not assumed.
