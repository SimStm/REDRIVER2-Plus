---
type: Change
title: Native Vulkan filtered artwork precision
created: 2026-10-07
tags: [okf, rendering, vulkan, validation]
---

# Native Vulkan filtered artwork precision

Five scoped Chicago tree colour comparisons failed the original two-byte ideal
tolerance. Constant-UV owned GPU patterns reproduced all five outside replay;
ten filtered cases failed that same ideal gate. Source/UV/LOD/shader/framebuffer
diagnosis isolated the fixed-format fractional interpolation. Failed evidence
is retained; no accuracy tolerance is widened.

The production native fragment shader now interpolates decoded sRGB texels and
linear alpha in FP32, using clamped explicit fetches and selected implicit LOD.
Nearest and base-only bilinear semantics remain. Extreme UVs clamp before integer
conversion; mip fetches stay within the owned chain. Sampling mode occupies the
private padding word at92, retaining push112/depth offset96 and public ABI9.
Formats, ownership, upload/fences/retirement and backend selection are unchanged.
Native shader source/SPIR-V/header and GPU tests changed in project fork426c9a3,
committed/pushed to origin/master; parent gitlink staged. Parent HEAD2e93f215 is
dirty with the active goal and unrelated edits; no clean parent revision claimed.

Executed on Windows/RTX3060Ti, preceding driverVersionRaw2559967232: shader
compilation, VS Release_dev x64, CPU scene suites and asset1536 PASS. Native/PSX
GPU PASS/exit0, Khronos enabled/no VUID. Fifteen fractional cases now gate ideal
accuracy; six mip/large-UV/last-level tests pass exact bytes, zero final ownership.
Fresh configured RenderDoc1.41 normal/HD and wide/original1920x1080 frame500
captures pass40598 scoped colour samples, holes/IDs/depth/occlusion and prior
terrain masks. Resource hashes/counts/formats/bindings match the preceding frames.
Private captures, original artwork and logs stay below ignored build output.

Five HIGH initial replay warnings are retained. Four timestamp samples vary
widely; filtered shader costs four/up to eight fetches, and GPU budget remains
unaccepted. Three tree footprints/six parts are tested; other views,38 inventoried
parts, silhouette/threshold boundaries and remaining region surfaces stay pending.
Six INI/VS test files hash-restored, all10 configuration pairs retained, debugger
Design, no breakpoints/owned game process. R03 incomplete; R04 unstarted.

See [full validation](../../../rendering/r03-cutout-validation.md),
[implemented surface](../../../product/native-static-region.md) and
[OpenGL backlog VK024](../../../roadmap/planned/port-vulkan-to-opengl-compatibility.md).
OpenGL implementation is deferred; preserved UI/legacy consumers do not validate
native-world success. Upstream8.0/b2d8857 was rechecked on the official release
page before the Unreleased entry; no release version or date was invented.
