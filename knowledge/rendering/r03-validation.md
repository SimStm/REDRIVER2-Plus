---
type: ValidationEvidence
title: R03 partial execution evidence
status: in-progress
created: 2026-10-07
tags: [okf, rendering, validation, vulkan]
---

# R03 partial execution evidence

R03 is not accepted. Every acceptance checkbox remains pending. R04 is unstarted.
Windows11/RTX3060Ti, VS Release_dev x64/NTSC,1920x1080 Chicago spawn:
`-nointro -mission 50 -gametype 1 -level 0 -playercar 0 -startpos 9046 -217980 -startdir 2785 -players 1 -nativeregion`.

- Initial CPU-only source inventory:144 resident cells/628 refs/98 duplicates,
  530 distinct copied objects, zero missing/invalid/rejected sources,3642
  triangles/672 material parts (before high-detail selection).
- Subsequent actual native city: same530 objects, all144 cells resident,
  zero decoder/capacity rejection, usagePreserved1;4814 triangles/876 parts
  after available high-detail selection.508 opaque/22 cutout/16 animated base
  objects, no water/transparency-flagged objects. Animated count overlaps surfaces.
- Read-only debugger at tick108: nativeDraws876, residentMeshes80,
  residentMaterials33, geometry131040 bytes/material36414208 bytes,
  legacyWorldDraws0/reason ready. Logical object identities/camera-facing trees
  are implemented, but logical picking/turning verification remains pending.
- RenderDoc1.46 capture frame610 replay:876 D32 indexed draws/14442 indices
  (4814 triangles),10 D24S8 UI draws/318 indices, zero debug messages.
  Original road/building/tree art visible; missing sky is a known unfinished
  backdrop. Frame20 capture caught the loading screen and is not city evidence.
- CPU owner/cache/import tests1219 PASS; scene/C ABI18, camera8/material13 PASS.
  New region excluded-platform host compilation PASS. No full foreign-target
  build/runtime claimed. VS build PASS; existing warnings remain. One build
  attempt failed because the owned RenderDoc game still held the output exe;
  closed that process and rebuilt. A misplaced diagnostic timer declaration
  caused another compile failure; moved it into PrepareFrame and rebuilt.
- Measured cumulative collection0.395 ms/assembly45.910 ms means over121
  samples before immutable PNG sharing; after0.324/3.710 ms over481 samples.
  Both include initial uploads (assembly peak421.417/378.642 ms respectively).
  Different windows: no matched benchmark, p95 or GPU time claim. Added a
  bounded recent120 mean/p95 diagnostic. Actual tick600: collection mean0.308/
  p950.396 ms, material/instance assembly mean2.983/p953.540 ms; initial uploads
  are outside this recent window. This is CPU preparation, not GPU/frame timing.
- Latest validation-enabled native GPU suite PASS, including back-facing
  two-sided cutout solid/empty texels and reversed back-cull policy, linear-light
  sampling, original-filter independence, eight material cycles, resize/UI,
  real submitted-upload failure/fence recovery/zero ownership. Fresh legacy PSX
  readback PASS. VS debug output loads Khronos validation and contains zero
  VUIDs/retirement errors on both runs. No process-exit-only acceptance claim.

Private nonredistributed logs/capture/PNG/report/backups:
`src_rebuild/build/native-renderer-r03/`. Five original INIs and `.user` were
restored byte-exactly from its runtime/project backups after tests. R03 fork
`304c14a2cc53b7ff30d9108daf4d5d0afe9b2d8d` is now committed/pushed to
origin/master; parent gitlink staged. Native/PSX reports retained privately.
Latest final guard run at tick1200:530 objects/876 parts/4814 triangles,
unknownBlendPolicy0/usagePreserved1; recent120 assembly mean2.934/p953.137 ms.
No renderer GPU timing or total frame p95 has been established.

## Original backdrop milestone

- New game-owned `NativeBackdropDecode.*` and `DeveloperNativeBackdrop.*`
  consume bounded MODEL/source-art spans before sky GTE projection. Four
  original sky models supply copied indexed geometry and current VRAM art;
  no projected positions or borrowed pointers survive. Actual VS source models
  0..3 each have30 vertices/20 family21 quads. The backend's C ABI v4 layer
  strips camera translation, retains copied rotation and original view-space
  offset, draws backdrop first and disables depth test/write for that layer.
- The first actual city run fell back with invalid descriptor because sky
  materials omitted their required revision. Corrected to the catalog model
  revision, rebuilt and reran successfully; the initial legacy screenshot is
  not native acceptance evidence. VS build Release_dev x64 PASS. An attempt to
  close the solution before an asynchronous build finished opened a warning;
  observed/dismissed it and recovered after build Done. No failed build is
  counted as a passing validation.
- Latest asset/cache/decoder suite1503 checks PASS, including every truncated
  synthetic horizon byte span, bad indices/art counts, quad families and the
  original geometry/UV correspondence. Scene/C ABI20, camera12, material13
  PASS; excluded Android/Emscripten/PSX backdrop host guards PASS. This is not
  a foreign-target build/runtime claim.
- Actual VS tick740: effective1/reasonOK/native956/backdrop80/worldLegacy0,
  owned geometry148320 bytes/material37446416 bytes. These are CPU ownership
  charges, not peak GPU allocation. Region remains530 objects/4814 world
  triangles/876 world part draws; sky adds160 triangles/80 part draws.
- RenderDoc1.46 `native-backdrop146_frame500.rdc` independently verifies80
  D32 depth-test/write-off backdrop draws/480 indices before876 D32
  depth-test/write-on world draws/14442 indices;10 D24S8 UI draws/342 indices.
  All966 draws replay with zero debug messages. After backdrop EID438, D32
  probes at(960,50),(960,1000),(400,500) are1.0. After world EID4821, sky is
  still1.0, world samples are0.9856392145 and0.9993946552. Original sky is
  visibly present in the actual game and replayed final image.
- Expanded `-vknativetest` actual GPU pixels PASS: producer array world-first
  still draws backdrop first; farther red world survives nearer green/blue
  backdrop, camera translation moves world while backdrop stays fixed;
  rotation moves copied backdrop artwork; original UI and retirement survive.
  Existing cull/material/filter/resize/upload-fence failure regressions PASS.
  Fresh `-vkpsxtest` PASS. Direct VS debug output loads Khronos validation and
  contains no VUID/ownership retirement errors.
- A separate CPU backdrop timer exposed repeated art decoding: at tick1080,
  recent120 mean8.676/p959.135 ms. Frame-local immutable decoded artwork now
  shares repeated UV/page/CLUT bindings across horizon models, still decoding
  current VRAM every frame and verifying exact pixels for GPU reuse. Subsequent
  tick480 recent120 mean4.000/p954.185 ms. Different run windows, not a total
  frame/GPU benchmark. World assembly mean2.929/p953.108 ms is separate.
  Both before/after RenderDoc frame500 sky-only images1920x1080 are pixel-exact
  equal; subsequent depth probes/draw counts match and there are no messages.
- Fork83ccdbc65dee841159f275fd113ea696bb11d9c5 committed/pushed to origin/master;
  parent gitlink staged. Five runtime INIs and `.user` restored byte-exactly,
  owned games stopped, VS debugger stopped. Generated new backdrop membership
  retains all10 configuration pairs. Root/fork `git diff --check` PASS.

Still required: logical native picking, outside-region coverage diagnostics,
controlled turns/wide FOV/filter/mod-on/off captures and an actual drive through
the region. Source inventory and compilation cannot accept any of these gates.

## Native mip milestone

- ABI v5 adds trilinear sampling without changing nearest/linear level-zero
  semantics. CPU-owned lower levels use linear-light alpha-weighted area
  filtering, NPOT/thin-safe dimensions and closest attainable cutout coverage.
  All derived RGBA bytes are budgeted before reading the source. Vulkan copies
  all levels and transitions/views the full range; no legacy mip algorithm edit.
- `run_native_scene_tests.ps1`: scene20/C ABI, camera12, material13 and mip13
  PASS, including exact sRGB188, transparent RGB rejection, NPOT last edges,
  both4096x1 orientations, shader-exact0.8 cutoff, retirement and pre-traversal
  total-chain cap rejection. Excluded-platform host guards PASS.
  `run_native_asset_tests.ps1`:1505 PASS, including adapter chain ownership and
  return to level-zero filtering. The first extended cache test left trilinear
  active for a later base-only budget assertion; reset its filter and reran PASS.
- VS Release_dev x64 build PASS, all10 configuration pairs preserved. Direct
  validation-enabled `-vknativetest` PASS: minified128x64 checker at eight pixels
  reads exact188 versus black for both base-only filters; cutout reads unchanged
  background/opaque white with no red halo. Full-chain ownership43692 bytes,
  reset/retirement zero; submitted two-level upload fault retains12 bytes and
  real GPU handles until fence recovery. Fresh `-vkpsxtest` PASS. Both direct
  Debug outputs loaded Khronos validation with no VUID lines.
- Actual Chicago VS tick120: effective1/OK/native956/backdrop80/worldLegacy0,
  owned RGBA49926792 bytes. Nearest at tick240 uses37446416; trilinear restored
  at tick360 uses49926792 and remains effective1/OK. These steady samples do
  **not** pass runtime filtering acceptance: both replacement directions logged
  `resource budget exceeded` and complete legacy fallback before recovery.
  Fix transient replacement ownership/sampler changes before accepting R03.
- Private evidence: `build/native-renderer-r03/native-mip-readback.txt`,
  `native-mip-city-toggle.log`, `mipCity*.json`, PSX/debug output and RenderDoc
  helpers/capture. Five runtime INIs and `.user` restored byte-exactly; owned
  games stopped. No GPU performance/peak allocation or silhouette parity claim.
- RenderDoc1.46 `native-mip146_frame500.rdc` replay PASS:80 depth-off backdrop,
  876 D32 world and10 original UI draws, zero messages. All61 bound native
  material resources have complete chains, full-range views and linear min/mip
  samplers with maxLOD equal to last level; each last1x1 subresource has4 bytes.
  Example2048x1024 has12 levels/maxLOD11. Original sky/roads/buildings/tree
  cutouts and HUD are present in the replay image; this is not full visual
  acceptance. First helper compared the wrong RenderDoc format spelling and
  classified all966 draws as UI; corrected the format match and reran PASS.
- Forkd3e5a4099aec4cf59661ffa55adb7e040385d7b0 committed/pushed to origin/master;
  parent gitlink staged, fork clean. Root/fork `git diff --check` PASS.

## Runtime sampling transition fix

ABI v6 copies per-instance sampling in the owned snapshot. Original world,
backdrop and import fixture keep one full trilinear chain even when runtime
sampling is nearest. Vulkan prewrites up to three retained descriptor sets per
image; nearest/linear clamp maxLOD0, trilinear clamps the last level. Recording
selects a set without creating/replacing images or mutating descriptors in flight.
Missing linear hardware support remains explicitly unsupported; a trilinear
override without a complete generic chain rejects the snapshot atomically.
The descriptor pool remains bounded at3*(256+1) sets.

- CPU scene20/camera12/material13/mip13/sampling8, asset1505 and excluded host
  guards PASS. Sampling tests verify C enum compatibility, copied independent
  choices on one material, unchanged ownership and atomic invalid/unsupported
  publication. VS Release_dev x64 build PASS, existing10 config pairs retained.
- Direct Khronos native GPU suite PASS/no VUID: eight choices change actual
  minified pixels between exact black and188 without new uploads, fallback or
  image/byte replacement. Two instances sharing one image independently read
  nearest black/trilinear188 in one frame. Existing cutout/material/culling/
  backdrop/resize/fault-retention retirement tests PASS. Fresh PSX GPU suite
  PASS, Khronos loaded/no VUID. Full native report8024 bytes is untruncated.
- Actual city started with filtering0: at tick120 native956/backdrop80/world0,
  effectiveFrames115/fallback0/materialUploads61/RGBA49926792. Eight alternating
  setting changes until tick1080: native956/backdrop80/world0 every sample,
  effectiveFrames1075/fallback0/uploads61, same image handle and RGBA bytes.
  Counter differences prove **all960 intervening submissions native**, not just
  successful final samples. No upload/budget-failure log during the fresh run.
  `city-sampling-transitions.json/.log` preserve actual VS expression evidence.
- Five INIs and `.user` restored byte-exactly; debugger/owned game processes
  stopped. This fixes the recorded filter transition defect; picking/outside/
  controlled camera/mod/drive gates still block R03 acceptance. R04 unstarted.
- RenderDoc1.46 nearest/trilinear city captures at frame500 independently replay
  PASS:61 complete native chains/full-range views in both,80 depth-off backdrop/
  876 D32 world/10 UI draws and zero messages. All nearest min/mip samplers clamp
  maxLOD0; all trilinear min/mip samplers are linear with maxLOD last. Sorted
  dimensions/level counts/view ranges/last1x1 RGBA profiles agree across both
  runs.2048x1024 retains12 levels in nearest mode with maxLOD0. Private
  `native-sampling-{nearest,tri}146_frame500.rdc` and replay reports retained.
  These prove sampler selection and retained chains; no total frame/GPU timing
  or full silhouette/visual acceptance claim. Upstream releases rechecked:
  baseline8.0/b2d8857 remains the recorded baseline for Unreleased edits.
- Sampling fix forkc051051a407c6405ca4ba9394a9b0a9a44e3e89e committed/pushed to
  origin/master; fork clean, parent gitlink staged. Text checks PASS.

## Native logical picking milestone

The v7 native API (included in current v8) uses a R32_UINT world attachment with
copied 64-bit placement identities and exact mesh/material handles from the
submitted snapshot. Alpha discard/culling/depth determine the hit; backdrop0,
original UI excluded. One pixel completes after the ordinary owner fence; no
extra picking wait. Bounded game-owned source tables preserve original model,
position/yaw, material-part and texture/page/index/CLUT/UV metadata, retaining
completed source copies even when the inspector is hidden. No borrowed spool
pointers, face highlighting/export or dynamic coverage claim.

- CPU scene20/camera12/material13/mip13/sampling8/picking19 and asset1505 PASS;
  C probe preserves identity0x100000065 and material generation. Host Android/
  Emscripten/PSX exclusion compiles PASS, not full cross-platform builds.
- Native GPU tests PASS: depth/alpha hole, reversed draw order, two shared-resource
  instances with different logical IDs, miss/backdrop/UI, supersession after real
  submission, cancel, resize and retired mesh/scene. Actual mapped-buffer allocation
  fault cleans all local handles, then retry renders/selects correctly. Self-review
  found and fixed partial-buffer allocation publication before accepting this gate.
- Actual Chicago 1920x1080 clicks: building285 at896,0,-220672/yaw48 keeps logical
  identity11956563823583399519 across part1/DTGEN011 (tex:DTGEN011:4:1) and part0/
  DTGENL04 (tex:DTGENL04:4:0). Tree162 at7392,-17,-219232 returns identity
  10562780346825143323 and tex:TREE01:1:9. Sky is an explicit miss. Building source
  remains valid hundreds of frames after selection, despite publication rollover.
- RenderDoc1.46 `native-pick146_frame500.rdc`:966 draws=80 depth-off backdrop,
  876 D32 world,10 original UI; no messages. R32_UINT1920x1080 has8,294,400 logical
  bytes (not device allocation charge),313 visible part IDs. Probes sky0,
  building812/tree708/road89; original UI leaves the entire ID image byte-exact.
  ID SHA2560942a97bbb8633e05b763d4253e92ff7881125f631c609bd71c75b50c36e645f.
  The first helper misread RenderDoc's fixed output-target slots; filtering active
  resources corrected the helper, then replay passed.
- VS Release_dev x64, direct native and fresh PSX suites PASS, Khronos loaded/no
  VUID. Native report9880 bytes untruncated before the subsequent failure-policy
  changes. Private click JPGs/readback/debug/replay reports retained under R03 build
  evidence. Five INIs/`.user` restored byte-exactly after that run.

Logical picking is accepted only for the declared static opaque/cutout slice;
the other region/visual/movement gates remain pending.

## Explicit native scene failure and expected loading

User steering supersedes historical automatic old-world fallback in R01/R02/early
R03 records. Current ABI v8 reports OFF/AUXILIARY/LOADING/READY/FAILED, with separate
submission counters. Game source adapters declare each world preparation/status.
PENDING means explicit source/upload loading; absent publication, stale/unsupported/
invalid/budget/upload results are errors. A failure invalidates pending publication;
per-submission consumption prevents old loading status leaking to a later world.

Unavailable native world frames clear colour every time, suppress old world,
legacy shadow/composites/modern experiments, preserve ordered VRAM/remaining UI
work and show reason/resource diagnostics independently of F11 visibility.
Frontend/loading pages without a world intent/boundary remain auxiliary consumers,
not native successes. No-boundary world errors suppress the whole unsafe on-screen
draw range; no guessed UI split. Existing OpenGL is a separate backend choice;
native options on an unsupported initialized backend fail startup explicitly.

- CPU frame-state13 plus scene20/camera12/material13/mip13/sampling8/picking19
  PASS. Tests cover explicit auxiliary versus missing world, expected loading,
  seven error classes, stale publication rejection and consumed loading state.
- GPU native suite PASS: actual upload-failure centre pixel(32,0,32,255), loading
  (0,0,32,255), versus old synthetic world(248,0,0,255). All failure/loading world
  draws0/suppressed1; tested UI remains1 except no-boundary failure. Missing/stale/
  unsupported/invalid/budget states reject picks instead of old ID reuse.
  Submitted-upload fault retains actual image/staging/command/fence until its
  real fence; replacement native resources produce white(255,255,255,255), then
  retirement returns owned mesh/material bytes to0. This is native recovery proof.
- Actual Chicago breakpoint injection before PrepareNativeFrame: loading/error
  native0/world0/UI10 with754/755 old-world splits suppressed; loadingFrames1/
  failedFrames1. Subsequent real publication returns native956/world0, uploads61,
  unchanged geometry148320 and RGBA49926792 bytes. This producer-reason injection
  complements the actual submitted-upload fault; it is not a real city driver fault.
  F11-closed screenshots show distinct loading/error text and cleared world with
  the remaining minimap. The first autosized diagnostic needed ImGui measurement;
  fixed known dimensions avoid hiding the first failed frame. No GPU timing claim.
- Private `native-explicit-failure-readback.txt`, city state JSON/JPGs and debug
  output retain evidence. Target remains Windows11/RTX3060Ti/610.88/NTSC,
  Release_dev x64, GPU analytic1280x720 and city1920x1080. Remaining R03 geometry/
  visual/camera/mod/drive acceptance still blocks R04; no full-renderer claim.

Final reruns: CPU suites above and asset1505 PASS; VS Release_dev x64 PASS.
Direct native GPU report10966 characters is untruncated, Khronos loaded/no VUID;
fresh PSX GPU regression PASS/no VUID. A real valid snapshot without a world
boundary produces explicit failure pixels; restoring the boundary recovers native
depth/colour. The missing-publication check exercises the backend's actual
NO_SNAPSHOT condition, not just a supplied error reason. A fresh city run shows
the diagnostic on its first failed frame. Actual `-nativeworld -opengl` displays
the unsupported-native startup message and exits1 after acknowledgement, without
entering gameplay; the OpenGL backend code remains intact.

A separate `-opengl` Chicago run actually renders the city/car/HUD: debugger
backend0, native requested=false, FrameCnt880; private
`independent-opengl-city.jpg` retains the visual evidence. This proves the
preserved independent backend, not a native OpenGL port or full GL acceptance.
All owned debug sessions are stopped. Five runtime INIs and the generated
`.vcxproj.user` were restored byte-exactly from `native-failure-backup`; reopened
Visual Studio retains all10 configuration/platform pairs and Release_dev x64.
Fork bd5b67540fcfb3028256ec2c1907212596d55185 is committed/pushed to origin/master,
clean, and recorded by the staged parent gitlink. Normal fork text checks PASS.

## Source/outside/publication accounting milestone

The frame-local ledger independently reconciles 14-bit source slots, copied
material-part counts and producer publication. Shared cell references select once;
publishing a slot twice, mismatching parts or omitting a selected source rejects
the complete native snapshot. Original PVS/frustum-accepted view candidates are
observed read-only before queue limits; outside means absent from all selected
region cells, not necessarily outside model bounds or GPU visibility.

- Asset1523 PASS, including18 accounting checks for slot boundaries, shared
  references, uncopied sources, reordered publication, wrong/duplicate parts,
  reset/recycled slots and invalid slots. Scene20/camera12/material13/mip13/
  sampling8/picking19/frame-state13 plus excluded-platform host compiles PASS.
- VS Release_dev x64 build PASS. Actual Chicago producer tick266:530 selected/
  copied/published,876 copied/published parts,228 view-inside/78 outside,0 uncopied/
  duplicate-view/invalid; usage preserved. Backend prior submission tick265:
  READY/OK/effective1,956 native/0 world/10 UI,0 failed/loading, geometry148320/
  RGBA49926792 owned bytes and61 material uploads. The different ticks are
  explicit because the breakpoint precedes backend preparation/submission.
- Private `region-coverage-city.json`, `region-coverage-vs.log` and JPG retain
  debugger/source/pixel evidence. Hardware/settings match the preceding milestone.
  Outside78 remain declared omissions; no full-city, performance or complete R03
  gate is accepted by source accounting alone.
- New RenderDoc1.46 `native-coverage146_frame500.rdc` replay PASS:80 depth-off
  backdrop/876 D32 world/10 original UI,0 debug messages;313 visible material-part
  IDs, sky0/building812/tree708/road89, UI leaves ID image byte-exact. ID SHA256
  matches the previous picking capture. PIL/NumPy comparison of both full1920x1080
  RGBA PNGs has0 changed pixels/max channel delta0, including the complete UI;
  no claim of wider camera/content coverage follows from this matched state.
- Five INIs/`.user` restored byte-exactly from `region-coverage-backup`, all10
  solution pairs preserved; debugger/capture game/helper processes stopped.
  Source accounting item accepted. Controlled camera/FOV/mod/driving and remaining
  R03 region/silhouette acceptance still block R04.

## Controlled camera/settings and physics-input milestone (2026-10-07)

Same Windows11/RTX3060Ti/610.88/NTSC Release_dev x64 target at1920x1080. Existing
binary/source from the accounting milestone, no new renderer code or build in
this validation. VS owner breakpoint is PsyX_Vk.cpp:5928 before preparation.
Captures use normal DeveloperDebugStart flow, not GPU function evaluation.
Source tick is one later than the captured completed-backend tick.

- Frozen camera(10066,-303,-217498), pitch25, roll0; yaw1312/2336/3360/288 at
  focal256 and128. Projection diagonal changes1.19999993/-2.13333344 to
  0.599999964/-1.06666672. A translation to(8066,-303,-219498) changes native
  pixels. All samples READY/native956/world0/UI10,530 published sources/876 parts,
  zero invalid, gameplay usage preserved. Global failed/loading counters remain0;
  completed native count1048 ->1788 matches FrameCnt1053 ->1793,740 intervening
  frames, including physics-input driving. Geometry148320 bytes stays fixed.
- Actual normal API HdTextureOverrides_SetEnabled toggles revisions383..389,
  including two frozen off/on cycles and one during driving. RGBA49926792 ->
  2174132 ->49926792; original-material uploads+34/restored-HD+33 per cycle,
  with unchanged geometry. Steady off GPU table has62 images/allocations; restored
  on61. Both have0 staging/upload commands/fences/submitted uploads/retiring
  materials. No handles are hidden behind successful old-world rendering.
- PIL/NumPy14 full GPU BMP comparisons PASS. Declared UI bounds are excluded
  only from world metrics. Normal/wide mod-off changes825341/848582 world pixels;
  nearest changes1657883. Restoring camera, FOV, mods or trilinear sampling yields
  zero world pixel differences; residual115..166 full-image pixels are inside
  declared original UI rectangles. Rotation/FOV/translation change more than
  1.7million world pixels. This tests real native outputs, not just flags.
- Finite fixture at StepSim main.c:1016, after UpdatePadData and before normal
  ProcessCarPad/StepCarPhysics:45+1 accel,30+1 accel/left,30+1 accel/right and25
  brake =133 physics steps. Assign only Pads[0].mapped; confirm cp is player0.
  From(9046,-217980)/speed0 to(6405,-222716)/speed33; intermediate speeds89/99/56
  and changed follow-camera poses. Entire route stays inside declared region;
  native remains READY/956/world0 without failed/loading submissions. Wide128/
  mods-off interval renders original images, then normal256/mods return safely.
  Three inspected GPU street BMPs show moved geometry/tree silhouettes/sky/UI.
  A foreground desktop Up pulse did not move the car; interactive keyboard
  driving is not claimed. No physics/camera transform is assigned by this fixture.
- Private camera-mod-final-live.json, drive-final-live.json, camera-mod-pixels.json,
  check_camera_pixels.py, camera-*.bmp, drive-*.bmp and camera-session-debug.log
  preserve values/pixels/actual PASS. An earlier debugger screenshot evaluation
  saved a BMP then its session exited0; cause unconfirmed and excluded from
  continuity evidence. The subsequent normal-capture session completes the checks.
- Camera/settings acceptance gate passes. Straight-drive near road markings show
  alternating strips; widened terrain needs seam review. These visible native
  defects are not accepted. Possible source overlap/depth ordering needs actual
  source/depth/ID evidence before choosing a fix; zBias in the old building path
  alone does not establish their cause. Geometry/silhouette gates still block R04.
- Owned debugger stopped, test breakpoint removed; five INIs and .user restored
  byte-exactly from camera-mod-backup. All10 VS pairs and Release_dev x64 verified.

The subsequent native road-paint source/depth diagnosis, ABI v9 implementation,
actual interior pixels/resource hashes and final regressions are recorded in
[R03 terrain validation](r03-terrain-validation.md). The marking defect above is
resolved for the reproduced normal/wide cases; ground seams remain unaccepted.
