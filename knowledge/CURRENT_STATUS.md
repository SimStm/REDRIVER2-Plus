---
type: Status
title: Project status
description: Current branch implementation, preserved rework and adopted engine migration.
tags: [okf, status]
---

# Current status

> Updated: 2026-10-09.
> Source of truth for implemented surfaces on the current branch; verify source/Git.
> [RECENT_CONTEXT](RECENT_CONTEXT.md) records the next task and session handoff.

## Branch and adopted plan

`feat/new-engine` starts from parent `2e93f215c54e61dd1bbb5d26e439c25303466709`,
the existing `master` baseline, through a newly created local `main` alias.
Remote `master`/default branch was not renamed. The parent records fork commit
`76d83df0e400bdff5c1e83c7be74d9e01ba0c660` (live Vulkan VSync), based on
`37d399d`; the submodule working tree has four uncommitted files
(`include/PsyX/PsyX_public.h`, `include/PsyX/PsyX_vk.h`, `src/PsyX_main.cpp`,
`src/render/PsyX_Vk.cpp`: headless presentation, post-frame handler and the
pad-backend disable). All E02/E03 worktree implementation and its docs are
likewise uncommitted on top of `036dfc34`. The rework fork history was not merged.

The former dirty native Vulkan work is committed/pushed separately at
`e68b6ea6b1e30543af77d6244c109f7993ff65cf` on
[`feat/vulkan-native-rework`](https://github.com/SimStm/REDRIVER2-Plus/tree/feat/vulkan-native-rework).
That snapshot includes source/tests/tooling/knowledge/archive moves and the fork
gitlink `ae83dd229f2c4a287575fb4cb853006996d9eef8`, already published in the fork.
R00-R02 Windows scopes are documented there; R03 is incomplete/R04 unstarted.
Those native APIs/results are not part of this branch's current implementation.
Ignored captures/private helpers/builds remain local and do not travel with Git.

Implemented here: [engine-migration planning](roadmap/planned/engine-migration.md),
Required E00-E07 contracts, optional final E08, five issue briefs, validation policy and agent/
catalog guidance. **The planning change itself implements no engine behavior.**
Partial implementation is recorded in E00-E03 below; current source still requires
PsyCross. Diligent/DiligentFX, FMOD Core, complete converted packages and eventual
removal remain migration work. Earlier hybrid graphics/
timing plans are superseded execution directions here. PopcornFX is optional
E08, only upon explicit request after full-game acceptance and PsyCross removal;
existing effects remain mandatory native consumers in E05.

[Backend discussion](discussions/engine-migration/index.md) records the adopted legacy
OpenGL for interim gameplay/regressions and deferring PsyCross Vulkan performance
work. No default, removal or stage status changed.
E03 still selects Diligent Vulkan, independently of the retiring legacy renderer.

## Engine migration E00 (in progress)

[E00 dependency inventory](engine-migration/e00/dependency-inventory.md) records
the PsyCross/Psy-Q contracts, NTSC/PAL tick/replay/RNG facts, draw-time mutation
sites and SDK pins (Diligent is now the official master clone `API256019`; FMOD
2.03.14 planned), with the mechanical appendix reproduced by
`scripts/engine_migration/e00_symbol_scan.py` (3618 declared / 400 used).
`utils/DeterministicBaseline.*` writes a per-tick dump (`-statedump`) compared by
`scripts/engine_migration/e00_compare_state.py` (0 mismatches: NTSC attract 714,
PAL 595, NTSC-vs-PAL 597, mission 726). `RED2_BUILD_DIR`/`RED2_BIN_DIR` produced
isolated NTSC/PAL `Release_dev|x64` builds (Chicago, Vulkan, ~29 FPS).
[Baseline scenarios](engine-migration/e00/baseline-scenarios.md) list the routes;
driving/collision/police, pause/loading, region transition and re-record remain
pending, so E00 is close but not formally accepted.

## Engine migration E01 (in progress)

- `src_rebuild/utils/ProjectClock.{h,cpp}` owns the desktop fixed tick from the
  monotonic counter (30 Hz NTSC, 25 Hz PAL), with bounded catch-up (max 4) and
  a pause API; `State_GameLoop` steps by due ticks and `State_GameInit` no
  longer forces a 30 FPS swap interval. Gameplay reads of the draw counter
  `FrameCnt` moved to the simulation tick in camera/debris/gamesnd/objanim/
  overlay/director/main.
- Authoritative determinism vs the pre-E01 E00 baselines (0 mismatches on all
  shared ticks): NTSC attract replay, NTSC mission, PAL replay.
- Presentation now draws every frame (fixed ticks decouple simulation from
  display rate). Presentation-rate independence verified with 0 mismatches:
  vsync (~30 FPS) vs uncapped (~250 FPS / 4 ms) on NTSC attract replay, NTSC
  mission and PAL replay; each vsync run also matched its E00 baseline. The
  dump covers mission/AI timers, message timers, `tannerDeathTimer` and the
  per-stream replay cursor; a 3x-vsync + 3x-uncapped NTSC mission matrix (all
  12 pairs) matched.
- Versioned replay contract: new tapes use `DRIVER2_REPLAY_MAGIC_V2` with an
  explicit version/region/tick-rate/tick-period and a 32-bit content checksum in
  the previously unused header reserved fields (on-disk size unchanged). Legacy
  `DRIVER2_REPLAY_MAGIC` tapes still load. Unknown magic, unsupported version,
  region/rate mismatch and checksum mismatch are diagnosed explicitly. A
  `-savereplay`/`-saveat` developer fixture exercises the save path without the
  pause menu; NTSC/PAL round trips and all four diagnostics were verified, and
  the V1 `ATTRACT.400` regression still matches the E00 baseline. Re-record
  writes real input when the player is controlled; a re-recorded tape replays
  the scenario from its `GameType`, so a playground tape replays as a normal
  game. Full mission re-record → replay reproduction is still pending.
- Visual snapshot/interpolation seam: `utils/VisualSnapshot.*` publishes car
  translation/rotation and per-view camera poses. The 2026-10-08 motion regression
  fix runs original camera controllers at regional ticks, interpolates camera and
  cars over the same interval, and restores tick camera globals before simulation.
  `DrawCar` uses local poses for the body, wheel transform and shadow corners;
  interpolation never writes to car physics/player state. Pause, camera cuts,
  teleports, slot replacement and session resets delimit snapshot history.
  `ProjectClock` uses exact counter-count x regional-rate phase, preserving the
  fractional remainder after bounded catch-up; rounded replay metadata is unchanged.
  Moving-route gameplay matched all 900 before/after ticks, including uncapped
  OpenGL (~220-410 FPS); NTSC/PAL attract baselines matched 714/597 shared ticks.
  A straight-route temporal probe reduced the maximum camera-relative car step
  from 143.13 to 1.41 units (ticks 40-155, Vulkan). NTSC/PAL timing tests cover
  25/30/60/120/144/1000 FPS and irregular schedules. Original
  wheel spin and pedestrians still need coverage. Tick-owned effects below extend
  the snapshot boundary; E05 remains pending. A developer toggle is a follow-up.
  [Fix and evidence](changes/2026-10-08/e01-presentation-motion-fix/index.md).
- Input service extraction: `utils/InputService.*` now owns the SDL2 keyboard/
  game-controller polling and the input mapping (types and the four binding
  tables moved from `PsyX_public.h`), and `Game/C/pad.c` uses it instead of the
  libpad calls; the pad-state/rumble contract is project-owned. It has no
  PsyCross dependency. Verified by live keyboard drive tests and unchanged
  NTSC/PAL replay baselines. PsyX's pad backend is disabled in the project fork
  (`PsyX_main.cpp`), so `InputService` is the single controller owner; the game
  still calls `PsyX_UpdateInput` for the SDL event pump (uncommitted fork change).
- Bounded backlog policy: `ProjectClock` caps catch-up at 4 ticks/frame and
  counts the excess; `State_GameLoop` now logs the dropped ticks (rate-limited)
  so discards are never silent. `SetPaused` re-bases on resume and the first
  frame re-bases so setup time is not accumulated. A `RED2_CLOCKJUMP_MS` fixture
  verified a 2 s stall yields exactly 4 applied + 56 dropped ticks. Dropped
  ticks change wall-clock pacing, not the input-determined state at each tick,
  so the baseline still matches.
- Complementary effects correction (2026-10-09): emissions, pool lifetime/motion,
  birds/rain/hubcaps, light phases, mission title/input unlock and bomb flash have
  regional tick owners; drawing consumes local interpolated poses. Palette uploads
  use per-frame packets to prevent catch-up OT cycles; pause messages use service ticks.
  Strict dumps include effect-pool
  digests and indicator/siren phases plus a recorded-field draw mutation guard.
  Full OpenGL ATTRACT runs match all 1378 ticks at NTSC 30/360 and PAL 25/360 FPS;
  both builds and fresh timing/settings/effect tests pass, including an isolated
  staged-source rebuild with the committed fork. Rainy ATTRACT.401 also
  matches all 2289 ticks at 30/360 after the palette fix. The wall-collision route
  remains deferred at the user's request: b2d8857 plus historical PsyCross produces the same
  gameplay on local assets, including Win32 versus x64. This is schedule
  independence, not replay trajectory acceptance. The official upstream 8.0
  Win32 binary also visually reproduces the wall collision with these NTSC data.
  [Scope/evidence](changes/2026-10-09/effect-timing-replay-regression/index.md).
- Presentation/sim separation: the camera collider and rear-light render scratch
  are excluded from gameplay acceptance. The earlier black screen was a `SetDispMask(1)` gating
  bug (it ran only on tick frames, so a presentation frame could pass
  `FrameCnt == 5` with `ticks == 0` and stay masked), not a draw-path
  limitation; unmasking every frame fixed it. Car/camera temporal interpolation
  is now corrected; complete presentation coverage, live pause/backlog stress
  and full mission replay re-record remain for later E01 blocks.

## Engine migration E02 (in progress)

- Reproducible conversion: exported texture manifest entries now carry a
  canonical 64-bit FNV-1a `contentHash` over the converted pixel data
  (`utils/HdTextureOverrides.cpp`), so two identical conversions produce equal
  entries. Covered by 3 new `InspectorExportTests` checks (107 passed;
  `AssetCatalogTests` 145 pass).
- Native asset package manifest: `Game/C/assetpackage.*` is a pure-data module
  (no PsyCross/GPU/game state) that parses, validates and canonically serializes
  a readable JSON package (`schemaVersion`, `id`, `entries[]` with
  `id`/`type`/`file`/`contentHash`/`dependencies[]`). Failures reset the package
  and return a named error with a diagnostic (schema version, missing fields,
  duplicate ids, unknown dependencies, trailing data). Covered by a new
  `AssetPackageTests` suite (39 checks).
- Conversion cache contract: `Game/C/conversioncache.*` is a pure-data module
  that keys a conversion by source hash, converter, schema version, options and
  active mods, with a canonical key string, malformed-key diagnostics,
  staleness reasons and deterministic cache paths. Covered by a new
  `ConversionCacheTests` suite (38 checks).
- The existing source-aware asset catalog (`Game/C/assetcatalog.*`) already
  provides model/texture/car identity, LOD policy and selection anchors, with
  standalone tests.
- Native mesh record: `utils/GltfWriter.*` writes a bounded GLB (positions/
  normals/UVs/indices/material/node matrix) and `utils/GltfLoader.*` reads it
  back and now retains the node transform, so geometry/UV/transform/flags agree
  in focused fixtures and identical input produces byte-identical GLB. Covered
  by a new `GltfRoundTripTests` suite (28 checks).
- Transactional converter: `utils/AssetConverter.*` turns a mesh record into a
  cached native GLB keyed by source hash/converter/schema/options/active mods;
  it reuses a fresh GLB, rebuilds a stale one, verifies the written GLB by
  reading it back, publishes via a temp file + rename, and returns the content
  hash. Covered by a new `AssetConverterTests` suite (28 checks).
- Package assembly: `AssetPackage_SetId`/`AssetPackage_AddEntry` let a converted
  resource's published content hash become a manifest entry with dependencies
  (added in any order; the parser still rejects a dangling dependency). Covered
  by a new `AssetPipelineTests` suite (25 checks).
- Sound metadata record: `Game/C/soundasset.*` carries sample rate, channels,
  bit depth, frame count, loop points, pitch reference, source bank/voice ids
  and music bank/program/sequence, with validation and canonical serialization.
  Covered by a new `SoundAssetTests` suite (32 checks).
- Car serializer wiring (2026-10-09): `utils/CarMeshSerializer.*` derives a
  deterministic conversion key (raw position/index/identity bytes, converter
  `car-mesh`, schema, options and a mods digest) and publishes through
  `AssetConverter_ConvertMesh`; `Cars_ConvertInspectorModel` feeds the resident
  car model (`NewCarModel` + `gTempCarVertDump`) from a new GLB button in the
  developer panel beside the OBJ export, and `CarMeshSerializerTests` (57 checks)
  covers reuse/rebuild, mods-key sensitivity, invalid arguments and
  missing-directory failure. All nine inspector suites pass (AssetCatalog 145,
  AssetPackage 39, ConversionCache 38, GltfRoundTrip 28, AssetConverter 28,
  CarMeshSerializer 57, AssetPipeline 25, SoundAsset 32, InspectorExport 107).
- Still pending: Chicago slice inventory reconciliation; the conversion cache is
  not yet consumed by the native renderer/loading path (E03/E06). Forward notes:
  derived artifacts live in `mods/<modId>/assets/cache`, so packaging must
  exclude that directory; converting the live (dented) vehicle vertices means a
  different dent state creates a new cache entry, so native loading should
  convert the pristine model and define cache eviction.

## Engine migration E03 (in progress)

- SDK pin/build (block 1, first half): DiligentEngine pinned at `v2.5.6`
  (DiligentCore commit `b036337d`), Vulkan-only static Release built. Acquisition
  is reproducible via `scripts/fetch_diligent.ps1`; the SDK lives in the
  gitignored `dependencies/DiligentEngine` and is never committed. New native
  renderer modules use C++17.
- Device/swapchain/present owner (block 1, second half): `native/NativeRenderer.*`
  owns one Diligent device/context/swap chain for one native window handle, with
  a Diligent-free C ABI, clear-and-present, resize and shutdown.
- Converted content, depth and a textured camera draw (blocks 2-3): a converted
  GLB's positions and indices are uploaded to immutable Diligent buffers and read
  back identical; overlapping triangles render through a depth-tested pipeline
  (D32 depth target) and depth keeps the nearer one; a textured quad samples a
  base-colour texture through a constant-buffer MVP, and a translated MVP moves
  it off the centre pixel. Verified by `scripts/run_native_renderer_smoke.ps1`
  (15 checks).
- In-game linkage and probe (block 1): the game links the native renderer +
  Diligent on Windows when the SDK is present (C++17 native file, v145 toolset,
  ATL by full path, `/GL` disabled in the SDK). A gated `RED2_NATIVE_RENDERER`
  probe calls the renderer on the game's PsyCross window: the device is created
  but the swap chain fails with `VK_ERROR_NATIVE_WINDOW_IN_USE_KHR` because
  PsyCross already owns the window's swap chain (the stage's warning, reproduced
  and captured through Diligent's debug callback).
- Headless PsyCross handoff (block 2): PsyCross can create the game window
  without a swapchain/present (`PsyX_SetHeadless`, `PsyXVkConfig.headless`), so
  the native renderer owns the window's presentation. Verified in-game: `native
  renderer active on the game window (1280x720)` / `native renderer released the
  window`, no Diligent errors; the default path is unchanged.
- The SDK was upgraded to a direct clone of the official repository (master,
  DiligentCore `API256019`), Vulkan-only static Release. The new third-party
  layout needs `volk.lib` and no longer has separate
  `GenericCodeGen`/`MachineIndependent`/`OSDependent` glslang libs; the native
  file needs an explicit `/std:c++17`.
- Camera seam + per-frame draw (block 3): a generic PsyCross post-frame handler
  (`PsyX_SetPostFrameHandler`) lets the game draw after each finished frame. With
  `RED2_NATIVE_RENDERER` set, the game keeps the native renderer active and feeds
  the game camera (view derived from `inv_camera_matrix`, same convention as the
  modern-mesh path) to `NativeRenderer_SetCamera`/`NativeRenderer_RenderScene`,
  which draws a depth-tested, textured ground/pillars scene with a D32 depth
  target. Verified in-game: `native renderer drawing the game window with the
  game camera`, no Diligent errors.
- `Release_dev_native` build configuration (premake, both projects): builds like
  `Release_dev` (`_native` suffix) and launches the debugger with
  `RED2_NATIVE_RENDERER=1` and `-nointro -nofmv`, so the native path is testable
  from Visual Studio. Verified: builds and runs the native renderer.
- Converted content in the native scene (block 4): `NativeRenderer_LoadContent`/
  `ClearContent`/`HasContent`/`GetContentStats` load a converted GLB through
  `utils/GltfLoader` into immutable buffers/texture (node transform as the model
  matrix, base-colour factor/texture, unlit) and draw it in place of the
  placeholder; `RenderSceneToRGBA` runs the same scene draw offscreen for pixel
  checks. `RED2_NATIVE_CONTENT` loads a converted GLB into the in-game native
  path; a failed load keeps the placeholder and does not abort the path. The
  game now releases the Diligent device before `PsyX_Shutdown` destroys the
  window, forwards window resizes to the swap chain, and logs scene failure /
  recovery transitions instead of only the first failure. Verified by the smoke
  suite (38 checks, 0 failures: node transform, camera movement, placeholder and
  resize readbacks) and an in-game probe: content loaded, drawn with the game
  camera, resize applied (`Resizing the swap chain to 1008x561`) and a clean
  shutdown (exit 0) with no scene-failure or crash lines.
- Not yet done: the native scene accepts one converted mesh/instance, not the
  game's world content; world meshes/materials/lighting and the player remain E03
  work (the car serializer wiring feeding this path is the next E02 block).
  Debug configs are not wired (only Release Diligent libraries exist). Headless
  native mode does not run the F11 ImGui overlay yet; a possible PAL double-aspect
  in the camera seam needs a PAL probe.
- Next E03 block: feed real world content/materials/lighting and the player to
  the native renderer; then commit the uncommitted E00-E03 work before starting
  E04 (FMOD Core). The session handoff in [RECENT_CONTEXT](RECENT_CONTEXT.md) has
  the full checklist.

## Project and current services

REDRIVER2 is reconstructed Driver 2 game logic, not an emulator. Physics,
collision, AI, missions and replays live primarily under `src_rebuild/Game/`,
but still call PsyCross math/GTE, clocks, pad, SPU/CD/streaming and GPU services.
The PSX branch still shares `FilterFrameTime`/`VSync`; on desktop `ProjectClock`
now separates the fixed simulation tick from presentation, which draws every
frame and interpolates car/camera translation and orientation between ticks.
Replay repeats and counters remain tick indexed. Local effect interpolation now
covers particles/birds/hubcaps/rain; complete pedestrian/wheel-spin/native VFX
coverage remains E05 work. E01
preserves regional semantics before changing clock owners.

PsyCross remains a separate submodule from `git@github.com:SimStm/PsyCross.git`.
Do not substitute upstream or infer removal from a roadmap. Existing C++11 and
Windows/Linux/web/Android/PSX guards remain. SDK/compiler/target compatibility
for the new stack is pending E00, not established by cross-platform advertising.

## Existing renderer baseline

Desktop defaults to Vulkan through PsyCross, with separately selected OpenGL;
this is the existing PSX primitive/hybrid architecture, not Diligent.
[Product behavior](product/vulkan-game-renderer.md) and the source govern actual
support.

- Vulkan PSX shaders implement CLUT/texture windows/dither/filtering, VRAM mirror,
  blend/stencil/depth, white primitives and queued VRAM copies; `-vkpsxtest` holds
  readback/CLUT/offscreen/export/blend regression fixtures.
- The modern mesh API/importer/PBR/shadow composite shares the PSX projection and
  depth; legacy lighting receptivity is optional and PGXP stays on that composite.
- World/overlay boundaries split gameplay composition from flare/fade/UI tails;
  split-screen keeps its limited composition path. This does not prove native
  mode/category coverage under the new architecture.
- Minimize/restore and fence-deadlock fixes are recorded in
  [minimize](changes/2026-09-21/vulkan-minimize-swapchain-crash/index.md) and
  [fence](changes/2026-09-21/vulkan-frame-fence-deadlock/index.md) evidence.
- Shadow-volume centers snap to texels while following the vehicle (historical
  aligned GL/Vulkan shadow-receive IoU 0.985). Legacy city geometry still lacks
  the planned caster path; only imported modern meshes cast. `PSYX_PERF_LOG`
  records submission CPU time (~6.4 ms at 1280x720 historically); GPU timestamps,
  peak memory and broad platform measurements remain pending. `-vkfixture`
  renders modern GLB fixtures only and accepts nothing about the new engine.

Existing UI parity/blend/offscreen details remain in
[UI product](product/vulkan-ui-image-parity.md) and
[lighting product](product/legacy-lighting-receptivity.md). New migration uses
targeted native contracts, not universal pixel equality with this old pipeline.

## Textures, mods and inspector

- Ordered JSON mods and `mods/enabled.json` retain deterministic precedence;
  texture keys are name/page/index with optional palette identity, model
  references stay metadata, and override alpha/cutout/blend semantics mirror the
  active compatibility policy (do not silently change during conversion).
- `AssetCatalog.*` separates stable resources from transient instances with
  provenance and model/texture/LOD links ([catalog product](product/asset-catalog-identity.md)).
- Inspector supports face/material/component/logical-object scopes, source/override
  previews, TXT and single/batch PNG exports with deduplication, progress/retry and
  diagnostics ([selection](product/inspector-selection-coverage.md),
  [batch export](product/object-texture-batch-export.md)).
- Versioned offline visual/audio packages and OpenDriver2Tools-based conversion are
  not implemented; the bounded `GltfLoader.*`/modern fixtures are reuse candidates,
  not the new city pipeline.

## Gameplay and developer tooling

- Resident procedural playground (flat surface/collidable boxes, disabled donor
  traffic/missions, reset/frontend return, `-playground`, Take a Ride) is recorded
  in [playground product](product/playable-testing-playground.md) P1-P5. Debug
  start settings preserve mission/city/vehicle/position/heading/player/chase and
  generate reproducible command lines (`scripts/run_debug_start.ps1`).
- F11 Developer Graphics Panel uses vendored ImGui1.91.9b (Windows/Linux) and
  `developer_graphics.ini`, independently from `config.ini`. Schema9 display
  controls include a 15..360 presentation FPS slider (defaults 30 NTSC/25 PAL),
  NTSC/PAL selection and VSync beside resolution; FPS/VSync apply live, region
  applies after restart, and the same binary selects the 30/25 Hz clock,
  screen/aspect/audio and replay metadata at startup. Window/fullscreen selection
  keeps its confirmation/revert (fullscreen untested); the input editor captures/
  cancels bindings and persists `developer_input.ini`.
  [Frame controls](changes/2026-10-08/display-frame-controls/index.md) and
  [runtime settings](product/runtime-settings-gui.md) hold the detail.
- Content/language/loading/dynamic-light/widescreen controls describe reload
  requirements; `DeveloperSettingsFile_WriteKeys` preserves unknown keys/comments
  with transactional temp/backup files.
- FMOD/PopcornFX, RmlUi, supplementary physics and in-game authoring editors are
  not implemented; ImGui/input/window dependencies must migrate before E07.

## Knowledge and validation state

English documentation separates planned intent, product behavior, completion
records and dated change evidence. [Migration validation](engine-migration/validation.md)
requires deterministic mechanics, conversion inventories and targeted native
resources/pixels/audio, with actual budgets and platform scope.

The planning-only validation is recorded in [its change record](changes/2026-10-08/engine-migration-plan/index.md).
Subsequent E00-E03 and complementary fixes above record their actual isolated
builds and bounded tests. The latest effect/replay evidence retains the unresolved
route fault; a matching dump does not accept a migration stage. Older generated
projects/executables may belong to the preserved rework branch.

Existing commands include `git diff --check`, Premake/generated builds,
`-vkpsxtest`, `-vkfixture`, backend flags and the inspector test script; they
prove their stated baseline scopes only, and export tests need a fresh directory.
