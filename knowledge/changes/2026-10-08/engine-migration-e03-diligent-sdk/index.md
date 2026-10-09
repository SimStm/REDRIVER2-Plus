---
type: Change
title: E03 Diligent SDK pin, Vulkan build and device/swapchain/present owner
created: 2026-10-08
tags: [okf, engine, e03, diligent, rendering]
---

# E03 Diligent SDK pin, Vulkan build and device/swapchain/present owner

## Outcome

E03 block 1 begins with the SDK pin/build. DiligentEngine is pinned at `v2.5.6`
(upstream latest release; the E00 inventory asked to verify the tag at
integration, and it matches). DiligentCore resolves to commit
`b036337d68be2353c9950a85929acf796b9a6d50`. Only the Vulkan backend is built, as
a static Release library, matching the selected desktop scope.

The SDK is an external dependency, so it lives in the gitignored
`src_rebuild/dependencies/DiligentEngine` and is never committed. Acquisition is
reproducible through `scripts/fetch_diligent.ps1` (clone at the tag, initialize
DiligentCore and its required third-party submodules, configure Vulkan-only,
build `Diligent-GraphicsEngineVk-static`). The script is idempotent.

The compiler/module boundary is decided: new native renderer modules use C++17,
as DiligentCore and its samples require. They live under a new `native/` module
so the Diligent headers and C++17 do not leak into the game's `utils/` glob.

The second half of block 1 is implemented as `native/NativeRenderer.*`, a
project-owned device/context/swap-chain/present owner over a single native window
handle. Its header is Diligent-free (opaque `void*` HWND), so callers never
include Diligent, and it fails explicitly when a renderer is already active, so a
window cannot end up with two independent presenters. A clear-and-present path
is the only draw call so far.

The first content step of block 2 is verified in the same smoke: a converted
GLB (written and read back through the project-owned glTF writer/importer) has
its vertex positions and indices uploaded into immutable Diligent buffers
(`BIND_VERTEX_BUFFER`/`BIND_INDEX_BUFFER`) and read back through a staging
buffer with `WaitForIdle`, so converted content is proven to reach GPU
resources. A full pipeline/draw and the game's own window remain.

Block 2 now also has a real draw with depth: the smoke creates a Diligent
graphics pipeline (runtime-compiled HLSL vertex/pixel shaders, a position+colour
input layout, an RGBA8 offscreen colour target and a D32 depth target), draws a
near green triangle then a far red one, and reads the centre pixel back through
a staging texture. Depth testing keeps the nearer triangle, so the centre pixel
is green; the per-vertex colour exercises a second input attribute.

Block 3 adds the camera and material seam: a second pipeline samples a base
colour texture (UV attribute + sampler + shader-resource binding) and transforms
the quad by a constant-buffer MVP. The smoke renders it twice: identity MVP
samples the red texel at the centre, and a translated MVP moves the quad off the
centre, proving the constant-buffer camera transform is applied.

Block 1 now also links the native renderer into the game on Windows. The game
compiles `native/**.cpp` as C++17 and links the Diligent static libraries; the
linkage is guarded so a build without the SDK is unaffected (the game then keeps
its v143 toolset and no Diligent). When the SDK is present the game uses the
v145 toolset (Diligent's, and where the ATL library it references lives) and
passes ATL's static library by full path. Diligent is now built without `/GL`
(whole-program optimization) because mixing `/GL` objects into a non-LTCG
executable breaks the link.

A gated probe (`RED2_NATIVE_RENDERER`) calls `NativeRenderer_Init` on the game's
own PsyCross SDL window and presents a few frames before releasing it. Running
it reproduces the stage's warning exactly: PsyCross has already created its
Vulkan swap chain on the window, and Diligent's `CreateSwapChainVk` fails with
`VK_ERROR_NATIVE_WINDOW_IN_USE_KHR` (captured through Diligent's debug callback
into `native_renderer.log`). The device is created; the swap chain is not. This
is a hard Vulkan constraint, so the native renderer cannot own the window while
PsyCross presents to it: PsyCross must be initializable without its swap chain
(headless) for the handoff.

A `Release_dev_native` configuration was added to `premake5.lua` (and to
`premake5_psycross.lua` for the PsyCross project, which otherwise fell back to
the debug runtime): it builds exactly like `Release_dev` (suffix `_native`) but
the debugger launches it with `RED2_NATIVE_RENDERER=1` and `-nointro -nofmv`, so
the native path can be tested from Visual Studio without setting the variable by
hand. Verified: `Release_dev_native|x64` builds and the exe reports the native
renderer active and drawing with the game camera.

Block 3 makes the native renderer draw every frame with the game's own camera.
PsyCross gained a generic post-frame handler (`PsyX_SetPostFrameHandler`),
invoked once per finished frame after the backend's present. With
`RED2_NATIVE_RENDERER` set, the game keeps the native renderer active and
registers a handler that derives the view from the game's `inv_camera_matrix`
(the same rotation-only, 1/4096-scaled convention the modern-mesh path uses),
adds the camera translation, and calls `NativeRenderer_RenderScene`. That draws
a depth-tested, textured ground and pillars (`NativeRenderer_SetCamera` plus a
lazily built pipeline, procedural checkerboard texture and D32 depth target)
into the swap chain and presents. Verified in-game: the log reports `native
renderer drawing the game window with the game camera`, no Diligent errors, and
the game keeps running; the default path is unchanged. The scene geometry uses
raw game world units, so the camera sees it in the same space as the game.

Block 2 completes the renderer-ownership handoff for the probe: PsyCross gained
a headless mode (`PsyX_SetHeadless`, `PsyXVkConfig.headless`) that still creates
and owns the game window but creates no swapchain and presents nothing
(`PsyX_Vk_RenderFrame` drops the frame). The game enables it when
`RED2_NATIVE_RENDERER` is set, so the native renderer is the sole presenter on
the window. Verified: the game log reports `native renderer active on the game
window (1280x720)` and `native renderer released the window`, with no Diligent
errors, and the game keeps running afterwards. The default path is unchanged
(no native-renderer lines, normal rendering).

Block 1 was re-verified against the updated SDK: the dependency is now a direct
clone of the official repository (`master`, DiligentCore `API256019`), built
Vulkan-only static Release. The native renderer's API use is unchanged, but the
new SDK's third-party layout differs: glslang now produces one `glslang.lib`
(no `GenericCodeGen`/`MachineIndependent`/`OSDependent`), and `volk` is a
separate target the application must link. The smoke and the game were updated
for that layout, and the game needs an explicit `/std:c++17` on the native file
(the VS generator did not emit `cppdialect` for the file filter). The in-game
probe behaves the same as before: device created, swap chain rejected with
`ERROR_NATIVE_WINDOW_IN_USE_KHR`.

## Evidence (executed)

- `cmake -S DiligentCore -B build_core -G "Visual Studio 18 2026" -A x64`
  with D3D11/D3D12/OpenGL/Metal disabled and tests off: configure exit 0.
- `cmake --build build_core --config Release --target
  Diligent-GraphicsEngineVk-static`: exit 0; produced
  `Diligent-GraphicsEngineVk-static.lib` plus `Diligent-GraphicsEngine`,
  `Diligent-Common`, `Diligent-GraphicsAccessories`, `Diligent-ShaderTools`,
  `Diligent-HLSL2GLSLConverterLib`, `Diligent-GraphicsEngineNextGenBase`,
  `Diligent-BasicPlatform`, `Diligent-Win32Platform`, `Diligent-Primitives` and
  the glslang/SPIRV static libraries.
- `pwsh -NoProfile -File scripts/fetch_diligent.ps1` re-run against the existing
  checkout: exit 0, "Diligent ready: ...Diligent-GraphicsEngineVk-static.lib".
- `DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h` and the rest
  of the public interface headers are present for integration.
- `premake5.lua` regenerated and the game rebuilt `Release_dev|x64` with the
  native renderer linked (guarded on the SDK). Run with `RED2_NATIVE_RENDERER=1`,
  the game log shows `native renderer init failed: the Diligent swap chain could
  not be created for the window` right after PsyCross reports its own
  `swapchain 1280x720` — the dual-swapchain conflict the stage warns about. The
  game continues normally afterwards, and the default (ungated) path is
  unchanged.
- `pwsh -NoProfile -File scripts/run_native_renderer_smoke.ps1`: built the
  smoke against the Diligent libraries and ran it: 15 checks, 0 failures. It
  created the Vulkan device and a swap chain on a hidden Win32 window, cleared
  and presented three frames, loaded a converted GLB and verified its positions
  and indices round-trip through immutable GPU buffers and a staging readback,
  rendered two overlapping triangles through a depth-tested graphics pipeline
  into an offscreen target and verified the nearer (green) one won the centre
  pixel, sampled a base-colour texture on a textured quad (red texel) and
  verified a translated constant-buffer MVP moved it off the centre pixel,
  resized the swap chain to 640x480 and presented again, and shut down cleanly.

## Limits and next steps

- Only DiligentCore (Vulkan) is built; DiligentFX/Tools/Samples are not, and are
  not needed for the first slice.
- The renderer owner, converted-content upload, depth draw and textured camera
  path are verified through a standalone smoke. Inside the game the device is
  created on the PsyCross window but the swap chain is not: Vulkan returns
  `VK_ERROR_NATIVE_WINDOW_IN_USE_KHR` because PsyCross owns the window's swap
  chain. The game must initialize PsyCross headless (no swap chain, no present)
  before the native renderer can be the sole presenter; that is the next E03
  block. Until then the in-game path is a probe, not a slice.
- The game build requires the Diligent SDK on Windows when present; Debug
  configurations are not yet wired (only the Release libraries exist).
- The built libraries are large because they carry debug info; a lean release
  configuration can be selected later.

## Files

- `scripts/fetch_diligent.ps1`: pinned, reproducible SDK acquisition/build.
- `scripts/run_native_renderer_smoke.ps1`: builds and runs the smoke.
- `src_rebuild/native/NativeRenderer.{h,cpp}`: device/swapchain/present owner.
- `src_rebuild/tests/NativeRendererSmoke.cpp`: the smoke program.
- `src_rebuild/premake5.lua`: guarded Diligent/native linkage (v145, ATL).
- `src_rebuild/redriver2_psxpc.cpp`: gated `NativeRenderer_ProbeGameWindow`.
- `scripts/fetch_diligent.ps1`: `/GL` disabled in the SDK build.
