---
type: Status
title: Recent engineering context
description: Vulkan rework preserved; new engine migration planned from the existing baseline.
tags: [okf, status, agents]
---

# Recent engineering context

> Updated: 2026-10-10.
> Read the handoff below first: E03 region native draw (block 2 item 5) done; E04 consumer slices next.
> Older task narratives are condensed; their change records
> hold the detail.

## Handoff (read first): plan, current step, what must finish first

**Plan:** staged engine migration on `feat/new-engine` — umbrella
[`roadmap/planned/engine-migration.md`](roadmap/planned/engine-migration.md),
execution [`engine-migration/index.md`](engine-migration/index.md), stage
contracts [`engine-migration/stages/`](engine-migration/stages/) (E00-E07; E08
PopcornFX optional, only on explicit request). User scope: at least through E06.
Preserve Driver 2 simulation/math/mission/cutscene/replay/regional semantics;
fixed ticks independent of presentation FPS and CPU speed.

**Current step: E03 (Diligent playable slice) — region native draw done; E04
consumer slices next.** Detail lives in [`CURRENT_STATUS.md`](CURRENT_STATUS.md) and
`CHANGELOG.md`; the notes below are the handoff summary.

E03 (done and verified):

- SDK/renderer/game: official Diligent clone in `dependencies/DiligentEngine` (master
  `API256019`), Vulkan-only static Release plus `volk` (`scripts/fetch_diligent.ps1`;
  layout needs `volk.lib`, `glslang.lib`, `/std:c++17`); `native/NativeRenderer.*` owns
  device/swapchain/present (Diligent-free C ABI) and draws converted GLB content
  (`NativeRenderer_LoadContent`/`ClearContent`/`HasContent`/`GetContentStats`/
  `RenderSceneToRGBA`), multi-primitive resources and per-instance draws. PsyCross is
  headless (`PsyX_SetHeadless`) with a generic post-frame hook (`PsyX_SetPostFrameHandler`);
  fork changes committed/pushed as `99950a7`. `RED2_NATIVE_RENDERER` gates it and
  `Release_dev_native` builds `REDRIVER2_native.exe` with `-nointro -nofmv`.
- Source MODEL decoder (block 1): `utils/SourceModelDecoder.*` is a pure, asset-free decoder
  of the MODEL poly block from an explicit `[begin,end)` span. Allowlist
  `{10,11,20,21,22,23}`, stride `PolySizes[ptype&0x1f]`, world quad vertex/UV order, named
  validation with zero output, instanced models rejected; `SourceModelDecoderTests` 1186.
- Block 2a: `model_byte_sizes[]` captured at both size-live sites (`models.c`, `spool.c`)
  and exposed as `ModelRecord.spanBytes` (`AssetCatalog_RegisterModelWithSpan`/
  `GetModelSpan`), cleared on invalidation; `AssetCatalogPlacement.sourceObjectIndex` from
  the walker's 14-bit index. Pure `Game/C/modelptypeprobe.{h,c}` + the `-modelptypes` flag;
  Chicago histogram 516 slots/5538 records; `ModelPtypeProbeTests` 68, `AssetCatalogTests`
  278.
- Block 2.1: `ConvertPolygonTypes` (draw.c:721-730) draws `{11,21,23}`; `SourceModelDecoder`
  `Strict` (default) vs `SkipUnsupported` (advance by `PolySizes`; zero-stride is
  `UNSKIPPABLE_PTYPE`) for building-class models only; tile/sprite stay `Strict`.
- Block 2.5: pure fixtures pin yang == `RotMatrixY(yang*64)`, view =
  `R_cam*(R_obj*p + pos - cam)`, the decoder's NDC winding and the raw-unit policy; scene
  pipelines use `CULL_MODE_BACK` with the `FrontCounterClockwise` pin re-derived in item 4.
- Block 2.4: `utils/TextureDecode.{h,c}` decodes indexed 4/8-bit + CLUT and 16-bit direct
  pages into RGBA8 (BGR555 `E5TO8`, `0000h` transparent, STP->128; texture window not
  applied); `TextureDecodeTests` 136.
- Item 3 (world mesh build + multi-primitive GLB, follow-up A+B): pure C++11
  `utils/WorldMeshBuilder.{h,cpp}` builds a `GltfMeshData` from `SourceMesh` (flat
  per-triangle normals with a degenerate `(0,0,1)` fallback; 3 dedicated vertices per
  triangle; base colour = mean stored face colours or neutral white for PL records; identity
  node transform; deterministic; named-error bounds validation with zero partial output);
  `WorldMeshBuilder_BuildMulti` maps each submesh to one `GltfPrimitiveData` and validates a
  contiguous, ordered, complete index partition (the game call site uses `BuildMulti`).
  `utils/GltfWriter.*`/`utils/GltfLoader.*` add `Gltf_WriteFileMulti`/`Gltf_LoadFileMulti`
  (deterministic layout; strict pre-write validation with partial file removed); the single
  writer is byte-identical by construction and by a dedicated test, and the multi loader is
  strict and rejects texture references. `native/NativeRenderer` represents a
  multi-primitive resource and an instance draws every primitive; `drawn` still counts
  instances. `assetcatalog_game.c` `ConvertModelSlot` builds with `BuildMulti` and publishes
  via a local transactional `PublishMultiMesh`; `LoadContent`/`RegisterResource` now reject
  texture references, so an embedded-image GLB is no longer accepted (the E02 writer never
  embeds images). Evidence: inspector suites green (`GltfRoundTripTests` 116;
  `WorldMeshBuilderTests` 211); smoke 154, 0 failures; builds exit 0; slot 1150 (referenced)
  published 3 primitives / 3 materials, 22 placements, submitted/drawn 22, residentResources
  1, offscreen 640x360 with 42 non-clear pixels. Follow-ups: promote `PublishMultiMesh` into
  `AssetConverter`; `MaterialReferencesTexture` misses `extensions` references. Limits: no
  textures/embedded images; flat normals only; one material = mean face colour.
- Item 4 (per-instance draw + game camera): `native/NativeRenderer.*` adds bounded resources
  (`NATIVE_MAX_RESOURCES`/`NATIVE_MAX_INSTANCES`, raised to 64/4096 for the region path) with
  `SetInstances`/`ClearInstances` (copied; unresolved keys skipped with a counter), a
  residency API, `SetClipPlanes` and no-fallback semantics; projection is +Z-forward [0,1]
  depth and +Y-down (the F-A fix negates camera-space Y; cull pin `CULL_MODE_BACK` +
  `FrontCounterClockwise=False`), pinned by smoke `TestVerticalConvention` and the re-pinned
  winding fixture. `utils/NativeInstanceTransform.*` (392) and `utils/NativeCameraView.*`
  (9) are pure; `redriver2_psxpc.cpp` feeds the real camera via `NativeCameraView_Compose`
  (clip planes 100/125000) with a gated one-shot report/dump; `assetcatalog_game.{h,c}` adds
  the slot registry + placements collector and `-drawmodel <slot> [count]`. Evidence: smoke
  130/0; suites pass; run `-drawmodel 169 32` (18/18 drawn, offscreen rgba-hash
  `241175f1491c392b`, 4 non-clear pixels at y[286..307] = 360-y). Limits: one slot + bounded
  K; FOV 1.0472 vs 0.9265 deferred; GLB node transform ignored.
- Item 5 (region native draw, `-drawregion`, 2026-10-10): capacities raised for the
  region path (`NATIVE_MAX_RESOURCES` 64, `NATIVE_MAX_INSTANCES` 4096,
  `ASSET_CATALOG_GAME_MAX_MODEL_RESOURCES` 64, new
  `ASSET_CATALOG_GAME_MAX_REGION_PLACEMENTS` 4096). `Game/C/assetcatalog_game.{h,c}`
  prepares once after `SpoolSYNC`: the level walker collects all resident placements
  (bounded at 4096, placement truncation flagged), derives the distinct referenced model
  slots (bounded at 64, resource truncation flagged), converts each slot once through
  `ConvertModelSlot` (a failed slot is a named warning and is skipped while the rest
  continue) and exposes bounded accessors for the bridge; cleared at `BeginLevel`;
  non-Windows/PSX stubs. `redriver2_psxpc.cpp` registers every converted resource (logs
  R/T), builds the full instance list in a static `NativeInstance[4096]` buffer (~1 MB BSS;
  `static_assert <= NATIVE_MAX_INSTANCES`, never on the stack), calls `SetInstances` once,
  and reuses the camera-validity gate and the one-shot report + `native_region_draw.rgba`
  640x360 dump with the real game camera; a level change resets the state; no per-frame
  re-collection, no re-decode at draw time, no placeholder fallback when instances are set.
  The bounded `-drawmodel` path is unchanged; `main.c` accepts `-drawregion` plus help line
  and a warn-only post-SpoolSYNC hook. Evidence: all inspector suites green;
  `Release_dev`/`Release_dev_native` builds exit 0; real mission-1 run prepared `4096
  placements (placement-truncated), 64 distinct slots, 46 converted, 18 failed
  (resource-truncated)`, the 18 failures named `decode failed: instance (instanced models
  cannot be decoded from a single span)`, `registered 46/46 resources`, report `submitted
  4096, drawn 2913, skippedUnresolved 1183, residentResources 46, placements 4096,
  placement-truncated 1, resource-truncated 1, camera-position 17387,-1050,142670,
  camera-z first 14956.2, min -49875.2, max 80857.4, inFrustum 2588/4096, laterallyVisible
  1919/4096`; offscreen 640x360 with 64174 non-clear pixels (~28%), full-frame bbox, 8
  distinct colours. This proves the full converted region is drawn with the real game
  camera (2913 instances, 46 resident resources); it is untextured (base colours only), the
  caps truncate the level's full content (E02 counted 6740 unique placements) and 18
  referenced slots are instanced models the single-span decoder rejects (follow-up:
  base-instance vertex resolution); no LOD/streaming, no texture/lighting parity; a dev run
  is not visual acceptance.
- ODT spike (temp-only, no repo integration): OpenDriver2Tools master @
  `89009f7f4f48f37a886b376292432cdab4a136` (2025-10-28) built headless (Win32), full
  Chicago export (498 model OBJs, 159 pages, 135 region OBJs) plus `CHICAGO_HEAD.glb`;
  adaptation surface and divergences in the discussion. Informs B2.4.

E03 must-not-claim: no "region/building renders correctly" (the region dev run is not
visual acceptance); the renderer no longer accepts embedded-image GLBs; no all-model/
all-slot coverage; no LOD/streaming/cars/animation/performance; single-vs-multi byte
identity is by construction plus empirical verification and the dedicated test. E03 still
has no playable original-world native slice.

E04 (in progress):

- Audio contract: `Game/C/audiocontract.{h,c}` is a pure-data gameplay-to-service boundary
  (stable event ids, play flags, named NaN-safe request diagnostics, a 64-slot voice table
  with 16-bit per-slot generations rejecting stale handles, inclusive loop metadata);
  `AudioContractTests` 221/0 and gameplay must not call FMOD directly. The ignored local
  FMOD SDK metadata identifies 2.03.15 (`FMOD_VERSION` 0x00020315, DLL resource 2.3.15); do
  not infer 2.03.21 from suffix `0x15`. `scripts/fetch_fmod.ps1` verifies the gitignored SDK
  and `knowledge/rules/fmod-integration.md` records version/distribution/threading/VAG
  rules.
- Gate 4B attempt 2 approved as a bounded SDK-isolated service only: true per-channel
  2D/3D, concurrent samples, persistent generations, checked listener updates and an x64
  SDK guard; inclusive endpoints unchanged, equal endpoints rejected, RESTART/stream loops
  unsupported, Doppler disabled pending unit validation. Final suites: catalog 258, audio
  contract 221 and remaining 39/38/28/28/64/25/32/107; `AudioServiceSmoke` 57/0 (exit 0),
  SDK fetch exit 0, nine FMOD-version fixtures 9/0; a reset-on-restart mutant fails 13/57 as
  expected. Parent fresh x64 `Release_dev`/`Release_dev_native` SDK-present and an
  SDK-absent x64 `Release_dev` all exit 0 (Win32 not built); copied DLL SHA256 matches the
  SDK DLL `F58048FB39BD10B68E3701A553D632BDFC8BE3C644E636E715BCA8E7049A6797`.
  Release-failure stream storage lifetime, captured output, device loss and stream stress
  are unverified.
- Game-target wiring (2026-10-09): premake links `fmod_vc.lib`, defines `RED2_AUDIO_FMOD`
  and copies `fmod.dll` beside the executables when the SDK is present (otherwise the
  service unit is excluded, so a default build is unchanged); no game system calls the
  service yet.
- P0 menu-bank PCM decode + registration seam: `Game/C/sounddecode.{h,c}` is an SDK-free
  PSX SPU ADPCM (VAG) decoder (16 B -> 28 frames, cross-block predictor carry, filter-nibble
  validation, inclusive block-aligned loop frames, zero partial output); `Game/C/sound.c`
  glue under `#ifdef RED2_AUDIO_FMOD` decodes samples 0-3 at the `SBK_ID_MENU` load into
  `SOUND_BANK_SFX`, registers `SFX(1,0..3)` via `AudioService_RegisterSoundMemory`, and
  unregisters on bank-1 reload. Loop start uses the PSX block-start convention, deliberately
  diverging from the fork reference's FIXME-marked `(block+1)*28` (needs an explicit
  acceptance decision when looping samples migrate).
- A1 FESound routing + lifecycle (2026-10-09): `Game/C/audiofmod_adapter.{h,c}` is SDK-free
  (mode parsed once from `RED2_FMOD_AUDIO`, master gain, pitch clamp, `SFX(1,sample)` id
  builder). In FMOD_ONLY mode `sound.c` routes `FESound` to `AudioService_Play` as a 2D
  one-shot and skips the SPU body (no double playback), never falling back to SPU.
  `redriver2_psxpc.cpp` initializes the service before `redriver2_main`, shuts it down
  after, and `VsyncProc` calls `AudioService_Update` once per rendered frame; all calls stay
  under `#ifdef RED2_AUDIO_FMOD`. Smoke 31 (pure) + 98 (service) checks/0. In-game probe
  (Release_dev, `RED2_FMOD_AUDIO=1`, `-nointro -nofmv`) routed 41 FESound calls as distinct
  FMOD handles (0 warnings). Gate accepted with two informational items (linear per-voice
  master gain vs the SPU ~1.4x curve; dormant lower pitch clamp). No audible-fidelity
  claim; no game consumer has migrated.

E02: Chicago inventory Gate 4A attempt 2 approved as a scoped diagnostic. Final isolated
run: 4/4 barrels, 6740 unique placements, 227 straddlers, 0 overlaps/missing model refs,
114 unknown material classifications, 0 errors/480 warnings (64 displayed). Corrected
walker includes terminal objects marked `0x8000`; the first 6078 count is superseded. This
is a source-union diagnostic, not conversion/world acceptance. Full metrics and limits:
[inventory evidence](engine-migration/e02/chicago-inventory.md).

**Remaining before full acceptance:**
1. E04 remaining consumer slices — A1 (FESound routing + service lifecycle) is complete;
   migrate the remaining audio consumers. Gate approval does not authorize audible
   acceptance.
2. E03 original-world conversion seam (blocked on the E02 city conversion slice); debug
   configurations are not wired (only the Release Diligent libraries exist).
3. E02 other cities/variants and dynamic/streamed categories (E06 coverage).

**Next concrete action:** implement the remaining E04 consumer slices as separately scoped
work. E05/E06 remain pending.

**Commands:** build `Release_dev|x64` or `Release_dev_native|x64` with `premake5.exe vs2022`
+ MSBuild (env `SDL2_DIR`/`OPENAL_DIR`/`JPEG_DIR`, `RED2_BUILD_DIR=build_e00`,
`RED2_BIN_DIR=bin/Release_dev`); `pwsh -NoProfile -File
scripts/run_native_renderer_smoke.ps1` (202 checks). Do not treat bounded SDK-isolated
service approval as consumer or audible correctness.

## Checkpoint commit and current uncommitted work (2026-10-10)

`feat/new-engine` checkpoint `7617bd92` consolidated the E00-E03 implementation, tests,
scripts, knowledge and CHANGELOG, and removed the tracked agent config (`opencode.jsonc`;
`.opencode/` stays gitignored). The PsyCross fork change (headless presentation, post-frame
hook, pad-backend disable) is committed and pushed as `99950a7` on
`origin/codex/new-engine-live-vsync`; the parent records the new gitlink. Both commits are
pushed.

Uncommitted after that checkpoint: the E02 inventory; the E04 blocks
(`Game/C/audiocontract.*`, `tests/AudioContractTests.cpp`, `scripts/fetch_fmod.ps1`,
`knowledge/rules/fmod-integration.md`), the E04 P0 seam (`Game/C/sounddecode.{h,c}`,
`Game/C/sound.c`, `tests/AudioServiceSmoke.cpp`, `scripts/run_audio_service_smoke.ps1`) and
E04 A1 (`Game/C/audiofmod_adapter.{h,c}`, `redriver2_psxpc.cpp`); the E03 Block 1-4
decoder, probe, world-mesh builder, item-4 draw path, multi-primitive writer and the item-5
region draw (`utils/SourceModelDecoder.{h,c}`, `utils/WorldMeshBuilder.{h,cpp}`,
`utils/GltfWriter.{h,cpp}`, `utils/GltfLoader.{h,cpp}`,
`utils/NativeInstanceTransform.{h,c}`, `utils/NativeCameraView.{h,c}`,
`Game/C/modelptypeprobe.{h,c}`, `Game/C/assetcatalog*`, `Game/C/models.*`, `Game/C/spool.c`,
`Game/C/main.c`, `native/NativeRenderer.*`, `tests/SourceModelDecoderTests.cpp`,
`tests/ModelPtypeProbeTests.cpp`, `tests/WorldMeshBuilderTests.cpp`,
`tests/GltfRoundTripTests.cpp`, `tests/AssetCatalogTests.cpp`,
`tests/NativeRendererSmoke.cpp` plus the new NativeInstanceTransform/NativeCameraView test
suites, `scripts/run_inspector_tests.ps1`, `tests/README.md`); and the suite
registration/README plus the status/handoff/changelog updates. The ignored FMOD SDK is
present locally at `src_rebuild/dependencies/fmod/core`; its local version metadata
identifies 2.03.15.

Note: the goal-mode auto-continue safety limit is plugin-side; adjust it in the
plugin/runtime for longer runs.

## Previous task: effect timing and replay trajectory (committed as 036dfc34)

FPS-driven effect acceleration is fixed: `StepDesktopGame` owns regional effect updates and
drawing consumes local interpolated pools/birds/rain/hubcaps and attached car lights;
indicators/sirens, engine/exhaust emission, title/input unlock, bomb flash and
pause-message expiry have tick owners. Full OpenGL ATTRACT dumps match all 1378 ticks
strictly at NTSC 30/360 and PAL 25/360 FPS; rainy ATTRACT.401 matches all 2289. The
wall-collision route is **not fixed** and is deferred by the user (`b2d8857` and upstream
8.0 Win32 reproduce it on local NTSC data). Valid replay commands use spaces between option
and value. [Evidence](changes/2026-10-09/effect-timing-replay-regression/index.md).

## Earlier task: display FPS, regional mode and VSync (committed)

FPS (15..360 presentation cap, defaults 30 NTSC/25 PAL), NTSC/PAL selection and VSync live
in the F11 panel and persist to `developer_graphics.ini` schema9 through transactional key
replacement; FPS/VSync apply live, region applies after restart, and active
missions/replays never switch clock/aspect/audio mid-session. `ProjectClock` owns the
preferred/active region and rational presentation deadlines. Fork `76d83df` wires the
Vulkan swap interval to supported present modes. Verified with NTSC/PAL builds,
timing/settings tests and replay dumps; the FPS cap is a ceiling, not proof of sustained
high FPS. [Record](changes/2026-10-08/display-frame-controls/index.md).

## Latest user decision

Backend discussion: user reports OpenGL at 360 FPS/~3ms versus legacy Vulkan rarely above
45 FPS and asks about OpenGL-to-Diligent focus. Source/log analysis identifies serialized
frame reuse, framebuffer readback/CPU conversion, complete 4MiB VRAM uploads with queue-idle
waits and automatic validation as candidates; no per-section timings establish the dominant
cost. The user adopted interim OpenGL and deferring legacy Vulkan optimization. E03 Diligent
Vulkan remains independent; retirement still requires consumer/build coverage. Updated the
existing discussion only plus status/handoff; no runtime/configuration edits.

The user adopted Diligent Engine/DiligentFX and FMOD Core (Studio later),
OpenDriver2Tools-informed original-content conversion, project-owned services/simulation
ticks and final PsyCross removal. Future directions include RmlUi, internal
map/model/sound/mission/cutscene editors and supplementary physics. The user confirms
permitted middleware use. Do not ask again about adopting it.

Preserve Driver 2 mechanics, missions/cutscenes and replay timing; high display FPS or CPU
clocks must not accelerate simulation. Initial fixed ticks retain actual NTSC/PAL behavior.
Conversion produces modern representations, not automatically new high-detail art or
complete gameplay packages.

2026-10-09 content decode-base decision: user adopted the hybrid strategy for
original-content conversion - the project's reconstructed decode remains the authority for
identity/bindings/simulation; OpenDriver2Tools is a format/placement reference and
gap-filler only (never a runtime dependency, never a second shipped parser); one pure decode
module serves the offline converter and runtime consumers. Authorized next steps: continue
E03 Block 2 (MODEL span capture, ptype coverage, one building to GLB, per-instance native
draw) and a bounded external ODT-to-GLB tooling spike. Acceptance gates and sequencing:
[engine-migration discussion](discussions/engine-migration/index.md).

## Branch preservation (completed)

The rework snapshot is preserved on `feat/vulkan-native-rework` at `e68b6ea6` (remote SHA
matched; archive moves included; R03 incomplete). Fork `ae83dd2` was already reachable from
the fork remote. `feat/new-engine` was created from unchanged `origin/master` via a local
`main` alias, initialized with fork gitlink `37d399df`; it holds the older baseline plus
planning docs, not the native rework — do not merge that branch wholesale. Ignored captures
under `src_rebuild/build/native-renderer-r03/` do not travel with Git.

## Documentation boundary

The [umbrella](roadmap/planned/engine-migration.md), [stages](engine-migration/index.md)
and [validation](engine-migration/validation.md) own execution. PopcornFX is optional E08
only on request after E07; existing effects remain required. Planning-only checks are
recorded separately from E00-E03 implementation below.

## Durable evidence and next action

E00/E01 implementation details and bounded validation are summarized in
[`CURRENT_STATUS.md`](CURRENT_STATUS.md) and the dated records linked there. E02 inventory
findings and limitations are in
[`chicago-inventory.md`](engine-migration/e02/chicago-inventory.md). E03 Block 1 and Block 2
(2a/2.1/2.4/2.5, item 3, item 4, the multi-primitive follow-up A+B and the item-5
region draw) are done; E03 still has no playable original-world native slice. E04 has
the PCM decode + registration seam (P0) and A1 routing; neither is audible acceptance — do
not describe historical smoke/contract results as accepted correctness. Continue only the
requested implementation block; E05/E06 remain pending. The wall-collision replay route
remains deferred by the user
([evidence](changes/2026-10-09/effect-timing-replay-regression/index.md)).

Night/rain follow-up: ATTRACT.401 first froze at 857 due to a static `cyclecluts` packet
linked twice by catch-up ticks. Stack/OT reads identified the self-cycle; tick-owned colours
now upload through frame-owned copies. Final 30/360 completes 2289 ticks with strict
equality. Both builds pass again. Final runner 0402adef passes 145354 timing/21 settings/2688
NTSC or 2448 PAL effect checks; the extra checks cover catch-up, distinct packet ownership
and loading invalidation.

Publication validation: the scoped index was exported to an ignored fresh tree with clean
fork 76d83df, excluding local input/replay-format/converter/Diligent edits. Both Release_dev
x64 rebuilds pass; runner 2745e12d passes all suites. Its 360 FPS night/rain dump matches all
2289 reference ticks strictly. Process stopped.
