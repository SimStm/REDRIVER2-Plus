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
post-frame-hook and pad-backend-disable changes are committed/pushed as `99950a7`. Subsequent
E02/E03/E04 implementation and documentation changes remain uncommitted.

The former dirty native Vulkan work is committed/pushed separately at
`e68b6ea6b1e30543af77d6244c109f7993ff65cf` on
[`feat/vulkan-native-rework`](https://github.com/SimStm/REDRIVER2-Plus/tree/feat/vulkan-native-rework)
(source/tests/tooling/knowledge/archive moves; fork gitlink
`ae83dd229f2c4a287575fb4cb853006996d9eef8`; R00-R02 documented, R03 incomplete, R04 unstarted;
those native APIs are not part of this branch; ignored captures and private helpers/builds
stay local).

Implemented here: [engine-migration planning](roadmap/planned/engine-migration.md), required
E00-E07 contracts, optional final E08, five issue briefs, validation policy and agent/catalog
guidance; the planning change implements no engine behavior. Partial implementation is
recorded in E00-E04 below; source still requires PsyCross, and Diligent/DiligentFX, FMOD Core,
complete converted packages and eventual removal remain migration work. Earlier hybrid
graphics/timing plans are superseded here. PopcornFX is optional E08, only on explicit request
after full-game acceptance and PsyCross removal; existing effects remain mandatory native
consumers in E05. [Backend discussion](discussions/engine-migration/index.md) records the
adopted interim legacy OpenGL for gameplay/regressions and deferring PsyCross Vulkan
performance work; no default, removal or stage status changed, and E03 still selects Diligent
Vulkan independently of the retiring legacy renderer.

## Engine migration E00 (in progress)

[E00 dependency inventory](engine-migration/e00/dependency-inventory.md) and
[baseline scenarios](engine-migration/e00/baseline-scenarios.md) record service contracts,
regional/replay/RNG facts and remaining routes. The symbol scan found 3618 declared / 400
used. Per-tick comparisons had 0 mismatches for NTSC attract (714), PAL (595), NTSC-vs-PAL
(597) and mission (726); isolated NTSC/PAL builds ran Chicago. Driving/collision/police,
pause/loading, region transition and re-record remain pending, so E00 is not formally
accepted.

## Engine migration E01 (in progress)

- `ProjectClock` owns 30 Hz NTSC/25 Hz PAL desktop ticks, bounded four-tick catch-up,
  dropped-tick diagnostics and presentation-rate-independent drawing; Replay V2 carries
  region/rate/period/checksum (legacy V1 still loads), with verified mismatch diagnostics and
  NTSC/PAL round trips and matching authoritative replay-state across baseline and
  vsync/uncapped schedules; full mission re-record/replay reproduction stays open.
- `VisualSnapshot` and local interpolation cover car/camera poses without writing gameplay
  state (exact regional phase retained); `InputService` owns desktop SDL polling/mapping and
  `pad.c` uses the project-owned pad state/rumble contract, with the fork's competing pad
  backend disabled. Motion evidence:
  [E01 presentation fix](changes/2026-10-08/e01-presentation-motion-fix/index.md).
- Tick-owned effects cover emissions, pool motion/lifetime, birds/rain/hubcaps, lights,
  mission title/input unlock, bomb flash and pause messages; strict OpenGL ATTRACT matched
  1378 ticks at NTSC 30/360 and PAL 25/360 FPS and rainy ATTRACT.401 matched 2289, proving
  schedule independence only, not replay trajectory acceptance (the wall-collision route
  remains user-deferred).
  [Scope/evidence](changes/2026-10-09/effect-timing-replay-regression/index.md).
- E01 remains in progress: wheel-spin/pedestrian presentation, full mission re-record
  reproduction and complete presentation coverage remain open.

## Engine migration E02 (in progress)

- Reproducible conversion: exported texture manifest entries carry a canonical 64-bit FNV-1a
  `contentHash` over the converted pixels (`utils/HdTextureOverrides.cpp`), so identical
  conversions produce equal entries. `Game/C/assetpackage.*` is a pure-data JSON package
  manifest (`schemaVersion`, `id`, `entries[]` with
  `id`/`type`/`file`/`contentHash`/`dependencies[]`) with named failure diagnostics;
  `Game/C/conversioncache.*` keys a conversion by source hash/converter/schema/options/active
  mods with a canonical key, malformed-key diagnostics, staleness reasons and deterministic
  paths; `Game/C/assetcatalog.*` provides model/texture/car identity and LOD anchors.
- Native mesh record: `utils/GltfWriter.*` writes a bounded GLB
  (positions/normals/UVs/indices/material/node matrix) and `utils/GltfLoader.*` reads it back
  retaining the node transform, so geometry/UV/transform/flags agree and identical input is
  byte-identical. `utils/AssetConverter.*` turns a mesh record into a cached native GLB keyed
  by source/converter/schema/options/mods, reuses a fresh GLB, rebuilds a stale one, verifies
  by read-back, publishes via temp file + rename and returns the content hash;
  `AssetPackage_SetId`/`AssetPackage_AddEntry` turn a converted resource's hash into a
  manifest entry with dependencies (any order; dangling deps rejected).
- Sound metadata record: `Game/C/soundasset.*` carries sample rate, channels, bit depth, frame
  count, loop points, pitch reference, source bank/voice ids and music bank/program/sequence,
  with validation and canonical serialization.
- Car serializer wiring (2026-10-09): `utils/CarMeshSerializer.*` derives a deterministic
  conversion key (raw position/index/identity bytes, converter `car-mesh`, schema, options,
  mods digest) and publishes through `AssetConverter_ConvertMesh`;
  `Cars_ConvertInspectorModel` feeds the resident car model (`NewCarModel` +
  `gTempCarVertDump`) from a new GLB button beside the OBJ export. Covered by
  `CarMeshSerializerTests` (64) and the new suite set (AssetPackageTests 39,
  ConversionCacheTests 38, GltfRoundTripTests 28, AssetConverterTests 28, AssetPipelineTests
  25, SoundAssetTests 32); earlier InspectorExportTests 107 / AssetCatalogTests 145 are
  superseded below.
- Chicago inventory (2026-10-09): Gate 4A attempt 2 approved a scoped diagnostic; the final
  fresh report scans all four eligible Chicago day barrels (6740 unique placements, 227
  straddlers, no overlaps or missing model refs, 114 unknown material classifications; 0
  errors/480 warnings, 64 displayed/truncated). This is a source-union diagnostic, not
  conversion or world acceptance. Corrected traversal includes terminal objects marked
  `0x8000` (the initial 6078-count report is superseded); the fresh report was copied to
  `src_rebuild/build_e04_gate4/logs/chicago_inventory_final.txt`. Other cities, variants and
  dynamic/streamed categories remain open; the conversion cache is not consumed by native
  loading. See [inventory evidence](engine-migration/e02/chicago-inventory.md).

## Engine migration E03 (in progress)

- Blocks 1-4 (prior narrative; full detail in CHANGELOG): DiligentEngine `v2.5.6`
  (DiligentCore `b036337d`) then official master (`API256019`), built Vulkan-only static
  Release via `scripts/fetch_diligent.ps1` (gitignored `dependencies/DiligentEngine`; C++17,
  `volk.lib`, one `glslang.lib`, `/std:c++17`). `native/NativeRenderer.*` owns the
  device/context/swap chain (Diligent-free C ABI) and draws converted GLB geometry (immutable
  buffers, depth-tested D32) and a base-colour texture (constant-buffer MVP, smoke 15). The
  game links it on Windows behind `RED2_NATIVE_RENDERER` (C++17 native file, v145 toolset, ATL
  by full path, `/GL` off in the SDK); PsyCross is headless (`PsyX_SetHeadless`,
  `PsyXVkConfig.headless`) and a post-frame handler (`PsyX_SetPostFrameHandler`) feeds the
  game camera (view from `inv_camera_matrix`) to
  `NativeRenderer_SetCamera`/`NativeRenderer_RenderScene`. `Release_dev_native` builds
  `RED2_NATIVE_RENDERER=1 -nointro -nofmv`; `NativeRenderer_LoadContent`/
  `ClearContent`/`HasContent`/`GetContentStats` load a converted GLB via `utils/GltfLoader`
  (node transform, base-colour, unlit), `RenderSceneToRGBA` draws it offscreen, and
  `RED2_NATIVE_CONTENT` loads it in-game (a failed load keeps the placeholder). The game
  releases the device before `PsyX_Shutdown`, forwards resizes and logs failure/recovery; the
  gated probe first hit `VK_ERROR_NATIVE_WINDOW_IN_USE_KHR`, and the in-game probe drew
  content, resized (`Resizing the swap chain to 1008x561`) and exited 0 (smoke 38).
- Source MODEL decoder (block 1, oracle-approved attempt 1 with F1-F8):
  `utils/SourceModelDecoder.*` is a pure, asset-free decoder of the reconstructed MODEL poly
  block from an explicit `[begin,end)` span. Allowlist `{10,11,20,21,22,23}`: 10/20/22
  triangles, 11/21/23 quads; 10 is a 16 B `PL_POLYFT4` textured triangle (v0/v1/v2 +
  uv0/uv1/uv2, no RGB; layout verified against `mdl.h`/`ConvertPolygonTypes`/ODT), 20/21 a 20
  B `POLYFT3`/`FT4` and 22/23 a 24 B `POLYGT3`/`GT4`; stride `PolySizes[ptype & 0x1f]`, world
  quad order `(v0,v1,v3,v2)` with UVs `(uv0,uv1,uv3,uv2)`. Validation yields named errors with
  zero output; instanced models rejected; per-submesh `hasColors`/`hasNormals` use aligned
  zero placeholders; face colours are raw stored. `SourceModelDecoderTests` 1186 checks;
  registered in `scripts/run_inspector_tests.ps1`/`tests/README.md`.
- Skip-vs-fail policy (block 2.1, gate-driven): runtime `ConvertPolygonTypes` (draw.c:721-730)
  converts every even declared ptype except 8 (10->11, 20->21, 22->23), so the building loop
  draws `{11,21,23}` and the declared types it can draw are `{10,11,20,21,22,23}`. `Strict`
  (default, hard error/zero output) and `SkipUnsupported` (advance by `PolySizes`, count;
  bounds strict; zero-stride is `UNSKIPPABLE_PTYPE`) apply to building-class models only;
  tile/sprite callers stay `Strict` (Chicago tile/sprite data contains only ptype 21).
- Loader span capture and source identity (block 2a): loader-owned `model_byte_sizes[]`
  captured at both size-live sites (`models.c` `ProcessMDSLump`, `spool.c`
  `init_spooled_models`) before relocation, exposed as catalog `ModelRecord.spanBytes` via
  `AssetCatalog_RegisterModelWithSpan`/`GetModelSpan`, cleared on slot invalidation (no stored
  pointers); `AssetCatalogPlacement.sourceObjectIndex` set from the walker's 14-bit index
  (dedup key unchanged).
- Real-model ptype probe (block 2a): `Game/C/modelptypeprobe.{h,c}` is a pure, asset-free
  declared-ptype counter (real `PolySizes` passed in; bounds; zero-stride reported and never
  advanced; no draw-path class guessed); `ModelPtypeProbeTests` 68 checks. The `-modelptypes`
  flag (accepted in `main.c` to avoid the modal helper) writes `model_ptypes_level_<N>.txt`
  via `InspectorExport_WriteText` with a per-level accumulator reset at
  `AssetCatalogGame_BeginLevel`. Real Chicago histogram (Release_dev,
  `-opengl -nointro -nofmv -mission 1 -modelptypes`): 516 slots, 5538 records; building-class
  379 slots (ptypes 9x2, 10x30, 11x350, 20x11, 21x28); tile-class 132 and sprite-class 5 slots
  contain only ptype 21; pre-policy outside-allowlist 118 (ptype 10 x116 + 9 x2, all
  building); after the final allowlist the re-run reports 2 outside (ptype 9 only); 0
  stride-zero, 0 invalid. `AssetCatalogTests` 278 checks.
- Camera/unit/winding fixture (block 2.5): pure fixtures pin yang == `RotMatrixY(yang*64)`
  (4096-unit turn), view = `R_cam*(R_obj*p + pos - cam)`, the NDC signed-area winding of the
  decoder's `(v0,v1,v3,v2)`->`(0,1,2)+(0,2,3)` emission, and the raw-unit policy (0.02/300 is
  not a valid raw-unit projection). Scene pipelines changed from `CULL_MODE_NONE` to
  `CULL_MODE_BACK` with `FrontCounterClockwise=True` (Vulkan negative-viewport emulation),
  asserted through the real offscreen pipeline; near/far and the game camera bridge remain
  deferred to B2.6. Native renderer smoke 46 checks (38 preserved + 8 new).
- Gate: attempt 1 found one High must-fix (ptype 10 is drawn by the runtime, not skipped) plus
  probe counting/reset items; remediation applied and verified; attempt 2 verdict COMMIT-ready
  on code (remaining item was this documentation). `Release_dev` and `Release_dev_native`
  builds exit 0 with the batch.
- ODT spike (user-authorized; temp-only, no repo integration): OpenDriver2Tools master @
  `89009f7f4f48f37a886b376292432cdab4a136` (2025-10-28) built headless (Win32), full Chicago
  export (498 model OBJs, 159 texture pages, 135 region OBJs) plus one reference GLB
  (`CHICAGO_HEAD.glb`) via a stdlib converter; adaptation surface and ODT divergences (no
  index dedup, normal-index bug, gouraud colours undecoded, `TEXINF` subrect not applied,
  transparency key) recorded in the discussion. Informs B2.4.
- CPU texture decode (block 2.4): `utils/TextureDecode.{h,c}` is a pure decoder for indexed
  4/8-bit pages + CLUT bytes and 16-bit direct pages into RGBA8 (explicit row stride; tight
  loader pages and the 1024-halfword VRAM stride covered; BGR555 `E5TO8`, `0000h`
  transparent, STP->128 with unknown blend context; texture window not applied). Bounded
  validation, named errors, zero partial output; `TextureDecodeTests` 136 checks. No GPU or
  game integration.
- Not yet done: the native scene accepts converted fixtures, not the game's
  world/materials/lighting and player, so E03 has no playable original-world native slice.
  Debug configs are not wired (only Release Diligent libraries exist). Headless native mode
  does not run the F11 ImGui overlay; a possible PAL double-aspect in the camera seam needs a
  PAL probe. Next (refined plan): one real building/tile -> `SourceMesh` -> GLB via
  `GltfWriter`/`AssetConverter` (item 3), then the per-instance draw API and converted
  region with the game camera (item 4).

## Engine migration E04 (in progress)

- Audio contract (E04-A): `Game/C/audiocontract.{h,c}` is a pure-data gameplay-to-service
  boundary (stable event ids with zero reserved, play flags, named request diagnostics
  NaN-safe for volume/pitch/distance, a 64-slot voice table with 16-bit per-slot generations
  rejecting stale handles and documenting zero wraparound, inclusive loop metadata matching
  FMOD `setLoopPoints`); Fix-5 reports 221/0 and gameplay must not call FMOD directly. The
  ignored local FMOD SDK metadata identifies 2.03.15 (`FMOD_VERSION` 0x00020315, DLL resource
  2.3.15); do not infer 2.03.21 from suffix `0x15`; no independent new-version verification is
  claimed. See [the FMOD integration rule](rules/fmod-integration.md).
- Gate 4B attempt 2 **approved as a bounded SDK-isolated service only**, not consumer
  migration or audible correctness: true per-channel 2D/3D, concurrent samples, persistent
  generations, checked listener updates and an x64 SDK guard; inclusive endpoints unchanged,
  equal endpoints rejected, RESTART/stream loops unsupported, Doppler disabled pending unit
  validation. Final suites: catalog 258, audio contract 221 and remaining
  39/38/28/28/64/25/32/107; AudioServiceSmoke 57/0 (exit 0), SDK fetch exit 0, nine
  FMOD-version fixtures 9/0. A reset-on-restart mutant fails 13/57 as expected; live-service
  source hash unchanged. Parent fresh x64 `Release_dev`/`Release_dev_native` SDK-present and
  an SDK-absent x64 `Release_dev` all exit 0 (Win32 not built); copied DLL SHA256 matches the
  SDK DLL `F58048FB39BD10B68E3701A553D632BDFC8BE3C644E636E715BCA8E7049A6797`. Release-failure
  stream storage lifetime, captured output, device loss and stream stress are unverified;
  earlier 39/0 and 216/0 are historical.
- Game-target wiring (2026-10-09): premake links `fmod_vc.lib`, defines `RED2_AUDIO_FMOD` and
  copies `fmod.dll` beside the executables when the SDK is present (otherwise the service unit
  is excluded, so a default build is unchanged); `Release_dev`/`Release_dev_native` build and
  link with it, and no game system calls the service yet.
- Menu-bank PCM decode + registration seam (P0, oracle-approved attempt 1 with F1-F4):
  `Game/C/sounddecode.{h,c}` is an SDK-free PSX SPU ADPCM (VAG) decoder (16 B -> 28 frames,
  cross-block predictor carry, filter-nibble validation, inclusive block-aligned loop frames,
  zero partial output, deterministic); loop start uses the PSX block-start convention,
  deliberately diverging from the fork reference's FIXME-marked `(block+1)*28` (needs an
  explicit acceptance decision when looping samples migrate, outside the FESound slice).
  `Game/C/sound.c` glue under `#ifdef RED2_AUDIO_FMOD` decodes samples 0-3 at the
  `SBK_ID_MENU` load into `SOUND_BANK_SFX`, registers `SFX(1,0..3)` via
  `AudioService_RegisterSoundMemory` (streaming=0), unregisters on bank-1 reload, registers
  nothing when base!=0 and is inert when the service is inactive; a read-only probe of
  `data/DRIVER2/SOUND/VOICES2.BLK` shows a 4-sample x 560-frame @4000 Hz menu lump with no
  loop and a clean level lump.
- FESound FMOD routing + service lifecycle (E04 A1, 2026-10-09):
  `Game/C/audiofmod_adapter.{h,c}` is an SDK-free helper (no FMOD includes): a consumer-mode
  enum parsed once from `RED2_FMOD_AUDIO` (`"1"` => FMOD_ONLY, default SPU_ONLY), master gain
  `clamp((10000+gMasterVolume)/10000, 0, 1)`, pitch `gamePitch/4096` clamped to `16383/4096`,
  and the `SFX(1,sample)` id builder. In FMOD_ONLY mode `sound.c` routes `FESound` to
  `AudioService_Play` as a 2D one-shot (one log line with sample/handle/VoiceCount) and skips
  the SPU body, so `channels[]` is untouched (no double playback); a missing event or inactive
  service logs once and stays silent, never falling back to SPU. `LoadSoundBankDynamic`
  threads the lump identity so only the fresh `SBK_ID_MENU` load registers `SFX(1,0..3)` and
  other bank-1 loads unregister them (the append path's latent drop is documented).
  `redriver2_psxpc.cpp` initializes the service before `redriver2_main` (init failure stays
  SPU) and shuts it down after; `VsyncProc` calls `AudioService_Update` once per rendered
  frame; all calls stay under `#ifdef RED2_AUDIO_FMOD` and non-FMOD builds are unchanged.
  Smoke `scripts/run_audio_service_smoke.ps1` 31 (pure) + 98 (service) checks/0; `cl /Zs` of
  `sound.c`/`redriver2_psxpc.cpp` exit 0 with and without the define. In-game probe
  (Release_dev, `-nointro -nofmv`, `RED2_FMOD_AUDIO=1`) initialised the service alongside
  OpenAL and routed 41 FESound calls as distinct FMOD handles (samples 0/2/3, voices 1-2, 0
  warnings; switch off inactive). Gate-accepted informational items: linear per-voice master
  gain (SPU ~1.4x pre-saturation curve) and the dormant lower pitch clamp. No
  audible-fidelity/tick-coverage/master-bus claim.
- Next: E03 Block 2 items 3/4 (real model -> GLB; instance draw) and E04 remaining consumer slices; E05/E06 remain pending.

## Project and current services

REDRIVER2 is reconstructed Driver 2 game logic, not an emulator. Physics, collision, AI,
missions and replays live under `src_rebuild/Game/` but still call PsyCross math/GTE, clocks,
pad, SPU/CD/streaming and GPU services. The PSX branch still shares `FilterFrameTime`/`VSync`;
on desktop `ProjectClock` separates the fixed simulation tick from presentation, which draws
every frame and interpolates car/camera translation/orientation between ticks. Replay repeats
and counters remain tick indexed. Local effect interpolation covers
particles/birds/hubcaps/rain; complete pedestrian/wheel-spin/native VFX coverage remains E05
work. E01 preserves regional semantics before changing clock owners.

PsyCross remains a separate submodule from `git@github.com:SimStm/PsyCross.git`; do not
substitute upstream or infer removal from a roadmap. Existing C++11 and
Windows/Linux/web/Android/PSX guards remain. SDK/compiler/target compatibility for the new
stack is pending E00, not established by cross-platform advertising.

## Existing renderer baseline

Desktop defaults to Vulkan through PsyCross, with separately selected OpenGL; this is the
existing PSX primitive/hybrid architecture, not Diligent.
[Product behavior](product/vulkan-game-renderer.md) and the source govern support.

- Vulkan PSX shaders implement CLUT/texture windows/dither/filtering, VRAM mirror,
  blend/stencil/depth, white primitives and queued VRAM copies (`-vkpsxtest` fixtures); the
  modern mesh API/importer/PBR/shadow composite shares the PSX projection and depth, with
  optional legacy lighting receptivity and PGXP. World/overlay boundaries split gameplay
  composition from flare/fade/UI tails, and split-screen keeps its limited path; this does not
  prove native mode/category coverage. Minimize/restore and fence-deadlock fixes are in
  [minimize](changes/2026-09-21/vulkan-minimize-swapchain-crash/index.md) and
  [fence](changes/2026-09-21/vulkan-frame-fence-deadlock/index.md) evidence.
- Shadow-volume centers snap to texels while following the vehicle (historical aligned
  GL/Vulkan shadow-receive IoU 0.985); legacy city geometry still lacks the planned caster
  path (only imported modern meshes cast). `PSYX_PERF_LOG` records submission CPU time (~6.4
  ms at 1280x720 historically); GPU timestamps, peak memory and broad platform measurements
  remain pending. `-vkfixture` renders modern GLB fixtures only and accepts nothing about the
  new engine.
- UI parity/blend/offscreen details remain in [UI product](product/vulkan-ui-image-parity.md)
  and [lighting product](product/legacy-lighting-receptivity.md). New migration uses targeted
  native contracts, not universal pixel equality with this old pipeline.

## Textures, mods and inspector

- Ordered JSON mods and `mods/enabled.json` retain deterministic precedence; texture keys are
  name/page/index with optional palette identity, model references stay metadata, and override
  alpha/cutout/blend semantics mirror the active compatibility policy (do not silently change
  during conversion).
- `AssetCatalog.*` separates stable resources from transient instances with provenance and
  model/texture/LOD links ([catalog product](product/asset-catalog-identity.md)).
- Inspector supports face/material/component/logical-object scopes, source/override previews,
  TXT and single/batch PNG exports with deduplication, progress/retry and diagnostics
  ([selection](product/inspector-selection-coverage.md),
  [batch export](product/object-texture-batch-export.md)).
- Versioned offline visual/audio packages and OpenDriver2Tools-based conversion are not
  implemented; the bounded `GltfLoader.*`/modern fixtures are reuse candidates, not the new
  city pipeline.

## Gameplay and developer tooling

- Resident procedural playground (flat surface/collidable boxes, disabled donor
  traffic/missions, reset/frontend return, `-playground`, Take a Ride) is recorded in
  [playground product](product/playable-testing-playground.md) P1-P5. Debug start settings
  preserve mission/city/vehicle/position/heading/player/chase and generate reproducible
  command lines (`scripts/run_debug_start.ps1`).
- F11 Developer Graphics Panel uses vendored ImGui1.91.9b (Windows/Linux) and
  `developer_graphics.ini`, independently from `config.ini`. Schema9 display controls include
  a 15..360 presentation FPS slider (defaults 30 NTSC/25 PAL), NTSC/PAL selection and VSync
  beside resolution; FPS/VSync apply live, region applies after restart, and the same binary
  selects the 30/25 Hz clock, screen/aspect/audio and replay metadata at startup.
  Window/fullscreen selection keeps its confirmation/revert (fullscreen untested); the input
  editor captures/cancels bindings and persists `developer_input.ini`.
  [Frame controls](changes/2026-10-08/display-frame-controls/index.md) and
  [runtime settings](product/runtime-settings-gui.md) hold the detail.
- Content/language/loading/dynamic-light/widescreen controls describe reload requirements;
  `DeveloperSettingsFile_WriteKeys` preserves unknown keys/comments with transactional
  temp/backup files.
- FMOD/PopcornFX, RmlUi, supplementary physics and in-game authoring editors are not
  implemented; ImGui/input/window dependencies must migrate before E07.

## Knowledge and validation state

English documentation separates planned intent, product behavior, completion records and dated
change evidence. [Migration validation](engine-migration/validation.md) requires deterministic
mechanics, conversion inventories and targeted native resources/pixels/audio, with actual
budgets and platform scope.

The planning-only validation is recorded in
[its change record](changes/2026-10-08/engine-migration-plan/index.md). Subsequent E00-E04 and
complementary fixes above record their actual isolated builds and bounded tests. The latest
effect/replay evidence retains the unresolved route fault; a matching dump does not accept a
migration stage. Older generated projects/executables may belong to the preserved rework
branch.

Existing commands include `git diff --check`, Premake/generated builds, `-vkpsxtest`,
`-vkfixture`, backend flags and the inspector test script; they prove their stated baseline
scopes only, and export tests need a fresh directory.
