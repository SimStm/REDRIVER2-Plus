---
type: Status
title: Recent engineering context
description: Vulkan rework preserved; new engine migration planned from the existing baseline.
tags: [okf, status, agents]
---

# Recent engineering context

> Updated: 2026-10-10.
> Read the handoff first: native GPU duplication removed; performance improved; full parity remains open.
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

**Current step: E03 native performance and window ownership, followed by world parity.**
The prior pending work was committed/pushed as `7fdce013`. User priority on 2026-10-10:
performance first, tick/audio cadence, remaining world textures/models/shaders, then player,
vehicles and other objects. Lighting conversion is deferred until geometry/materials are
correct. No stage is accepted by this checkpoint.

Current goal continues configuration/diagnostics + native F9, then static world/city
coverage and player vehicle/on-foot pedestrian. Configuration now reads `[render]
presentationFps` before developer overrides; absent developer keys preserve baseline.
Native F9 GPU adapter is built and captured over mission 1 (2214 indexed draws, zero
replay messages); legacy texture handles are explicitly unsupported, old PGXP/light
controls disabled. Bilinear sampler replacement requires Diligent's
ALLOW_OVERWRITE flag and a rare GPU idle wait for safe mutable descriptor changes;
GPU smoke passes 1207/0 (blend, new resources, nearest restoration). Timing runner passes
NTSC/PAL settings 24 each, timing 145364 each and effects 2688/2448. Native VS build passed.
The SDK chooses MAILBOX before IMMEDIATE with VSync off. Two images reproduced stationary
~60 FPS / Present ~7-8 ms; requesting three gives ~106-115 FPS / Present ~0.16-0.18 ms,
same stationary view/2212 world draws, panel open, cap 360, ticks 30/31 and zero drops.
Prior moving-camera samples were ~54-59 FPS; `native-overlay-three-images.rdc/png`
shows bilinear output, 2214 draws/zero messages. UI reports actual images; perf separates UI time.
Actual base-config inheritance startup logged cap=144 when config.ini specified 144 and
the developer FPS key was omitted. Both INI files restored byte-for-byte afterwards.
Prior OpenGL startup passed (3.3.0). Generated native debugger no longer auto-opens F9.
Resident streaming now observes local-file sectors, area, model revisions and texture-page
residency. Publications rebuild transforms/LOD provenance, retain unchanged GPU keys and
retire removed keys; loading clears without the renderer fixture. Source/native placement
bounds agree at 16384. ATTRACT.400 changed to regions [198,199,214,215] (8594 placements,
206 resources) then [198,199,182,183] (10167/185); zero conversion failures/truncations.
Synchronous reconversion dropped 107 ticks; reuse reduced this to 37. New preparation
captures owned geometry, indexed pixel buffers and colours, then runs build/hash/cache
publication in a single asynchronous task. Normal frames poll; level loading/shutdown join.
Obsolete source-state results are discarded. GPU uploads use 4 ms batches; region readback
is opt-in (`RED2_NATIVE_REGION_CAPTURE=1`). New pure WorldMeshConversion module preserves
the historical nonstandard hash seed; the compatibility test caught the initial wrong
seed and now passes. Final replay reached tick 5551, two transitions, zero drops before capture.
Published presentation now owns placement/LOD data and retains the previous native scene
during preparation/uploads, retiring old keys after replacement. Replay transitions held
99/35 frames, zero empty loading frames; tick 717 zero drops before capture (+15 during it).
Initial loading/staging overflow still clears; final GPU capture 5897 draws/zero messages.
WorldMeshConversionTests: 14/0 (owned pixels, concurrent .tmp safety, cache identity,
16 MiB owned/64 MiB expanded image bounds). GPU smoke: 1220/0. Evidence:
native-performance/async-world-*. Final RenderDoc: 6347 draws/zero messages, PNG inspected;
capture added 17 drops. Native/OpenGL builds/startups passed; debugger profile restored exactly.
Native draw distance uses preserved cell/list aliases and DrawMap's approved cell record;
F9 control enabled, tests 15/0 (18 suites pass). Tick 30 selects 155/233 instances at
441/1800 with identical camera; GPU captures 308/461 draws, zero messages. Replay through
tick 2128 dropped zero ticks; sector swaps retained 108/38 frames, zero empty frames.
Published cell visibility as `34e104ed`. Orientation-aware building shading passes all 18
inspector suites (WorldMeshBuilder 358/0), native smoke 1249/0 and both x64 solution builds. VS
mission 1 reached level 0 (6740 placements, 160/160 resources, 186 draws). RenderDoc was given
the process PID instead of a target identifier; capture and visual inspection remain pending.
Next: remaining models, prefetch, vehicle/on-foot, PVS/streaming, split-screen and F9/input/display;
full scene parity remains open.

- Fixed dynamic constant updates (Map/Discard), wired configured VSync and added
  conservative AABB culling against the exact homogeneous MVP. Local mission 1 baseline
  3.7-4.6 FPS improved to 41-60 FPS, with 30/31 tick deltas and 0 dropped ticks in
  post-fix one-second windows. GPU/driver/scene and camera position limit comparison.
- User requested no PsyCross Vulkan alongside Diligent. Headless mode 2 creates only
  SDL window + CPU VRAM; skips GL/Vulkan initialization and compatibility GPU draws.
  VRAM copies/reads/clears, OT command parsing and services remain. Native failure exits.
  Native ImGui was pending at that checkpoint and is now wired above. F9 replaces the panel's
  F11, and its previous light-receptivity binding moved to F8.
- VS `build_e00/REDRIVER2.sln`, Release_dev_native|x64 builds and launches mission 1.
  Log confirms no PsyCross GPU, 160/160 resources, 6740 placements and zero conversion
  failures. Existing missing CD-image warnings remain. Ignored performance logs live
  under `build_e00/native-performance/`; `RED2_NATIVE_PERF=1` is opt-in.
- RenderDoc1.41 logs register one Vulkan surface. MCP attach/trigger needed a persistent
  local Python TargetControl connection pumping ReceiveMessage to finish capture/copy.
  Native capture: one colour/depth pass, 5300 indexed draws, 28828 triangles, empty log.
  MCP rt export assumes Descriptor.resourceId; local 1.41 uses Descriptor.resource.
  Local replay exported PNG; target was R8G8B8A8_SRGB, exposing extra gamma encoding of
  UNORM/prelit values. Window target now explicitly requests RGBA8_UNORM; offscreen already
  used it. Second GPU capture verified R8G8B8A8_UNORM at 1280x720, 5705 draws; PNG
  exported. These different-tick images are not a parity comparison. The earlier Window 1 label did not prove two SDL windows; overlay unverified. Computer-control access to
  REDRIVER2_native was rejected by automatic approval review; use supported GPU/log paths.
- Release_dev|x64 compatibility build and -opengl mission 1 launch passed; log confirms
  OpenGL 3.3.0. Native smoke passed 1200 checks; sessions stopped and local diagnostics
  are not committed.
Prior bounded parity evidence (preserved; not full acceptance):

- World pipeline: `CULL_NONE`, opaque alpha / `0000h` discard, LEQUAL; legacy world colours
  travel through RGBA8 COLOR_0 and shader scale 2. Region caps are 1024 resources / 8192
  placements; source instance transforms were verified. Projection uses live GTE
  `scr_z`/`ofx`/`ofy`. Six LOD and four tile-HD variants were observed; per-frame legacy-Z
  key selection is implemented. This supersedes old 64/4096, fixed-FOV, no-LOD and universal
  alpha-blend narratives, but does not establish full scene/material/lighting parity.
- Diagnostics default off; magenta currently enables on environment-variable presence.
  `DelayTicks=600` freezes captures by simulation tick. Both baseline logs reached tick 600:
  legacy dropped 0, native dropped 397. Matching a tick alone is NOT full determinism proof:
  interpolation phases may differ. Earlier wall-clock image comparisons are confounded.
- Pre-quad-fix tick-600 baseline: clear-background count 102440 (11.1%), no-cutout 99811
  (10.8%); magenta confirmed untouched background. Earlier baseline logs certified tick 600,
  but the new saved capture logs do not retain a tick-anchored line; invocation alone does
  not independently certify matched state, and interpolation remains a caveat.
- Source-confirmed decoder fix: raw cyclic A,B,C,D stays stored A,B,D,C with its original
  UV/colour/shade mapping. `012,132` covers ABD+BCD consistently, unlike old `012,023`
  overlap/hole topology. Triangle handling/counts stay unchanged. Index bytes are already
  in `HashSourceMesh`, changing the world sourceHash/cache key; schema stays 1.
- Parent's actual inspector runner now passes all 16 suites (decoder 1271 / builder 347,
  others unchanged), after adding the missing decoder source. Coverage includes interior
  samples, winding/attributes, old-index overlap/hole sensitivity and MODEL -> BuildMulti.
  MSBuild `Release_dev|x64` succeeded with existing LNK4075/LNK4098 warnings.
- Parent captured both overrides with `-DelayTicks 600` into `build/debug-tick-quadfix`;
  Pillow/NumPy `verify_artifacts.py` exited 0 and wrote `verification.json`. Images are
  bounded Chicago mission 1, 1280x720: pre-clear 102440 (11.115451%), new 1733 (0.188043%) each override, reduction
  100707; override RGB MAE 0. Full-image MAE versus legacy: new 17.917348, pre 33.696842.
  Mean RGB-delta bins <20 / [20,60) / >=60: 78.370877% / 13.863281% / 7.765842%,
  denominator 921600 with no exclusions. Observer confirmed navy wedges gone at bottom
  pavement/right wall/top fire escape and broad alignment; textures/shading still differ,
  HUD/car/player are absent. These are artifact observations, not full parity acceptance.
- GLB position+UV triangle-pair sample verifies new ABD+BCD versus old ABD+ADC:
  `world-mesh_043b5f9ac2fa7a9a_f3121675359b72cb_0000000000000000_1.bin` versus old
  sourceHash `3cda93cb69ffadda` with identical options/mods/schema. 160 timestamped new
  cache entries, 288 old preserved; sample inspection is not exhaustive. One-shot logs
  report 160 converted / 0 failed / 6740 drawn / 0 unresolved, not capture-tick statistics.
- Limits: no PAL graphics evidence, full scenes/cars/pedestrians/UI/native VFX/streaming
  parity, complete interpolation/determinism proof or broad performance acceptance.
  E04 remaining consumers are pending; E05/E06 are user-blocked until parity. No commits
  were made in this parity block. See [current status](CURRENT_STATUS.md) for other surfaces.

### Native launch and capture operations

Windows `Release_dev` binary: `src_rebuild/bin/Release_dev/REDRIVER2_dev.exe`.
From repository-root PowerShell, launch through the fire-and-forget helper:

```powershell
$env:RED2_NATIVE_RENDERER = '1'
& .\scripts\run_debug_start.ps1 -Mission 1 -ExtraArgs @('-drawregion')
```

The environment selects Diligent Vulkan; `-drawregion` prepares the resident converted
world. Simulation/game/services still depend on PsyCross, whose presentation is headless;
the native window draws `native_cache` GLBs with live camera/LOD selection. Preparation is
a resident snapshot, not full driving streaming or permission to accept the full game.
Optional `-drawmodel <slot>` replaces the region argument for a bounded single-slot probe.

Capture both texture-override settings (capture mode freezes simulation at tick 600;
omit capture/freeze options for normal gameplay):

```powershell
& .\scripts\run_debug_start.ps1 -Mission 1 -ExtraArgs @('-drawregion') -Capture -DelayTicks 600 -OutDir src_rebuild\build\native-capture
Remove-Item Env:\RED2_NATIVE_RENDERER -ErrorAction SilentlyContinue
```

The reset returns the **next launch** to legacy presentation; it does not switch a running
window. Normal launches need no `RED2_NATIVE_NO_*`, filter or clear-magenta diagnostics.
When troubleshooting, inspect and clear inherited debug environment variables explicitly;
the helper does not promise an automatic reset. Capture-state/interpolation caveats above
still apply. Current evidence fixes large holes, not residual colours/dither or absent
HUD/cars/pedestrians/VFX; PAL graphics and full parity remain unverified and stages blocked.

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
  service outside the bounded A1 FESound routing described below.
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
  claim; only the bounded FESound consumer path is routed.

E02: Chicago inventory Gate 4A attempt 2 approved as a scoped diagnostic. Final isolated
run: 4/4 barrels, 6740 unique placements, 227 straddlers, 0 overlaps/missing model refs,
114 unknown material classifications, 0 errors/480 warnings (64 displayed). Corrected
walker includes terminal objects marked `0x8000`; the first 6078 count is superseded. This
is a source-union diagnostic, not conversion/world acceptance. Full metrics and limits:
[inventory evidence](engine-migration/e02/chicago-inventory.md).

**Next concrete action:** investigate residual colours/shading/dither and missing consumers
only within user scope; retain parent final validation ownership. Future comparisons must
retain capture-tick/state evidence and match interpolation as well as simulation tick.
Continue only user-scoped parity work; E04 consumers remain open and E05/E06 are blocked.

## Publication and next user scope (2026-10-10)

Git inspection confirms HEAD `856b4398` and zero divergence from `origin/feat/new-engine`:
E02 inventory, E04 service/P0/A1 and E03 blocks through texture/finalize-rendering were
already committed/pushed after `7617bd92`. The bounded world-parity, quad correction,
shading/projection/LOD and tick-capture source/test/documentation diff is being committed
as a separate checkpoint. PsyCross is clean at `99950a7`.

The user now requests publication of all pending work, moving the developer panel to F9,
then prioritizing measured native Diligent performance (reported 4-5 FPS) and tick/audio
behavior before remaining texture/model/shader corrections and dynamic scene consumers.
Native lighting conversion from time-of-day, sun direction and lamp data follows correct
rendering. Use Serena for symbols, Visual Studio `Release_dev_native` for native debugging,
`Release_dev` with `-opengl` for the legacy reference, logs and RenderDoc for GPU evidence.
No full-stage acceptance or wholesale engine removal is implied.

Official [OpenDriver2 releases](https://github.com/OpenDriver2/REDRIVER2/releases) page checked
2026-10-11: current baseline is release `8.0`, published 2026-07-02 at commit `b2d8857`.
The 2026-10-10 API lookup below was stale; the changelog comparison baseline is `8.0...HEAD`.

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

The previous metadata discrepancy is resolved by the official release page: release `8.0`
at `b2d8857` was published 2026-07-02. Earlier API-limitation prose above is retained only
as historical context; the current changelog baseline is `8.0...HEAD`.

The [umbrella](roadmap/planned/engine-migration.md), [stages](engine-migration/index.md)
and [validation](engine-migration/validation.md) own execution. PopcornFX is optional E08
only on request after E07; existing effects remain required. Planning-only checks are
recorded separately from E00-E03 implementation below.

## Durable evidence and next action

E00/E01 implementation details and bounded validation remain in
[`CURRENT_STATUS.md`](CURRENT_STATUS.md) and its dated records. E02 inventory limits remain
in [Chicago inventory](engine-migration/e02/chicago-inventory.md). E03 parity source fixes
are not acceptance; verified parent evidence and the next action are recorded above.
E04 PCM registration and A1 routing are bounded implementation, not audible acceptance.
The wall-collision replay route remains user-deferred
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
