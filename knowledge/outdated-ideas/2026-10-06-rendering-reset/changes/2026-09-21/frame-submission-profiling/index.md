---
type: Change
title: Profile the frame-submission CPU cost and confirm the 30 FPS game gate
description: The opt-in performance log now reports submit_ms, and the frame rate ceiling is the emulated PSX vblank, not the renderer.
tags: [performance, profiling, vulkan, opengl, tooling]
---

# Profile the frame-submission CPU cost and confirm the 30 FPS game gate

## Problem

The renderer-debt closure left "raw uncapped GPU throughput and CPU/GPU
frame-time profiling" open. The existing opt-in log (`PSYX_PERF_LOG=1` ->
`psyx_perf.log`) reported the wall-clock rate between presented frames
(`frame_ms`), which the game's own timestep can gate and therefore cannot say
how much of the frame budget the renderer consumes.

## Implementation

PsyCross fork, `src/PsyX_main.cpp`: the submission path (`GR_SwapWindow()` and,
on OpenGL, the overlay handler) is timed with `SDL_GetPerformanceCounter` and
the per-frame mean is reported as `submit_ms` in the existing sample line. On
Vulkan that block contains the ImGui frame and the whole frame's command
recording, because the backend records at present time.

## Evidence

Windows `Release_dev|x64` and `Release_dev_gl|x64` (`-opengl`), `PSYX_PERF_LOG=1`,
1280x720, 30 one-second samples each, mission 50 spawn:

| Run | fps | frame_ms | submit_ms |
| --- | --- | --- | --- |
| Vulkan, FIFO present (default) | 30.0 | 33.4 | 6.2 - 6.7 |
| Vulkan, `PSYX_VK_PRESENT_MODE=immediate` | 30.0 | 33.3 | 6.2 - 6.7 |
| OpenGL, `vsync=0` | 30.0 | 33.4 | 0.17 - 0.20 |

- **The ceiling is the game, not the renderer.** Uncapping present and
  disabling vsync changed nothing: the frame rate stays at 30.0 because the
  game's logic waits on the emulated PSX vblank (`VSync(0)` ->
  `PsyX_WaitForTimestep` -> the interrupt thread's vblank counter). A renderer
  measurement through the game loop therefore cannot exceed it.
- **CPU cost of a Vulkan frame is ~6.4 ms**, about 19 % of the 33.3 ms budget,
  versus ~0.18 ms on OpenGL. The difference is where the work happens: the
  OpenGL renderer draws during the game's own frame, while the Vulkan backend
  records the entire frame (ImGui included) inside the timed block.
- **Raw pipeline throughput reference:** the fixture window
  (`-vkfixture -vknogui`, `PSYX_VK_PRESENT_MODE=immediate`, 12 draws with the
  shadow and modern passes) sustains ~660-900 FPS (1.1-1.5 ms per frame) on the
  RTX 3060 Ti, so per-frame pipeline overhead is about 1 ms and GPU headroom is
  large for the game's ~700-draw frame.
- Not measured: the game frame's own GPU time. Isolating it needs Vulkan
  timestamp queries around the shadow and main passes; that is the proposed
  next step if GPU-side numbers are wanted.

## Consequences

- Any future high-FPS timestep (roadmap `high-fps-timestep`) is bounded by the
  Vulkan recording cost: ~6.4 ms CPU per frame caps such a build near 155 FPS
  before any GPU limit.
- Renderer work that adds a second pass over the frame's geometry (for example
  a legacy-caster shadow pass) adds to that recording cost; the measured number
  should be the baseline for judging it.
