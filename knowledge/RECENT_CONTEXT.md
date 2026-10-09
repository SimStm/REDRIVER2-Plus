---
type: Status
title: Recent engineering context
description: Vulkan rework preserved; new engine migration planned from the existing baseline.
tags: [okf, status, agents]
---

# Recent engineering context

> Updated: 2026-10-09.
> Read the handoff below first: E03 Block 1, E04 P0 and the next A1/Block 2 step.
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
- Source MODEL decoder foundation (E03 Block 1, oracle-approved attempt 1 with
  F1-F8 remediation): `utils/SourceModelDecoder.*` is a pure, asset-free decoder
  of the reconstructed MODEL poly block from an explicit `[begin,end)` span
  (MODEL has no byte span; loader span capture is Block 2). Allowlisted ptypes
  11/20/21/22/23, stride `PolySizes[ptype&0x1f]`, world quad vertex/UV order,
  declared 20/22 decoded as triangles, total span/index/normal validation with
  named errors and zero output, unsupported ptypes fail the model, instanced
  models rejected, per-submesh colour/normal flags with raw stored face colours.
  New `SourceModelDecoderTests` 336/0 (synthetic byte-exact fixtures); the full
  inspector runner passes (catalog 258, audio contract 221, others as before).

**Current step: E03 Block 2 / E04 A1 next, in progress.** E03 Block 1 and E04 P0
are complete; the next audio step is A1 and the next E03 step is Block 2. Gate 4B
attempt 2 approved the SDK-isolated service only, not consumer migration or
audible correctness. Parent integration builds/suites and the final Chicago
diagnostic are complete. E03 is still without a playable original-world native
slice. No
game consumers are migrated.
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
- E02 Chicago inventory Gate 4A attempt 2 is approved as a scoped diagnostic.
  Final isolated integration run: 4/4 barrels, 6740 unique placements, 227
  straddlers, 0 overlaps/missing model refs, 114 unknown material classifications,
  0 errors/480 warnings (64 displayed). Corrected walker includes terminal
  objects marked `0x8000`; the first 6078 count is superseded. This is a
  source-union diagnostic, not conversion/world acceptance. Full metrics and
  limits: [inventory evidence](engine-migration/e02/chicago-inventory.md).

**Remaining before full acceptance:**
1. E03 Block 2 — loader span capture (`models.c:131-139`), `source_object_index`
   identity (the E02 placement record lacks it), CPU TIM/page/CLUT decode,
   per-instance draw API, camera/unit/winding fixture, then the original-world
   conversion seam (blocked on the E02 city conversion slice).
2. A1 — FESound routing (runtime SPU_ONLY/FMOD_ONLY switch defaulting to SPU,
   master gain helper, pitch `gamePitch/4096` clamped to `16383/4096`, lazy
   service init, per-frame Update in `VsyncProc`, shutdown at exit,
   `gInFrontend` menu gating, in-game log evidence).
3. E02 — other cities/variants and dynamic/streamed categories (E06 coverage);
   debug configurations are not wired (only the Release Diligent libraries exist).

**Next concrete action:** implement A1 and E03 Block 2 as separately scoped work.
Gate approval does not authorize audible acceptance. E05/E06 remain pending.

**Commands:** build `Release_dev|x64` or `Release_dev_native|x64` with
`premake5.exe vs2022` + MSBuild (env `SDL2_DIR`/`OPENAL_DIR`/`JPEG_DIR`,
`RED2_BUILD_DIR=build_e00`, `RED2_BIN_DIR=bin/Release_dev`);
`pwsh -NoProfile -File scripts/run_native_renderer_smoke.ps1` (38 checks);
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
`knowledge/rules/fmod-integration.md`) and the E04 P0 seam
(`Game/C/sounddecode.{h,c}`, `Game/C/sound.c`, `tests/AudioServiceSmoke.cpp`,
`scripts/run_audio_service_smoke.ps1`); the E03 Block 1 decoder
(`utils/SourceModelDecoder.{h,c}`, `tests/SourceModelDecoderTests.cpp`,
`scripts/run_inspector_tests.ps1`, `tests/README.md`); and the suite
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
(source MODEL decoder) is done; E03 still has no playable original-world native
slice, and Block 2 loader capture follows. E04 has the PCM decode + registration
seam (P0); A1 is the next audio step, and neither is audible acceptance — do not
describe historical smoke/contract results as accepted correctness. Continue only
the requested implementation block; E05/E06 remain pending. The wall-collision
replay route remains deferred by the user
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
