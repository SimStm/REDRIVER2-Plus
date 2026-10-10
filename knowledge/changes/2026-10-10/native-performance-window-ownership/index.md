---
type: Change
title: Native performance and sole GPU ownership
created: 2026-10-10
tags: [okf, diligent, performance, psycross]
---

# Native performance and sole GPU ownership

User requested publication of pending work, F9 for the developer panel, investigation
of 4-5 FPS native mission 1, and removal of redundant PsyCross Vulkan when Diligent starts.
Prior pending work was committed/pushed as 7fdce013. This follow-up changes presentation
and diagnostics; reconstructed mechanics and regional fixed ticks are preserved.

## Implementation

- NativeRenderer: discard-mapped dynamic per-draw constants; configured Present sync;
  conservative converted-mesh AABB culling; explicit map failure propagation; CPU frame stats.
- NativeCameraView: pure homogeneous bounds test, including boundary and near-plane cases.
- Desktop entry: opt-in perf log, window-only PsyCross mode and explicit native init failure.
- Project PsyCross fork: existing headless contract gains mode 2 (SDL + CPU VRAM), bypassing
  GL/Vulkan startup, draws, GPU uploads/readbacks and teardown. CPU VRAM operations and OT
  parsing remain; GPU-only texture requests fail explicitly. Mode 1/normal paths retain behavior.
- Legacy developer panel: F9; old receptivity bind moved to F8. Native persisted settings load
  without invoking compatibility ImGui; native overlay remains pending.
- RenderDoc native swapchain evidence exposed sRGB presentation over display-encoded
  UNORM textures/colours; window explicitly uses UNORM, matching offscreen output.
- Stage record explains next static-world/dynamic-object priority and later lighting mapping.

## Validation and limits

VS build_e00 solution, Release_dev_native|x64 and Release_dev|x64 builds passed with existing
warnings. Native and Release_dev -opengl mission 1 launched. OpenGL 3.3.0 initialization remains
active; native logs confirmed no PsyCross GPU, 160 resources,
6740 placements, no conversion failures. Measured post-fix windows have no tick drops.
See [product evidence](../../../product/native-renderer-performance.md) for scoped FPS.

Executed inspector runner: all 16 suites passed (camera 41); native renderer smoke:
1200 checks / 0 failures. Presentation timing: 145364 checks per NTSC/PAL, frame settings
21 per region, effects 2688 NTSC / 2448 PAL. Text diffs pass whitespace checks.

RenderDoc1.41 persistent TargetControl connection captured the native frame; replay
reports one colour/depth pass, 5300 draws, 28828 triangles and an empty message log.
Local replay exported PNG using Descriptor.resource after MCP export failed on its old
resourceId assumption. A second native capture verifies the new R8G8B8A8_UNORM target at 1280x720 (5705 draws);
PNG export passed. The captures differ in tick/camera, so no image parity is claimed.
Captures and SDK/assets stay ignored locally.

These checks do not accept E03, prove original-game deterministic parity, verify the
RenderDoc on-screen Window 0/FPS text, or complete actors/UI/VFX/full streaming/lighting.
Computer-control access was denied by automatic approval review; GPU/log evidence was used.
