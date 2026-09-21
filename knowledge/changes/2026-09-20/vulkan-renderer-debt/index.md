---
type: Change
title: Close the Vulkan renderer debt: D32 fallback, OpenGL parity, R5 shadows
description: Exercise the stencil-less depth fallback, re-run the OpenGL parity capture and measure the modern shadow receive on both backends.
tags: [rendering, vulkan, validation, shadows]
---

# Close the Vulkan renderer debt: D32 fallback, OpenGL parity, R5 shadows

Three non-blocking items were left open when the Vulkan game renderer completed
(see [`vulkan-game-renderer.md`](../../roadmap/done/vulkan-game-renderer.md)).
This change record closes or advances each with executed evidence. The PsyCross
change is in the fork working tree (`src/render/PsyX_Vk.cpp`); the parent gitlink
was not bumped yet.

## 1. `D32_SFLOAT` depth fallback

`PickDepthStencilFormat` already preferred the combined formats and fell back to
`VK_FORMAT_D32_SFLOAT`, but nothing could exercise the fallback on a driver that
always exposes `D24_UNORM_S8_UINT`. Added `PSYX_VK_DEPTH_FORMAT=d32`, a
developer validation override (same pattern as `PSYX_VK_PRESENT_MODE`), and made
the PSX self-test assert the documented degradation instead of skipping it.

Executed:

- `REDRIVER2_dev.exe -vkpsxtest` with the override: log begins
  `main depth format 126 stencil=0`, the stencil case reports
  `stencil unsupported: mask is a no-op: got (248,0,0,255) worst=0 ok`, all
  other checks (white texture, CLUT, five blends, offscreen, persistence, VRAM
  export `1048594/1048594`) pass and the report ends `psx self-test: PASS`.
  The same build without the override still reports `stencil=1` and the
  protected-pixel case.
- A 30 s game run with the override rendered the normal city frame (HUD, minimap,
  modern gallery), and the log shows the validation layer enabled with no
  VUID/error. The PSX mask bit is a no-op there by design.

## 2. Fresh OpenGL parity run

Same deterministic spawn (`developer_debug_start.ini`, mission 50), classic
renderer (modern path off), one 1280x720 capture per backend at the same delay:

- Vulkan (`REDRIVER2_dev.exe -ini config.capture.ini`) and OpenGL (`-opengl`).
- Full frame: mean absolute RGB difference 4.08/255, max 197; 0.22 % of channels
  differ by more than 16. The residual is world traffic and animation phase, not
  a missing surface. HUD (Damage/Felony), compass, minimap and loading art are
  present on both.
- This is a visual parity check, not pixel-identical output.

## 3. R5 shadow-quality comparison against OpenGL

Deterministic scene with the modern gallery ahead of the spawn and a low sun
(`lightdir=-0.25,0.22,-0.38`), one capture with `shadows=1` and one with
`shadows=0` per backend (Vulkan via the timed tick, OpenGL via F12 because the
tick runs before `GR_EndScene` draws the modern path):

- Same sun and camera: both backends project modern-caster shadows onto legacy
  ground in the same direction.
- In the sampled grass/kerb band (x 256-1024, y 380-540, per-pixel change > 12):
  Vulkan changed 7722 pixels, OpenGL 2264, with 2257 shared pixels. OpenGL's
  changed pixels are almost a subset of Vulkan's; IoU 29.2 %.
- Visually Vulkan's ground shadows are longer/stronger; OpenGL additionally
  darkens the legacy tree canopy with the shadow term while Vulkan barely
  changes it at this sun.
- Conclusion: the receive term is implemented on both backends but not yet
  equivalent in coverage/intensity. Recorded as an R5 follow-up; no baseline
  regression is implied because no earlier comparison existed.

## 4. Previous-frame history investigation

`LOAD_OP_LOAD` with a `PRESENT_SRC` initial layout gives **per-image**
persistence, not previous-frame persistence. A natural partial-update sequence
was sampled with a breakpoint inside `ShowLoading` (bar-only frames) on Vulkan,
capturing the presented frame at each hit:
`load-frames/brk1..brk4.bmp`.

- Two distinct presented frames were obtained. The four loading-art quadrants
  measured identical means in all of them (30.36/121.99/54.95/123.01), i.e. the
  art is complete and stable; the progress bar advanced from the empty border
  (303 red pixels) to the filled bar (1458).
- The base content is safe because the art phase presents 32-64 frames (the
  per-effect fade loop), far more than the swapchain depth (~3), so every image
  already holds the art before `ShowLoading` draws only the bar. The shutter
  bands are likewise redrawn every frame with a monotonic height.
- No natural corruption reproduced, so an explicit per-frame image copy was not
  implemented. It becomes necessary only if a future path draws a base frame
  once and then partially updates more frames than the swapchain holds; the
  `-vkpsxtest` partial-presentation case remains the canary.

## Not validated

- macOS/MoltenVK build, raw uncapped GPU throughput, CPU/GPU frame-time
  distribution and Linux/web/Android builds remain unprofiled.
- The shadow comparison used one camera, one sun and one GPU; it is a bounded
  measurement, not a general quality verdict.
