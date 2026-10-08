---
type: Evidence
title: R01 native bootstrap validation
status: accepted-windows-analytic-slice
created: 2026-10-07
tags: [okf, rendering, validation, vulkan, native]
---

# R01 native bootstrap validation

Source implementation and tests are described in
[the product](../product/native-renderer-bootstrap.md). The parent remains at
`2e93f215c54e61dd1bbb5d26e439c25303466709` with substantial pre-existing dirty
changes and R00/R01 work. PsyCross R01 is committed and pushed to fork origin/master
at `99fa6418e03c95f4db71e07c2f09adf554669f51`; the parent gitlink is staged.
No original game asset was edited.

## Environment and recipes

Windows11 Pro26120, RTX3060Ti, NVIDIA610.88, Vulkan loader/validation1.4.357;
VS MCP `Release_dev|x64`, NTSC,1280x720, near10/far125000, bilinear1, original
HD override mod order from the R00 baseline. Capture and replay both use installed
RenderDoc1.46; the configured1.41 MCP cannot replay this capture version.
Context7 and official SDL2 documentation were checked for window/event APIs.

Use the existing generated solution through VS MCP. Native city recipe:

```text
-nointro -mission 50 -gametype 1 -level 0 -playercar 0 -startpos 9046 -217980 -startdir 2785 -players 1 -nativeworld
```

Use `-vknativetest` for analytic GPU acceptance and `-vkpsxtest` for the legacy
regression. Both report in `vk_fixture.log`, not `REDRIVER2.log`; check the last
fresh start/PASS pair, not an older appended result. `-opengl -nativeworld` with
the city recipe exercises unsupported-native fallback. Tests and raw local
captures are ignored under `src_rebuild/build/native-renderer-r01/`.

## Executed checks

- VS Release_dev x64 build PASS. A later verification build initially hit
  LNK1104 because the owned capture process was still running; stopped that
  verified executable and the next build passed. Existing APIENTRY/C4267/import
  warnings remain; they are not introduced by this native surface.
- `pwsh -NoProfile -File scripts/run_native_scene_tests.ps1`:18 owned resource/
  C ABI cases +8 camera/clip cases PASS, MSVC/W4. Cases cover copied memory,
  immutable publish, stale/reused handles, pending/failed/retiring states, budgets,
  malformed/finite/index/bounds rejection, generation exhaustion, reset and camera
  basis/Y/depth/NTSC/PAL/FOV transformations. Host compilation of excluded
  Android/Emscripten/PSX branches PASS; not complete target builds.
- Native GPU readback PASS, tolerance3 RGB bytes selected before comparison.
  Near red drawn before far blue still occludes it; asymmetric green survives,
  reversed magenta and before-near/beyond-far markers disappear. Actual Vulkan
  depth-state0/1 produces identical frame bytes. UI uses separate legacy depth.
  Wide projection exposes the expected background pixel and shrinks geometry;
  camera translation moves red into the expected pixel.
- Resize800x600, minimize skipping successful submission, restore1280x720 PASS.
  These tests initially exposed native framebuffer allocation using the previous
  swapchain dimensions; allocation now takes the newly resolved extent. Direct
  VS validation then exposed a test scissor based on the previous extent;
  the test producer now samples SDL drawable dimensions. Added actual original
  UI pixel checks after resize and restore; both PASS with zero byte error.
- Destroy after successful GPU submit immediately rejects the stale handle,
  retains retirement until the next owner fence and then clears ownership.
  Eight reset/upload/render cycles keep exactly one ready resource; final reset
  leaves zero ready/pending/retiring meshes and zero owned CPU payload bytes.
  Native off and missing snapshot each render actual legacy red and correct
  effective reason/counts. Fresh legacy PSX self-test PASS.
- Plain VS GPU test with Khronos validation loaded:1280→800→1280 swapchains,
  successful exit0, no VUID/Validation Error messages after scissor correction.
  This direct test resolves application validation independently of capture.

## Original game and capture inspection

Stable city frame430 has1 native18-index draw with independent1280x720 D32,
then10 original UI draws/318 indices with D24S8. Source category native counts
remain0; the analytic mesh is not a migrated building/road/car. First city replay
has zero messages. Inspected actual post-VS float4 clip output, CCW front/back
culling, enabled depth/write, LessEqual, no stencil, single-sample BGRA8_UNORM.
Normals and UVs come from the input mesh, not reconstructed legacy depth.

PGXP texture/depth1 versus0 at matched state:PNG SHA256 both
`7BD70C3445FC09122857D4D63C5F340FE2AB83CAFCB682A76ECB2145B1C8F71F`.
The images are byte-identical. With both PGXP options off and focal128 instead
of256, the frame retains1 native+10 UI draws and zero replay messages;
geometry contracts around the camera centre, HUD/minimap remain independent.

Native ImGui widgets/coverage compose above the scene. To inspect widgets,
visibility was set through a VS breakpoint because automated F11 delivery was
unreliable; do not claim that key injection passed. Actual Sky mouse interaction
with the new checkbox restored the complete legacy city, showing0 native,
1008 legacy-world and10 UI draws in the inspected frame. Re-enabled native mode.
OpenGL3.3 NVIDIA610.88 launches the same original city/HUD/ImGui and logs
`Native analytic scene: unsupported backend or depth format`; no native GL parity.

## Capture validation limitation

Instrumented analytic captures2/6 have zero messages. Capture5 spans recreation/
minimize and records one VUID00900 at the **legacy main render pass**, before
the native D32 draw:expected PRESENT_SRC, tracked GENERAL. Structured capture
data contains the message; it is not merely a later repeated replay warning.
The application uses no GENERAL image layout, and the same completed GPU test
under plain VS validation is clean. Native/world/UI readback passes both ways.

Inference:the mismatch is introduced by the capture/layer combination, rather
than native resource ownership. Primary RenderDoc1.46 source explicitly remaps
presentation layouts during replay and uses GENERAL for fake presentation
images during capture:[replay layout sanitization](https://github.com/baldurk/renderdoc/blob/v1.46/renderdoc/driver/vulkan/vk_common.cpp#L476),
[capture presentation wrappers](https://github.com/baldurk/renderdoc/blob/v1.46/renderdoc/driver/vulkan/wrappers/vk_wsi_funcs.cpp#L1390).
This does not identify which interacting layer selects fake images. Duplicate
1.41/1.46 and third-party hooks remain installed; none were changed globally.
Use direct validation for resize, and clean RenderDoc city/fixture frames for
mesh/material/pass evidence. Do not call every capture warning-free.

## Bounds and next step

Only the declared Windows single-player analytic slice is tested here. Full
original content, additional modes, Linux/macOS runtime, GPU timing/peak memory
and native GL are not delivered by R01. CPU payload counts do not replace GPU
allocation measurements; the actual game fixture CPU mesh payload is936 bytes.
Legacy CPU producers still run; their draw-time side effects are
preserved. Native lighting/material/asset coverage belongs to later stages.

Acceptance closeout completed: source review and text checks passed; all five
test INIs and the original debugger settings were restored byte-exactly after
stopping owned sessions. The fork is published and clean, and the parent gitlink
is staged. R02 original resource decoding is next; no R03 advance until that
stage's own source/material/identity gates execute.
