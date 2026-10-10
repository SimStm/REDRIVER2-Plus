---
type: Status
title: Project status
description: Current branch implementation, preserved rework and adopted engine migration.
tags: [okf, status]
---

# Current status

> Updated: 2026-10-10.
> Source of truth for implemented surfaces on the current branch; verify source/Git.
> [RECENT_CONTEXT](RECENT_CONTEXT.md) records the next task and session handoff.

## Branch and adopted plan

`feat/new-engine` checkpoint `7fdce013` is committed/pushed, including the bounded world-parity,
quad-topology and tick-capture work. PsyCross's previous headless GPU bridge is `99950a7`.
The 2026-10-10 native performance/window-only follow-up is documented below.
No migration-stage acceptance follows from publication. The tracked Serena project configuration
selects the C++ language server; generated state and proprietary assets/SDKs are not included.

The former dirty native Vulkan work is committed/pushed separately at `e68b6ea6b1e30543af77d6244c109f7993ff65cf`
on [`feat/vulkan-native-rework`](https://github.com/SimStm/REDRIVER2-Plus/tree/feat/vulkan-native-rework)
(source/tests/tooling/knowledge/archive moves; fork gitlink `ae83dd229f2c4a287575fb4cb853006996d9eef8`; R00-R02
documented, R03 incomplete, R04 unstarted; those native APIs are not part of this branch; ignored captures and
private helpers/builds stay local).

Implemented here: [engine-migration planning](roadmap/planned/engine-migration.md), required E00-E07 contracts,
optional final E08, five issue briefs, validation policy and agent/catalog guidance; the planning change
implements no engine behavior. Partial implementation is recorded in E00-E04 below; source still requires
PsyCross, and Diligent/DiligentFX, FMOD Core, complete converted packages and eventual removal remain migration
work. Earlier hybrid graphics/timing plans are superseded here. PopcornFX is optional E08, only on explicit
request after full-game acceptance and PsyCross removal; existing effects remain mandatory native consumers in
E05. [Backend discussion](discussions/engine-migration/index.md) records the adopted interim legacy OpenGL for
gameplay/regressions and deferring PsyCross Vulkan performance work; no default, removal or stage status changed,
and E03 still selects Diligent Vulkan independently of the retiring legacy renderer.

## Engine migration E00 (in progress)

[E00 dependency inventory](engine-migration/e00/dependency-inventory.md) and
[baseline scenarios](engine-migration/e00/baseline-scenarios.md) record service contracts, regional/replay/RNG
facts and remaining routes. The symbol scan found 3618 declared / 400 used. Per-tick comparisons had 0 mismatches
for NTSC attract (714), PAL (595), NTSC-vs-PAL (597) and mission (726); isolated NTSC/PAL builds ran Chicago.
Driving/collision/police, pause/loading, region transition and re-record remain pending, so E00 is not formally
accepted.

## Engine migration E01 (in progress)

- `ProjectClock` owns 30 Hz NTSC/25 Hz PAL desktop ticks, bounded four-tick catch-up, dropped-tick diagnostics and
  presentation-rate-independent drawing; Replay V2 carries region/rate/period/checksum (legacy V1 still loads),
  with verified mismatch diagnostics and NTSC/PAL round trips and matching authoritative replay-state across
  baseline and vsync/uncapped schedules; full mission re-record/replay reproduction stays open.
- `VisualSnapshot` and local interpolation cover car/camera poses without writing gameplay state (exact regional
  phase retained); `InputService` owns desktop SDL polling/mapping and `pad.c` uses the project-owned pad
  state/rumble contract, with the fork's competing pad backend disabled. Motion evidence:
  [E01 presentation fix](changes/2026-10-08/e01-presentation-motion-fix/index.md).
- Tick-owned effects cover emissions, pool motion/lifetime, birds/rain/hubcaps, lights, mission title/input
  unlock, bomb flash and pause messages; strict OpenGL ATTRACT matched 1378 ticks at NTSC 30/360 and PAL 25/360
  FPS and rainy ATTRACT.401 matched 2289, proving schedule independence only, not replay trajectory acceptance
  (the wall-collision route remains user-deferred).
  [Scope/evidence](changes/2026-10-09/effect-timing-replay-regression/index.md).
- E01 remains in progress: wheel-spin/pedestrian presentation, full mission re-record reproduction and complete
  presentation coverage remain open.

## Engine migration E02 (in progress)

- Reproducible conversion: exported texture manifest entries carry a canonical 64-bit FNV-1a `contentHash` over
  the converted pixels (`utils/HdTextureOverrides.cpp`), so identical conversions produce equal entries.
  `Game/C/assetpackage.*` is a pure-data JSON package manifest (`schemaVersion`, `id`, `entries[]` with
  `id`/`type`/`file`/`contentHash`/`dependencies[]`) with named failure diagnostics; `Game/C/conversioncache.*`
  keys a conversion by source hash/converter/schema/options/active mods with a canonical key, malformed-key
  diagnostics, staleness reasons and deterministic paths; `Game/C/assetcatalog.*` provides model/texture/car
  identity and LOD anchors.
- Native mesh record: `utils/GltfWriter.*` writes a bounded GLB (positions/normals/UVs/indices/material/node
  matrix) and `utils/GltfLoader.*` reads it back retaining the node transform, so geometry/UV/transform/flags
  agree and identical input is byte-identical. `utils/AssetConverter.*` turns a mesh record into a cached native
  GLB keyed by source/converter/schema/options/mods, reuses a fresh GLB, rebuilds a stale one, verifies by
  read-back, publishes via temp file + rename and returns the content hash;
  `AssetPackage_SetId`/`AssetPackage_AddEntry` turn a converted resource's hash into a manifest entry with
  dependencies (any order; dangling deps rejected).
- Sound metadata record: `Game/C/soundasset.*` carries sample rate, channels, bit depth, frame count, loop points,
  pitch reference, source bank/voice ids and music bank/program/sequence, with validation and canonical
  serialization.
- Car serializer wiring (2026-10-09): `utils/CarMeshSerializer.*` derives a deterministic conversion key (raw
  position/index/identity bytes, converter `car-mesh`, schema, options, mods digest) and publishes through
  `AssetConverter_ConvertMesh`; `Cars_ConvertInspectorModel` feeds the resident car model (`NewCarModel` +
  `gTempCarVertDump`) from a new GLB button beside the OBJ export. Covered by `CarMeshSerializerTests` (64) and
  the new suite set (AssetPackageTests 39, ConversionCacheTests 38, GltfRoundTripTests 28, AssetConverterTests 28,
  AssetPipelineTests 25, SoundAssetTests 32); earlier InspectorExportTests 107 / AssetCatalogTests 145 are
  superseded below.
- Chicago inventory (2026-10-09): Gate 4A attempt 2 approved a scoped diagnostic; the final fresh report scans all
  four eligible Chicago day barrels (6740 unique placements, 227 straddlers, no overlaps or missing model refs,
  114 unknown material classifications; 0 errors/480 warnings, 64 displayed/truncated). This is a source-union
  diagnostic, not conversion or world acceptance. Corrected traversal includes terminal objects marked `0x8000`
  (the initial 6078-count report is superseded); the fresh report was copied to
  `src_rebuild/build_e04_gate4/logs/chicago_inventory_final.txt`. Other cities, variants and dynamic/streamed
  categories remain open; the conversion cache is not consumed by native loading. See
  [inventory evidence](engine-migration/e02/chicago-inventory.md).

## Engine migration E03 (in progress; parity not accepted)

- Native performance follow-up (2026-10-10): dynamic constants use Map/Discard, window
  Present honors configured VSync, and conservative homogeneous AABB culling removes
  invisible instance draws. Opt-in `RED2_NATIVE_PERF=1` records CPU phases, draw/cull counts
  and tick/drop deltas. Mission 1 samples improved from 3.7-4.6 FPS to 41-60 FPS after
  these changes; no dropped ticks in the measured post-fix intervals. This is bounded
  local evidence, not a GPU benchmark or full scene acceptance. RenderDoc1.41 captured
  one colour/depth pass (5300 indexed draws, 28828 triangles, empty validation log).
  Its sRGB window target exposed extra encoding of display-encoded original colours;
  native window output now explicitly requests RGBA8_UNORM, matching offscreen output.
- Native startup selects existing PsyCross headless mode 2: SDL window and CPU VRAM only.
  No PsyCross Vulkan instance/device/surface/swapchain or GL context is initialized.
  CPU upload/copy/read/clear VRAM and OT parsing remain; compatibility draws/readbacks
  are skipped. Diligent startup failure exits explicitly. Framebuffer-derived effects,
  native ImGui and legacy GPU texture consumers remain migration work, not hidden fallback.
  Saved settings load without creating a compatibility ImGui context. The legacy panel
  toggle is F9; the existing receptivity shortcut moved from F9 to F8.
  [Details/evidence](product/native-renderer-performance.md).
- Diligent Vulkan (`API256019`, static Release, C++17) is gated by `RED2_NATIVE_RENDERER`;
  `Release_dev_native` uses PsyCross window-only mode (no compatibility GPU) and the post-frame camera hook.
  `native/NativeRenderer.*` owns device/swapchain/resources and bounded instance submission;
  missing resources are diagnosed/skipped, never replaced by the legacy world. Debug-library
  configurations and native F9 overlay remain unwired.
- Windows operational path also uses `src_rebuild/bin/Release_dev/REDRIVER2_dev.exe`:
  from root PowerShell, set `$env:RED2_NATIVE_RENDERER = '1'` and invoke
  `& .\scripts\run_debug_start.ps1 -Mission 1 -ExtraArgs @('-drawregion')` (fire-and-forget).
  Simulation/game/services still depend on PsyCross; its presentation is headless while
  Diligent presents the native window, resident `native_cache` GLBs and live camera/LOD.
  This prepares a resident-world snapshot, not full driving streaming or full-game acceptance.
  [Launch/capture/reset instructions](RECENT_CONTEXT.md#native-launch-and-capture-operations)
  include the optional bounded `-drawmodel <slot>` probe and legacy next-launch reset.
- `SourceModelDecoder.*` validates explicit MODEL spans with zero partial output, supports
  `{10,11,20,21,22,23}`, and resolves instanced shared vertex/normal arrays from validated base
  spans. Building policy is strict or skip-unsupported; the Chicago histogram found 516 slots /
  5538 records (two outside the allowlist, ptype 9). Source instance transforms were verified.
- `WorldMeshBuilder.*` expands triangle corners and builds one primitive per submesh;
  `GltfWriter.*`/`GltfLoader.*` retain multi-primitive geometry, node transforms, RGBA8 COLOR_0
  and embedded images with bounded validation. PNG handling verifies CRC/Adler and diagnoses
  unsupported material texture references. Single-writer compatibility remains separately tested.
  `ConvertModelSlot` publishes transactionally through local `PublishMultiMesh`.
- `TextureDecode.*` handles 4/8-bit CLUT and direct BGR555 pages; `texturebake.*` snapshots
  resident VRAM at prepare, deduplicates page/CLUT images (64 images / 16 MiB), and keys baked
  pixels. Native sampling is RGBA8_UNORM NEAREST/CLAMP, raw UVs scaled once by 1/256, with a
  white untextured texel. Later-resident pages and full streaming remain outside this snapshot.
- Uncommitted parity corrections: world draws use `CULL_NONE`, opaque alpha with `0000h`
  discard, and LEQUAL depth. Legacy world shading is baked into COLOR_0, with scale 2 in the
  shader; shading bytes participate in conversion hashes. This supersedes the earlier universal
  straight-alpha world blend and mean-face-colour description; dynamic/per-frame lighting and
  PSX additive/subtractive modes are not accepted by this bounded bake.
- Region capacities are now 1024 resources / 8192 placements, with explicit truncation and
  failed-resource diagnostics. Camera projection consumes live GTE `scr_z`/`ofx`/`ofy` rather
  than a fixed FOV. The observed scenario prepares six distinct LOD and four tile high-detail
  variants; per-frame legacy-Z thresholds choose resource keys. This is implemented bounded
  tier selection, not complete LOD/streaming or scene acceptance.
- Diagnostics are default-off; the magenta-background environment diagnostic currently enables
  when its variable is present. Source-bounded content/cutout probes are diagnostic only.
  Debug captures can freeze at `DelayTicks=600`; both baseline logs reached tick 600, with
  dropped ticks legacy 0 / native 397. Equal tick numbers do not prove equal interpolation
  phase or full deterministic state. Earlier wall-clock comparisons are confounded.
- Pre-quad-fix matched-tick baseline: clear-background count 102440 (11.1%); no-cutout count
  99811 (10.8%). Magenta confirmed untouched background. These are bounded diagnostic
  observations, not a stage acceptance metric.
- Source-proven quad correction: cyclic raw A,B,C,D remains stored A,B,D,C with unchanged
  UV/colour/shade mapping; indices `012,132` cover legacy ABD+BCD with consistent winding,
  replacing overlapping `012,023`. Triangle paths/counts are unchanged. `HashSourceMesh`
  hashes index bytes (`assetcatalog_game.c`), so corrected sourceHash invalidates old world
  cache entries; global schema remains 1 (no unrelated car-cache bump).
- Parent validation: the actual inspector runner passed all 16 suites (1271 decoder / 347
  builder checks, others unchanged); `Release_dev|x64` MSBuild succeeded with existing
  LNK4075/LNK4098 warnings. The parent owns final validation; no new native-smoke result is implied.
- Fresh `-DelayTicks 600` captures in `build/debug-tick-quadfix` were verified by its
  Pillow/NumPy `verify_artifacts.py` (exit 0, `verification.json`): bounded Chicago mission 1,
  at 1280x720, clear pixels
  fell from 102440 (11.115451%) to 1733 (0.188043%) for each override, a reduction of 100707;
  override RGB images were identical (MAE 0). Full-image RGB MAE against legacy fell from
  33.696842 to 17.917348. Mean RGB-delta bins <20 / [20,60) / >=60 were 78.370877% /
  13.863281% / 7.765842% (all 921600 pixels; no exclusions). Observer inspection confirmed
  large navy wedges disappeared at bottom pavement/right wall/top fire escape; broad alignment
  improved, but textures/shading differ and HUD/car/player remain absent.
- Actual GLB position/UV sample: new sourceHash `043b5f9ac2fa7a9a` versus old
  `3cda93cb69ffadda`, same optionsHash `f3121675359b72cb`, mods 0, schema 1: new ABD+BCD
  versus old ABD+ADC. There are 160 newly timestamped cache entries; 288 old entries remain.
  This sample is not exhaustive. One-shot logs report 160 converted / 0 failed / 6740 drawn /
  0 unresolved, not capture-tick statistics. New saved logs do not retain a tick-anchored line:
  invocation requests tick 600 but independently matched state/interpolation is not certified.
- Acceptance remains open: no PAL graphics validation, full-scene/cars/pedestrians/UI/VFX/
  streaming parity, broad performance or full determinism claim. E04 remaining consumers are
  still pending; the user blocks E05/E06 until parity. No commits were made for this block.

## Engine migration E04 (in progress)

- Audio contract (E04-A): `Game/C/audiocontract.{h,c}` is a pure-data gameplay-to-service boundary (stable event
  ids with zero reserved, play flags, named request diagnostics NaN-safe for volume/pitch/distance, a 64-slot
  voice table with 16-bit per-slot generations rejecting stale handles and documenting zero wraparound, inclusive
  loop metadata matching FMOD `setLoopPoints`); Fix-5 reports 221/0 and gameplay must not call FMOD directly. The
  ignored local FMOD SDK metadata identifies 2.03.15 (`FMOD_VERSION` 0x00020315, DLL resource 2.3.15); do not
  infer 2.03.21 from suffix `0x15`; no independent new-version verification is claimed. See
  [the FMOD integration rule](rules/fmod-integration.md).
- Gate 4B attempt 2 **approved as a bounded SDK-isolated service only**, not consumer migration or audible
  correctness: true per-channel 2D/3D, concurrent samples, persistent generations, checked listener updates and an
  x64 SDK guard; inclusive endpoints unchanged, equal endpoints rejected, RESTART/stream loops unsupported,
  Doppler disabled pending unit validation. Final suites: catalog 258, audio contract 221 and remaining
  39/38/28/28/64/25/32/107; AudioServiceSmoke 57/0 (exit 0), SDK fetch exit 0, nine FMOD-version fixtures 9/0. A
  reset-on-restart mutant fails 13/57 as expected; live-service source hash unchanged. Parent fresh x64
  `Release_dev`/`Release_dev_native` SDK-present and an SDK-absent x64 `Release_dev` all exit 0 (Win32 not built);
  copied DLL SHA256 matches the SDK DLL `F58048FB39BD10B68E3701A553D632BDFC8BE3C644E636E715BCA8E7049A6797`.
  Release-failure stream storage lifetime, captured output, device loss and stream stress are unverified; earlier
  39/0 and 216/0 are historical.
- Game-target wiring (2026-10-09): premake links `fmod_vc.lib`, defines `RED2_AUDIO_FMOD` and copies `fmod.dll`
  beside the executables when the SDK is present (otherwise the service unit is excluded, so a default build is
  unchanged); `Release_dev`/`Release_dev_native` build and link with it; the bounded FESound
  consumer routing is described below, while remaining game audio consumers still need migration.
- Menu-bank PCM decode + registration seam (P0, oracle-approved attempt 1 with F1-F4): `Game/C/sounddecode.{h,c}`
  is an SDK-free PSX SPU ADPCM (VAG) decoder (16 B -> 28 frames, cross-block predictor carry, filter-nibble
  validation, inclusive block-aligned loop frames, zero partial output, deterministic); loop start uses the PSX
  block-start convention, deliberately diverging from the fork reference's FIXME-marked `(block+1)*28` (needs an
  explicit acceptance decision when looping samples migrate, outside the FESound slice). `Game/C/sound.c` glue
  under `#ifdef RED2_AUDIO_FMOD` decodes samples 0-3 at the `SBK_ID_MENU` load into `SOUND_BANK_SFX`, registers
  `SFX(1,0..3)` via `AudioService_RegisterSoundMemory` (streaming=0), unregisters on bank-1 reload, registers
  nothing when base!=0 and is inert when the service is inactive; a read-only probe of
  `data/DRIVER2/SOUND/VOICES2.BLK` shows a 4-sample x 560-frame @4000 Hz menu lump with no loop and a clean level
  lump.
- FESound FMOD routing + service lifecycle (E04 A1, 2026-10-09): `Game/C/audiofmod_adapter.{h,c}` is an SDK-free
  helper (no FMOD includes): a consumer-mode enum parsed once from `RED2_FMOD_AUDIO` (`"1"` => FMOD_ONLY, default
  SPU_ONLY), master gain `clamp((10000+gMasterVolume)/10000, 0, 1)`, pitch `gamePitch/4096` clamped to
  `16383/4096`, and the `SFX(1,sample)` id builder. In FMOD_ONLY mode `sound.c` routes `FESound` to
  `AudioService_Play` as a 2D one-shot (one log line with sample/handle/VoiceCount) and skips the SPU body, so
  `channels[]` is untouched (no double playback); a missing event or inactive service logs once and stays silent,
  never falling back to SPU. `LoadSoundBankDynamic` threads the lump identity so only the fresh `SBK_ID_MENU` load
  registers `SFX(1,0..3)` and other bank-1 loads unregister them. `redriver2_psxpc.cpp` initializes the service
  before `redriver2_main` (init failure stays SPU), shuts it down after, and `VsyncProc` calls
  `AudioService_Update` once per rendered frame; all calls stay under `#ifdef RED2_AUDIO_FMOD`. Smoke
  `scripts/run_audio_service_smoke.ps1` 31 (pure) + 98 (service) checks/0; `cl /Zs` of
  `sound.c`/`redriver2_psxpc.cpp` exit 0 with and without the define. In-game probe (Release_dev, `-nointro
  -nofmv`, `RED2_FMOD_AUDIO=1`) initialised the service alongside OpenAL and routed 41 FESound calls as distinct
  FMOD handles (samples 0/2/3, voices 1-2, 0 warnings; switch off inactive). Gate-accepted informational items:
  linear per-voice master gain (SPU ~1.4x pre-saturation curve) and the dormant lower pitch clamp. No
  audible-fidelity/tick-coverage/master-bus claim.
- Next: E04 remaining consumer slices; E05/E06 are user-blocked until parity.

## Project and current services

REDRIVER2 is reconstructed Driver 2 game logic, not an emulator. Physics, collision, AI, missions and replays live
under `src_rebuild/Game/` but still call PsyCross math/GTE, clocks, pad, SPU/CD/streaming and GPU services. The
PSX branch still shares `FilterFrameTime`/`VSync`; on desktop `ProjectClock` separates the fixed simulation tick
from presentation, which draws every frame and interpolates car/camera translation/orientation between ticks.
Replay repeats and counters remain tick indexed. Local effect interpolation covers particles/birds/hubcaps/rain;
complete pedestrian/wheel-spin/native VFX coverage remains E05 work. E01 preserves regional semantics before
changing clock owners.

PsyCross remains a separate submodule from `git@github.com:SimStm/PsyCross.git`; do not substitute upstream or
infer removal from a roadmap. Existing C++11 and Windows/Linux/web/Android/PSX guards remain. SDK/compiler/target
compatibility for the new stack is pending E00, not established by cross-platform advertising.

## Existing renderer baseline

Desktop defaults to Vulkan through PsyCross, with separately selected OpenGL; this is the existing PSX
primitive/hybrid architecture, not Diligent. [Product behavior](product/vulkan-game-renderer.md) and the source
govern support.

- Vulkan PSX shaders implement CLUT/texture windows/dither/filtering, VRAM mirror, blend/stencil/depth, white
  primitives and queued VRAM copies (`-vkpsxtest` fixtures); the modern mesh API/importer/PBR/shadow composite
  shares the PSX projection and depth, with optional legacy lighting receptivity and PGXP. World/overlay
  boundaries split gameplay composition from flare/fade/UI tails, and split-screen keeps its limited path; this
  does not prove native mode/category coverage. Minimize/restore and fence-deadlock fixes are in
  [minimize](changes/2026-09-21/vulkan-minimize-swapchain-crash/index.md) and
  [fence](changes/2026-09-21/vulkan-frame-fence-deadlock/index.md) evidence.
- Shadow-volume centers snap to texels while following the vehicle (historical aligned GL/Vulkan shadow-receive
  IoU 0.985); legacy city geometry still lacks the planned caster path (only imported modern meshes cast).
  `PSYX_PERF_LOG` records submission CPU time (~6.4 ms at 1280x720 historically); GPU timestamps, peak memory and
  broad platform measurements remain pending. `-vkfixture` renders modern GLB fixtures only and accepts nothing
  about the new engine.
- UI parity/blend/offscreen details remain in [UI product](product/vulkan-ui-image-parity.md) and
  [lighting product](product/legacy-lighting-receptivity.md). New migration uses targeted native contracts, not
  universal pixel equality with this old pipeline.

## Textures, mods and inspector

- Ordered JSON mods and `mods/enabled.json` retain deterministic precedence; texture keys are name/page/index with
  optional palette identity, model references stay metadata, and override alpha/cutout/blend semantics mirror the
  active compatibility policy (do not silently change during conversion).
- `AssetCatalog.*` separates stable resources from transient instances with provenance and model/texture/LOD links
  ([catalog product](product/asset-catalog-identity.md)).
- Inspector supports face/material/component/logical-object scopes, source/override previews, TXT and single/batch
  PNG exports with deduplication, progress/retry and diagnostics
  ([selection](product/inspector-selection-coverage.md), [batch export](product/object-texture-batch-export.md)).
- Versioned offline visual/audio packages and OpenDriver2Tools-based conversion are not implemented; the bounded
  `GltfLoader.*`/modern fixtures are reuse candidates, not the new city pipeline.

## Gameplay and developer tooling

- Resident procedural playground (flat surface/collidable boxes, disabled donor traffic/missions, reset/frontend
  return, `-playground`, Take a Ride) is recorded in [playground product](product/playable-testing-playground.md)
  P1-P5. Debug start settings preserve mission/city/vehicle/position/heading/player/chase and generate
  reproducible command lines (`scripts/run_debug_start.ps1`).
- F9 Developer Graphics Panel uses vendored ImGui1.91.9b (Windows/Linux) and `developer_graphics.ini`,
  independently from `config.ini`. Schema9 display controls include a 15..360 presentation FPS slider (defaults 30
  NTSC/25 PAL), NTSC/PAL selection and VSync beside resolution; FPS/VSync apply live, region applies after
  restart, and the same binary selects the 30/25 Hz clock, screen/aspect/audio and replay metadata at startup.
  Window/fullscreen selection keeps its confirmation/revert (fullscreen untested); the input editor
  captures/cancels bindings and persists `developer_input.ini`.
  [Frame controls](changes/2026-10-08/display-frame-controls/index.md) and
  [runtime settings](product/runtime-settings-gui.md) hold the detail.
- Content/language/loading/dynamic-light/widescreen controls describe reload requirements;
  `DeveloperSettingsFile_WriteKeys` preserves unknown keys/comments with transactional temp/backup files.
- FMOD has the bounded E04 service/menu routing above, not full audio migration. PopcornFX, RmlUi,
  supplementary physics and in-game authoring editors are not implemented;
  ImGui/input/window dependencies must migrate before E07.

## Knowledge and validation state

English documentation separates planned intent, product behavior, completion records and dated change evidence.
[Migration validation](engine-migration/validation.md) requires deterministic mechanics, conversion inventories
and targeted native resources/pixels/audio, with actual budgets and platform scope.

The planning-only validation is recorded in
[its change record](changes/2026-10-08/engine-migration-plan/index.md). Subsequent E00-E04 and complementary fixes
above record their actual isolated builds and bounded tests. The latest effect/replay evidence retains the
unresolved route fault; a matching dump does not accept a migration stage. Older generated projects/executables
may belong to the preserved rework branch.

Existing commands include `git diff --check`, Premake/generated builds, `-vkpsxtest`, `-vkfixture`, backend flags
and the inspector test script; they prove their stated baseline scopes only, and export tests need a fresh
directory.
