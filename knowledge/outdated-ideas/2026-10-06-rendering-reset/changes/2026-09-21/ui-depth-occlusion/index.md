---
type: Change
title: Screen-space draws keep the modern meshes out of the UI
description: Make blended 2D legacy draws write depth while the modern scene is enabled, so the modern meshes no longer paint over the HUD, the pause menu and messages.
tags: [rendering, vulkan, opengl, depth, ui, modern-mesh]
---

# Screen-space draws keep the modern meshes out of the UI

> Superseded by [explicit world/overlay composition](../modern-overlay-composition/index.md).
> The user found that this workaround removed meshes behind translucent panels
> and corrupted world/effect depth, including angle-dependent scene loss. The
> captures below did not establish correct transparency; disappearance beneath
> a translucent panel was itself a bug. Retained as investigation history only.

User report: imported modern meshes were always drawn in front of the game's UI
elements, most visibly the pause menu (ESC); the overhead map, on the other
hand, was correct. Follow-up to
[`vulkan-game-renderer`](../../../product/vulkan-game-renderer.md) and the modern
mesh path.

## Cause

The modern pass records after the whole legacy frame (Vulkan) or draws at
`GR_EndScene` (OpenGL) and shares the legacy depth buffer, so a modern mesh is
rejected only where the legacy frame left a nearer depth. Blended screen-space
draws run with the depth test disabled, and both backends disable depth writes
when the test is off, so the pause menu and the HUD left the *world's* depth in
the buffer and the meshes passed the test over them. Opaque 2D draws
(`BM_NONE`) use the depth-writing pipelines and therefore already occluded the
meshes - the map, the loading art and the opaque HUD parts were never covered,
which matches the report.

## Change

While the modern scene is enabled, every screen-space draw writes its constant
2D depth (`~0.25`) with an always-passing depth test, which cannot change the
blended result but keeps the overlay in front of the world for the modern pass.

- **Vulkan** (`PsyX_Vk.cpp`): `VkPsxDraw.screenSpace` is captured from the first
  vertex of the draw (`scr_h <= 100`, the same per-vertex test the PSX shader
  uses), the frame sets `screenSpaceDepthWrite` from
  `gameMode && gameModernEnabled`, and the pipeline selection prefers the new
  `screenSpacePipelines` / `screenSpacePipelinesStencilWrite` variants. The
  pipeline helper now takes `depthEnable` as `0` off, `1` the PSX depth test and
  write, `2` always-pass test that still writes. **The test cannot be disabled in
  mode 2**: the Vulkan specification states that `depthWriteEnable` only applies
  "when depthTestEnable is VK_TRUE" and that "depth writes are always disabled
  when depthTestEnable is VK_FALSE", so the first implementation (test off,
  write on) stored nothing and changed no pixel. Offscreen draws are excluded
  (their target has no depth).
- **OpenGL** (`PsyX_render.cpp`): `GR_UpdateVertexBuffer` remembers the CPU-side
  vertices and `GR_DrawTriangles` classifies the draw from `start_vertex`. For a
  screen-space draw it forces `glEnable(GL_DEPTH_TEST)` with
  `glDepthFunc(GL_ALWAYS)` and restores `GL_LEQUAL` and the tracked test state
  afterwards; OpenGL has the same "no writes without the test" rule, and the
  always-passing compare satisfies it. `g_PreviousOffscreenState` keeps the
  offscreen target (no depth attachment) out of it.
- The change is gated on the modern scene, so with the feature off the legacy
  depth buffer is byte-identical to before. The composite's `worldPixel` test
  now excludes the overlay pixels, so the UI also stops receiving the modern sun
  tint and shadow.

## Verification

- `Release_dev|x64` and `Release_dev_gl|x64` build with 0 failed projects.
- In-game with the eight fixtures visible, pausing shows the pause panel with
  every menu line readable: the yellow bollard that used to sit over
  "Sfx Volume 100" is gone from inside the panel while the bollard outside it
  (left of the panel edge) is unchanged. Region averages on the client-area
  capture: the covered text area reads `102,92,58` before the fix (yellow
  bollard over the panel) and `108,108,104` after (panel and text only), and a
  world region outside the panel is byte-identical (`138,113,62` in both). The
  panel area itself brightens (`100,100,100` -> `117,116,115`) because the
  composite now excludes the overlay pixels from the modern sun term.
- The baseline log line is unchanged (`legacyDrawSplits=579`, `modernCalls=8`,
  `legacyShadowPass=1 legacyLightPass=1`), the map, HUD and world render
  normally, and no Khronos validation error appears.
- OpenGL, same paused scene: the fixture that in the Vulkan "before" frame sat
  over the panel interior is absent there, and the fixture straddling the
  panel's left edge is dimmed inside it (`63,61,54`) versus outside (`90,96,91`),
  matching the panel's dimming of the road reference (`111,108,102` inside,
  `138,135,126` outside). A mesh drawn over the panel would have been brighter
  inside, not darker.

## Correcting an earlier claim

The first version of this record reported the fix as verified with a crate
fixture dimmed by the panel. That measurement was wrong: the crate is a legacy
prop, so it proved nothing about the modern meshes, and the Vulkan "write
without testing" pipeline wrote no depth at all. The user's re-test exposed
both. The verification above uses a fixture whose identity is unambiguous (the
yellow bollard, inside versus outside the panel edge) and compares the same
region across the two builds.

## Environment note

The scripted ESC never reaches SDL on this machine: the desktop-control MCP
injects only keys with a usable scancode (the arrow keys), so `w`, `Return` and
`Escape` report `SDL_GetKeyboardState == 0`. The pause was therefore reached by
temporarily rebinding `start` to `Up` in `config.ini` (with `up` moved to
`PageDown`, because the pause check is `paddp == MPAD_START` exactly), and the
file was restored afterwards.
