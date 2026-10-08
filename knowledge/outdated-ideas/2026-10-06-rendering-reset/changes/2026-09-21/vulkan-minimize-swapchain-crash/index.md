---
type: Change
title: Fix the Vulkan access violation when minimizing the game window
description: A failed swapchain recreation left VK_NULL_HANDLE for the next acquire, crashing inside the NVIDIA driver.
tags: [rendering, vulkan, crash, swapchain, window]
---

# Fix the Vulkan access violation when minimizing the game window

## Report and cause

Minimizing the running game (Vulkan backend, the desktop default) raised an
ACCESS VIOLATION in `PsyX_Vk_RenderFrame` immediately, every time.

Reproduced under the debugger: break at `PsyX_Vk.cpp:5399`
(`vkAcquireNextImageKHR`), `g_vk.swapchain == 0`. The Visual Studio Debug
output named the chain:

```
Validation Error: vkAcquireNextImageKHR(): swapchain is VK_NULL_HANDLE.
Validation Error: [ VUID-vkAcquireNextImageKHR-swapchain-parameter ] ...
Exception thrown at 0x... (nvoglv64.dll): 0xC0000005: Access violation reading location 0xE8
```

Sequence:

1. A minimized window has no client area, so the surface extent is `0x0`.
2. `vkAcquireNextImageKHR`/`vkQueuePresentKHR` return
   `VK_ERROR_OUT_OF_DATE_KHR`/`VK_SUBOPTIMAL_KHR`, which sets
   `g_vk.resizePending`.
3. The next frame runs `RecreateSwapchain()`, which destroys the current
   swapchain and then calls `CreateSwapchain()`. With a `0x0` surface,
   `CreateSwapchain` returns 0 before `vkCreateSwapchainKHR`, so
   `g_vk.swapchain` stays `VK_NULL_HANDLE` and nothing is recreated.
4. `RenderFrame` returns 1 ("try again next frame") and the next frame calls
   `vkAcquireNextImageKHR` with the null handle. The validation layer reports
   it but does not stop the call; the NVIDIA driver dereferences the invalid
   object and the process dies.

## Implementation

PsyCross fork, `src/render/PsyX_Vk.cpp` only.

- Extracted `ResolveSwapchainExtent()`, the single place that turns surface
  capabilities plus `g_vk.windowWidth/Height` into the extent a new swapchain
  would get, so size validity can be tested without creating one.
  `CreateSwapchain()` now consumes it.
- `RecreateSwapchain()` calls it *before* touching the current swapchain: a
  window without a usable extent keeps the existing swapchain instead of
  being left with none.
- `PsyX_Vk_RenderFrame()` skips the frame while
  `SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED` (nothing is visible and
  no swapchain can be acquired from), and recovers a null swapchain with one
  recreation attempt before the acquire, never passing a null handle on.

Restoring the window changes the drawable size back, which sets
`resizePending`; the first present after restore re-creates the swapchain at
the valid size. The old swapchain surviving the minimized interval is not a
leak: it is destroyed by the next successful recreation.

## Evidence

- Before: reproduce with the debugger, minimize via Win32 `ShowWindow`,
  `ExceptionThrown` at `PsyX_Vk.cpp:5399`, `g_vk.swapchain == 0`, driver AV in
  `nvoglv64.dll`; Debug output contained the two validation errors above.
- After: `Release_dev|x64` builds; the same minimize/restore flow repeated
  while debugging stayed in Run mode with no exception. The Debug output held
  no VUID or validation error for the session, and `psyx_vk.log` recorded
  `swapchain recreated 1280x720 images=3` after one restore, i.e. the normal
  recreation path ran.
- `REDRIVER2_dev.exe -vkpsxtest` passed after the refactor
  (`VkFixture: psx self-test PASS`), covering swapchain creation and the
  PSX-path readbacks.
- The OpenGL backend was not re-tested for this scenario; the defect is
  Vulkan-specific (null `VkSwapchainKHR`).
