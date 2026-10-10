---
type: Product
title: Native renderer performance and window ownership
tags: [okf, engine, diligent, performance]
---

# Native renderer performance and window ownership

Diligent owns the sole GPU renderer when `RED2_NATIVE_RENDERER` is enabled on Windows.
PsyCross headless mode 2 owns the SDL window, CPU VRAM and compatibility services; mode 1
still means compatibility GPU without presentation. Normal OpenGL/Vulkan paths remain
selectable. Native initialization failure exits rather than falling back to legacy draws.

`GR_ClearVRAM`, `GR_CopyVRAM`, `GR_ReadVRAM` and packet processing retain CPU semantics.
GPU drawing, uploads, framebuffer mirroring/readback and swaps do nothing in mode 2;
compatibility texture creation reports an explicit error and returns the invalid handle 0.
Consequently framebuffer-derived effects and legacy GPU-only UI/textures need native
replacements. Native ImGui is pending; persisted graphics settings load independently.
F9 toggles the legacy developer panel; F8 now owns its former receptivity shortcut.

Native dynamic constants use `MapBuffer(MAP_WRITE, MAP_FLAG_DISCARD)`/Unmap, rather than
UpdateBuffer on USAGE_DYNAMIC. Failure propagates from primitive submission. Window Present
receives the configured VSync value. Instance AABBs are derived from converted positions
and tested in homogeneous clip space against the exact draw MVP; touching planes and
near-plane crossings remain visible. Culling changes presentation only, not simulation.

`RED2_NATIVE_PERF=1` writes CPU draw/flush/present/instance-submit timing and draw/cull/tick
counts to `native_performance.log`. These are CPU durations, not GPU counter measurements.

The window target explicitly uses RGBA8_UNORM, matching the offscreen target and
display-encoded original texels/prelit colours. The previous default sRGB target
applied an extra encoding to that output. Future PBR lighting needs an explicit
linear-workflow policy rather than reusing these prelit values as linear energy.

## Bounded local evidence (2026-10-10)

Windows x64, Release_dev_native, Chicago mission 1, 1280x720, RTX 3060 Ti, cap 360,
VSync configured off. Baseline 3.7-4.6 FPS and 11,783-13,234 primitive draws/frame showed
substantial draw/present cost and dropped ticks. Map/Discard and VSync forwarding reached
24-26 FPS; culling reached 46-52 FPS during the moving camera and about 60 FPS when
stationary. Window-only runs measured 41-60 FPS, with 0 dropped ticks and 30/31 ticks
per roughly one-second window. Different camera positions and capture overhead limit
comparison; removing compatibility GPU is not claimed to independently improve FPS.
The stationary scene still plateaus near 60 despite cap 360; presentation/driver pacing
needs investigation before promising higher rates.

VS native and Release_dev compatibility builds passed; native and Release_dev -opengl
mission 1 started. OpenGL log confirms version 3.3.0, retaining its GPU renderer. Inspector tests: all 16 suites passed, camera 41 checks;
native smoke: 1200 checks, 0 failures. Smoke checks identical pixels with an offscreen
second instance and counts submitted/drawn/unresolved independently. Timing tests pass
for NTSC/PAL. Original-game deterministic parity is not established by these unit tests.

RenderDoc1.41 logs register one Vulkan surface in window-only runs. MCP attach/trigger
connected but returned no capture until a persistent local Python TargetControl connection
pumped ReceiveMessage through capture/copy. The resulting replay reports one colour/depth
pass, 5300 indexed draws and 28828 triangles, with an empty validation-message log.
MCP rt export failed because the tool expects Descriptor.resourceId; RenderDoc1.41
uses Descriptor.resource. Local replay exported the PNG successfully. This capture showed
R8G8B8A8_SRGB and exposed the extra-encoding defect; A second capture verified R8G8B8A8_UNORM at 1280x720 (5705 indexed draws);
its target exported successfully. Camera/tick differ, so the images do not constitute
a matched-state parity comparison.
Window 0 selection/FPS overlay is not yet verified.
Only one SDL game window is created in source. Native parity, dynamic actors, HUD/menus,
VFX, lighting and full streaming remain open. No E03 acceptance is implied.
