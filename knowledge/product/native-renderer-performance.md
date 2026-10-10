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
replacements. F9 toggles the developer panel through a project-owned ImGui GPU adapter
on the existing Diligent render target, before Present. SDL input capture remains active;
no compatibility GPU or additional presenter is initialized. F8 owns its former receptivity shortcut.

## Graphics settings and native developer panel

Startup precedence is compiled defaults, `config.ini`, then keys present in
`developer_graphics.ini`. Omitted developer keys preserve the baseline, even if the
file specifies a different next-startup video region. `[render] presentationFps`
in `config.ini` accepts 15-360 after clamping; compiled defaults remain NTSC 30/PAL 25.
Simulation frequency is independent. The panel's explicit save continues to write
the developer file; FPS/VSync autosave does not persist provisional display changes.

Native VSync, window size/fullscreen, camera projection and bilinear filtering consume
the live settings. Filtering updates both resident samplers and subsequently registered
resources; conversion keys stay unchanged.
Mutable sampler rebinding uses ALLOW_OVERWRITE after a GPU idle wait only when the
setting changes; the SDK requires that synchronization for Vulkan descriptor access.
Draw distance still requires native sector coverage integration: the original setting
counts cells and is not a physical far-plane
distance. The panel disables that control and the unmigrated PGXP/legacy lighting controls.
Native lighting remains deferred until geometry/material coverage is correct.

The panel displays ImGui's measured FPS separately from the requested cap, plus CPU
draw/flush/present phases, draw/cull counts and tick/drop counters. These are not GPU timings.
`RED2_NATIVE_PANEL_OPEN=1` opens it at initialization for GPU capture without desktop input.
The adapter uses the existing ImGui 1.91.9b; current DiligentTools expects newer texture APIs.
Legacy inspector image handles are never cast to Diligent pointers: unsupported images
are omitted with a named diagnostic until a native texture registration path exists.

Windows native build and mission 1 startup passed. RenderDoc capture `native-overlay.rdc`
(ignored `build_e00/native-performance/`) shows the panel over the world, 2214 indexed draws
and zero replay validation messages; PNG export was visually inspected. Keyboard toggling,
interactive controls and resize confirmation still need interactive validation. GPU smoke
passed 1207 checks, including linear red/green interpolation, inheritance by new resources
and byte-identical restoration of nearest sampling. NTSC/PAL frame settings each passed
24 checks; timing each passed 145364, effects 2688/2448. These do not accept E03.
An actual startup with `[render] presentationFps=144` and no developer FPS key logged
cap=144, buffers=3, 30-tick windows and zero drops. Both local INI files were then
restored byte-for-byte from their backups. Release_dev compatibility build and `-opengl`
mission 1 launch passed; the log reports OpenGL 3.3.0. Debug sessions were stopped.

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
The earlier stationary plateau near 60 despite cap 360 was reproduced with two images:
2212 world draws, draw ~7-8 ms and Present ~7-8 ms. Requesting three swapchain images
removed that plateau locally: 106-115 FPS, draw ~7.2-7.8 ms and Present ~0.16-0.18 ms,
30/31 ticks per one-second window and zero drops. Both runs used the stationary mission 1
view, cap 360, VSync off and the native panel open. Moving-camera samples remain CPU-bound
near 54-59 FPS with ~15-16 ms draw cost. This is measured Windows/driver evidence;
Vulkan surface limits may clamp the image count, which the panel reports explicitly.
The three-image RenderDoc replay again has 2214 draws and zero validation messages;
its inspected PNG shows the panel, three images and the configured bilinear material output.

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

## Resident sector refresh (2026-10-10)

The local-file bridge observes completed sector IDs, loaded area, model registration
revisions and page residency. Changes republish placements and resource keys. It does
not schedule gameplay IO or read asynchronous CD-thread buffers. Reused model/shared-base
revisions and unchanged page/area state permit conversion reuse; content-addressed GPU
keys survive while removed keys are retired. Loading clears with the native overlay.
The bounded source/native instance capacity is 16384, rather than mission 1's 8192.

ATTRACT.400 reached 8594/10167 placements and 206/185 resources without truncation or
conversion failures. The second run reused 152/166 converted slots. Tick drops fell from
107 to 37 over the observed two transitions, but synchronous new conversions still block
the loop. Async content preparation is required before driving/streaming acceptance.
Native smoke passes 1213 checks, including a visible instance at the final capacity index
and loading-frame counters; all 16 inspector suites pass. These tests do not establish
whole-city coverage or deterministic replay parity through conversion stalls.
The final RenderDoc replay/export has 5904 draws and zero replay messages; the PNG
shows the new-sector street/buildings. Capture introduced 13 additional dropped ticks
(runtime total 50), separate from the 37 drops observed across the two transitions.
