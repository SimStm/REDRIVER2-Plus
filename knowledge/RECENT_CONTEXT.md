---
type: Status
title: Recent engineering context
description: Vulkan rework preserved; new engine migration planned from the existing baseline.
tags: [okf, status, agents]
---

# Recent engineering context

> Updated: 2026-10-09.
> Read the handoff below first: the plan, the current step (E03) and what must
> finish before E04. Older task narratives are condensed; their change records
> hold the detail.

## Handoff (read first): plan, current step, what must finish first

**Plan:** staged engine migration on `feat/new-engine` — umbrella
[`roadmap/planned/engine-migration.md`](roadmap/planned/engine-migration.md),
execution [`engine-migration/index.md`](engine-migration/index.md), stage
contracts [`engine-migration/stages/`](engine-migration/stages/) (E00-E07; E08
PopcornFX optional, only on explicit request). User scope: at least through E06.
Preserve Driver 2 simulation/math/mission/cutscene/replay/regional semantics;
fixed ticks independent of presentation FPS and CPU speed.

**Current step: E03 (Diligent playable slice), in progress.** Done and verified:
- SDK: official clone in `dependencies/DiligentEngine` (master `API256019`),
  Vulkan-only static Release plus `volk`; `scripts/fetch_diligent.ps1` rebuilds it
  (`/arch:AVX2` disables `/GL`). Layout needs `volk.lib`, one `glslang.lib` and an
  explicit `/std:c++17` on `native/NativeRenderer.cpp`.
- Renderer: `native/NativeRenderer.*` owns device/swapchain/present (Diligent-free
  C ABI) and draws a depth-tested textured scene via
  `NativeRenderer_SetCamera`/`NativeRenderer_RenderScene`.
- PsyCross: headless window (`PsyX_SetHeadless`, `PsyXVkConfig.headless`) so the
  native renderer owns presentation; generic post-frame hook
  (`PsyX_SetPostFrameHandler`). Both are submodule working-tree edits (uncommitted).
- Game: `RED2_NATIVE_RENDERER` gates it; the game feeds its own camera and draws
  every frame. Log: `native renderer active on the game window (1280x720)` and
  `native renderer drawing the game window with the game camera`, 0 Diligent errors.
- VS config `Release_dev_native` (premake): builds `bin/Release_dev/REDRIVER2_native.exe`
  with debugger env `RED2_NATIVE_RENDERER=1` and args `-nointro -nofmv`.
- Converted content (E03-A, verified 2026-10-09): `NativeRenderer_LoadContent`/
  `ClearContent`/`HasContent`/`GetContentStats`/`RenderSceneToRGBA` draw a
  converted GLB (node transform + base-colour factor/texture, unlit) instead of
  the placeholder; `RED2_NATIVE_CONTENT` loads it in-game. Shutdown before the
  window is destroyed, resize forwarding and edge-triggered failure/recovery
  logging were added after the oracle gate. Evidence: smoke 38/0 (orchestrator
  re-run), `Release_dev_native` build exit 0, in-game probe with resize applied
  and graceful exit code 0.

**Must finish before E04 (FMOD):**
1. E03 — converted fixtures are done; remaining: feed the game's actual world
   meshes/materials/lighting and the player (later E03 block).
2. E02 — serializer wiring done (`utils/CarMeshSerializer`,
   `Cars_ConvertInspectorModel`, panel GLB button, `CarMeshSerializerTests`
   57 checks, all nine inspector suites pass); remaining: Chicago slice
   inventory reconciliation.
3. Commit/separate the uncommitted work (see below) — nothing from E02/E03 is
   committed yet.
4. Debug configurations are not wired (only the Release Diligent libraries exist).

**Next concrete action:** finish the E02 serializer wiring (in progress), verify
the game build and inspector suites, then commit the E00-E03 checkpoint; after
that start E04 (FMOD Core).

**Commands:** build `Release_dev|x64` or `Release_dev_native|x64` with
`premake5.exe vs2022` + MSBuild (env `SDL2_DIR`/`OPENAL_DIR`/`JPEG_DIR`,
`RED2_BUILD_DIR=build_e00`, `RED2_BIN_DIR=bin/Release_dev`);
`pwsh -NoProfile -File scripts/run_native_renderer_smoke.ps1` (38 checks);
`scripts/run_inspector_tests.ps1` (8 E02 suites);
`scripts/run_presentation_timing_tests.ps1`.

## Uncommitted state (2026-10-09)

`feat/new-engine` HEAD is `036dfc34`; the E02/E03 implementation and the doc
edits are **uncommitted**:
- Worktree: `src_rebuild/native/`, `Game/C/{assetpackage,conversioncache,soundasset}.*`,
  `utils/{GltfWriter,AssetConverter,InputService}.*`, `premake5.lua`,
  `premake5_psycross.lua`, `redriver2_psxpc.cpp`, `replays.c`, `pad.c`,
  `dr2types.h`, tests/scripts and knowledge docs.
- PsyCross submodule (parent records `76d83df`) has four modified files:
  `include/PsyX/PsyX_public.h`, `include/PsyX/PsyX_vk.h`, `src/PsyX_main.cpp`,
  `src/render/PsyX_Vk.cpp` (headless presentation, post-frame handler, pad-backend
  disable).
- Note: the goal-mode auto-continue safety limit (25) cannot be changed from
  inside a session; adjust it in the goal-mode plugin/runtime for longer runs.

## Previous task: effect timing and replay trajectory (committed as 036dfc34)

FPS-driven effect acceleration is fixed: `StepDesktopGame` owns regional effect
updates and drawing consumes local interpolated pools/birds/rain/hubcaps and
attached car lights; indicators/sirens, engine/exhaust emission, title/input
unlock, bomb flash and pause-message expiry have tick owners. Full OpenGL ATTRACT
dumps match all 1378 ticks strictly at NTSC 30/360 and PAL 25/360 FPS; rainy
ATTRACT.401 matches all 2289. The wall-collision route is **not fixed** and is
deferred by the user (`b2d8857` and upstream 8.0 Win32 reproduce it on local NTSC
data). Valid replay commands use spaces between option and value.
[Evidence](changes/2026-10-09/effect-timing-replay-regression/index.md).

## Earlier task: display FPS, regional mode and VSync (committed)

FPS (15..360 presentation cap, defaults 30 NTSC/25 PAL), NTSC/PAL selection and
VSync live in the F11 panel and persist to `developer_graphics.ini` schema9
through transactional key replacement; FPS/VSync apply live, region applies after
restart, and active missions/replays never switch clock/aspect/audio mid-session.
`ProjectClock` owns the preferred/active region and rational presentation
deadlines. Fork `76d83df` wires the Vulkan swap interval to supported present
modes. Verified with NTSC/PAL builds, timing/settings tests and replay dumps; the
FPS cap is a ceiling, not proof of sustained high FPS.
[Record](changes/2026-10-08/display-frame-controls/index.md).

## Latest user decision

Backend discussion: user reports OpenGL at 360 FPS/~3ms versus legacy Vulkan
rarely above45 FPS and asks about OpenGL-to-Diligent focus. Source/log analysis
identifies serialized frame reuse, framebuffer readback/CPU conversion, complete
4MiB VRAM uploads with queue-idle waits and automatic validation as candidates;
no per-section timings establish the dominant cost. The user adopted interim OpenGL
and deferring legacy Vulkan optimization. E03 Diligent Vulkan
remains independent; retirement still requires consumer/build coverage. Updated
the existing discussion only plus status/handoff; no runtime/configuration edits.

The user adopted Diligent Engine/DiligentFX and FMOD Core (Studio later),
OpenDriver2Tools-informed original-content conversion, project-owned services/
simulation ticks and final PsyCross removal. Future directions include RmlUi,
internal map/model/sound/mission/cutscene editors and supplementary physics.
The user confirms permitted middleware use. Do not ask again about adopting it.

Preserve Driver 2 mechanics, missions/cutscenes and replay timing; high display
FPS or CPU clocks must not accelerate simulation. Initial fixed ticks retain
actual NTSC/PAL behavior. Conversion produces modern representations, not
automatically new high-detail art or complete gameplay packages.

## Branch preservation (completed)

The rework snapshot is preserved on `feat/vulkan-native-rework` at `e68b6ea6`
(remote SHA matched; archive moves included; R03 incomplete). Fork `ae83dd2` was
already reachable from the fork remote. `feat/new-engine` was created from
unchanged `origin/master` via a local `main` alias, initialized with fork gitlink
`37d399df`; it holds the older baseline plus planning docs, not the native rework
— do not merge that branch wholesale. Ignored captures under
`src_rebuild/build/native-renderer-r03/` do not travel with Git.

## Documentation boundary

The [umbrella](roadmap/planned/engine-migration.md), [stages](engine-migration/index.md)
and [validation](engine-migration/validation.md) own execution. PopcornFX is
optional E08 only on request after E07; existing effects remain required.
Planning-only checks are recorded separately from E00-E03 implementation below.

## E00 progress (this session)

[E00 dependency inventory](engine-migration/e00/dependency-inventory.md) records
the PsyCross consumers with file evidence, the NTSC/PAL tick facts, the replay
format and the `srand(0x1234)` libc-RNG risk (SDK pins; mechanical appendix
generated by `scripts/engine_migration/e00_symbol_scan.py`).
`utils/DeterministicBaseline.*` writes a per-tick canonical dump (`-statedump`)
compared by `scripts/engine_migration/e00_compare_state.py` (0 mismatches: NTSC
attract 714, PAL 595, NTSC-vs-PAL 597, NTSC mission 726). `premake5.lua` gained
`RED2_BUILD_DIR`/`RED2_BIN_DIR`; isolated NTSC/PAL `Release_dev|x64` builds ran
in-game Chicago. Remaining scenarios (driving/collision/police, pause/loading,
region transition, re-record) are open; E00 is close but not formally accepted.

## E01 progress (this session)

- `utils/ProjectClock.{h,cpp}` now owns the desktop fixed tick (30 Hz NTSC,
  25 Hz PAL) from the monotonic counter with bounded catch-up (max 4) and a
  pause API; `State_GameLoop` steps by due ticks, and gameplay reads of the
  draw counter `FrameCnt` moved to the simulation tick. `State_GameInit` no
  longer forces the 30 FPS swap interval.
- Authoritative determinism vs the pre-E01 E00 baselines (0 mismatches on all
  shared ticks): NTSC attract replay, NTSC mission, PAL replay.
- Presentation draws every frame; presentation-rate independence verified with
  0 mismatches between vsync (~30 FPS) and uncapped (~250 FPS / 4 ms) on NTSC
  attract replay, NTSC mission and PAL replay. Each vsync run also matched its
  E00 baseline. The earlier black screen was a `SetDispMask(1)` gating bug
  (unmask ran only on tick frames), not a draw-path limitation; unmasking every
  frame fixed it. The dump now also covers mission/AI timers, message timers,
  `tannerDeathTimer` and the per-stream replay cursor; a 3x-vsync + 3x-uncapped
  NTSC mission matrix (all 12 pairs) matched.
- Versioned replay contract (E01 block 4): new tapes use `DRIVER2_REPLAY_MAGIC_V2`
  carrying version/region/rate/period and a content checksum in the header's
  reserved fields; legacy V1 tapes still load. Unknown magic, bad version,
  region/rate mismatch and checksum mismatch all diagnose explicitly. Verified
  NTSC+PAL round trips and all four diagnostics; V1 `ATTRACT.400` regression
  still matches the E00 baseline. New `-savereplay`/`-saveat` fixture drives the
  save path without the pause menu.
- Visual snapshot/interpolation seam (E01 block 5): `utils/VisualSnapshot.*`
  publishes previous/current car positions per tick; `DrawCar` interpolates car
  translation with the clock's Q8 factor. Baselines still match (0 mismatches)
  with matching gameplay fields. Later complementary car/camera/effect fixes
  extend this seam; matching ATTRACT state does not prove its route correct.
- Bounded backlog policy: `ProjectClock` caps catch-up at 4 ticks/frame and
  counts the excess; `State_GameLoop` logs dropped ticks (rate-limited), so
  discards are explicit. Verified with `RED2_CLOCKJUMP_MS=2000` (4 applied, 56
  dropped) and a normal run (startup hitch dropped 16, baseline still matched).
  Dropped ticks change pacing, not the input-determined state per tick. Wiring
  the clock pause to the pause menu still needs a precise "sim frozen" signal.
- Findings: the camera collider and `ap.life`/`ap.life2` are draw-path state
  (excluded from the authoritative dump). Replay re-record and interpolation
  coverage beyond car translation remain.
- Input service extraction (E01 block 1, done): `utils/InputService.*` owns the
  SDL2 polling and the input mapping; the mapping types/four tables moved from
  `PsyX_public.h` into the project, `redriver2_psxpc.cpp` applies the active
  table to `g_inputKeyboardMapping`/`g_inputControllerMapping`, and `pad.c` uses
  `InputService_*` (init/update/pad-state/rumble) instead of libpad. Verified
  with a live keyboard drive (car moved) and unchanged NTSC/PAL replay baselines.
- Replay re-record finding: `-savereplay` writes a V2 tape with real input when
  the player is controlled (empty during an uncontrolled mission intro), and a
  re-recorded tape replays the scenario from its `GameType` (a playground tape
  replays as a normal game with traffic), so full mission re-record → replay
  reproduction is still pending. The clock pause API is intentionally not wired:
  the PSX vblank keeps ticking while paused, and `CheckForPause` runs inside the
  tick loop, so freezing the clock would diverge from PSX.
- PsyX pad backend disabled in the project fork (`src_rebuild/PsyCross/src/
  PsyX_main.cpp`: no `PsyX_Pad_InitSystem`/`PsyX_Pad_InternalPadUpdates`/
  controller event dispatch; `PsyX_UpdateInput` pumps events only). The fork
  change is in the submodule working tree and NOT committed. Verified: NTSC/PAL
  baselines still match and a live keyboard drive still works, so `InputService`
  is the single controller owner. The CD/filesystem path is already project-owned
  on desktop (stdio + `utils/fs.*`); the GTE and logging/window services are
  coupled to PsyX (PGXP/renderer, log file/SDL events) and belong to E03/E05. The
  keyboard-controller F4 selection is fixed at port 0.
- The Visual Studio debugger confirmed `car_data[21]` is the camera collider
  updated under `RenderGame2 -> InitCamera -> PlaceCameraFollowCar`.

## E02 progress (this session)

- Reproducible conversion: the texture export path records a canonical 64-bit
  FNV-1a `contentHash` over the converted pixels in each new manifest entry
  (`utils/HdTextureOverrides.cpp`: `ComputeContentHash`, `BuildManifestEntry`).
  `InspectorExportTests` gained 3 checks proving two identical conversions
  produce equal hashes (107 passed; `AssetCatalogTests` 145 pass). The manifest
  parser skips unknown keys, so the field is forward-compatible and a re-export
  preserves the existing hash.
- Native asset package manifest: `Game/C/assetpackage.{h,c}` is a pure-data
  module that parses/validates/canonically-serializes a readable JSON package
  (`schemaVersion`, `id`, `entries[]` with `id`/`type`/`file`/`contentHash`/
  `dependencies[]`), with named diagnostics and reset-on-failure. New
  `AssetPackageTests` suite (39 checks) verifies reproducibility and every
  diagnostic path. `scripts/run_inspector_tests.ps1` now runs all three suites
  (145 + 39 + 107 checks pass); the game build picks up the regenerated project.
- Conversion cache contract: `Game/C/conversioncache.{h,c}` is a pure-data
  module keying a conversion by source hash/converter/schema/options/active
  mods, with canonical keys, malformed-key diagnostics, staleness reasons and
  deterministic cache paths. New `ConversionCacheTests` suite (38 checks);
  `scripts/run_inspector_tests.ps1` now runs four suites (145 + 39 + 38 + 107
  checks pass).
- Native mesh record: `utils/GltfWriter.{h,cpp}` writes a bounded GLB and
  `utils/GltfLoader.{h,cpp}` reads it back (now retaining the node transform),
  so geometry/UV/transform/flags round-trip and repeated writes are
  byte-identical. New `GltfRoundTripTests` suite (28 checks);
  `scripts/run_inspector_tests.ps1` now runs five suites (145 + 39 + 38 + 28 +
  107 checks pass).
- Transactional converter: `utils/AssetConverter.{h,cpp}` turns a mesh record
  into a cached native GLB (keyed by source/converter/schema/options/mods),
  reusing fresh artifacts, rebuilding stale ones, verifying the written GLB by
  reading it back and publishing via temp+rename. New `AssetConverterTests`
  suite (28 checks); `scripts/run_inspector_tests.ps1` now runs six suites
  (145 + 39 + 38 + 28 + 28 + 107 checks pass).
- Package assembly: `AssetPackage_SetId`/`AssetPackage_AddEntry` turn a
  converted GLB's published content hash into a manifest entry with
  dependencies; the parser still rejects a dangling dependency. New
  `AssetPipelineTests` suite (25 checks); `scripts/run_inspector_tests.ps1` now
  runs seven suites (145 + 39 + 38 + 28 + 28 + 25 + 107 checks pass).
- Sound metadata record: `Game/C/soundasset.{h,c}` carries sample rate,
  channels, bit depth, frame count, loop points, pitch reference, source
  bank/voice ids and music bank/program/sequence, with validation and canonical
  serialization. New `SoundAssetTests` suite (32 checks);
  `scripts/run_inspector_tests.ps1` now runs eight suites
  (145 + 39 + 38 + 28 + 28 + 25 + 32 + 107 checks pass).
- Next E02 work: Chicago slice inventory reconciliation and driving the
  pipeline from the game-side car serializer (`Game/C/cars.c`
  `Cars_ExportInspectorModel`). The package/cache/converter/sound modules are
  project-owned but not yet wired into the game's export path. E03 (Diligent) is
  feasible next: git/cmake/python/network are available and Diligent is not yet
  vendored.

## E03 progress (this session)

- SDK pin/build (block 1, first half): DiligentEngine `v2.5.6` (DiligentCore
  commit `b036337d`), Vulkan-only static Release built with CMake/VS 18. The SDK
  is in the gitignored `dependencies/DiligentEngine` (133 MB of sources; the
  Vulkan engine lib is large with debug info). `scripts/fetch_diligent.ps1`
  pins, acquires and builds it reproducibly and was re-run green. New native
  renderer modules use C++17.
- Device/swapchain/present owner: `native/NativeRenderer.{h,cpp}` (Diligent-free
  C ABI; Diligent types stay in the .cpp, kept out of the game's `utils/` glob)
  owns one device/context/swap chain for one HWND, with clear-and-present,
  resize and shutdown. It also exposes `GetDevice`/`GetContext` for a caller
  that owns the Diligent headers.
- Converted content + depth + textured camera draw: the smoke loads a converted
  GLB, verifies positions/indices round-trip through immutable Diligent buffers
  and a staging readback, renders overlapping triangles through a depth-tested
  pipeline (D32 target) where depth keeps the nearer one, and samples a
  base-colour texture on a quad transformed by a constant-buffer MVP (identity
  samples red; a translated MVP moves it off-centre). Shader note: use
  `mul(MVP, v)` (column-vector) with the column-major matrix.
  `scripts/run_native_renderer_smoke.ps1` ran green (15 checks).
- In-game linkage + probe: the game links the native renderer + Diligent on
  Windows when the SDK is present. Build gotchas fixed: Diligent's `/GL` breaks
  a non-LTCG link (disabled in `fetch_diligent.ps1`); ATL (`atls.lib`) only
  exists under toolset v145, so the game uses v145 and passes ATL by full path;
  the linkage is guarded by the SDK's presence so a default build is unaffected.
  A gated `RED2_NATIVE_RENDERER` probe calls `NativeRenderer_Init` on the game's
  PsyCross window: the device is created but the swap chain fails with
  `VK_ERROR_NATIVE_WINDOW_IN_USE_KHR` (captured via Diligent's debug callback to
  `native_renderer.log`) because PsyCross owns the window's swap chain — the
  stage's warning, reproduced and now definitive.
- E03 block 2 (done): PsyCross is initializable headless — `PsyX_SetHeadless`
  / `PsyXVkConfig.headless` create the window but no swapchain and skip
  presentation, so the native renderer owns the window. The game enables it when
  `RED2_NATIVE_RENDERER` is set; the log shows `native renderer active on the
  game window (1280x720)` and a clean release. The SDK was also upgraded to the
  official master clone (`API256019`), which needs `volk.lib`, one `glslang.lib`
  and an explicit `/std:c++17` on the native file.
- E03 block 3 (done): the native renderer draws every frame with the game
  camera. PsyCross has a generic post-frame handler (`PsyX_SetPostFrameHandler`);
  the game derives the view from `inv_camera_matrix` (same rotation-only,
  1/4096-scaled convention as the modern-mesh path) and calls
  `NativeRenderer_SetCamera`/`NativeRenderer_RenderScene`, which draws a
  depth-tested textured ground/pillars scene (procedural checkerboard, D32
  depth) and presents. Verified: `native renderer drawing the game window with
  the game camera`, no Diligent errors.
- `Release_dev_native` VS configuration added in premake (game + PsyCross): same
  build as `Release_dev` (`_native` suffix), debugger env `RED2_NATIVE_RENDERER=1`
  and args `-nointro -nofmv`. Note: `premake5_psycross.lua` must list a new
  configuration in its release filters or premake gives it the debug runtime
  (`UseDebugLibraries=true`), which fails the link with LNK2038.
- Next E03 work: the native scene is placeholder geometry, not the game's world;
  feed the game's actual world meshes/materials/lighting and the player, then
  continue to E04.

## Evidence and next concrete action

Next: continue only a separately requested block. FPS/effects commit `036dfc34` is
pushed to `origin/feat/new-engine` (remote SHA verified); replay routes remain deferred. Complete routes, targeted
rain/hubcap visuals and other targets remain unaccepted. E00-E03
retain their in-progress status. Recheck release binaries and CLI against source;
do not infer native correctness or PsyCross removal from a successful legacy run.

Night/rain follow-up: ATTRACT.401 first froze at 857 due to a static `cyclecluts`
packet linked twice by catch-up ticks. Stack/OT reads identified the self-cycle;
tick-owned colours now upload through frame-owned copies. Final 30/360 completes
2289 ticks with strict equality. Both builds pass again. Final runner 0402adef
passes 145354 timing/21 settings/2688 NTSC or 2448 PAL effect checks; the extra
checks cover catch-up, distinct packet ownership and loading invalidation.

Publication validation: the scoped index was exported to an ignored fresh tree
with clean fork 76d83df, excluding local input/replay-format/converter/Diligent
edits. Both Release_dev x64 rebuilds pass; runner 2745e12d passes all suites.
Its 360 FPS night/rain dump matches all 2289 reference ticks strictly. Process stopped.
