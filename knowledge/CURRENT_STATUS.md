---
type: Status
title: Project status
description: Source of truth for everything implemented in this fork and where each surface stands.
tags: [okf, status]
---

# Current status

> Updated: 2026-10-08. Renderer paused; R03 incomplete/R04 unstarted. [Resume checkpoint](rendering/r03-pause-checkpoint.md): two-view composition accepted; original-file topology preliminarily matched530 instances/876 parts/4814 triangles per view, negative controls unrun. [Branch snapshot](changes/2026-10-08/vulkan-rework-branch-snapshot/index.md) preserves existing work; no new stage acceptance.
> Role: **source of truth for what has been implemented in this project.**
> Update after each implementation; confirm against source and `git status`.
> [`RECENT_CONTEXT.md`](RECENT_CONTEXT.md) holds recent handoff details.

## What this project is

REDRIVER2 reconstructs *Driver 2* in C/C++; PsyCross supplies PSX/Psy-Q graphics,
input, audio, CD/GTE and desktop services. It is not an emulator.

- Game code: `src_rebuild/Game/` (`C/`, `engine/`, `Frontend/`, `ASM/`).
- Backend/platform layer: `src_rebuild/PsyCross/` - a Git **submodule pinned to
  the project fork** `git@github.com:SimStm/PsyCross.git` (tracked branch
  `origin/master`). The fork, never `OpenDriver2/PsyCross`, is the build
  source: it carries the Vulkan backend and MoltenVK/portability work upstream
  lacks. Project-specific renderer changes are committed there and recorded by
  staging the gitlink in the parent (`knowledge/rules/psycross-fork.md`).
- Developer tools: `src_rebuild/utils/`, `scripts/`, `PSXToolchain/`.
- Runtime data: `data/` (local game assets; do not modify or redistribute).
- Build generation: Premake 5 (`src_rebuild/premake5.lua`,
  `premake5_psycross.lua`); generated output is not tracked.
- Targets: Windows, Linux, Emscripten/web, Android NDK, PSX. `GAME_REGION` is
  `NTSC_VERSION` by default.

## Renderer

The source-backed implemented baseline is consolidated in
[rendering/current-state.md](rendering/current-state.md). Dedicated older renderer
product/change/plan documents were archived by user request; code remains intact.

### Implemented Vulkan backend and graphics experiments

- R03 ABI9 retains double view/model-first transforms and canonical world-height
  depth/native ties, authored D32 paint bias, explicit errors/alpha/occlusion/IDs/
  fences. Prior grass39558/71120 and paint45191/35601 normal/wide masks pass.
  Mixed-height meshes now derive consecutive indexed ranges: horizontal tops
  retain canonical depth, sides use raster depth, triangle/source order and
  logical ID stay unchanged. Physical range caps4096 per mesh/frame and separate
  32MiB CPU metadata ownership preserve atomic rejection and mesh-fence retirement.
  Public ABI/push/shaders/upload budgets unchanged. CPU17 new range/bounds/fence
  checks/remaining suites, asset1536, VS/native/PSX GPU PASS. GPU11685 top pixels
  per order, holes/nearer floor/IDs/zero-owned retirement pass. Actual1.41 source
  transforms confirm grass569/sidewalk681 worldY17; normal/wide overlap11456/2639
  wrong4462/2111 ->0, D32 bits identical, source-colour56030/tree542851 samples PASS.
  Bindings/index partitions match;876 parts/1023 world/80 backdrop/10 UI; geometry148320.
  Native FP32 filtering retains the strict two-byte gate, fifteen fractional/six
  mip/clamp GPU regressions; eleven-view tree3079701 colours/87152 bands PASS.
  All44 original-art/36 HD parts have colour evidence. Four/five replay warnings,
  a Pixel History/attachment discrepancy, GPU cost, vertical-side city pixels,
  West144 lists/530 slots match530 copies. Source-road1217385 colour/ID/depth samples/
  4616 occlusions/3861031 backdrop colours/795828 sky samples pass; blue cap is original art.
  [Auxiliary producers](rendering/r03-auxiliary-producer-validation.md):12 wheel3/39 body vertices belong to Car#11/model1/high/palette3; HD/VRAM4/1 batches are clipped/no colour change. [Count scopes](rendering/r03-count-contract-validation.md): static80/876/ranges match; auxiliary-baseline/combined remain false. [Complete605](rendering/r03-clipped-static-validation.md#complete605-footprint-and-canonical-ties-2026-10-08):212377 source-colour/ID/D32/117047 outside-footprint samples,0 exclusions;125 prior exclusions resolved,74 canonical ties/501-605 equation/shader hashes match. Source/post/24 raster variants establish a scoped clip/viewport model; seven pixel/four trace mutations reject/order reversal passes. Side range correctly back-culled/151073 crop pixels unchanged. [Vertical surfaces](rendering/r03-vertical-static-validation.md) and [fragment traces](rendering/r03-raster-depth-trace-validation.md):22 part/views pass886246 colour/ID/D32/12181 occlusions/185206 outside-footprint samples,0 exclusions;34 traced decisions/three exact ties, buffer/reflection/state and seven corruption/comparison controls pass. Native raster references supplement source/artwork; no universal offline arithmetic or driver guard-band claim. Prior failures/receipts retained. [Complete backdrop pixels](rendering/r03-full-backdrop-validation.md):3861830 source-colour/796034 visible sky/285370 outside-source preservation samples PASS,0 exclusions;206 sky edges resolved. Source/post masks match; seven controls reject/404 raw hashes intact. Direct1.41 final SDK1.425s/1.357s matches attachments/28 materials/196 mips/80 states per view, four HIGH messages retained. [Complete-view audit](rendering/r03-complete-view-coverage-validation.md):all1023 ranges/876 parts/33 world materials per view match hashes/partitions;3351166 observed native pixels have source/post geometry attribution,0 footprint differences.8919 observed sky absences explained,58 threshold pixels/72 traces/16048 steps; source sampled alpha/exact backdrop/ID/D32 pass. Six resource/geometry and20 sky controls reject; original hashes intact. Exports41.262s/32.849s/final traces2.609s/2.627s via1.41, four HIGH warnings retained. [Source-generated scene composition](rendering/r03-complete-composition-validation.md): expected owners computed before final IDs, all1023 ranges/876 parts/33 materials per view.4145830 decided colour/ID/depth/sky pixels PASS;1370 alpha/depth decisions unaccepted, including277 provisional source/observed-ID differences.40 state/pixel controls reject/14 original hashes intact. Actual1.41 shader/constants/depth/blend/sample state match, final SDK14.406s/13.768s/host42.656s/31.531s exit0; no RPC timeout. Four HIGH diagnostics retained; no production/build/game/new capture/GPU fixture. [All-candidate composition references](rendering/r03-composition-trace-validation.md) subsequently resolve1370 exclusions/277 provisional ID differences:2873 source-selected traces/764548 steps,4147200 full two-view colour/ID/depth/sky pixels PASS,0 errors/exclusions;1365 traced solid winner depths bit-exact/five exact sky preserves.42 new controls reject/20 original hashes intact; prior40 controls retain14 hashes.119 exact depth tie comparisons retain native LESS_OR_EQUAL order. Thin-facade UV fixed-gate failure retained; derived conservative FP32 compatibility envelope supplements strict raw-artwork/source-colour/alpha/LOD/depth checks, no exact/universal interpolation claim. Direct1.41 SDK163.973s/84.143s exit0 without RPC timeout. Four HIGH warnings/no clean waiver; no production/build/game/new capture/GPU fixture. Other-view/original-source/region/silhouette/resources/cost remain pending. R06/R10 debt persists; forkae83dd2 unchanged.
  Four HIGH messages are [persisted capture diagnostics](rendering/r03-capture-warning-provenance.md), exact metadata/text match; five app image/no buffer barriers per frame and official1.41 internal HOST_READ/ALL_COMMANDS support tooling attribution (injected stack uncollected). Direct native GPU PASS/exit0 24.931s, validation enabled/no recorded VUID. Messages/validationClean=false retained. Fixture enum propagates failure to exit1; real no-driver/native+PSX/guards pass. [Exit evidence](rendering/r03-fixture-exit-validation.md). Probe removed/recompiled, six settings hash-restored; four warnings/count gate retained. R03 incomplete/R04 unstarted. [West evidence](rendering/r03-west-road-validation.md),
  [tree evidence](rendering/r03-tree-view-validation.md), [depth rule](rules/native-coplanar-depth.md).
  Prior camera/settings gate: exact restored pixels, native956/world0 over740
  frames, fixed geometry/restored61 images/no uploads;133-step physics/follow-
  camera drive PASS, keyboard unclaimed. [Evidence](rendering/r03-validation.md).

- R03 source/publication accounting reconciles530 selected/copied/published
  Chicago objects and876 parts, with228 original view-inside/78 outside candidates,
  zero uncopied/duplicate/invalid. Outside sources remain declared omissions.
  Asset1523 (18 accounting checks), existing scene/host suites and VS build PASS.
  New RenderDoc1.46 frame500:80 backdrop/876 world/10 UI/no messages; colour and
  ID images byte-exactly match the previous capture. Geometry148320/RGBA49926792
  owned bytes and61 uploads unchanged. Test INIs/`.user` restored byte-exactly.
  Accounting item accepted; other R03 visual/movement gates still pending.

- R03 ABI v8 policy (retained by v9): automatic native-to-old-world scene fallback is
  disabled. Loading, failure, ready and remaining auxiliary consumers are
  explicit; unavailable world frames clear their image, suppress old world/
  composites and display reason/resource diagnostics. Native recovery preserves
  fence ownership. CPU frame-state13 and GPU error/loading/new-native-image
  recovery checks PASS; actual Chicago loading/error world0/UI10, recovery
  native956/world0, geometry148320/RGBA49926792 bytes unchanged/uploads61.
  Existing OpenGL backend remains separate; unsupported native startup fails.
  Logical native picking uses R32_UINT depth/cutout IDs, asynchronous one-pixel
  readback and copied original model/placement/material metadata. CPU picking19,
  GPU allocation rollback/retry/supersession/resize/retirement and real Chicago
  building/tree/miss checks PASS. RenderDoc1.46 confirms IDs/80 backdrop/876
  world/10 UI, no messages; UI does not alter IDs. Picking and explicit failure
  gates accepted at that milestone; current camera/settings evidence is above.
  No native face highlight/export or whole-renderer acceptance. R04 unstarted.
  Fork bd5b675 committed/pushed, parent gitlink staged. Native-on-OpenGL startup
  exits1; independent OpenGL Chicago city renders. Test INIs/`.user` restored.
  Source and details: [R03 evidence](rendering/r03-validation.md).

- R03 in progress: camera-independent Chicago cell inventory measured144 resident
  cells/628 references/98 duplicate references/530 copied distinct objects, with
  no missing/rejected source;508 opaque/22 cutout/16 animated base objects.
  Initial inventory3642 triangles/672 part instances. Actual high-detail draw:
  4814 triangles/876 parts, usage bitset intact, world legacy0; RenderDoc1.46
  confirms876 D32 draws plus10 UI draws/no messages. Shared material ownership
  and C ABI v6 culling/backdrop/filter contracts are
  implemented; payload caps512 meshes/4096 instances/32MiB geometry and256
  materials/64MiB RGBA. Immutable PNG owners remove large per-frame copies;
  measured assembly mean45.910 ms/121 samples before,3.710 ms/481 after
  (different cumulative windows, initial uploads included). Latest asset1503,
  scene20/camera12/material13 tests and VS build pass. Four original horizon
  models now submit80 unlit native backdrop draws with copied art/brightness
  and camera-relative offset. RenderDoc1.46 frame500 confirms80 depth-off
  backdrop draws,876 depth-on world draws and10 original UI draws; no messages.
  D32 sky probes remain1.0; world probes become<1.0. GPU readbacks also prove
  backdrop ordering, camera translation/rotation, UI and retirement.
  Frame-local shared art cuts recent backdrop CPU mean8.676 to4.000 ms;
  before/after1920x1080 sky-only replay pixels are identical (CPU windows only).
  Native trilinear mip chains now use linear-light alpha-weighted area filtering,
  closest attainable cutout coverage and full-chain64MiB ownership accounting.
  Asset1505/scene20/camera12/material13/mip13 CPU cases and actual GPU
  minification/cutout/upload-retention readbacks PASS. City tick120/360 retains
  956 native draws/zero world fallback with49926792 owned RGBA bytes. ABI v6
  per-instance sampling chooses prewritten retained descriptor sets over one
  full chain; original runtime filters no longer replace textures/geometry.
  Scene sampling8 and actual GPU independent-instance/eight-toggle checks PASS.
  Actual city eight toggles/ticks120-1080 keep all960 frames native,61 uploads
  total/zero additional, fallback0,956 draws and fixed image/RGBA ownership.
  At that sampling milestone, picking and visual coverage remained pending;
  latest picking/failure evidence is above. R03 is not accepted.
  Native two-sided/back-cull readback and fresh PSX regression PASS, Khronos
  loaded/no VUID. Recent120 assembly mean2.983/p953.540 ms at tick600; CPU only.
  Forkc051051 pushed to origin/master; parent gitlink staged. Five INIs/`.user`
  restored byte-exactly after tests; generated configuration pairs preserved.
  [Product](product/native-static-region.md), [evidence](rendering/r03-validation.md).
- R00 native-replacement baseline is implemented:17-category producer counters,
  F11 coverage table/optional log and read-only Windows Vulkan capability query.
  Native counts stay0; older static experiments are distinct. Inventory/contracts,
  measured hardware/CPU/process memory, numerical future budgets and repeatable
  city/replay/wide-FOV/playground/fixture states are recorded.12 invariant/C ABI
  tests, VS build and fresh legacy readback passed; matched city captures are
  RGB-identical with 773 draws. Five INIs and debugger settings restored exactly.
  GPU time/renderer memory peaks and cross-platform acceptance remain unknown.
  [Product](product/native-renderer-baseline.md); R01 is accepted below.
- R01 has an opt-in indexed analytic world (`-nativeworld`, F11 Rendering),
  a copied C scene boundary, bounded generational mesh handles and fence-driven
  upload/retirement inside the existing Vulkan owner. Final game camera matrices
  drive RH native geometry with independent D32 depth; original UI uses legacy
  D24S8. Native mode suppresses the old world/composites and preserves ordered
  VRAM work.26 CPU/C ABI/camera cases and excluded-platform host guard checks
  pass; the new `-vknativetest` GPU readback and fresh `-vkpsxtest` pass, including
  resize/minimize/restore, clipping, winding, camera/FOV, UI and eight reloads.
  A resize extent defect was found and fixed by those tests. Installed RenderDoc
  1.46 city capture:1 native indexed draw +10 UI draws, zero messages; actual
  post-VS/depth state inspected. **R01 accepted for the tested Windows slice**;
  PGXP-on/off city PNGs are byte-identical; wide FOV, native UI/checkbox/rebuild
  and OpenGL unsupported-native city fallback checked. Direct VS Vulkan validation
  is clean; resize capture5 has a legacy PRESENT/GENERAL instrumentation warning
  documented separately. Fork99fa641 pushed; gitlink staged. Five INIs/debugger
  settings restored byte-exactly. Original categories remain
  unmigrated; this analytic fixture does not complete original-world rendering.
  [Product](product/native-renderer-bootstrap.md),
  [evidence](rendering/r01-validation.md). R02 accepted; R03 in progress, R04-R12 pending.
- R02 is accepted on the tested Windows original-import slice: byte-bounded owned original static/placement and
  4/8/16-bit texture decoders, separate loader-span metadata and copied native
  PNG/CLUT resolution using existing mod precedence and revision invalidation.
  Native C ABI v2 adds generational materials; the same Vulkan owner uploads
  sRGB images/clamp samplers and retires descriptors after submitted use. Initial
  upload waits on a dedicated fence in the existing queue. Asset/cache/GLB1202, catalog145/export120,
  scene/C ABI18/camera8/material11 checks and host guards passed; shader generation
  and VS build passed. Validation-enabled GPU readback passes analytic regression,
  exact188 linear-light midpoint, cutout, independent legacy filtering and eight
  material cycles; fresh legacy PSX readback also passes. No VUID output. Owned
  geometry cache now keys model AND parent catalog handles/version/LOD links;
  asset/cache/owned-GLB suite totals1202 passing checks. Pre-GTE Chicago probe
  records69 entries/257972 accounted bytes,68 complete/one unsupported with
  cached rejection accounting corrected. Owned bollard counts/bounds/UV/PNG and
  unchanged file hash verified through shared primitive contract. Native GPU
  original cache now owns indexed per-part mesh/material handles and keys content,
  palette, mod revision and filtering, with catalog invalidation and atomic rollback.
  `-nativeassetview` adds an incomplete Chicago road/building import fixture;
  actual road36/source37 and facade273 sample shows14 native draws/world0/UI10,
  four meshes/materials and enabled PNGs/bilinear1, with clean direct validation.
  RenderDoc matches four complete vertex/index/artwork hashes and14 GPU transforms,
  with zero messages. Live filter/PGXP/mod/reload and actual catalog slot/context
  reuse pass; four resources return to7176/4247296 owned bytes. Forced reset during
  creation found/fixed orphan resources; CPU regression and actual GPU repeat pass.
  [Product](product/native-original-assets.md), [evidence](rendering/r02-validation.md).
  Decoder allocation-error reporting itself performs no allocation; submitted
  Vulkan upload wait failure now retains command/staging/image/fence until actual
  completion. Real-submission fault injection passes legacy pixels/draw fallback,
  retained handles, fence recovery and zero ownership; validation loaded/no VUID.
  Shutdown retains all resources on unconfirmed device-idle failure.
  Original atlas crops include source UVs beyond named texture rectangles;
  active PNGs retain declared regions and explicit clamp semantics.
- Desktop defaults to Vulkan; `-opengl` remains the existing fallback. Vulkan
  still renders the PSX/GTE primitive stream through `GR_*`/`PsyX_Vk_Game*`,
  including CLUT, texture windows, dither, bilinear sampling, blend/stencil,
  ordered VRAM uploads, offscreen effects, minimap and framebuffer mirror/export.
  Current caps are 65,536 submitted vertices and 512 game texture slots.
- Resize, minimize/restore, out-of-date acquire/fence and depth/stencil-copy defects
  have fixes. Swapchain image persistence is per acquired image, not a guaranteed
  previous-presented-frame history. D32 fallback has no mask-bit stencil.
- Modern indexed mesh/GLB fixtures, reference metallic/roughness PBR, eight-light
  set, optional shadow/AO/legacy-light composites and original source fixture
  assets exist. The importer is bounded, not full glTF coverage. The game modern
  path still shares legacy projection/depth and reconstructs legacy lighting
  positions/normals from depth. Game-derived point-light publication is capped
  at four. World/overlay composition uses a game-provided OT boundary; split-screen
  still uses the older end-of-frame fallback.
- Experimental original static migration shares eight resource keys/sixteen textures
  across up to 128 placements (default 64), converted types 11/21/23, 8-256 polygons
  and four materials. Source normals light eligible scenery. Roads, tree cutouts,
  transparency/water, cars/pedestrians, damage/event geometry stay outside this adapter.
  It requires single-player Vulkan, both PGXP settings, nearest filtering and no
  enabled registered overrides. An enabled checkbox alone is not effective coverage.
- Resident opaque world casting is independent of camera/PVS submission but never
  loads missing regions. The two-tile 4096x2048 D32 atlas uses 32 MiB even with far
  disabled; player-centred snapped volumes are not camera-fitted cascades.
  Far requested range defaults 18,000 (10,000-40,000 units); collection caps at
  65,536 triangles with complete requested/75%/50%/near-only overflow retries and
  one-tier recovery after 120 successful frames. Actual CPU light volumes/source
  bounds reduce unrelated geometry; no GPU-mapped CPU reads or retained slot pointers.
  Optional legacy casters remain camera-submitted and depth-valid only.
- Source/instance generation, VRAM changes and unload invalidate pooled static
  resources. Camera selection uses already resident data; `UpdateSpoolPC` caches
  16 MiB raw spool data, not a fully decoded native city. Gameplay loading and
  simulation activation distances have not changed.
- Previous milestone evidence: VS MCP Release_dev x64 and `-vkpsxtest` passed;
  fresh RenderDoc 1.46 capture/first replay had zero messages. At 18k sampled
  triangle counts fell 21,766->8,751 and 28,632->8,535 with identical colour/raw
  shadow maps. A 40k low-sun route fit both volumes at 48,477 triangles.
  These are sampled parity/collection results, not universal visual correctness.
  Real dense-scene tier recovery, game GPU time and peak memory remain unvalidated.
- User reports sky/facade artifacts and limited shadow reach. Composite shader
  review exposes a sky-depth classification candidate; exact screenshot/facade
  causes require matching capture/pixel history. Original prelit textures and
  painted skyline require explicit native material/backdrop policy.
- Current presentation remains tied to native cadence (NTSC 30 Hz/PAL 25 Hz).
  Rendering has counter-driven effects/state mutations; an FPS unlock is not implemented.
  Windows is the verified runtime; Linux/web/Android and macOS/MoltenVK coverage
  must not be inferred from Windows checks.

### Rendering documentation replacement (2026-10-06)

- Implemented documentation surface: [rendering/index.md](rendering/index.md),
  baseline, requirements review, architecture, file compatibility, glossary,
  validation gates and thirteen detailed stage records R00-R12.
- [The native renderer roadmap](roadmap/planned/native-vulkan-renderer.md) is the
  single active graphics plan. That reset implemented no new rendering code;
  R00 baseline/diagnostics were subsequently delivered on 2026-10-07 (above).
  R01 analytic bootstrap and R02 original imports are now accepted on Windows;
  R03 remains in progress. Adopted 2026-10-08 order: R00-R07 -> R10 -> R11 -> R08 -> R09 -> R12; Chicago water is not a R03 blocker, Rio coverage stays pending. Documentation checks PASS; no runtime change or new stage accepted.
- Fifty dedicated older graphics documents/supporting files were moved with
  original SHA-256 verification and link maintenance to `knowledge/outdated-ideas/`.
  AGENTS, the active archive rule and a Cursor wrapper exclude it from routine
  analysis unless explicitly requested. Shared asset/mod/tool contracts remain active.
- The Vulkan/OpenGL compatibility backlog remains at its mandated path, unscheduled.
  Graphics behaviour, source files, settings, data and PsyCross were not changed
  by the documentation reset.

## Textures, mods and the inspector

- HD texture mods: discoverable JSON mods under `mods/<id>/manifest.json` with a
  deterministic `mods/enabled.json` load order; entries match on
  `(texture, texturePage, textureIndex)`, may carry a `clut` for palette
  variants, and `modelReferences` as metadata that never changes the key.
- Opt-in proportional override alpha (default off) makes `BM_AVERAGE` blend the
  imported PNG alpha instead of the binary 0.5 cutout; opaque, additive and
  subtractive draws keep the compatibility cutout and the CPU picker mirrors the
  active mode.
- Source-aware asset catalog (`AssetCatalog.*`): stable resource identity
  separate from runtime instances, explicit provenance, many-to-many model to
  texture/LOD links, and invalidation when slots are reused or the level
  changes. Covered by the standalone `AssetCatalogTests`.
- 3D Debug tab: click-based primitive inspection with Face/Material/Component/
  Logical-object scopes, catalog context, override previews, detail TXT export,
  single and batch PNG export (deduplicated by identity, cooperative across
  frames with progress and retry), and a live page-registration cost diagnostic.
- Diagnostic tooling: configurable selection highlighting, draw-source labels,
  live car-slot positions, zero-TIM flicker diagnostics and alpha-semantics
  notes in `knowledge/product/`.

## Gameplay and developer tooling
- Resident procedural playground (roadmap item 13, P1-P5): generated flat
  surface, collidable boxes, donor traffic/police/missions disabled, car reset,
  safe return to the frontend, a `-playground` launch path and a **Playground**
  entry on the Take a Ride city screen. Fixture id `playground.flatpad.v1`.
- Debug start snapshots: `developer_debug_start.ini` restores mission, city,
  vehicle, position, heading, player count and chase at startup; the panel's
  **Reproduce this state** generates the command line; `scripts/run_debug_start.ps1`
  captures scenes with overrides on and off.
- Developer Graphics Panel (F11), Dear ImGui, Windows/Linux only, persisted in
  `developer_graphics.ini` (schema 8). Live controls cover `dynamicLights`,
  `widescreenOverlays` and `fastLoadingScreens`; the panel file overrides
  `config.ini` defaults. Reader/apply and save were verified.
- Display modes use `PsyX_ApplyWindowMode`: fullscreen desktop and window-size
  presets with a 15 s keep/revert confirmation, drawn in a separate bottom-left
  window even when the panel is closed, with cursor and input capture. Windowed
  persistence, revert and picking at changed sizes were verified; fullscreen
  remains untested. Timer contribution is capped at 0.25 s per frame; Vulkan
  stops producing frames while minimized, so the timer resumes on restore.
  See [`product/runtime-settings-gui.md`](product/runtime-settings-gui.md).
- Panel layout: checkbox/slider help follows the label; Input uses
  `Action | Binding | Also bound to | Reset` with wrapping conflict notes.
  **Modern sun and look** exposes the modern light settings. See
  [`changes/2026-09-21/lighting-and-panel-followup`](rendering/current-state.md).
- Input-binding editor (runtime-settings-gui milestone 4): game/menu keyboard
  and controller mappings with click-to-capture, Escape/right-click/Cancel/10 s
  cancellation, conflicts and per-action/device reset. `DeveloperInputMapping.*`
  owns the model; `SwitchMappings` restricts live edits to the active table and
  capture holds game input. Synthetic SDL events verified capture, mapping
  isolation, conflicts, cancellation and reset to `config.ini` defaults.
- Runtime settings GUI complete (milestone 5, 2026-09-20): **Content and
  language** exposes content override, Chicago bridges, language and Driver 1
  music with reload/restart requirements; restart-only Free camera is disabled
  with its reason through `gFreeCameraConfiguration`.
  `DeveloperSettingsFile_WriteKeys` preserves comments/unknown keys, removes
  unwanted keys and writes through `.tmp`/`.bak`; `developer_input.ini` stores
  overrides only. Comment/key preservation, binding round-trip, override removal
  on reset and the seeded Input-tab override were verified.
- Direct MSBuild/rdc capture/local1.41 runner avoids30s daemon RPC; explicit GPU/
  errors/ordered SDK shutdown. Fresh build/native GPU/capture/33s replay/pixels PASS;
  reports equal MCP. R03 incomplete; VS MCP retained for debugger investigations.
- `AGENTS.md` requires readable, cohesive C++, separated responsibilities,
  explicit error handling, no unnecessary global state or out-of-scope refactors,
  and a self-review for duplication, coupling and unclear execution paths.

## Knowledge process

- Vulkan-first development rule (2026-10-06): `AGENTS.md` and
  [`rules/vulkan-opengl-port-tracking.md`](rules/vulkan-opengl-port-tracking.md)
  require every Vulkan modification to be recorded in the separate
  [`OpenGL compatibility backlog`](roadmap/planned/port-vulkan-to-opengl-compatibility.md).
  OpenGL implementation is deferred; the backlog has no priority, execution
  order or deadline. The user authorized Vulkan continuation on 2026-10-06.
- `knowledge/` is an OKF bundle: `rules/` (repeatable constraints), `product/`
  (how a surface works), `discussions/` (evolving topics with evidence),
  `roadmap/planned|done` (planning vs completion records), `changes/<date>/`
  (why a change shipped).
- This file summarizes implementation; `RECENT_CONTEXT.md` is the handoff; `CHANGELOG.md` holds release entries (baseline `8.0`/`b2d8857`).

## Validation commands that actually exist

| Command | What it proves |
| --- | --- |
| VS build `Release_dev\|x64` (or `premake5` + `gmake2`) | the tree compiles |
| `REDRIVER2_dev.exe -vkpsxtest` | Vulkan PSX readback, CLUT, offscreen, VRAM export |
| `REDRIVER2_dev.exe -vkfixture -vkcapture N -vkshot <bmp>` | modern fixture render + capture |
| `REDRIVER2_dev.exe -opengl` / `-vulkan` | backend selection still works |
| `pwsh -NoProfile -File scripts/run_inspector_tests.ps1` | `AssetCatalogTests`, `InspectorExportTests` |
| `git diff --check` | whitespace-level hygiene |
| `git -C src_rebuild/PsyCross status` | fork and gitlink state |
