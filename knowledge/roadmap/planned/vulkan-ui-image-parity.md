---
type: Roadmap
title: Vulkan UI and image parity
status: planned
tags: [roadmap, rendering, vulkan, ui, psycross, parity]
---

# Vulkan UI and image parity

## Problem

After Vulkan became the desktop default, the user reported a group of visual
defects around UI and image-drawing elements:

1. the loading-screen progress bar does not appear;
2. the map screen and its navigation icons do not appear (pause menu ->
   `Show Map`);
3. minimap elements are wrong - the police "white blob"/direction cone is
   missing and the police-colour blinking is intermittent;
4. the top-left Damage/Felony HUD, which blinks the police colours, changes
   colour and tone with the player's position, as if alpha or depth were wrong;
5. animated effects such as the clapperboard loading-to-gameplay transition do
   not play correctly.

Root cause direction (partly confirmed): the Vulkan backend records the entire
frame's PSX draws at present time, while the OpenGL renderer executes every
`DrawSync` flush immediately. Two consequences are already fixed on the fork
(commit `8510b31`): the vertex upload used to overwrite the previous flush's
vertices, and every draw sampled the frame's final VRAM state instead of the
state at its own flush. The overhead map now draws its tiles and labels, but its
tile sampling still does not match OpenGL: line-work is scattered across the
screen and the map's background reaches only part of it.

## Intended behaviour

Vulkan must produce the same image as OpenGL for every UI and image element,
including those that stream data mid-frame. The OpenGL renderer is the reference
implementation; backends may differ in mechanism, not in result.

## Scope

- Complete the overhead-map parity fix, then verify items 1-5 against OpenGL
  captures on the same build and spawn.
- Keep the flush-order contract documented: any future mid-frame game-side write
  that draws later in the same frame must be replayed in order.

## Non-goals

- Redesigning the emulated PSX GPU interface or moving the Vulkan backend to an
  eager recording model (a candidate follow-up, not part of this record).
- Changing game drawing code, where the OpenGL result is correct.
- Restoring effects that are absent on both backends for non-renderer reasons.

## Dependencies and risks

- Depends on the delivered Vulkan game renderer
  ([done record](../done/vulkan-game-renderer.md)) and its product document.
- Risk: the deferred model has more ordering hazards than the two already found;
  the map is the only known mid-frame streamer, but minimap and transition
  effects may share the class. Each fix must be validated against the OpenGL
  image, not only against "it draws something now".
- Risk: closing and reopening the main render pass around replayed VRAM writes
  adds passes per frame; measure before extending it.

## Acceptance criteria

- Each of the five reported items matches the OpenGL capture on the same build,
  spawn and state, verified with before/after images.
- `-vkpsxtest` and the inspector suites still pass, and no OpenGL, Emscripten,
  Android or PSX path changes.

## Validation plan

- Force the relevant state deterministically (debugger writes for `gShowMap`, the
  debug-start ini for the spawn, `fastLoadingScreens=0` for a visible load).
- Capture Vulkan and `-opengl` images of the same frame and compare.
- Run `REDRIVER2_dev.exe -vkpsxtest`, then `scripts/run_inspector_tests.ps1` when
  the change touches the export or catalog paths.

## Delivery progress

### 2026-09-19 - three flush/depth defects fixed

The deferred draw list is the source of the image-streaming defects. OpenGL
executes every `DrawSync` flush immediately; Vulkan records the frame's draws at
present time, so state that the game mutates between flushes has to be replayed
in order. Three defects were found and fixed in the PsyCross fork:

1. **Vertex buffer overwrite** (`8510b31`). `PsyX_Vk_GameUpdateVertexBuffer`
   always wrote from offset 0, so any frame with more than one `DrawAllSplits`
   (the overhead map flushes every 16 tiles) overwrote the vertices of every
   earlier flush. Uploads now append, the buffer keeps four flushes, and each
   draw is offset by the base of its upload.
2. **VRAM write ordering** (`8510b31`). `GR_CopyVRAM` only set a dirty flag that
   `GR_UpdateVRAM` turned into one whole-mirror upload at the next scene, so
   every draw sampled the frame's final VRAM contents and all sixteen recycled
   map slots held the last batch. Writes now queue their rectangle with the
   pixels (`PsyX_Vk_GameCopyVRAM`) and are replayed in generation order while the
   deferred draws are recorded; the main pass is closed and immediately reopened
   around each replay, because a transfer cannot be recorded inside a render
   pass.
3. **Depth writes with the depth test off** (`da6d693`). OpenGL only toggles
   `GL_DEPTH_TEST` and never touches `glDepthMask`, so a draw with the test
   disabled still writes depth; the Vulkan pipelines disabled both. The 2D UI is
   drawn against a depth-tested 3D scene and depends on that write to occlude it.
   `depthWriteEnable` is now on wherever the pass owns a depth attachment (the
   offscreen pass is depth-less and stays off).

### Open - fullscreen map tile sampling

The overhead map now draws its tiles and labels instead of scattered garbage, but
it still does not match OpenGL: line-work appears across the screen and the
background covers only part of it. Evidence gathered so far:

- The **minimap renders correctly** on Vulkan. It samples the whole map image
  loaded once at init, so the map's tpage, CLUT, VRAM page and shader path are
  right; only the fullscreen map's per-tile streaming (`LoadMapTile`) differs.
- The slot layout is consistent: `LoadMapTile` writes an 8x32 rect at
  `MapRect.x + (MapSegmentPos[slot].x >> 2)`, and the tile poly's 32(u)x32(v)
  window resolves to the same 8x32 VRAM rect for the 4-bit page
  (`v_page_clut.x = fract(tpage/16)*1024`). Captured uploads match
  (`960,0 8x32`, `968,0`, `976,0`, `984,0`, `960,32`, ...).
- The Vulkan draw list places the map's 2D draws before the world's 3D draws, and
  the per-draw VRAM generation looked correct (`gen=1` for the first tile,
  `gen=126` for a later one), so the remaining difference is not obviously order
  or generation.

Next step: dump a single tile draw's `u`/`v`/`tpage`/`clut` and the VRAM pixels
its window resolves to on Vulkan, and compare with the same tile on OpenGL. The
OpenGL capture of the map screen is the acceptance image.
