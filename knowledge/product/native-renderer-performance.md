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
Draw distance follows the original approved cell/event-list selection, not a physical
far-plane distance. The native panel enables that control; unmigrated PGXP/legacy
lighting controls remain disabled. The transitional visibility bridge is described below.
Native lighting remains deferred until geometry/material coverage is correct.

Normal-shaded building GLBs retain `_RED2_WORLD_SHADE` per emitted vertex; the attribute is
included in conversion hashes and verified on GLB read-back. The native shader combines each
source shade bucket with the instance's original yaw bucket using `(yawBucket * 4 - shade) & 31`.
The 32-entry RGB palette follows the same plane-colour pattern as `SetupPlaneColours` and updates
from the current world ambience at presentation. Combo-shaded buildings, sprites and tile/path
models keep their existing conversion-time colours. The palette producer preserves packed
`0xRRGGBB` channel order when writing RGB8. This restores per-instance static building orientation
shading; event-specific lighting overrides, PSX blend modes and full scene parity are not accepted.

Inspector validation after this change passed all 18 suites; `WorldMeshBuilderTests` passed 358
checks, including asymmetric RGB palette order. `NativeRendererSmoke` passed 1249 checks, covering
all eight rotation buckets and two orientations sharing one converted resource. The native x64
solution build passed. A Visual Studio native mission 1 launch reached world rendering: the log
records level 0/variant 0, 6,740 placements, 160/160 resources registered, 233 submitted, 186
drawn and zero unresolved resources. This confirms scene publication and submission, not visual
shading correctness. The failed RenderDoc attach passed process PID `73980` where a target-control
identifier is required; this was an invalid capture argument, not evidence of a renderer failure.
A later controlled capture and original-car result are recorded below.

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

## Resident original car bodies and texture identity (2026-10-11)

Car preparation owns a CPU snapshot; its worker does not dereference live `MODEL` data. Model
generation/revision changes invalidate stale preparations. World and actor instances are submitted
through one `SetInstances` call. The car pose applies full rotation and COG offset to the clean
body mesh; captured original body vertices retain neutral lighting. Chicago Take A Drive
(`-nointro -nofmv -mission 1 -drawregion`) logs showed 14/14 active bodies ready and submitted,
none hidden/waiting/failed/missing, plus 200/200 world resources and 8,876 placements with zero
failed, unresolved or truncated placements. RenderDoc 1.41 capture `native-car-pose-20261011.rdc`
replayed 1,036 draws/6,706 triangles with zero debug messages. The inspected PNG shows the player
body, traffic, road and buildings. Steady samples at ticks 2,481–2,903 reported 30/31 ticks and
zero drops, excluding capture/debug pauses. This was Take A Drive, not a story-mission run. Wheels,
damage, pedestrians and on-foot/story views remain unvalidated or incomplete; this is not parity.

The source catalog extracts texture set/index keys from declared polygon types 10 and 11 through
the same helper used by production classification; a focused `AssetCatalogTests` case covers those
links, not every unknown source material. Focused `NativeCarMeshTests` passed and all 19 inspector
suites passed. `NativeRendererSmoke` passed 1249/0 during the earlier shading milestone. After the
final diagnostic-latch change, isolated x64 `Release_dev_native` and reference `Release_dev` builds
passed. OpenGL startup was not run because the Visual Studio configuration MCP call hung. Probe
profile and INI files were restored byte-for-byte, verified against backups.

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
107 to 37 over the observed two transitions with synchronous new conversions.
Native smoke passes 1213 checks, including a visible instance at the final capacity index
and loading-frame counters; all 16 inspector suites pass. These tests do not establish
whole-city coverage or deterministic replay parity through conversion stalls.
The final RenderDoc replay/export has 5904 draws and zero replay messages; the PNG
shows the new-sector street/buildings. Capture introduced 13 additional dropped ticks
(runtime total 50), separate from the 37 drops observed across the two transitions.

### Owned asynchronous preparation

New conversions capture geometry, indexed CPU pixel buffers and legacy colours on the
game thread. One worker builds, hashes and publishes immutable owned inputs; it never
reads live MODEL/VRAM/game pointers or accesses the GPU. Normal frames poll completion;
level loading and shutdown join. Completed results are published only when their sector,
area, model and page provenance still matches. Failed slots retain named diagnostics
until source state changes. GPU registration resumes across frames with a 4 ms batch
target; one indivisible resource upload may exceed that target.

WorldMeshConversion preserves the historical cache seed and encoded key bytes. Pixel
inputs are bounded to 64 buffers/16 MiB, expanded images to 64 MiB and cache reads to
128 MiB. Cache writers serialize transactional publication. Automatic region readback
now requires `RED2_NATIVE_REGION_CAPTURE=1`; normal diagnostic reports do not dump images.

Final Release_dev_native replay observed both sector transitions above, then tick 5551,
with zero dropped ticks before GPU capture. Later steady samples were about 36-38 FPS,
30/31 ticks per interval, with 10167 placements and 6347 draws. The RenderDoc capture
`native-async-world.rdc` reports 6347 draws/zero replay messages; its inspected PNG shows
the new-sector street and buildings. Capture induced 17 dropped ticks, recorded separately.
All 17 inspector suites passed; WorldMeshConversionTests passed 14 checks covering owned
inputs, concurrent cache publication, historical key compatibility and allocation bounds.
VS native and Release_dev x64 builds passed; OpenGL mission 1 startup reports version
3.3.0. Both debug sessions stopped; the original generated debugger profile was restored
byte-for-byte and Visual Studio returned to Release_dev_native|x64.
These observations do not establish whole-city coverage or full replay determinism.
### Published-scene continuity

Presentation now owns copied placement values and resource keys independently of the
catalog's next preparation. During conversion and incremental GPU upload, it retains
the published native scene and updates its camera-dependent LOD. Replacement instances
are submitted before obsolete keys retire. Level changes and explicit source failures
invalidate the old scene. A conservative combined old/new resource reservation exceeding
1024 enters named loading instead of exhausting the registry silently. Initial loading
still clears. This does not prefetch visual sectors beyond gameplay residency.

Reports wait for the committed publication; intermediate catalog counts are not presented
as active scene counts. Commit diagnostics count retained and loading frames. Native GPU
smoke passes 1220 checks, including byte-identical output during staging and visible new
pixels/no unresolved instance after replacement and retirement.

Final ATTRACT.400 transitions committed 206/185 resources and 8594/10167 placements,
retaining the prior publication for 99/35 frames respectively, with zero empty loading
frames between scenes. Initial loading recorded 132 frames without a published scene.
Performance intervals through tick 717 report zero dropped ticks before capture.
RenderDoc `native-continuity.rdc/png` has 5897 draws/zero messages; the inspected image
shows the committed new-sector street/buildings. Capture added 15 ticks of bounded
backlog loss separately. These bounded observations do not accept whole-city residency.
VS Release_dev_native/Release_dev x64 builds passed; compatibility mission 1 started
with OpenGL 3.3.0. Sessions stopped and the original generated debugger profile restored
byte-for-byte; VS returned to Release_dev_native x64. No gameplay/tick source changed.

Prefetch, native draw distance and player vehicle/on-foot presentation remain required.
### Cell and event-list visibility

The resident collector now preserves every object/cell/list membership, including
straddler aliases, separately from unique placements. A bounded 262144-reference store
fails explicitly on overflow instead of publishing partial visibility. Presentation
copies those memberships when committing a publication, independently of later spooling.

DrawMap records only cells approved by its original square spiral, draw-distance budget,
PVS, map bounds and camera planes, plus the active event-list selector. This project-owned
main-thread presentation record does not alter the original loop or gameplay/RNG state.
Diligent builds an object mask and compacts its own instances from these memberships;
an object may be visible through any approved cell, but default and event lists do not
form a visibility union. GPU AABB culling/LOD still apply to those native instances.

This is a transitional selection bridge, not independent native PVS/streaming ownership
or removal of GTE/ordering-table generation. Split-screen and full event-mode coverage
remain unaccepted. The F9 draw-distance control is now enabled; settings precedence and
its original cell-count meaning remain unchanged. Performance logs include visible
instance, approved cell, draw-distance and membership counts. Inventory camera counters
use the inventory denominator; unresolved diagnostics scan only submitted instances.

All 18 inspector suites passed; NativeWorldVisibilityTests passed 15 checks covering
straddler aliases, event-list replacement, clearing, absent selection and inventory/
coordinate bounds. In mission 1 frozen at tick 30, both logs show identical camera
position/matrix: draw distance 441 selected 34 cells/155 instances/308 draws, while
1800 selected 77/233/461. Both RenderDoc replays have zero messages; inspected 1280x720
PNGs are byte-identical at this occluded camera view, retaining near buildings/fire escapes.
Extra selected instances may be culled or depth-occluded; this is not universal image
parity. Tick-600 runs selected 35/70
cells but the same 156 instances, demonstrating that extra cells can be empty.

ATTRACT.400 again reached 8594/10167 placements (206/185 resources), zero conversion
failures/truncations and 108/38 retained frames with zero empty transition frames.
Observed intervals through tick 2128 had zero dropped ticks; this is a bounded route,
not complete city/event/replay acceptance. Temporary INI settings were restored exactly.
The final native build/startup also crossed both sectors (111/38 retained frames, zero
empty transition frames), with zero dropped ticks through the observed tick 1075.
VS Release_dev_native and Release_dev x64 builds passed; compatibility mission 1 started
with OpenGL 3.3.0. Sessions stopped, INIs/debugger profile restored byte-for-byte and VS
returned to Release_dev_native x64. No scene/gameplay acceptance follows from these checks.
