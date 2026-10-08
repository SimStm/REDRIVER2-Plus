---
type: Evidence
title: R00 measured baseline and repeatable states
status: validated-baseline
created: 2026-10-07
tags: [okf, rendering, baseline, validation]
---

# Measured baseline and repeatable states

This is the current **legacy/experimental** renderer baseline, not native R01
acceptance. The category inventory and adopted contracts live in
[source inventory](r00-source-inventory.md) and
[native contracts](r00-native-contracts.md). No performance gain is claimed.

## Source and tools

- Parent HEAD `2e93f215c54e61dd1bbb5d26e439c25303466709` plus existing dirty
  graphics/documentation changes and R00 diagnostics. Fork PsyCross is clean at
  `446b96daaf74b0aeb6a68338aac7e82d7fd799c5`, origin `SimStm/PsyCross`, master.
  The parent gitlink was already staged before R00. Original game data untouched.
- Windows 11 Pro, build 26120; NVIDIA GeForce RTX 3060 Ti, driver 610.88,
  vendor/device IDs 4318/9353. MSVC/VS2022 `Release_dev|x64`, NTSC build.
- Vulkan physical API 1.4.0; loader 1.4.357 from `vulkaninfo --show-formats`.
  Game log's `API 1.4.341` describes the instance/API request, not that physical
  device property. Game log confirms validation enabled. Installed validation
  layer reports 1.4.357; the individual loaded game layer binary was not traced.
- RenderDoc capture and first replay both **1.46** (installed qrenderdoc Python
  API). Configured MCP uses 1.41: injection failed and it cannot replay the newer
  Vulkan capture section. Retried with installed 1.46, not with stale evidence.
  Loader reports duplicate 1.41/1.46 capture layers and a stale Epic manifest.
  No registry/layer changes were made. Context7 Vulkan Guide consulted for
  image-format usage and depth/sample-count queries.

Raw local evidence is ignored under `src_rebuild/build/native-renderer-r00/`.
It is not committed or required as redistributed game data. Recipes below and
source tools regenerate it. Build-date strings can remain from unchanged objects;
fresh timestamps, process arguments and new diagnostic output identify these runs.

## Executed device queries

Run from the repository root:

```powershell
pwsh -NoProfile -File scripts/query_vulkan_baseline.ps1
```

The checked-in probe compiles with repository Vulkan headers and dynamically
loads Vulkan 1.1 query entry points. It creates a temporary instance only, no
logical device/window/swapchain. MSVC `/W4` build and JSON query passed.

- `sampleRateShading` and `samplerAnisotropy`: supported; timestamp period 1 ns.
  Current game `CreateDevice` enables neither optional core feature. Support does
  not imply effective use, and queue timestamp-valid bits still need R01 queries.
- Framebuffer colour/depth sample intersection: **1, 2, 4, 8**.
- Optimal image-format queries succeeded for BGRA8_UNORM (colour + transfer-src),
  RGBA8_SRGB and RGBA16_SFLOAT (colour + sampled), D24_S8 and D32_SFLOAT
  (depth + sampled). Each returned 1/2/4/8 and the same framebuffer intersection.
  This is support for the queried usages, not a tested resolve or AA mode.
- `VK_EXT_memory_budget` supported. Large device-local heap: 8,413,773,824 bytes;
  available budget at query: 7,608,467,456 bytes. Host heap size/budget:
  34,324,828,160 / 33,519,521,792 bytes. Small device-local heap size/budget:
  224,395,264 / 211,021,824 bytes; reported usage 13,373,440 bytes.
  Probe large-heap usage zero is **not game GPU memory usage**.
- The captured renderer uses single-sample BGRA8 colour, D24_S8 main depth and
  4096x2048 D32 shadow atlas. Physical 8x support does not change that baseline.

## Native acceptance budgets adopted from this baseline

These are numerical **limits for later measured acceptance**, not allocations or
claims that unimplemented native stages meet them. Reduce effective quality or
residency visibly when over budget; retain a valid fallback. Re-query on hardware
changes. Initial resolution is 1280x720 SDR; additional resolutions need tests.

- Preserve two-vblank simulation/presentation: NTSC 33.333 ms, PAL 40 ms. R11
  may target 60 Hz presentation (16.667 ms) without accelerating simulation.
- Native CPU render submission/collection p95 target **10 ms** and GPU complete
  world/post/UI p95 target **12 ms** on this GPU. Current observed worst one-second
  CPU submission mean is 8.07 ms; 10 ms allows about 2 ms headroom. GPU target
  leaves 4.667 ms of a future 60 Hz interval. It is a policy target, not GPU data.
  Report p50/p95/p99 of actual frames with matching scenarios in later stages.
- Native GPU resources cap **2 GiB** (28.2% of measured large-heap budget),
  including a **128 MiB** shadow sub-budget and **64 MiB** transient upload cap.
  Existing atlas alone is 32 MiB, so 128 MiB allows measured cascade/resolve growth.
  Do not count driver/process memory as proof of compliance.
- Decoded native CPU assets cap **512 MiB**; whole-process private-byte warning
  threshold **1.5 GiB** (roughly twice the measured 773 MiB process sample).
  Do not retain unbounded duplicate original/native decoded payloads.
- Bound per-frame streaming/decode/upload scheduling to **2 ms CPU** initially;
  defer queued visual work and count pending/stale/rejected records. R04 must
  measure stalls and tune this against the 10 ms render budget.
- Actual GPU timestamp distribution, renderer GPU allocation peak, decoded-asset
  peak and streaming stalls: **unknown**. R00 does not substitute RenderDoc replay
  timing or capped FPS for these measurements. Linux acceptance still required:
  build under declared C++11, query its GPU, repeat fixture/original-city tests,
  validation, cadence, resize/minimize and quality fallbacks before claiming support.

## Effective test settings

All game cases use Vulkan, 1280x720 windowed, vsync=1, bilinear=1, both PGXP
settings=1, draw distance=1800, focal `fieldOfView=256` unless specified. This
setting is a focal scalar, not degrees. Config content override=0. Original data
is read from the configured local installation; no conversion/export is required.

Developer modern settings: enabled=1, eight owned GLB fixtures, analytic=0,
source-static requested=1 but **inactive with bilinear=1**; source lighting=1,
strength=.5, object limit64/range 22706. Modern source sun vector
(-.5917,.6820,-.4299), intensity1, ambient.210, exposure1, AO1, shadows1,
legacy/world shadow casters1; near half-extent6991, far shadows0; point lights1,
limit2/range 4000/intensity1.5, headlights/siren/probe1, legacy receptivity.380.
These experiments are separate from the future native renderer.

HD overrides requested=1 and proportional-alpha=1. Runtime debugger found one
valid enabled mod, `remaster-textures`, with 35 manifest entries and33 registered
overrides, despite two manifest files on disk. It is the only effective mod in
the order; directory presence alone is not effective mod coverage. The original
static experiment is gated by both bilinear and enabled registered overrides.

INI bytes were backed up in `ini-backup/` before tests, with SHA256 manifest
`ini-before-runtime.json`. All five files (config, graphics, modern mesh, debug
start, ImGui) were restored and hashes matched exactly. Generated `.user`
debugger arguments/environment were also restored byte-for-byte after closing VS.
No settings are shipped. Evidence manifest: `ini-restored.json`.

## Repeatable cases

Set the **VS startup project's debugger arguments**, close/reopen the solution
after editing `.vcxproj.user`, and verify the running command line. Launch via
VS MCP. Set `PSYX_COVERAGE_LOG=1` for counters and `PSYX_PERF_LOG=1` for profiling;
discard earlier appended log sections and initial loading windows. Keep the F11
panel closed and avoid input for fixed captures. Use installed RenderDoc 1.46
`ExecuteAndInject`/`QueueCapture(430,1)` for the recorded capture ordinal;
RenderDoc frame numbers are not game `FrameCnt` or `CameraCnt`.

- **city-facade-256**: `-nointro -mission 50 -gametype 1 -level 0 -playercar 0
  -startpos 9046 -217980 -startdir 2785 -players 1`. Chicago day, weather0,
  fixed sun above. Capture430: diagonal facade shadow visible; 773 draws
  (9 shadow, 764 main), zero first-replay debug messages. Before/after R00
  captures have identical pass/index counts and **byte-identical RGB pixels**
  (1280x720, zero tolerance). This proves this sample's instrumentation parity,
  not all modes or correctness of existing facade shading.
- **city-wide-128**: same args/state, temporary `developer_graphics.ini`
  fieldOfView128. Capture430: widened framing/sky inspected; 1150 draws
  (9 shadow, 1141 main), zero first-replay messages. Do not infer that the user's
  earlier sky artifact has the same cause; no matching pixel-history root-cause
  analysis has been executed.
- **city-camera-native-frame**: same 256 setup, native process without RenderDoc.
  At DrawGame entry FrameCnt1604/CameraCnt1605, camera position
  (10066,-303,-217498), angles(25,1312,0), scr_z256, weather0/time-of-day1.
  `PsyX_TakeScreenshot()` saved preceding completed frame 1603. This documents the
  final game-camera seam; neither camera nor tick variables were modified.
- **original-replay-route**: `-replay <absolute local data>/DRIVER2/REPLAYS/ATTRACT.400`.
  Original file 14,676 bytes, SHA256
  `0e6a2f385233b9edc528416feb0e4446be1cac14ba4bfc3f5cd10bcf21307df4`.
  File stays local. Original replay loader accepts its magic `0x14793209`.
  Capture430: 560 draws (9 shadow, 551 main), zero first-replay messages.
  This is a repeatable route sample, not long-route/full-city acceptance.
- **playground-flatpad-v1**: `-playground`, same saved renderer/light settings.
  Capture430: 51 draws (9 shadow, 42 main), zero first-replay messages. Original
  donor sky/car and modern gallery visible; generated floor has existing visual
  discontinuities. This records a baseline defect, not a visually accepted world.
- **analytic-and-owned-fixture**: `-vkfixture -vkcapture 120 -vknogui -vkshot
  <absolute ignored evidence>/analytic-fixture.bmp`. Existing standalone fixture
  exits after 120 frames; readback saved 1280x720, 12 instances/draws, eight owned
  GLBs plus analytic geometry. This is complementary and does not satisfy R01's
  native-camera-in-original-city gate. Screenshot API writes BMP even if given
  a `.png` suffix; use the actual BMP output.
- **legacy-packet-tests**: `-vkpsxtest`. Fresh final diagnostic build executed
  through VS MCP; pixel blend/composition/stencil/persistence and VRAM export
  checks reported PASS. This protects the selectable legacy fallback only.

## Observations beyond compilation

- `scripts/run_render_coverage_tests.ps1`: 12 invariant tests passed, including
  separate `/TC` C consumer linked to C++ implementation. Tests cover culled
  attempts, experimental/native separation, persistent packets, multi-view
  aggregation, lifecycle/reset and invalid category safety.
- Native (uninjected) city run: 53 one-second windows after discarding4 loading
  windows. Mean frame 33.4319 ms (window means32.97..34.59); mean submission
  6.9562 ms (6.24..8.07); vertices9015..10836; draw splits697..864.
  These are **distributions of window means**, not per-frame p95/GPU throughput.
- Same process: working set274,690,048 bytes, peak378,814,464; private810,610,688,
  peak paged902,344,704. This is process memory, not renderer allocation tracking.
- Coverage sample frame 635: road234/234 requested/legacy, scenery64/60,
  foliage8/7, car bodies5/5, wheels8/8, pedestrians1/1, events2/0,
  particles7/2, decals9/6, secondary20/16, HUD5/1. All native counts0;
  static experiment counts0 under bilinear. Absent water is not water validation.
  Full mode/category rendering acceptance remains R03/R06/R10/R12 work.

F11 Graphics coverage table was opened/expanded and all17 rows inspected in the
running city. Keyboard/gamepad capture flags were3 while visible. Existing saved
ImGui position(1535,35) was outside the 1280-wide window; a temporary visible
position allowed verification, then original bytes were restored. Keyboard-only
F11 close attempts were inconclusive; owned debugger sessions were stopped.
This is not a delivered fix for existing panel positioning or input automation.

Final VS MCP Release_dev x64 build passed (failed projects 0); final PSX readback
self-test passed. `git diff --check` passed with existing CRLF normalization
warnings. No Linux/OpenGL runtime claim. Original/fork assets and staged gitlink
were preserved. See [implemented diagnostics](../product/native-renderer-baseline.md).
