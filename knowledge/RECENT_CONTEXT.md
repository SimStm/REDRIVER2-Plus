---
type: Status
title: Recent engineering context
description: Vulkan rework preserved; new engine migration planned from the existing baseline.
tags: [okf, status, agents]
---

# Recent engineering context

> Updated: 2026-10-09.
> Read the handoff below first: E03 Block 2 (2a/2.1/2.4/2.5, item 3, item 4) and E04 A1; next the multi-primitive writer extension.
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
  (`PsyX_SetPostFrameHandler`). Fork changes are committed/pushed as `99950a7`.
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
- Source MODEL decoder foundation (E03 Block 1, extended in Block 2):
  `utils/SourceModelDecoder.*` is a pure, asset-free decoder of the reconstructed
  MODEL poly block from an explicit `[begin,end)` span. Allowlist
  `{10,11,20,21,22,23}` (10/20/22 triangles, 11/21/23 quads), stride
  `PolySizes[ptype&0x1f]`, world quad vertex/UV order, named span/index/normal
  validation with zero output, instanced models rejected, per-submesh
  colour/normal flags with raw stored face colours. `SourceModelDecoderTests`
  1186 checks (synthetic byte-exact fixtures).
- E03 Block 2a (2026-10-09): loader-owned `model_byte_sizes[]` captured at both
  size-live sites (`models.c` `ProcessMDSLump`, `spool.c` `init_spooled_models`)
  before relocation and exposed as catalog `ModelRecord.spanBytes` via
  `AssetCatalog_RegisterModelWithSpan`/`GetModelSpan`, cleared on slot
  invalidation (no stored pointers); `AssetCatalogPlacement.sourceObjectIndex` set
  from the walker's 14-bit index (dedup key unchanged). New pure
  `Game/C/modelptypeprobe.{h,c}` counts declared ptypes from real `PolySizes`
  (bounds; zero-stride reported, never advanced; no draw-path class guessed);
  `ModelPtypeProbeTests` 68 checks. The `-modelptypes` flag (accepted in `main.c`)
  writes `model_ptypes_level_<N>.txt` via `InspectorExport_WriteText` with a
  per-level accumulator reset at `AssetCatalogGame_BeginLevel`. Real Chicago
  histogram (Release_dev, `-opengl -nointro -nofmv -mission 1 -modelptypes`): 516
  slots, 5538 records; building-class 379 slots (ptypes 9x2, 10x30, 11x350, 20x11,
  21x28); tile-class 132 and sprite-class 5 slots contain only ptype 21; pre-policy
  outside-allowlist 118 (ptype 10 x116 + 9 x2, all building); after the final
  allowlist the re-run reports 2 outside (ptype 9 only); 0 stride-zero, 0 invalid.
  `AssetCatalogTests` 278 checks.
- E03 Block 2.1 (gate-driven skip-vs-fail policy): the runtime
  `ConvertPolygonTypes` (draw.c:721-730) converts every even declared ptype except
  8 (10->11, 20->21, 22->23); the building loop draws `{11,21,23}`, so the declared
  types it can draw are `{10,11,20,21,22,23}`. `SourceModelDecoder` adds a policy
  enum: `Strict` (default, unchanged hard error/zero output) and
  `SkipUnsupported` (advance by `PolySizes`, count; bounds stay strict; zero-stride
  is `UNSKIPPABLE_PTYPE`) for building-class models only; tile/sprite callers stay
  `Strict` (Chicago tile/sprite data contains only ptype 21).
- E03 Block 2.5 (camera/unit/winding fixture): pure fixtures pin yang ==
  `RotMatrixY(yang*64)` (4096-unit turn), view = `R_cam*(R_obj*p + pos - cam)`,
  the NDC signed-area winding of the decoder's `(v0,v1,v3,v2)`->`(0,1,2)+(0,2,3)`
  emission, and the raw-unit policy (0.02/300 is not a valid raw-unit projection).
  Renderer scene pipelines changed from `CULL_MODE_NONE` to `CULL_MODE_BACK`
  (Vulkan negative-viewport emulation), asserted through the real offscreen
  pipeline; the `FrontCounterClockwise` pin was re-derived in item 4 (`False` after
  the F-A +Y-down fix). Native renderer smoke 46 checks (38 preserved + 8 new).
- E03 Block 2 gate: attempt 1 found one High must-fix (ptype 10 is drawn by the
  runtime, not skipped) plus probe counting/reset items; remediation applied and
  verified; attempt 2 verdict COMMIT-ready on code (remaining item was this
  documentation). `Release_dev` and `Release_dev_native` builds exit 0 with the
  batch.
- ODT spike (user-authorized; temp-only, no repo integration): OpenDriver2Tools
  master @ `89009f7f4f48f37a886b376292432cdab4a136` (2025-10-28) built headless
  (Win32) and produced a full Chicago export (498 model OBJs, 159 texture pages,
  135 region OBJs) plus one reference GLB (`CHICAGO_HEAD.glb`) via a stdlib
  converter; adaptation surface recorded (lift rgb5a1 conversion/RLE/`PolySizes`/
  byte structs into tooling; reimplement decode/inventory/LOD/writers in the
  project module; ODT divergences: no index dedup, normal-index bug, gouraud
  colours undecoded, `TEXINF` subrect not applied, transparency key). Informs B2.4.
- E03 Block 2.4 (CPU texture decode, 2026-10-09): `utils/TextureDecode.{h,c}` is a
  pure decoder for indexed 4/8-bit pages + CLUT bytes and 16-bit direct pages into
  RGBA8 (explicit row stride; tight loader pages and the 1024-halfword VRAM stride;
  BGR555 `E5TO8`, `0000h` transparent, STP->128 with unknown blend context; texture
  window not applied - sample-time). Bounded validation, named errors, zero partial
  output; `TextureDecodeTests` 136 checks (synthetic + optional read-only real-page
  probe via `REDRIVER2_REPO_ROOT`). No GPU/game integration.
- E03 Block 2 item 3 (2026-10-09): NEW `utils/WorldMeshBuilder.{h,cpp}` (pure C++11
  `SourceMesh` -> `GltfMeshData`) synthesizes one flat geometric normal per emitted
  triangle (winding-normalized; degenerate fallback `(0,0,1)`), expands each triangle
  to 3 dedicated vertices, base colour = mean of the raw stored face colours (neutral
  opaque white for PL records without RGB), identity node transform, deterministic,
  full bounds validation with named errors and zero partial output; >1 submesh ->
  `multiple-submeshes-unsupported` with zero output (never a silent drop). The
  `-convertmodel <slot>` flag (accepted in `main.c`, value skipped; warn-only with
  named diagnostics) resolves `AssetCatalog_FindModel(slot)` +
  `AssetCatalog_GetModelSpan`, decodes with `SkipUnsupported`, builds, and publishes
  via `AssetConverter_ConvertMesh` (converter `world-mesh`, `native_cache/`;
  `GltfWriter` writes, `Gltf_LoadFile` re-verifies, atomic publish). Real Chicago
  evidence (Release_dev, `-opengl -nointro -nofmv -mission 1 -convertmodel 0`): slot
  0, span 676 bytes, submeshes 1, vertices 120, triangles 40, skipped 0; source-hash
  `ed3098e5f1551505`, content-hash `e05d775f5e30a78b`; output
  `native_cache/world-mesh_ed3098e5f1551505_0000000000000000_0000000000000000_1.bin`
  (GLB, 5256 bytes); two runs gave identical source/content hashes (determinism);
  slots 1/2 converted with 1 submesh each. `WorldMeshBuilderTests` 122 checks (flat
  normals incl. a vertical triangle, base colour present/absent, multi-submesh
  diagnostic with zero output, bounds/argument errors, uncovered index, determinism:
  byte-identical GLB + equal content hash, `GltfLoader` reload); all 14 inspector
  suites pass; `Release_dev` and `Release_dev_native` builds exit 0. Limits: no
  textures/embedded images here (follow-up: per-face `texture_id`/CLUT + `TEXINF`
  sub-rect recovery + writer extension), flat normals only (stored gouraud point
  normals not consumed), one material = mean face colour, multi-submesh unsupported.
- E03 Block 2 item 4 (2026-10-10, per-instance draw + game camera): `native/NativeRenderer.*` adds
  bounded resources (`NATIVE_MAX_RESOURCES` 16) and instances (`NATIVE_MAX_INSTANCES` 256);
  `NativeInstance` (resourceKey[160], column-major transform[16],
  level/variant/sourceObjectIndex/modelSlot/modelRevision diagnostics);
  `RegisterResource`/`UnregisterResource`/`ClearResources`/`ResourceResident` (idempotent, capacity =
  named error, no eviction); `SetInstances`/`ClearInstances` (copied by value; unresolved keys skipped
  with a counter + first-key diagnostic, never fatal); `SetClipPlanes`; `GetInstanceStats`;
  `GetFirstInstanceDiagnostic`. No-fallback rule: with instances set the draw is only the instances
  (all-unresolved -> clear + diagnostic, never the placeholder); modes placeholder|content|instances are
  explicit and `LoadContent` stays the N=1 compat path. Projection is +Z-forward with [0,1] depth
  (`out[10]=F/(F-N)`, `out[11]=+1`, `out[14]=-N*F/(F-N)`, Diligent's convention) and +Y-down like the
  game: the F-A fix negates camera-space Y (`BuildPerspective out[5]=-f`) because the game camera is
  +Y-down (GTE `SY=OFY+IR2*h/sz`) while Diligent's D3D-like viewport is +Y-up
  (`DeviceContextVkImpl.cpp:1812-1837`; the modern-mesh path already negated Y, `PsyX_ModernMesh.cpp:105`).
  The reflection is cancelled by the cull pin flip (`CULL_MODE_BACK` + `FrontCounterClockwise=False` keeps
  the same world faces front; the game-front triangle `(v0,v1,v3)` area is negative after the reflection),
  pinned by the new smoke `TestVerticalConvention` (a +y marker the game maps downward must appear in the
  lower half; reverting the fix fails exactly the three vertical checks) and the re-pinned winding fixture
  (`SourceModelDecoderTests` asserts frontArea<0/backArea>0). New pure `utils/NativeInstanceTransform.*`
  (T = `Translate(pos)*RotY(yang*64)` raw column-major; 392 checks) and `utils/NativeCameraView.*` (raw
  view = `[R|-R*cam]`; 9 checks). `redriver2_psxpc.cpp` feeds the real game camera (`inv_camera_matrix` +
  `camera_position`) through `NativeCameraView_Compose` with raw clip planes 100/125000 (`draw.c
  farClipLimit`); a one-shot report/dump is gated on a valid camera (non-degenerate view + non-zero
  `camera_position` + 3 consecutive frames; a degenerate first frame caused an early clear dump) and
  submits the K placements nearest the camera (in-frustum and laterally-visible preferred, fallback
  nearest), logging camera-position, view diag, camera-z first/min/max, inFrustum and laterallyVisible,
  then dumping one 640x360 offscreen RGBA frame via the public readback; a registry miss clears
  instances/unregisters and resets the one-shot state (F-C). `Game/C/assetcatalog_game.{h,c}` gains a
  bounded slot->{key, glbPath, contentHash} registry populated on publish and a bounded resident-placements
  collector reusing the level walker (no re-decode, <=64 placements per slot with the model handle
  revision); the `-drawmodel <slot> [count]` dev flag (accepted in `main.c`; warn-only, named diagnostics).
  Evidence: native renderer smoke 130 checks / 0 failures (was 46 at the start of item 4; adds instance
  lifecycle, screen regions incl. yang=16, culling, depth, no-fallback, real-style +Z camera, vertical
  convention, placeholder); all inspector suites pass (SourceModelDecoder 1186, NativeInstanceTransform
  392, NativeCameraView 9, WorldMeshBuilder 122, others unchanged); `Release_dev` and `Release_dev_native`
  builds exit 0. Real bounded run (Release_dev, `RED2_NATIVE_RENDERER=1 -nointro -nofmv -mission 1
  -drawmodel 169 32`): slot 169, 18 real placements, submitted 18, drawn 18, skippedUnresolved 0,
  residentResources 1; camera-position 17387,-1050,142670; camera-z first 10824.0, min -1481.9, max
  77735.4, inFrustum 14/18, laterallyVisible 10/18; offscreen 640x360 rgba-hash `241175f1491c392b` with 4
  non-clear pixels at y[286..307] (the same x band at y[52..73] before F-A = 360-y, empirically confirming
  the vertical correction). This proves the end-to-end pipeline executes and rasterizes the converted
  resource at its real placements with the game camera; it does NOT prove on-screen shape/region beyond
  what the offscreen suite establishes, because the selected model is a micro-prop (POSITION bounds
  134x130x68 raw ~3.2 cm) and larger buildings are multi-submesh. Limits: one slot + bounded K;
  single-primitive meshes only - bigger buildings fail with the named `multiple-submeshes-unsupported`, so
  the multi-primitive writer extension is the next mandatory item before any "visible building/region"
  claim; unreferenced slots have no placements by data; FOV 1.0472 native vs the game's 0.9265 is deferred;
  the GLB node transform is ignored in instance mode (latent, resources are identity today); no
  textures/LOD/streaming/cars/animation/performance. Gate: attempt 1 verdict was commit-conditional on F-A
  (vertical) verification/fix and the docs; F-A and F-C/F-D were remediated and verified; this
  documentation update resolves F-B.

**Current step: E03 Block 2 item 4 done; multi-primitive writer extension next.**
E03 Block 2 (2a/2.1/2.4/2.5, item 3 and item 4) is complete and gate-approved on
code (item 4 attempt 1 was commit-conditional on F-A and the docs; F-A/F-C/F-D are
remediated and this update resolves F-B); E04 A1 is complete (below). Next mandatory
item: the multi-primitive writer extension (larger buildings currently fail
`multiple-submeshes-unsupported`), then E04 remaining consumer slices. E03 still has
no playable original-world native slice, and no game consumers are migrated.
Evidence and boundaries:
- Fix-5 originally reported AudioContractTests 221/0 and AudioServiceSmoke 49/0;
  the final smoke follow-up is 57/0. Implemented
  behavior includes inclusive endpoints unchanged/equal rejected, concurrent
  samples, explicit RESTART/stream-loop unsupported diagnostics, persistent
  service generations, actual per-channel 2D/3D replacement, checked listener
  updates and an x64 SDK guard. Doppler is disabled pending unit validation.
  Gate approval is bounded to SDK-isolated service behavior. Release-
  failure stream storage lifetime, captured output, device loss and stream stress
  are unverified. Earlier 216/0 and 39/0 are historical pre-remediation only.
- Final nit follow-up: `AudioServiceSmoke` 57/0 (exit 0), SDK fetch check exit 0,
  and `scripts/engine_migration/test_fmod_version.ps1` nine fixtures 9/0. A
  temporary reset-on-restart mutant fails 13/57 checks as expected; live-service
  source hash is unchanged. Both nits are resolved. Test/verifier-only changes
  needed no game rebuild. Temporary logs:
  `temp/gate4b-attempt2-167892a6e365428c8711411e2f7632e5`.
- Independent integration validation: fresh `build_e04_gate4` x64 `Release_dev`
  and `Release_dev_native` builds exited 0; SDK-absent `build_e04_gate4_absent`
  x64 `Release_dev` exited 0. SDK-present Win32 project XML omitted the FMOD
  service/link/define/copy; absent Win32/x64 XML omitted FMOD integration. Win32
  was not built. Copied DLL SHA256 matched SDK DLL
  (`F58048FB39BD10B68E3701A553D632BDFC8BE3C644E636E715BCA8E7049A6797`).
  Catalog/audio contract and remaining suites passed: 258, 221, and
  39/38/28/28/64/25/32/107 respectively. Logs are under
  `src_rebuild/build_e04_gate4/logs`. JPEG generator user/project files were
  restored byte-for-byte. These checks establish build/configuration and bounded
  test behavior, not audio acceptance.
- Acquisition/distribution (E04-B): `scripts/fetch_fmod.ps1` verifies the
  gitignored SDK at `src_rebuild/dependencies/fmod\core` (local version metadata
  identifies **2.03.15**, from header 0x00020315 and DLL resource 2.3.15);
  `knowledge/rules/fmod-integration.md` records version, runtime-only
  distribution, attribution/logo, threading and VAG/CD-XA transcoding rules.
  Consumer migration map: `knowledge/engine-migration/e04/audio-consumers.md`.
- There is no requirement to reserve the final sample frame. No game consumer
  has migrated.
- Game wiring (2026-10-09): premake links `fmod_vc.lib`, defines
  `RED2_AUDIO_FMOD` and copies `fmod.dll` beside the executables when
  the SDK is present (default builds exclude the service unit); the postbuild
  needs absolute Windows-separator paths (xcopy rejects forward slashes).
  `Release_dev` and `Release_dev_native` build exit 0 with the service linked.
- Menu-bank PCM decode + registration seam (E04 P0, oracle-approved attempt 1
  with F1-F4 remediation): `Game/C/sounddecode.{h,c}` is an SDK-free PSX SPU
  ADPCM (VAG) decoder (16 B -> 28 frames, coefficient recursion with cross-block
  predictor carry, filter-nibble validation, inclusive block-aligned loop frames,
  bounded validation with zero partial output). `Game/C/sound.c` glue under
  `RED2_AUDIO_FMOD` decodes samples 0-3 at the `SBK_ID_MENU` load into
  `SOUND_BANK_SFX` and registers `SFX(1,0..3)` via
  `AudioService_RegisterSoundMemory` (streaming=0), unregisters on bank-1 reload,
  registers nothing when base!=0 and is inert until A1 initializes the service.
  Loop start uses the PSX block-start convention, deliberately diverging from the
  fork reference's FIXME-marked `(block+1)*28` (documented; needs an acceptance
  decision when looping samples migrate, outside the FESound slice). Caveat: the
  base==0 window also matches the level's first bank-1 load; A1 adds strict menu
  discrimination. `scripts/run_audio_service_smoke.ps1` (pure decode phase
  without FMOD headers plus the service phase) reports 20 + 87 checks, 0 failures.
- FESound FMOD routing + lifecycle (E04 A1, 2026-10-09): new SDK-free
  `Game/C/audiofmod_adapter.{h,c}` (mode parse from `RED2_FMOD_AUDIO`,
  master-gain, pitch clamp, `SFX(1,sample)` id builder) plus `sound.c` FESound
  routing that skips the SPU body in FMOD_ONLY mode, `LoadSoundBankDynamic`
  lump-identity gating (only `SBK_ID_MENU` registers), service init before
  `redriver2_main` / Update in `VsyncProc` / shutdown at exit in
  `redriver2_psxpc.cpp`. Smoke 31 (pure) + 98 (service) checks, 0 failures;
  `cl /Zs` exit 0 with and without `RED2_AUDIO_FMOD`; regenerated Release_dev /
  Release_dev_native builds exit 0. In-game probe (parent, `RED2_FMOD_AUDIO=1`,
  `-nointro -nofmv`): the service initialised alongside OpenAL and 41 FESound
  calls routed as distinct FMOD handles (0 warnings); switch off stays inactive
  with no route lines. Gate accepted with two informational items recorded
  (linear master-gain mapping vs the SPU curve; dormant lower pitch clamp). No
  audible-fidelity claim.
- E02 Chicago inventory Gate 4A attempt 2 is approved as a scoped diagnostic.
  Final isolated integration run: 4/4 barrels, 6740 unique placements, 227
  straddlers, 0 overlaps/missing model refs, 114 unknown material classifications,
  0 errors/480 warnings (64 displayed). Corrected walker includes terminal
  objects marked `0x8000`; the first 6078 count is superseded. This is a
  source-union diagnostic, not conversion/world acceptance. Full metrics and
  limits: [inventory evidence](engine-migration/e02/chicago-inventory.md).

**Remaining before full acceptance:**
1. E03 Block 2 — item 4 (per-instance draw + game camera) is done; the next
   mandatory item is the multi-primitive writer extension (larger buildings
   currently fail `multiple-submeshes-unsupported`), then the original-world
   conversion seam (blocked on the E02 city conversion slice). Block 2a loader span
   capture, `sourceObjectIndex` identity, ptype coverage, the camera/unit/winding
   fixture, B2.4 CPU texture decode, item 3 and item 4 are done.
2. E04 remaining consumer slices — A1 (FESound routing + service lifecycle) is
   complete; migrate the remaining audio consumers.
3. E02 — other cities/variants and dynamic/streamed categories (E06 coverage);
   debug configurations are not wired (only the Release Diligent libraries exist).

**Next concrete action:** implement the E03 multi-primitive writer extension and
the remaining E04 consumer slices as separately scoped work. Gate approval does not
authorize audible acceptance. E05/E06 remain pending.

**Commands:** build `Release_dev|x64` or `Release_dev_native|x64` with
`premake5.exe vs2022` + MSBuild (env `SDL2_DIR`/`OPENAL_DIR`/`JPEG_DIR`,
`RED2_BUILD_DIR=build_e00`, `RED2_BIN_DIR=bin/Release_dev`);
`pwsh -NoProfile -File scripts/run_native_renderer_smoke.ps1` (130 checks);
Previous smoke commands/results are historical where noted; do not treat bounded
SDK-isolated service approval as consumer or audible correctness.

## Checkpoint commit and current uncommitted work (2026-10-09)

`feat/new-engine` checkpoint `7617bd92` consolidated the E00-E03 implementation,
tests, scripts, knowledge and CHANGELOG, and removed the tracked agent config
(`opencode.jsonc`; `.opencode/` stays gitignored). The PsyCross fork change
(headless presentation, post-frame hook, pad-backend disable) is committed and
pushed as `99950a7` on `origin/codex/new-engine-live-vsync`; the parent records
the new gitlink. Both commits are pushed.

Uncommitted after that checkpoint: the E02 inventory; the E04 blocks
(`Game/C/audiocontract.*`, `tests/AudioContractTests.cpp`, `scripts/fetch_fmod.ps1`,
`knowledge/rules/fmod-integration.md`), the E04 P0 seam
(`Game/C/sounddecode.{h,c}`, `Game/C/sound.c`, `tests/AudioServiceSmoke.cpp`,
`scripts/run_audio_service_smoke.ps1`) and E04 A1
(`Game/C/audiofmod_adapter.{h,c}`, `redriver2_psxpc.cpp`); the E03 Block 1-4
decoder, probe, world-mesh builder and item-4 draw path
(`utils/SourceModelDecoder.{h,c}`, `utils/WorldMeshBuilder.{h,cpp}`,
`utils/NativeInstanceTransform.{h,c}`, `utils/NativeCameraView.{h,c}`,
`Game/C/modelptypeprobe.{h,c}`, `Game/C/assetcatalog*`, `Game/C/models.*`,
`Game/C/spool.c`, `Game/C/main.c`, `native/NativeRenderer.*`,
`tests/SourceModelDecoderTests.cpp`, `tests/ModelPtypeProbeTests.cpp`,
`tests/WorldMeshBuilderTests.cpp`, `tests/AssetCatalogTests.cpp`,
`tests/NativeRendererSmoke.cpp` plus the new NativeInstanceTransform/NativeCameraView
test suites, `scripts/run_inspector_tests.ps1`, `tests/README.md`); and the suite
registration/README plus the status/handoff/changelog updates. The ignored FMOD SDK
is present locally at `src_rebuild/dependencies/fmod/core`; its local version
metadata identifies 2.03.15.

Note: the goal-mode auto-continue safety limit is plugin-side; adjust it in the
plugin/runtime for longer runs.

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

2026-10-09 content decode-base decision: user adopted the hybrid strategy for
original-content conversion - the project's reconstructed decode remains the
authority for identity/bindings/simulation; OpenDriver2Tools is a format/
placement reference and gap-filler only (never a runtime dependency, never a
second shipped parser); one pure decode module serves the offline converter and
runtime consumers. Authorized next steps: continue E03 Block 2 (MODEL span
capture, ptype coverage, one building to GLB, per-instance native draw) and a
bounded external ODT-to-GLB tooling spike. Acceptance gates and sequencing:
[engine-migration discussion](discussions/engine-migration/index.md).

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

## Durable evidence and next action

E00/E01 implementation details and bounded validation are summarized in
[`CURRENT_STATUS.md`](CURRENT_STATUS.md) and the dated records linked there.
E02 inventory findings and limitations are in
[`chicago-inventory.md`](engine-migration/e02/chicago-inventory.md). E03 Block 1
and Block 2 (2a/2.1/2.4/2.5, item 3 and item 4) are done; E03 still has no playable
original-world native slice, and the multi-primitive writer extension follows (larger
buildings currently fail `multiple-submeshes-unsupported`). E04 has the
PCM decode + registration seam (P0) and
A1 routing; neither is audible acceptance — do not describe historical
smoke/contract results as accepted correctness. Continue only the requested
implementation block; E05/E06 remain pending. The wall-collision replay route
remains deferred by the user
([evidence](changes/2026-10-09/effect-timing-replay-regression/index.md)).

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
