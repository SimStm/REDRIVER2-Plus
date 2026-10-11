---
type: Status
title: Recent engineering context
description: Vulkan rework preserved; new engine migration planned from the existing baseline.
tags: [okf, status, agents]
---

# Recent engineering context

> Updated: 2026-10-11.
> Read the handoff first: native GPU duplication removed; performance improved; full parity remains open.
> Older task narratives are condensed; change records hold the detail.

## Handoff (read first): plan, current step, what must finish first

**Plan:** staged engine migration on `feat/new-engine` — umbrella
[`roadmap/planned/engine-migration.md`](roadmap/planned/engine-migration.md),
execution [`engine-migration/index.md`](engine-migration/index.md), stage
contracts [`engine-migration/stages/`](engine-migration/stages/) (E00-E07; E08
PopcornFX optional, only on explicit request). User scope: at least through E06.
Preserve Driver 2 simulation/math/mission/cutscene/replay/regional semantics;
fixed ticks independent of presentation FPS and CPU speed.

**Current step: native original-city world, vehicle, and bounded pedestrian rendering; no stage is accepted.**
Checkpoint `7e116b8d` is published. Original textures and full city coverage remain required;
HD/mods await functional original-city coverage, and later GLB/glTF edit/reimport remains desired
with undefined contracts. Dynamic car damage is implemented but not visually verified. Player/on-foot
control, all pedestrian classes, and lighting remain open. The end goal includes complete world,
vehicles, Tanner, and all pedestrians.

The existing F9 GPU overlay, sampler controls, actual base-config FPS inheritance, and
OpenGL 3.3 reference startup are validated in earlier records. Resident streaming publishes
owned geometry/pixels/colours asynchronously, retains unchanged GPU keys, and uses bounded upload
batches; source/native placement capacity is 16384. Draw-distance selection and world-shading
milestones are published at `34e104ed` and `09a542d3`. Latest Chicago mission 50 showed 193 world
resources and 8890 placements; this is a sampled resident set, not complete city coverage.
Detailed historical measurements remain in `knowledge/product/native-renderer-performance.md`.

The working HUD is opt-in (`RED2_NATIVE_VISUAL_DEBUG=1`), text-only on the existing overlay and
independent of F9/input capture. Preserve the user's corrected `build_jointcheck` DLL working
directories; do not restore older profile backups. HD/mod work remains deferred until functional
original-city coverage, and later GLB/glTF edit/reimport is desired with contracts still undefined.

The early headless/window/render-target bring-up and performance history remain recorded in
[`product/native-renderer-performance.md`](product/native-renderer-performance.md). The current
milestone extends the bounded original-world path with captured sky geometry; it does not accept
whole-city coverage, complete visual parity, damage pixels or controlled on-foot play.

2026-10-11 car-damage validation: `pwsh -NoProfile -File scripts/run_inspector_tests.ps1`
passed all 21 suites; `pwsh -NoProfile -File scripts/run_native_renderer_smoke.ps1` passed
1285 checks / 0 failures. Native and OpenGL x64 builds passed. Initial MSBuild failed with
MSB8052. Successful commands used VS18 Community
`C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe`:
`MSBuild.exe build_jointcheck/REDRIVER2.sln /m /p:Configuration=Release_dev_native /p:Platform=x64 /p:PlatformToolset=v145 /p:VCToolsVersion=14.51.36231 /p:VCTargetsPath="C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Microsoft\VC\v180\"`
(repeat with `Release_dev` for the reference build). This was command-line selection only, not a
Premake/toolchain project fix.
Logs: `build_e00/native-performance/native-car-damage-native-msbuild.log` and
`native-car-damage-opengl-msbuild.log`. The inspector runner includes `WorldSpriteShadeTests`,
`NativeCarMeshTests` and `NativePedMeshTests`; smoke output is in `build/native-renderer-smoke`.

Chicago TAD50 (`-mission 50 -drawregion -nointro -nofmv`) capture replayed 703 draws with zero
debug messages. Its HUD showed 193 world resources/8,890 placements/523 selected, 14 cars and
56 wheels submitted; steady samples were about 114–120 FPS, 30/31 ticks and 0 interval drops.
Chicago also logged one startup drop and four further catch-up drops around capture; those are not
the steady-window intervals.
Havana TAD uses `-mission 52` directly: `-level 1 -mission 50` had resolved Chicago because M50's
header overwrote `GameLevel`. The corrected log reports level 1, 430 models, 452 textures, 6,669
placements, 279 converted and zero failures. Capture replayed 959 draws/0 debug messages; HUD
showed 277 resources, 554 selected, 12/12 cars and 48/48 wheels submitted, with 8 pedestrians
active (2 ready, 1 submitted, 1 waiting). Both captures show the intended distinct cities, not full
city/stream-route acceptance. Steady Havana samples were about 113–120 FPS, 30/31 ticks, 0 drops.
No session emitted `native-car-damage`; actual damaged appearance remains unverified. Logs and
captures are `native-car-damage-chicago-20261011.{txt,rdc,png}` and
`native-car-damage-havana52-20261011.{txt,rdc,png}` under ignored `build_e00/native-performance/`;
both game processes were stopped. Profile hash
`004E53AAC096DABC2EADD2F0E0C24CC98B795AFA84DE7059CA02340CE5E906E8`, `config.ini` hash
`2A20E6C1BAA18EF0625C3DFA2670C217B57D40DB9222422841B5C296EA2722B4`, and developer INI hash
`C11CF94E5F1045A75FCBC9537FB934A7914E724945EB063C26C34D7C08CE9C15` were unchanged.

The user reports entering the car and moving successfully (2026-10-11); backend and visibility of
controlled on-foot Tanner were not specified. Tool evidence remains bounded to submitted car bodies
and sampled pedestrians; player-exit/on-foot control, an actual damage image, broader streamed-region
coverage, lighting and complete city coverage remain follow-up gates.
The earlier sprite-shading evidence remains in the [native renderer product record](product/native-renderer-performance.md#native-world-sprite-lighting-2026-10-11).

2026-10-11 native sky follow-up: the bounded 20-byte horizon polygon format is supported without
relaxing source-span checks. The 22-suite run preceded the type-5 fix; focused sky tests then passed 45 checks;
latest NativeRendererSmoke passed 1330/0. Serial native and OpenGL x64 builds passed after a
single-node MSBuild retry. Injected RenderDoc captures show 80 sky quads/160 triangles in Havana
and Chicago. Corrected Havana replay: 942 draws/0 debug messages; Chicago ATTRACT.400 frame 1783:
280 draws/0 messages after the 8,594-to-10,167-placement publication. Neither final image shows
the earlier thin horizon line at the visible sky pixels. Chicago HUD: 185 resources, 10,167
placements, 193 selected; 11 cars and 44 wheels submitted, 6 pedestrians ready/1 submitted.
This is bounded two-city evidence, not full coverage. The earlier OpenGL Havana image is not a
matched-tick comparison. Pre-fix PixelHistory files (`pixelhistory_sky_havana.py/.log`) identified
the gray-row/sky-row bilinear phase cause. All captures/logs are ignored under
`build_e00/native-performance/sky-*20261011*`; corrected profile hash A56E272D… was preserved.
Private converted GLBs keep raw texel UVs scaled once by 1/256; external GLB/mod UV contracts remain
future work. No INI was modified. Processes were stopped.
Validation logs: `sky-inspector-tests-20261011.log`, `sky-nativeskymesh-focused-retry-20261011.log`,
`sky-havana-seamfix-smoke-retry-20261011.log`, and `sky-havana-seamfix-{native,opengl}-serial-20261011.log`.
For replay, use `-replay DRIVER2/REPLAYS/ATTRACT.400` from `bin/Release_dev`; bare `ATTRACT.400` fails.
`developer_debug_start.ini` remains at mission 1. `main.c` applies it only without explicit
`-mission` or a successfully loaded `-replay`; tests used those overrides without editing the INI.
Prior bounded parity evidence (preserved; not full acceptance):

- World pipeline: `CULL_NONE`, opaque alpha / `0000h` discard, LEQUAL; legacy world colours
  travel through RGBA8 COLOR_0 and shader scale 2. Region caps are 1024 resources / 16384
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
