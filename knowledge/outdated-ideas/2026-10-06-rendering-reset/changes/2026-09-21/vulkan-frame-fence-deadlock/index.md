---
type: Change
title: Fix the Vulkan frame-fence deadlock after an out-of-date acquire
description: Resetting the frame fence at the top of the frame left it unsignaled whenever the frame exited early.
tags: [rendering, vulkan, hang, swapchain, minimize]
---

# Fix the Vulkan frame-fence deadlock after an out-of-date acquire

## Report and cause

Found while closing the minimize/restore path of
[the swapchain crash fix](../vulkan-minimize-swapchain-crash/index.md):
minimizing the window for a longer period and restoring it left the game
rendering nothing, its window not responding, while the process stayed alive.

Traced with a temporary log around the acquire. The log ends with:

```
acquire: enter f=1926
acquire: leave f=1926 result=-1000001004      (VK_ERROR_OUT_OF_DATE_KHR)
swapchain recreated 1280x720 images=3
```

and no further frame. `PsyX_Vk_RenderFrame` waited on the frame fence and then
reset it *before* the acquire. The out-of-date acquire re-creates the swapchain
and returns early, without submitting, so the fence was left unsignaled and the
next frame's `vkWaitForFences(..., UINT64_MAX)` - in `RenderFrame` and again in
`PsyX_Vk_GameBeginFrame` - blocked forever. Any early return between the reset
and the submit had the same effect (`imageIndex >= swapchainImageCount`, a
failed queue submit).

A restore after a short minimize often took the resize path instead (the
drawable size changes, so `resizePending` recreates the swapchain before the
acquire), which is why the defect only showed up on longer occlusions.

## Implementation

PsyCross fork, `src/render/PsyX_Vk.cpp`:

- The fence is no longer reset after the wait at the top of the frame. It is
  reset immediately before the `vkQueueSubmit` that signals it again, so every
  early exit leaves it signaled and the next frame's waits return immediately.
- If that submit fails, an empty `vkQueueSubmit` signals the fence, so a
  submission that never happened cannot deadlock the next frame either.

## Evidence

Windows `Release_dev|x64`, debugger-free run, minimize confirmed with
`IsIconic` before and after the interval, `psyx_vk.log` with the temporary
acquire log:

- Before: minimize ~10 s, restore, acquire `OUT_OF_DATE`, recreate, then no
  further frame - `responding=False` and no recovery (reproduced twice, once
  after a 2.5-minute occlusion).
- After: minimize 46 s and separately 60+ s (iconic checked during the
  interval), restore, the swapchain re-created and frames continued past the
  restore (`acquire: leave` up to f=10246 and climbing), `responding=True`, and
  a capture showed the normal world, traffic, HUD and minimap.
- `REDRIVER2_dev.exe -vkpsxtest` passed on the cleaned build; both
  `Release_dev|x64` and `Release_dev_gl|x64` compile.
- The instrumentation was removed after the run.
