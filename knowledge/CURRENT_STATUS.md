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
Subsequent E02/E03/E04 implementation and documentation changes remain uncommitted.

The former dirty native Vulkan work is committed/pushed separately at
`e68b6ea6b1e30543af77d6244c109f7993ff65cf` on
[`feat/vulkan-native-rework`](https://github.com/SimStm/REDRIVER2-Plus/tree/feat/vulkan-native-rework),
including source/tests/tooling/knowledge/archive moves and fork gitlink
`ae83dd229f2c4a287575fb4cb853006996d9eef8`; R00-R02 Windows scopes are documented
there, R03 is incomplete/R04 unstarted, and those native APIs/results are not part
of this branch. Ignored captures/private helpers/builds remain local.

Implemented here: [engine-migration planning](roadmap/planned/engine-migration.md),
required E00-E07 contracts, optional final E08, five issue briefs, validation
policy and agent/catalog guidance. **The planning change itself implements no
engine behavior.** Partial implementation is recorded in E00-E04 below; current
source still requires PsyCross, and Diligent/DiligentFX, FMOD Core, complete
converted packages and eventual removal remain migration work. Earlier hybrid
graphics/timing plans are superseded execution directions here. PopcornFX is
optional E08, only upon explicit request after full-game acceptance and PsyCross
removal; existing effects remain mandatory native consumers in E05.

[Backend discussion](discussions/engine-migration/index.md) records the adopted
legacy OpenGL for interim gameplay/regressions and deferring PsyCross Vulkan
performance work; no default, removal or stage status changed. E03 still selects
Diligent Vulkan, independently of the retiring legacy renderer.

## Engine migration E00 (in progress)

[E00 dependency inventory](engine-migration/e00/dependency-inventory.md) and
[baseline scenarios](engine-migration/e00/baseline-scenarios.md) record service
contracts, regional/replay/RNG facts and remaining routes. The symbol scan found
3618 declared / 400 used. Per-tick comparisons had 0 mismatches for NTSC attract
(714), PAL (595), NTSC-vs-PAL (597) and mission (726); isolated NTSC/PAL builds
ran Chicago. Driving/collision/police, pause/loading, region transition and
re-record remain pending, so E00 is not formally accepted.

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

- Reproducible conversion: exported texture manifest entries carry a canonical
  64-bit FNV-1a `contentHash` over the converted pixel data
  (`utils/HdTextureOverrides.cpp`), so identical conversions produce equal
  entries. Native asset package manifest `Game/C/assetpackage.*` is pure data (no
  PsyCross/GPU/game state) that parses, validates and canonically serializes a
  JSON package (`schemaVersion`, `id`, `entries[]` with
  `id`/`type`/`file`/`contentHash`/`dependencies[]`) with named failure
  diagnostics. Conversion cache `Game/C/conversioncache.*` keys a conversion by
  source hash/converter/schema/options/active mods with a canonical key,
  malformed-key diagnostics, staleness reasons and deterministic paths; the
  existing `Game/C/assetcatalog.*` provides model/texture/car identity and LOD
  anchors.
- Native mesh record: `utils/GltfWriter.*` writes a bounded GLB
  (positions/normals/UVs/indices/material/node matrix) and `utils/GltfLoader.*`
  reads it back retaining the node transform, so geometry/UV/transform/flags
  agree and identical input is byte-identical. `utils/AssetConverter.*` turns a
  mesh record into a cached native GLB keyed by source/converter/schema/options/
  mods, reuses a fresh GLB, rebuilds a stale one, verifies by read-back, publishes
  via a temp file + rename and returns the content hash.
  `AssetPackage_SetId`/`AssetPackage_AddEntry` let a converted resource's content
  hash become a manifest entry with dependencies (any order; dangling dependencies
  still rejected).
- Sound metadata record: `Game/C/soundasset.*` carries sample rate, channels,
  bit depth, frame count, loop points, pitch reference, source bank/voice ids and
  music bank/program/sequence, with validation and canonical serialization.
- Car serializer wiring (2026-10-09): `utils/CarMeshSerializer.*` derives a
  deterministic conversion key (raw position/index/identity bytes, converter
  `car-mesh`, schema, options and a mods digest) and publishes through
  `AssetConverter_ConvertMesh`; `Cars_ConvertInspectorModel` feeds the resident
  car model (`NewCarModel` + `gTempCarVertDump`) from a new GLB button beside the
  OBJ export. Covered by `CarMeshSerializerTests` (64) and the new suite set
  (AssetPackageTests 39, ConversionCacheTests 38, GltfRoundTripTests 28,
  AssetConverterTests 28, AssetPipelineTests 25, SoundAssetTests 32); earlier
  InspectorExportTests 107 / AssetCatalogTests 145 are superseded below.
- Chicago inventory (2026-10-09): Gate 4A attempt 2 approved a scoped diagnostic.
  The final fresh report scans all four eligible Chicago day barrels: 6740 unique
  placements, 227 straddlers, no overlaps or missing model refs, and 114 unknown
  material classifications, reporting 0 errors/480 warnings (64 displayed,
  truncated). This is a source-union diagnostic, not conversion or world
  acceptance. Corrected traversal includes terminal objects marked `0x8000`; the
  initial 6078-count report is superseded. Fresh report was copied to
  `src_rebuild/build_e04_gate4/logs/chicago_inventory_final.txt`. Other cities,
  variants and dynamic/streamed categories remain open; conversion cache is not
  consumed by native loading. See
  [inventory evidence](engine-migration/e02/chicago-inventory.md).

## Engine migration E03 (in progress)

- SDK pin/build: DiligentEngine is pinned at `v2.5.6` (DiligentCore `b036337d`),
  later upgraded to the official repository master (`API256019`), and built
  Vulkan-only as a static Release through `scripts/fetch_diligent.ps1`. The SDK
  stays in the gitignored `dependencies/DiligentEngine`; new native modules use
  C++17 and the layout needs `volk.lib`, one `glslang.lib` and `/std:c++17`.
- Device/swapchain/present owner: `native/NativeRenderer.*` owns one Diligent
  device/context/swap chain for one native window handle, with a Diligent-free C
  ABI, clear-and-present, resize and shutdown. Converted GLB positions/indices
  upload to immutable buffers and read back identical; overlapping triangles
  render through a depth-tested pipeline (D32) with the nearer one kept; a
  textured quad samples a base-colour texture through a constant-buffer MVP.
  Verified by `scripts/run_native_renderer_smoke.ps1` (15 checks).
- In-game linkage, headless handoff and camera seam: the game links the native
  renderer + Diligent on Windows when the SDK is present (C++17 native file, v145
  toolset, ATL by full path, `/GL` disabled in the SDK). A gated
  `RED2_NATIVE_RENDERER` probe first reproduced
  `VK_ERROR_NATIVE_WINDOW_IN_USE_KHR` (PsyCross owned the window's swap chain);
  PsyCross can now create the window headless (`PsyX_SetHeadless`,
  `PsyXVkConfig.headless`), and a generic post-frame handler
  (`PsyX_SetPostFrameHandler`) lets the game feed its camera (view from
  `inv_camera_matrix`, same convention as the modern-mesh path) to
  `NativeRenderer_SetCamera`/`NativeRenderer_RenderScene`, drawing a depth-tested,
  textured scene every frame. `Release_dev_native` builds like `Release_dev`
  (with `_native` suffix) and launches with `RED2_NATIVE_RENDERER=1 -nointro
  -nofmv`; the default path is unchanged.
- Converted content in the native scene (block 4):
  `NativeRenderer_LoadContent`/`ClearContent`/`HasContent`/`GetContentStats` load
  a converted GLB through `utils/GltfLoader` into immutable buffers/texture (node
  transform as model matrix, base-colour factor/texture, unlit) and draw it in
  place of the placeholder; `RenderSceneToRGBA` runs the same draw offscreen for
  pixel checks; `RED2_NATIVE_CONTENT` loads a converted GLB in-game and a failed
  load keeps the placeholder without aborting. The game releases the Diligent
  device before `PsyX_Shutdown`, forwards window resizes and logs scene
  failure/recovery transitions. Verified by the smoke suite (38 checks, 0
  failures) and an in-game probe: content loaded and drawn with the game camera,
  resize applied (`Resizing the swap chain to 1008x561`) and a clean exit (0),
  no scene-failure or crash lines.
- Source MODEL decoder foundation (block 1, oracle-approved attempt 1 with F1-F8
  remediation): `utils/SourceModelDecoder.*` is a pure, asset-free decoder of the
  reconstructed MODEL poly block from an explicit `[begin,end)` span (MODEL has no
  byte span; loader span capture is Block 2). Allowlisted ptypes 11 (`PL_POLYFT4`,
  16 B, no RGB), 20/21 (`POLYFT3`/`FT4`, 20 B) and 22/23 (`POLYGT3`/`GT4`,
  24 B), stride `PolySizes[ptype & 0x1f]`, world quad vertex order
  `(v0,v1,v3,v2)` with UVs `(uv0,uv1,uv3,uv2)`, and declared 20/22 decoded as
  triangles; the runtime `ConvertPolygonTypes` mutation (20->21/22->23) is
  deliberately not relied on. Total span/index/normal validation yields named
  errors with zero output; unsupported ptypes fail the model (the runtime skips
  them, and skip-vs-fail is a Block 2 real-data decision); instanced models are
  rejected; per-submesh `hasColors`/`hasNormals` use aligned zero placeholders;
  face colours are the raw stored colours, not legacy shading. No
  GPU/GLB/texture/loader/renderer work. Covered by the new
  `SourceModelDecoderTests` (336/0, synthetic byte-exact fixtures), registered in
  `scripts/run_inspector_tests.ps1` and `tests/README.md`; the full runner passes
  (catalog 258, audio contract 221, others as before).
- Not yet done: the native scene accepts converted fixtures, not the game's
  world/materials/lighting and player, so E03 has no playable original-world
  native slice; the next seam is original-world content. Debug configs are not
  wired (only Release Diligent libraries exist). Headless native mode does not run
  the F11 ImGui overlay; a possible PAL double-aspect in the camera seam needs a
  PAL probe. Deferred to Block 2: loader span capture (`models.c:131-139`),
  `source_object_index` identity (the E02 placement record lacks it), CPU
  TIM/page/CLUT decode, a per-instance draw API and a camera/unit/winding fixture
  (the bridge mixes rotation/4096 with raw translation against near/far 0.02/300).

## Engine migration E04 (in progress)

- Project audio contract (E04-A): `Game/C/audiocontract.{h,c}` is a pure-data
  boundary between gameplay and the future audio service: stable event ids
  (kind/bank/voice-or-sequence packing, zero reserved), play flags, request
  validation with named diagnostics (NaN-safe volume/pitch/distance), a 64-slot
  voice table with 16-bit per-slot generations (stale handles rejected, zero
  wraparound documented) and inclusive loop-metadata validation matching the FMOD
  `setLoopPoints` semantics. Fix-5 reports 221/0. Gameplay must not call FMOD
  directly.
- Local ignored FMOD SDK metadata identifies 2.03.15 (`FMOD_VERSION` 0x00020315
  and DLL resource version 2.3.15). Do not infer 2.03.21 from hexadecimal suffix
  `0x15`; no independent new version verification is claimed. See
  [the FMOD integration rule](rules/fmod-integration.md).
- Gate 4B attempt 2 **approved as a bounded SDK-isolated service only**, not
  consumer migration or audible correctness. It implements true per-channel
  2D/3D, concurrent samples, persistent service generations, checked listener
  updates and an x64 SDK guard; inclusive endpoints are unchanged, equal
  endpoints are rejected, RESTART/stream loops are unsupported and Doppler is
  disabled pending unit validation. Final suites: catalog 258, audio contract 221
  and remaining 39/38/28/28/64/25/32/107; AudioServiceSmoke 57/0 (exit 0), the SDK
  fetch check exits 0 and nine FMOD-version fixtures pass 9/0. A reset-on-restart
  mutant fails 13/57 checks as expected; live-service source hash unchanged.
  Parent fresh x64 `Release_dev`/`Release_dev_native` SDK-present builds and an
  SDK-absent x64 `Release_dev` all exit 0; Win32 was not built. Copied DLL SHA256
  matches the SDK DLL:
  `F58048FB39BD10B68E3701A553D632BDFC8BE3C644E636E715BCA8E7049A6797`.
  Release-failure stream storage lifetime, captured output, device loss and stream
  stress are unverified; earlier smoke 39/0 and contract 216/0 are historical.
- Game-target wiring (2026-10-09): premake links `fmod_vc.lib`, defines
  `RED2_AUDIO_FMOD` and copies `fmod.dll` next to the built executables when the
  SDK is present; without it the service unit is excluded, so a default build is
  unchanged. `Release_dev` and `Release_dev_native` build and link with the
  service compiled in; no game system calls the service yet.
- Menu-bank PCM decode + registration seam (P0, oracle-approved attempt 1 with
  F1-F4 remediation): `Game/C/sounddecode.{h,c}` is an SDK-free PSX SPU ADPCM
  (VAG) decoder: 16 B -> 28 frames, coefficient recursion with cross-block
  predictor carry, filter-nibble validation, inclusive block-aligned loop frames,
  bounded validation with zero partial output, deterministic. Loop start uses the
  PSX block-start convention, deliberately diverging from the fork reference's
  FIXME-marked `(block+1)*28` (documented; needs an explicit acceptance decision
  when looping samples migrate, outside the FESound slice). `Game/C/sound.c` glue
  under `#ifdef RED2_AUDIO_FMOD` decodes samples 0-3 at the `SBK_ID_MENU` load
  into `SOUND_BANK_SFX` and registers `SFX(1,0..3)` via
  `AudioService_RegisterSoundMemory` (streaming=0), unregisters on bank-1 reload,
  registers nothing when base!=0 and is a no-op when the service is inactive, so
  the seam is inert until A1 initializes the service. Known caveat: the base==0
  window also matches the level's first bank-1 load; A1 adds strict menu
  discrimination (e.g. `gInFrontend`) plus seam-level evidence. A read-only local
  probe of `data/DRIVER2/SOUND/VOICES2.BLK`: menu lump 4 samples x 560 frames
  @4000 Hz, no loop; the level lump decodes cleanly. Evidence:
  `scripts/run_audio_service_smoke.ps1` (pure decode phase without FMOD headers,
  plus the service phase) 20 + 87 checks, 0 failures.
- Next: A1 (FESound routing with a runtime SPU_ONLY/FMOD_ONLY switch defaulting to
  SPU, master gain helper `(10000+gMasterVolume)/10000`, pitch `gamePitch/4096`
  clamped to `16383/4096`, lazy service init, per-frame Update in `VsyncProc`,
  shutdown at exit, `gInFrontend` menu gating, in-game log evidence) and E03 Block
  2. E05/E06 remain pending.

## Project and current services

REDRIVER2 is reconstructed Driver 2 game logic, not an emulator. Physics,
collision, AI, missions and replays live primarily under `src_rebuild/Game/`,
but still call PsyCross math/GTE, clocks, pad, SPU/CD/streaming and GPU services.
The PSX branch still shares `FilterFrameTime`/`VSync`; on desktop `ProjectClock`
now separates the fixed simulation tick from presentation, which draws every
frame and interpolates car/camera translation and orientation between ticks.
Replay repeats and counters remain tick indexed. Local effect interpolation now
covers particles/birds/hubcaps/rain; complete pedestrian/wheel-spin/native VFX
coverage remains E05 work. E01 preserves regional semantics before changing clock
owners.

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
  blend/stencil/depth, white primitives and queued VRAM copies (`-vkpsxtest`
  readback/CLUT/offscreen/export/blend fixtures); the modern mesh API/importer/
  PBR/shadow composite shares the PSX projection and depth, with optional legacy
  lighting receptivity and PGXP on that composite. World/overlay boundaries split
  gameplay composition from flare/fade/UI tails, and split-screen keeps its
  limited path; this does not prove native mode/category coverage under the new
  architecture.
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
Subsequent E00-E04 and complementary fixes above record their actual isolated
builds and bounded tests. The latest effect/replay evidence retains the unresolved
route fault; a matching dump does not accept a migration stage. Older generated
projects/executables may belong to the preserved rework branch.

Existing commands include `git diff --check`, Premake/generated builds,
`-vkpsxtest`, `-vkfixture`, backend flags and the inspector test script; they
prove their stated baseline scopes only, and export tests need a fresh directory.
