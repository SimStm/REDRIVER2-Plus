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

`feat/new-engine` is at checkpoint `7617bd92`; the PsyCross headless-presentation,
post-frame-hook and pad-backend-disable changes are committed/pushed as `99950a7`.
Subsequent E02/E04 implementation and documentation changes remain uncommitted.
The earlier native rework fork history was not merged.

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

[E00 dependency inventory](engine-migration/e00/dependency-inventory.md) and
[baseline scenarios](engine-migration/e00/baseline-scenarios.md) record service
contracts, regional/replay/RNG facts and remaining routes. The symbol scan found
3618 declared / 400 used. Per-tick comparisons had 0 mismatches for NTSC attract
(714), PAL (595), NTSC-vs-PAL (597) and mission (726); isolated NTSC/PAL builds
ran Chicago. Driving/collision/police, pause/loading, region transition and
re-record remain pending, so E00 is not formally accepted. Local FMOD metadata
identifies 2.03.15; Diligent is official master `API256019`.

## Engine migration E01 (in progress)

- `ProjectClock` owns 30 Hz NTSC/25 Hz PAL desktop ticks, bounded four-tick
  catch-up, explicit dropped-tick diagnostics and presentation-rate-independent
  drawing. Replay V2 carries region/rate/period/checksum while legacy V1 remains
  supported; mismatch diagnostics and NTSC/PAL round trips were verified.
- Authoritative replay-state comparisons matched across baseline runs and vsync/
  uncapped schedules. Full mission re-record/replay reproduction remains open.
- `VisualSnapshot` and local interpolation cover car/camera poses without writing
  gameplay state; exact regional phase is retained. Motion regression evidence:
  [E01 presentation fix](changes/2026-10-08/e01-presentation-motion-fix/index.md).
- `InputService` owns desktop SDL polling/mapping; `pad.c` uses the project-owned
  pad state/rumble contract. The project fork disables its competing pad backend.
- Tick-owned effects cover emissions, pool motion/lifetime, birds/rain/hubcaps,
  lights, mission title/input unlock, bomb flash and pause messages. Strict
  OpenGL ATTRACT runs matched 1378 ticks at NTSC 30/360 and PAL 25/360 FPS;
  rainy ATTRACT.401 matched 2289. This proves schedule independence only, not
  replay trajectory acceptance; wall-collision route remains user-deferred.
  [Scope/evidence](changes/2026-10-09/effect-timing-replay-regression/index.md).
- E01 remains in progress: wheel-spin/pedestrian presentation, full mission
  re-record reproduction, and complete presentation coverage remain open.

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
  developer panel beside the OBJ export, and `CarMeshSerializerTests` (64 checks)
  covers reuse/rebuild, mods-key sensitivity, invalid arguments and
  missing-directory failure. Current integration suite results are recorded
  below; prior per-suite counts in older change records are historical.
- Chicago inventory (2026-10-09): Gate 4A attempt 2 approved a scoped diagnostic.
  The final fresh report scans all four eligible Chicago day barrels: 6740 unique
  placements, 227 straddlers, no overlaps or missing model refs, and 114 unknown
  material classifications. It reports 0 errors/480 warnings (64 displayed,
  truncated). This is a source-union diagnostic, not conversion or world
  acceptance. Corrected traversal includes terminal objects marked `0x8000`;
  the initial 6078-count report is superseded. Fresh report was copied to
  `src_rebuild/build_e04_gate4/logs/chicago_inventory_final.txt`. Other cities,
  variants and dynamic/streamed categories remain open; conversion cache is not
  consumed by native loading. See [inventory evidence](engine-migration/e02/chicago-inventory.md).
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
  work; next is the original-world conversion seam.
  Debug configs are not wired (only Release Diligent libraries exist). Headless
  native mode does not run the F11 ImGui overlay yet; a possible PAL double-aspect
  in the camera seam needs a PAL probe.
- E03 remains without a playable original-world native slice: the native scene
  accepts converted fixtures, not the game's world/materials/lighting and player.
  The next conversion seam is original-world content; see [RECENT_CONTEXT](RECENT_CONTEXT.md).
## Engine migration E04 (in progress)

- Project audio contract (E04-A): `Game/C/audiocontract.{h,c}` is a pure-data
  boundary between gameplay and the future audio service: stable event ids
  (kind/bank/voice-or-sequence packing, zero reserved), play flags, request
  validation with named diagnostics (NaN-safe volume/pitch/distance), a 64-slot
  voice table with 16-bit per-slot generations (stale handles rejected,
  zero-wraparound documented) and inclusive loop-metadata validation matching
  the FMOD `setLoopPoints` semantics. The 216-check test result is historical
  pre-remediation; fix-5 reports 221/0. Gameplay must not call FMOD directly.
- Local ignored FMOD SDK metadata identifies 2.03.15 (`FMOD_VERSION` 0x00020315
  and DLL resource version 2.3.15). Do not infer 2.03.21 from hexadecimal
  suffix `0x15`; no independent new version verification is claimed. See
  [the FMOD integration rule](rules/fmod-integration.md).
- Gate 4B attempt 2 **approved as a bounded SDK-isolated service only**, not
  consumer migration or audible correctness. Fix-5 contract tests report 221/0;
  initial smoke was 49/0 and final smoke follow-up is 57/0. It implements true per-channel 2D/3D,
  concurrent samples, persistent service generations, checked listener updates
  and an x64 SDK guard. Inclusive endpoints remain unchanged; equal endpoints
  are rejected; RESTART/stream loops are unsupported; Doppler is disabled pending
  unit validation. Parent independent builds/suites completed: fresh x64
  `Release_dev` and `Release_dev_native` SDK-present builds exit 0; SDK-absent
  x64 `Release_dev` exits 0. SDK-present Win32 project XML excludes service,
  FMOD link/define and DLL copy; SDK-absent Win32/x64 XML excludes all FMOD
  integration. Win32 was not built. Copied DLL SHA256 matches SDK DLL:
  `F58048FB39BD10B68E3701A553D632BDFC8BE3C644E636E715BCA8E7049A6797`.
  Final suites: catalog 258; audio contract 221; remaining suites 39/38/28/28/
  64/25/32/107. Final AudioServiceSmoke is 57/0 (exit 0), SDK fetch check exits
  0, and `scripts/engine_migration/test_fmod_version.ps1` passes nine fixtures
  (9/0). A temporary reset-on-restart mutant fails 13/57 checks as expected;
  live-service source hash is unchanged. Both prior nits are resolved. No game
  rebuild was needed for test/verifier-only changes. Release-failure stream
  storage lifetime, captured output, device loss and stream stress are unverified.
  Earlier smoke 39/0 and contract 216/0 are historical only. No game consumer
  has migrated; this does not establish audible acceptance.
- Game-target wiring (2026-10-09): premake links `fmod_vc.lib`, defines
  `RED2_AUDIO_FMOD` and copies `fmod.dll` next to the built
  executables when the SDK is present; without it the service unit is excluded,
  so a default build is unchanged. `Release_dev` and `Release_dev_native` build
  and link with the service compiled in. No game system calls the service yet;
  E04 remains partial, with E05/E06 pending.

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
