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
