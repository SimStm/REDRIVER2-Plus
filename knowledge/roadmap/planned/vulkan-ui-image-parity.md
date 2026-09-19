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

4. **Depth and mask discarded at a pass split** (`bddec0c`). A replayed VRAM
   write closes the main render pass, records the transfer and reopens the pass.
   The depth/stencil attachment used `VK_ATTACHMENT_STORE_OP_DONT_CARE`, so the
   reopened pass loaded undefined contents and everything drawn after the split
   was no longer depth-tested against what came before: the world drew over the
   overhead map and the 2D UI stopped occluding the 3D scene (which is why its
   colour shifted with whatever was behind it). The attachment now stores depth
   and stencil; both are still cleared at the start of every frame.

### Resolved - fullscreen map

The overhead map now matches the OpenGL reference: tiles, roads, the compass and
the district labels ("Downtown", "Greek Town", "Grant Park") render fully opaque.
Before the fixes it did not draw at all (vertex overwrite), then drew scattered
garbage (VRAM ordering), then drew correctly but was painted over by the world
(depth store).

## Remaining verification

- The minimap and the Damage/Felony HUD render as on OpenGL in normal play.
- Still to capture with the reported state: the police direction cone and
  police-colour blinking (item 1.3, needs a wanted state) and the clapperboard
  level transition (item 1.5). Both use the same 2D-over-3D path that the depth
  fix repaired, but the objective asks for before/after evidence per item.
- The loading progress bar is verified: the frontend boot load
  (`ShowLoadingScreen("GFX\\FELOAD.TIM", 1, 12)` + `ShowLoading`) draws the bar on
  Vulkan with `fastLoadingScreens=0`.
