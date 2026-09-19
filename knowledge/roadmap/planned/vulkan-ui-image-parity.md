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
