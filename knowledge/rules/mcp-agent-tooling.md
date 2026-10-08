---
type: Rule
title: Use configured MCP servers for docs, debugging, and game runs
description: Check for context7, visual-studio-ide-mcp, and desktop-control MCPs before documentation or debugging work, and never block on a terminal game launch.
tags: [okf, tooling, debugging, workflow]
---

# Use configured MCP servers for docs, debugging, and game runs

**When** a task needs library documentation, a build/debug cycle, or a running
game, **then** check which MCP servers the session exposes and select the route
that supplies verifiable results. The user's2026-10-08 instruction authorizes
direct build/capture/replay when it avoids MCP/client response limits.

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

## RenderDoc version and diagnostic ownership (2026-10-07)

When initial warnings appear, compare their full metadata/descriptions with
serialized capture `DebugMessages` before calling them new replay errors.
Inspect captured application commands and the exact tool-version source before
changing renderer synchronization. Retain every warning and false clean flag;
an inferred tooling owner is separate from clean validation. Use a direct native
validation-enabled control and record its tested scope and attribution limits.
[Executed provenance evidence](../rendering/r03-capture-warning-provenance.md).

Use the user-configured RenderDoc1.41 `rdc-cli`/`rdc-tools` for new work.
`rdc doctor` verifies the replay module; inspect the capture binary separately.
Here `%LOCALAPPDATA%/rdc/renderdoc` contains1.41 while `Program Files/RenderDoc`
still contains1.46. Do not select the latter implicitly or relabel old captures.
The Python API capture path uses the configured1.41 module; a CLI fallback to
`renderdoccmd` must verify its own version before launch.

`scripts/inspect_native_capture.py` runs via `rdc script`, rejects replay versions
other than1.41 and exports private native world/ID/depth/resource evidence.
Preserve messages before and after event changes: `SetFrameEvent` resets current
diagnostics. Initial HIGH messages cannot become a clean validation claim merely
because a later draw has no messages. Keep resource/pixel success distinct from
tool validation status; see [executed evidence](../rendering/renderdoc-141-validation.md).

The installed `rdc script` client also imposes30s daemon socket response timeout;
bypassing MCP alone does not remove it. Routine validation now uses direct
MSBuild, `rdc capture` and `scripts/run_renderdoc_analysis.py`: configured1.41
Python replay, explicit unique GPU and exception-safe resource shutdown without
daemon RPC. See [verified direct workflow](../rendering/renderdoc-direct-workflow.md).
Keep observing the same command process/session across tool yields; an observation
limit or RPC timeout is not proof the job stopped. Inspect progress/artifacts
before retrying. Use absolute capture `-o` paths: relative paths resolve below
the target working directory here. Reserve VS MCP for debugger investigations.
The fixture handler propagates test failure as exit1. Require exit0 together
with a fresh completed `vk_fixture.log` with PASS/no failure, never a stale log.
Successful analysis execution likewise cannot override failed report gates.

Pixel History flags can disagree with actual attachments for fragment-written
depth in1.41: preserve both reports and use independent raw colour/ID/depth and
source masks to adjudicate pixel gates. Equal displayed depth does not validate
the history's failure flag. See [mixed-height evidence](../rendering/r03-mixed-height-validation.md).

## Visual Studio startup and settings preservation (2026-10-08)

After closing/reopening a solution, verify the startup project before launching.
In this generated C++ solution the MCP setter accepts `REDRIVER2.vcxproj`;
`REDRIVER2` fails and the reopened default can be FontTool. A tool-process exit
is not native GPU evidence. Confirm the launched process and its own test log.
When editing generated`.user` XML for a temporary test, retain the default
MSBuild namespace; Python ElementTree needs `register_namespace('', uri)`.
Back up/restore every field byte-exactly. VS can rewrite cached debugger-argument
history on reopen: reverify afterward and restore the original generated file
again if needed, preserving all configuration/platform pairs.

## Terminal launches can look like hangs

The game is an interactive foreground application; a normal terminal invocation
does not return until the window is closed, so it blocks the agent. Launch the
game through Visual Studio MCP or the verified direct capture path. For a direct
launch outside capture:

- start the process fire-and-forget (`Start-Process` without `-Wait`);
- wait with `Start-Sleep` or a bounded timeout loop, never on the process
  handle;
- read the actual game log (`REDRIVER2.log` here, plus
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
