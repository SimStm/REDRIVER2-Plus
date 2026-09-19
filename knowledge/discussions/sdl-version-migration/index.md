---
type: Discussion
title: SDL2 to SDL3 migration
status: exploring
created: 2026-09-18
updated: 2026-09-19
tags: [discussions, platform, sdl, build]
---

# SDL2 to SDL3 migration

## Context and decision status

While moving the renderer to Vulkan, the user asked whether the platform layer
should migrate from SDL2 to SDL3, given that SDL2 is the previous major line and
SDL3 is now the active one. **No decision is adopted.** This record captures the
evidence and a recommendation.

**Current recommendation:** keep SDL2 for now and treat SDL3 as a separate,
bounded milestone after Vulkan parity is proven. Vulkan does not require SDL3;
both SDL2 and SDL3 expose the `SDL_Vulkan_*` surface/extension entry points, so
the graphics work is orthogonal to the SDL major version.

## Evidence from the current checkout

- The dependency is pinned to **SDL2 2.30.2** in `windows_dev_prepare.ps1`
  (`$sdl2_ver = '2.30.2'`) and looked up as `dependencies/SDL2-2.30.2`.
- Premake references SDL2 explicitly: `SDL2_DIR` in `premake5.lua:21`, the
  Windows library `SDL2` (`premake5_psycross.lua:60`) and Linux include path
  `/usr/include/SDL2` (`premake5_psycross.lua:78`).
- Emscripten/web builds pass `-s USE_SDL=2`
  (`premake5.lua:64`, `premake5.lua:79`).
- **93 source/header files** under `src_rebuild` (excluding vendored third_party)
  reference the `SDL_` API; the platform layer is PsyCross
  (`PsyCross/src/...`), not the game core.
- The vendored Dear ImGui 1.91.9b already ships an SDL3 backend
  (`third_party/imgui/backends/imgui_impl_sdl3.cpp`), but the build adds the
  SDL2 backend (`premake5_psycross.lua:41`).

## What SDL3 changes

SDL3 is a new major version, not a drop-in. The migration is a broad API
break: most symbols are renamed or re-namespaced, `SDL_Init`/`SDL_CreateWindow`
return booleans instead of error enums, the event enum is renamed
(`SDL_EVENT_*`), `SDL_RWops` becomes `SDL_IOStream`, the audio API is rebuilt
around `SDL_AudioStream`, the gamepad API is reworked, and high-DPI/pixel-size
handling changes (`SDL_GetWindowSizeInPixels`). New capabilities such as the
`sdlgpu` render API, main-callback entry points and improved Wayland support are
real but are not required by anything currently in this fork.

## Recommendation

1. **Do not migrate as part of the Vulkan work.** Keep the two changes
   independent so a regression can be attributed to one axis.
2. If migrated, do it as its own milestone: an SDL3 build configuration beside
   the SDL2 one, with the platform matrix (Windows, Linux/WSL, Emscripten,
   Android) built and the input/timing behaviour compared on a debug-start
   snapshot before the SDL2 path is retired.
3. The first concrete benefit to exploit would be high-DPI/sRGB handling on
   Windows and the cleaner Vulkan surface API; neither is a blocker today.

## Alternatives

- Stay on SDL2 indefinitely: viable while 2.30.x remains functional, but
  eventually loses upstream fixes.
- Adopt SDL3 only for new desktop code while keeping SDL2 for portable targets:
  rejected as a default because it doubles the platform surface for no current
  requirement.
- Replace SDL with the platform's native windowing: out of scope and contrary to
  the portability goals of PsyCross.

## Open questions

- Does SDL2 2.30.x still receive security fixes at the time of a decision?
- Which SDL3 version would be pinned, and does it build on the Android NDK and
  Emscripten toolchains used here?
- How much of the ~93-file `SDL_` surface is in generic PsyCross code versus
  game-owned code that could be insulated behind PsyCross?

## History

- 2026-09-18: record created after the user asked whether to migrate while the
  renderer moves to Vulkan. Recommendation: defer, keep SDL2, and scope SDL3 as
  a separate milestone; no decision adopted.
- 2026-09-19: the gating condition ("after Vulkan parity is proven") is now met.
  The Vulkan game renderer is the desktop default and its post-flip defects are
  fixed (see
  [`../renderer-modernization/index.md`](../renderer-modernization/index.md) and
  [`../../roadmap/done/vulkan-game-renderer.md`](../../roadmap/done/vulkan-game-renderer.md)),
  so an SDL3 migration is no longer blocked by renderer work. No decision has
  been adopted; the recommendation to treat SDL3 as its own bounded milestone
  still stands.
