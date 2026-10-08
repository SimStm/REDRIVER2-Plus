---
type: Plan
title: Native renderer implementation sequence
status: planned
created: 2026-10-06
tags: [okf, rendering, implementation]
---

# Native renderer implementation sequence

R00-R02 are implemented and accepted for their declared Windows scopes;
R03 is in progress and R04-R12 remain pending. Existing experiments are described in
[current state](current-state.md); they do not automatically satisfy new stage
acceptance. This plan replaces the archived hybrid graphics roadmaps.
The [umbrella roadmap](../roadmap/planned/native-vulkan-renderer.md) owns lifecycle.

**Execution paused by the user on2026-10-08; the goal was cancelled.** Resume
from R03 only on a new instruction. The [pause checkpoint](r03-pause-checkpoint.md)
records accepted two-view composition, the preliminary original-topology audit,
unrun negative controls, private evidence locations and the next concrete steps.

## Adopted execution policy (2026-10-07)

This separate development version focuses entirely on the native Vulkan renderer,
with the final objective of replacing **100% of the old graphics path** in the
declared supported modes. Native failures must never select the old scene renderer.
This decision supersedes automatic scene fallback in earlier milestone records.

- Explicit source/resource loading is an expected state, distinct from missing
  publication, unsupported modes/assets/capabilities, stale handles, exhausted
  budgets and GPU failures. Display the state/reason and resource/source diagnostics.
- Unavailable native world frames clear their image and suppress legacy world
  drawing/composites. Retry or recovery may use the native path after readiness
  and synchronization are established; old scene pixels never prove recovery.
- Add no new legacy features or legacy recovery mechanisms. Remove dependencies
  as their last consumers migrate. Preserve still-required UI, menus, loading,
  effects, secondary targets and platform services until their own replacements
  pass acceptance. Inventory these as pending consumers, not fallback success.
- OpenGL backend selection remains a separate decision. Preserve its existing
  code/guards; a native scene request on an unsupported backend fails explicitly.
  This policy does not authorize deleting OpenGL or implementing its native port.
- Keep error reporting, bounded ownership, fence-protected lifetime and cleanup.
  Adapt fallback tests to assert explicit failure pixels/draws and genuine native
  recovery/resource retirement. Test expected loading separately from real errors.
- Finish the first incomplete stage, **R03**, one item at a time. Advance only
  after every stage gate has verifiable native behaviour, pixels and resource
  evidence. Compilation, lack of crashes or an image produced by the old path
  are insufficient. The adopted scope covers all thirteen stages; execution is currently paused.

Current pending dependency inventory: original frontend/loading/pause and HUD
overlay draws, minimap/overhead-map and ordered VRAM/offscreen consumers (R10);
cars/pedestrians/damage/event/dynamic world geometry (R06); city residency outside
the fixed R03 slice (R04). These dependencies cannot make a failed native world
frame render successfully. Audit and remove each when its consumer is migrated.

## Sequence

Execution order (user-adopted 2026-10-08): **R00 -> R01 -> R02 -> R03 -> R04 ->
R05 -> R06 -> R07 -> R10 -> R11 -> R08 -> R09 -> R12**. Stage identifiers and
filenames remain stable; their numeric values no longer define execution order.
Native effects/UI and independent presentation precede image-quality refinement
and production modern-asset support. R03 remains the first incomplete stage.

- **[R00 — Baseline, coverage inventory and contracts](stages/00-baseline-and-contracts.md)**: Define a reproducible baseline and the native renderer boundary before adding rendering behaviour.
- **[R01 — Native renderer bootstrap and camera/depth](stages/01-native-renderer-bootstrap.md)**: Render genuine native 3D geometry with a conventional camera and reliable depth inside the game.
- **[R02 — Decode original files into persistent native assets](stages/02-original-asset-decoding.md)**: Build native geometry from existing Driver 2 files without requiring conversion of the installation.
- **[R03 — Complete native static region](stages/03-complete-static-region.md)**: Prove a coherent original-world slice, including all static surface types, before extending the whole city.
- **[R04 — Whole-city visual resources, streaming and draw distance](stages/04-city-residency-and-distance.md)**: Make camera-visible and shadow-relevant original city resources available beyond the legacy resident/PVS limits.
- **[R05 — Native sun, materials and global shadow coverage](stages/05-native-lighting-and-global-shadows.md)**: Replace depth-derived legacy lighting/shadows with consistent native surface lighting and cascaded sun shadows.
- **[R06 — Native cars, pedestrians and dynamic world geometry](stages/06-dynamic-geometry.md)**: Render and shadow every dynamic 3D category without changing gameplay or physics.
- **[R07 — Clustered Forward+ and scalable local lighting](stages/07-scalable-local-lights.md)**: Support a large city light population with a bounded visible lighting workload.
- **[R10 — Native effects, UI and every game mode](stages/10-effects-ui-and-game-modes.md)**: Complete the graphics path beyond world meshes, so the native mode no longer relies on the PSX GPU for presentation.
- **[R11 — Independent presentation with preserved simulation](stages/11-frame-pacing-and-interpolation.md)**: Allow smoother native presentation without changing Driver 2 simulation, regional timing or replays.
- **[R08 — Anti-aliasing and subtle image quality](stages/08-anti-aliasing-and-image-quality.md)**: Reduce jagged/flickering edges and improve image consistency without obscuring original art or UI.
- **[R09 — Production GLB/glTF and modern PBR overrides](stages/09-modern-assets-and-pbr-mods.md)**: Accept modern 3D content through the same native scene while keeping original installations and texture mods compatible.
- **[R12 — Full-game acceptance and native renderer cutover](stages/12-full-game-validation-and-cutover.md)**: Make the native Vulkan renderer the validated default for its supported desktop targets.

## How to execute a stage

Read this catalog, the chosen stage, current state, status/handoff, applicable
active rules and live source/diffs. Implement the smallest coherent accepted
slice; use verified direct MSBuild/capture/local RenderDoc1.41 analysis for routine
checks, reserve Visual Studio MCP for debugger state, and consult configured Vulkan
documentation/capability tools. Keep observing the same process across tool yields;
a client response limit is not an execution failure. Use owned fixtures and
original-city cases; the playground is complementary, not the sole proof.

Each stage records objective, dependencies, concrete steps, deliverables,
acceptance, planned validation and native failure handling. Record executed results with hardware,
settings, source revision and real limits. Failed acceptance leaves the stage
pending or partially delivered; do not mark it complete because its code compiles.
Publish/update product documentation for implemented surfaces and update
CURRENT_STATUS, RECENT_CONTEXT, CHANGELOG and the Vulkan/OpenGL backlog.

The adopted scope covers all thirteen stages; the user cancelled the execution goal. Review and
validate one item at a time within each stage; retain incomplete gates across
sessions and advance only when every gate passes. Routine reversible decisions
can be made autonomously; ask only when missing direction materially changes scope.

## Dependencies and bounded parallel preparation

The main implementation/acceptance order is the adopted sequence above. R00 can establish
AA and GLB contracts before their production stages; R05 reference lighting can
be tried on the R03 slice while R04 residency is validated. Read-only importer,
lighting or timing investigations may happen earlier without claiming delivery.
A complete static region precedes full-city reach; reliable residency precedes
claims of global shadows. Dynamic coverage precedes full-game lighting claims.

R10 follows R07 using original content and the native world/material/target
contracts; production GLB/glTF support and final AA are not prerequisites.
R11 follows accepted R10 and the draw-time mutation audit, preserving fixed-tick
simulation. R08 then regression-tests accepted effects/UI, multiple views and
interpolated presentation. R09 follows R08 and validates replacements against
the same accepted scene/presentation contracts. R12 still requires every stage.

R03 Chicago water is not an acceptance blocker: its copied source inventory has
no water surfaces, and the user identifies apparent water there as empty space
over the sky/background. Record water as not applicable to this slice; validate
the backdrop and actual cutout/transparent content without inventing water geometry.
Rio water remains future category coverage during its city-specific validation,
before full-game R12 acceptance. This does not accept pending R03 terrain or silhouettes.

R11 is included to absorb the former high-FPS plan; native-rate operation remains
valid during earlier stages. Temporal AA/upscaling, ray tracing, a full world
editor, authored replacements for painted skyline and a new OpenGL renderer are
optional follow-ups and do not block completion. Meshy is optional test authoring,
not a network requirement or a mandatory credit spend.

## Progress record

- 2026-10-08: user adopted R10/R11 before R08/R09, retaining stage IDs and
  acceptance-before-advance. Dependencies and regression gates updated; Chicago
  water is not applicable, with Rio coverage retained. Documentation only;
  no renderer behaviour changed and no additional stage accepted.
  Validation: scoped `git diff --check`, stage-order/dependency assertions,
  local-link/text hygiene and both status files under350 lines PASS. No runtime
  build or GPU test was run for this planning update.

- 2026-10-07: R03 bounded source/publication accounting accepted as an item,
  not the stage.530 Chicago sources reconcile with876 material parts; original
  view228 inside/78 outside candidates are explicit. Asset1523 (18 accounting
  checks), scene/host suites and VS build PASS. New RenderDoc1.46 frame500 has
  80 backdrop/876 world/10 UI/no messages; complete colour and ID images equal
  the previous native capture. Owned geometry/RGBA bytes and uploads unchanged.
  Test INIs/`.user` restored byte-exactly. Next: controlled turns/wide FOV,
  mod/filter images and actual driving. Remaining R03 gates block R04.

- 2026-10-06: created the adopted documentation structure and archive policy.
  No stage implemented; no runtime behaviour changed. Next: R00.
- 2026-10-07: R00 delivered source category counters, device queries, native
  ownership/data contracts, numerical budgets and original-city/replay/wide-FOV/
  playground/analytic states. Build,12 invariant/ABI tests and fresh PSX readback
  passed; matched city colour/pass counts unchanged, RenderDoc1.46 first replays
  had zero messages, five INIs/debugger settings restored byte-exactly. See
  [product](../product/native-renderer-baseline.md) and
  [executed evidence](r00-measured-baseline.md). User goal authorizes the full
  sequence, acceptance one stage at a time. Next: R01; no native world shipped yet.
- 2026-10-07: R01 delivered the opt-in indexed analytic world, copied C resource/
  snapshot boundary, final game camera, separate native depth and original UI.
  VS build,26 CPU/C ABI/camera tests, excluded-platform host compilation, native
  GPU pixel/resize/minimize/reload/rollback and fresh PSX tests passed. PGXP city
  PNGs identical; clean city/wide/post-VS captures, actual UI selector/rebuild and
  GL unsupported-native fallback checked. Direct Vulkan validation clean;
  instrumented resize legacy layout warning is isolated/documented. Fork99fa641
  pushed/gitlink staged; five INIs/debugger settings restored exactly. All R01
  gates accepted for the tested Windows analytic slice. See
  [product](../product/native-renderer-bootstrap.md) and
  [executed evidence](r01-validation.md). Next:R02; original categories unmigrated.

- 2026-10-07: R02 original-import slice implemented and accepted on Windows.
  Native GPU material readback/regression and fresh PSX tests PASS, validation
  loaded/no VUID.1202 asset/cache/owned-GLB checks,37 scene/camera/material cases,
  catalog145/export120 checks, host guards and VS build PASS. Read-only pre-GTE
  Chicago collection recorded69 cache entries,68 complete/one unsupported, stable
  257972 accounted payload bytes; explicit cached fallback counts corrected.
  GPU resource/material cache now keys content, palettes, mod revision and filter;
  atomic rollback/retirement and original atlas UV expansion have targeted checks.
  Real submitted-upload wait-failure injection passes retained handles, original
  world fallback, actual fence recovery and zero ownership. Five INIs/debugger
  file are restored from R02 backups after checks. Four original vertex/index/
  artwork hashes and14 GPU/source transforms match in RenderDoc1.46, no messages.
  Live PGXP/filter/mod/UI reload and catalog slot/context reuse pass. Reset during
  creation found/fixed orphan resources; regression and actual GPU repeat pass.
  [Product](../product/native-original-assets.md), [evidence](r02-validation.md),
  [accepted R02 gate](stages/02-original-asset-decoding.md). Next: R03 complete region.

- 2026-10-07: R03 in progress. Fixed Chicago source-cell inventory copies530
  distinct placements from144 resident cells, skipping98 duplicate references.
  Actual high-detail draw876 material parts/4814 triangles, zero legacy world
  draws; RenderDoc1.46 frame610 confirms876 D32 draws/14442 indices plus10
  D24S8 UI draws/318 indices, no messages. Logical identities, quantized original
  placement/camera-facing basis, shared materials and explicit two-sided culling
  are implemented. Asset/cache1219 and scene18/camera8/material13 checks PASS;
  VS build PASS. Immutable PNG ownership removes per-frame large image copies;
  actual material/instance mean fell from45.910 ms/121 samples to3.710 ms/481
  samples, both including initial uploads. These cumulative windows differ;
  no matched p95 or GPU timing claim. Original native backdrop is now implemented:
  bounded pre-GTE source/art copies, C ABI v4 camera-relative layer,80 depth-off
  draws/480 indices before876 world draws. Frame500 D32 sky probes remain1.0,
  world probes<1.0, zero messages; native rotation/translation/UI GPU readbacks
  and fresh PSX regression PASS. Latest asset1503/scene20/camera12/material13
  PASS. Picking, outside-region
  diagnostics and camera/mod/drive acceptance remain pending. No R03 acceptance
  box is checked; R04 has not started. Product/evidence records below describe
  this partial implementation, not a completed region. Subsequent direct native
  two-sided/back-cull GPU readback and fresh PSX regression PASS, Khronos loaded/
  no VUID. Recent120 assembly p953.540 ms at tick600 (CPU only). Fork304c14a
  pushed; gitlink staged; five INIs/`.user` restored byte-exactly after testing.

- 2026-10-07: R03 mip milestone: ABI v5 complete native trilinear chains with
  linear-light alpha-weighted NPOT filtering, closest attainable cutout coverage
  and full-chain64MiB ownership. CPU asset1505/scene20/camera12/material13/mip13,
  direct native GPU minification/cutout/fence-retention and fresh PSX suites PASS.
  Khronos loaded/no VUID. RenderDoc1.46 city frame500 verifies all61 complete
  material chains/views/linear mip samplers,80 backdrop/876 world/10 UI draws,
  no messages. Steady native956/RGBA49926792 bytes; live filter replacements
  initially exceeded the cap and fell back before recovery; subsequent ABI v6
  per-instance sampling keeps one full chain and prewritten retained descriptor
  sets. CPU sampling8 and GPU independent-instance/eight-choice tests PASS.
  Actual city eight toggles/ticks120-1080 retain all960 native submissions,
  uploads61/no additional/fallback0/image and49926792 RGBA bytes unchanged.
  At this sampling milestone, picking/outside/visual/drive gates remained pending.
  No R03 acceptance box was checked at that milestone; R04 remains unstarted.

- 2026-10-07: native GPU picking now uses the actual R32_UINT world attachment,
  shared alpha/depth/culling and asynchronous one-pixel fence readback. Copied
  frame identities resolve original logical model/placement/material artwork;
  CPU picking19 and actual GPU/Chicago building/tree/miss/RenderDoc checks PASS.
  The R03 logical picking gate is accepted; face highlight/export remain pending.
  Subsequent ABI v8 removes automatic native-world scene fallback, distinguishes
  expected loading/errors/remaining auxiliary consumers and reports visible
  diagnostics. CPU frame-state13 and GPU explicit error/loading/native recovery
  checks PASS; real city failure/loading world0 and recovery native956 verified.
  The remaining R03 geometry/visual/camera/mod/drive gates still blocked advancement
  at that milestone. Subsequent controlled camera/settings validation is below.
  R04-R12 are unstarted; see [executed evidence](r03-validation.md).

- 2026-10-07: bounded source/publication accounting passes530 objects/876 parts,
  no invalid/uncopied/duplicate counts and exact matched RenderDoc colour/ID pixels.
  Controlled four-quarter normal/wide FOV, translation, nearest/trilinear and
  mod-off/on readbacks pass native956/world0, exact restored world pixels and
  bounded resources. A133-step mapped-pad physics fixture moves the car/follow
  camera, including steering, braking and wide-FOV/original textures. The R03
  camera/settings gate passes. Actual near road markings exposed alternating strips;
  the subsequent diagnosis/fix follows. Terrain seams and complete static pixel
  coverage remain unaccepted; R04 remains unstarted.

- 2026-10-07: R03 authored coplanar terrain layers delivered with ABI v9, bounded
  source classification and Vulkan constant depth bias, without vertex/art changes
  or old-scene recovery. Captured paint interior wrong IDs17050 ->0/45000 normal
  pixels; wide/original art0/35470. Exact original/fixed geometry/image hashes,
  shared colour/ID depth, holes, nearer occlusion, reversed order and retirement
  PASS. Asset1536, scene22/remaining CPU/host suites, VS build, native/PSX GPU
  regressions PASS;80 backdrop/876 world/10 UI/no capture messages. See
  [terrain evidence](r03-terrain-validation.md). Five INIs/`.user` restored exactly.
  A widened ground seam and remaining region/silhouette gates still block R04.

- 2026-10-07: R03 large-city transform precision corrected. A12-coordinate source
  regression fails old float projection/view-first composition; double CPU
  view/model-first composition and GPU near-face/two-origin/order/ID/retirement
  tests PASS. Actual normal/wide paint coverage persists; grass plane-slope spread
  falls to8.01e-8. Equal-depth grass overlaps now exhibit visible z-fighting and
  remain unaccepted, not hidden through false depth or an old scene. Next: native
  overlap ownership and original-city surface proof. R04 remains unstarted.

- 2026-10-07: R03 reproduced equal-height terrain conflict resolved with canonical
  horizontal fragment depth and explicit native submitted-order ownership. Copied
  source planes produce identical coefficients; authored paint keeps bounded bias,
  cutout/nearer occlusion/IDs/retirement pass and invalid transforms fail explicitly.
  CPU15 plane checks and GPU11685 pixels per order PASS. Real normal/wide visible
  overlaps39558/71120: wrong IDs17677/34474 ->0; paint45191/35601 also0. Bound
  geometry/all-mip artwork unchanged;80 backdrop/876 world/10 UI/no messages.
  Source artwork boundaries and remaining geometry/silhouette/water/drive coverage
  still require evidence. R03 incomplete; R04 remains unstarted.

- 2026-10-07: user-configured RenderDoc1.41 `rdc-cli`/`rdc-tools` now captures
  and inspects normal/HD and wide/original native frames. Colour/ID/depth and
  bound geometry/all-mip art match previous final evidence exactly. New1.41
  inspector retains initial/per-event diagnostics: five HIGH replay barrier
  messages remain visible, `validationClean=false`; no full R03 acceptance.
  See [tool transition evidence](renderdoc-141-validation.md). Continue R03.

- 2026-10-07: R03 tree inspection through1.41 now verifies44 bound two-sided
  cutout parts. Source-alpha/ID/depth/occlusion masks pass for three tree footprints
  at normal/wide FOV. Colour oracle has five normal failures; filtered sample
  diagnosis is partial, probes restored. See [tree evidence](r03-cutout-validation.md).
  No tree/surface gate accepted; R04 remains unstarted.
  Subsequent five-pixel traces match shader/framebuffer bytes; centre sampling
  isolates fractional filtering at276,472. Device query reports8-bit texel/mip/
  raster precision. Subsequent direct native 2x2 constant-UV fixture reproduces
  all five captured colours; RenderDoc1.41 MCP confirms ten exact comparisons.
  Uniformity/filter/ownership/retirement invariants and native/PSX suites PASS,
  but strict ideal accuracy remains unaccepted. Fork7616e0d pushed/gitlink staged;
  INIs/VS restored. Establish the accuracy oracle and remaining tree views next.
  Subsequent native FP32 filtering corrects those errors without relaxing the
  two-byte gate: fresh1.41 normal/wide40598 colour samples have zero errors.
  Fifteen direct fractional and six mip/clamp GPU cases, native/PSX/CPU suites
  and existing terrain ID masks PASS. Bound geometry/all-mip hashes unchanged.
  Fork426c9a3 pushed/gitlink staged; settings restored. Five HIGH replay warnings,
  GPU cost and remaining tree/surface views remain pending; R03 is incomplete.
  Subsequent three new1.41 heading captures and a tightened five-view oracle
  pass1150047 eligible colours/35929 boundary-band comparisons. All44 parts have
  footprint evidence,39 opaque colour evidence; parts731/733/734/735/751 and
  excluded boundaries remain unaccepted. See [view evidence](r03-tree-view-validation.md).

- 2026-10-08: six additional RenderDoc1.41 frame500 captures extend the tree
  oracle to3079701 eligible colours/87152 neighbour-band comparisons across
  eleven views, zero scoped errors. All44 original-art parts now have opaque
  colour evidence,36 HD; tiny part751/remaining HD/edges are not full silhouette
  acceptance. Content inventory matches after excluding capture-local IDs only.
  New eastnorth terrain stippling and west ground/backdrop coverage require
  camera/source diagnosis. Four HIGH replay warnings per new capture remain.
  Fork426c9a3 unchanged; six settings files restored/all10 VS pairs/Design.
  No build/self-test rerun claimed. [Evidence](r03-tree-view-validation.md);
  R03 remains incomplete, R04 unstarted. Compatibility VK026.

For future partial progress, amend this section with stage, delivered slice,
product/source evidence, actual validation and remaining acceptance. Never infer
completion from the existence of a document or from archived milestone numbers.

- 2026-10-08: R03 mixed-height top conflict resolved after copied transforms prove
  worldY17 for grass569/sidewalk681. Native consecutive indexed ranges preserve
  source order/bytes/logical IDs with bounded physical draws/CPU metadata and
  fence retirement. Actual1.41 normal/wide overlap11456/2639: wrong4462/2111 ->0;
  identical depth bits,56030 source-colour and542851 tree regression samples PASS.
  All source bindings/payloads match;876 logical parts now1023 physical world draws.
  CPU17 new checks/remaining suites, asset1536, VS/native/PSX GPU PASS. Four HIGH
  replay warnings and cost remain pending. Forkae83dd2 pushed/gitlink staged;
  six settings files restored/all10 VS pairs/Design. [Evidence](r03-mixed-height-validation.md).
  Subsequent direct source investigation reconciles144 cells/628 references/
  530 unique slots with530 copies; the old west spawn has an empty resident cell.
  Model54/part309 contains replacement[-7000,-229300]. New normal/wide1.41 views
  reconstruct actual GPU camera[-7726,303,-230042] with ten source/MVP/post-VS checks;
  road1217385 source-colour/ID/depth/4616 occlusion samples pass, unchanged resources
  and729600 native lower pixels/view. Subsequent backdrop3861031 colour/795828 sky
  samples pass, no visible sky without source; angular cap is original1x1 art.
  Negative control rejects one blue byte+5. Auxiliary14/11 has4/1 fully clipped
  PGXP draws (51 vertices/17 triangles, unchanged colour) plus ten screen-space
  draws/342 vertices. Exact producer/605/edges remain open; four HIGH warnings retained. Direct build/native
  GPU/fresh capture/33s replay/normal810425 pixel rerun pass, report equals MCP.
  [Evidence](r03-west-road-validation.md), [direct workflow](renderdoc-direct-workflow.md).
  Six files hash-restored; no renderer code change or whole west/R03 acceptance.
  Next within the same item: clipped PGXP producer/auxiliary attribution, then remaining
  region/silhouette/side/cost checks; R04 unstarted. Compatibility VK027/VK028.

- 2026-10-08: [fixture exit propagation](r03-fixture-exit-validation.md) fixes
  discarded native self-test failures. Real missing-driver exit1 and restored
  native/PSX PASS exit0 verified; host guarded branches compile. Fresh1.41 normal
  game startup retains all native geometry/mips/bindings and native pixels.
  Pixel/region acceptance remains separate; R03 incomplete, R04 unstarted. VK030.

- 2026-10-08: [west auxiliary attribution](r03-auxiliary-producer-validation.md)
  matches all51 PGXP vertices in two fresh1.41 captures to12 wheel3/39 body
  vertices of Car#11, model1/high/palette3. Actual HD/VRAM bindings explain4/1
  draws; all17 triangles clipped/no colour change, native hashes exactly match
  uninstrumented captures. Negative byte control rejects; probe/settings restored.
  Retain car/wheel R06 and screen-space R10 debt. Count gate stays failed pending
  contract reconciliation; next605/edges/region/cost. R03 incomplete, VK031.

- 2026-10-08: [capture count scopes](r03-count-contract-validation.md) distinguish
  static80/876/full indexed ranges from the unchanged ten-draw auxiliary baseline.
  Three host tests/eight scenarios and two1.41 real replays pass. All prior report
  fields/resources/pixels/messages match exactly; combined and clean-validation
  remain false. R06/R10 debt persists. Next605 clipped geometry/edges/region/cost;
  R03 incomplete/R04 unstarted. No runtime/build/settings change. VK032.

- 2026-10-08: [part605 clipped surface](r03-clipped-static-validation.md) passes
  source/MVP/post-VS gates and six-plane geometry/attribute clipping with actual
  CCW/back culling. Existing normal/wide1.41 captures pass212270 horizontal
  source-colour/ID/D32 samples and117029 outside-footprint crop checks. Copied
  one-byte colour corruption is rejected.125 excluded raster/depth pixels and
  vertical ranges remain pending; no full west/R03 acceptance. Production code,
  settings/backend and forkae83dd2 unchanged. Continue sides/edges/region/cost
  within R03; R04 unstarted. Compatibility VK033.

- 2026-10-08: [static vertical face/culling](r03-vertical-static-validation.md)
  proves expected back-face rejection for605 sides and native raster-depth
  colour/ID/D32 for visible762:175832 samples/29158 outside-footprint crop
  checks pass, buffers/full mips match. Five copied mutation controls fail;
  originals unchanged. Per-component FP32 source arithmetic bound replaces
  an inappropriate copied small-model threshold; pixel gates stay unchanged.
  Other faces/78 facade raster pixels/previous exclusions/full region/cost remain
  pending; four HIGH warnings retained. No production/build/settings change;
  R03 incomplete/R04 unstarted. Compatibility VK034.

- 2026-10-08: ten more [visible static parts](r03-vertical-static-validation.md#additional-ten-part-coverage-2026-10-08)
  pass709948 source-colour/ID/D32,12136 occlusion and155912 outside-footprint crop
  checks; twenty full source/mip/range bindings match. Initial815 oracle failures
  lead to independent within-draw nearest-depth/foreground-edge handling, with
  unchanged strict gates and preserved failed receipts. Order reversal gives
  identical rows; copied matrix/depth mutations reject.504 raster/65 ambiguous
  pixels and other surfaces/full region/warnings/cost remain pending. No production/
  build/game/settings change; R03 incomplete/R04 unstarted. Compatibility VK035.

- 2026-10-08: [warning provenance](r03-capture-warning-provenance.md) proves the
  four HIGH messages are persisted capture diagnostics, not newly generated
  replay errors. Each frame has five application image barriers/no buffer
  barriers; official1.41 internal readback matches HOST_READ/ALL_COMMANDS.
  Attribution remains source-backed without an injected-binary stack. Messages/
  validationClean=false remain unchanged. Direct native GPU suite PASS/exit0
  24.931s with validation enabled/no recorded VUID. No production/build/new capture
  change; full region/edges/cost remain pending. R03 incomplete/R04 unstarted. VK036.

- 2026-10-08: [facade edge evidence](r03-vertical-static-validation.md#complete-part762-raster-footprint-and-remaining-edge-failure-2026-10-08)
  resolves762's78 prior raster exclusions: complete175892 source-colour/ID/D32
  pixels and29176 outside-footprint crop checks pass, unchanged strict gates.
  Source/post-VS fixed-grid coverage, shared-edge/order and copied corruption
  controls pass. Broader twenty-view set exits1: normal779 at1125,397 is uncovered
  natively despite predicted coverage. Preserve failure and diagnose actual
  raster arithmetic next;66 broad depth exclusions remain. No production/build/
  game/replay change; R03 incomplete/R04 unstarted. Compatibility VK037.

- 2026-10-08: [FP32 viewport evidence](r03-vertical-static-validation.md#fp32-viewport-model-resolves779-coverage-2026-10-08)
  resolves779's oracle mismatch: one vertex snaps one subpixel differently,
  correctly predicting preserved sky.22 part/views pass886209 colour/ID/D32,
  12152 occlusion and185206 outside-footprint samples with0 raster exclusions;
  source/post coverage identical, three corruptions/order controls pass.66 depth
  ambiguities/other surfaces/region/cost remain pending; old failures retained.
  No production/build/game/replay change. R03 incomplete/R04 unstarted. VK038.

- 2026-10-08: [bounded depth evidence](r03-vertical-static-validation.md#bounded-depth-decision-and-retained-ownership-ambiguity-2026-10-08)
  resolves32 of66 exclusions with the existing5e-7 D32 bound:22 part/views pass
  886226 source-colour/ID/D32,12167 occlusion and185206 outside-footprint samples.
  34 remaining samples pass separate attachment-integrity checks but source winner
  stays unaccepted. Eight mutations fail/order reversal identical; promote depth
  to FP64 before bounds, retain initial rounded-threshold receipt. No production/
  build/game/replay change; R03 incomplete/R04 unstarted. Compatibility VK039.

- 2026-10-08: [raster-depth traces](r03-raster-depth-trace-validation.md) verify34
  formerly uncertain decisions:20 native/14 preserved/three exact D32 ties,
  source-selected primitives/reflection mappings/exact shader-depth bits and
  buffer/state checks.22 part/views pass886246 colour/ID/D32/12181 occlusion/
  185206 outside-footprint samples with0 exclusions. Seven controls reject.
  GPU raster-input reference supplements source/artwork; no exact offline winner
  or universal arithmetic claim. Other surfaces/region/cost remain pending.
  No production/build/game/new capture/GPU fixture; R03 incomplete/R04 unstarted. VK040.

- 2026-10-08: [complete605 checks](r03-clipped-static-validation.md#complete605-footprint-and-canonical-ties-2026-10-08)
  resolve125 prior exclusions:212377 source-colour/ID/D32 and117047 outside-
  footprint samples PASS,0 exclusions. Initial three edge failures retained;
  24 raster variants establish a device-scoped guard-band-style/multiply-add
  model.74 exact canonical ties/17168 trace steps and501/605 equation/shader
  hashes match; seven pixel/four trace controls reject, order reversal unchanged.
  22 previous vertical cases pass the changed viewport arithmetic. No production/
  build/game/new capture/GPU fixture. Sky/other surfaces/region/cost still pending;
  R03 incomplete/R04 unstarted. Compatibility VK041.

- 2026-10-08: [complete tested backdrop pixels](r03-full-backdrop-validation.md)
  pass3861830 source-colour/796034 visible sky samples and285370 outside-source
  preservation checks,0 edge exclusions. All206 previous visible sky exclusions
  resolved; source/post masks match and seven copied corruption controls reject.
  Direct1.41 replays match six attachments/28 materials/196 mip levels/80 states
  per view, SDK1.425s/1.357s; four HIGH messages retained. No production/build/
  game/new capture/GPU fixture. Other surfaces/full region/silhouette/cost remain
  pending; R03 incomplete/R04 unstarted. Compatibility VK042.

- 2026-10-08: [complete-view geometry/observed sky audit](r03-complete-view-coverage-validation.md)
  checks all1023 ranges/876 parts/33 world materials per view. All3351166 native
  world pixels have source/post geometry attribution,0 footprint differences.
  All8919 observed geometry-covered sky absences are source-alpha explained,
  including58 boundary pixels/72 traces/16048 steps; original artwork and exact
  backdrop/ID/depth preservation pass. Six resource/geometry and20 sky controls
  reject. Direct1.41 exports41.262s/32.849s and final traces2.609s/2.627s exit0;
  failed UV/raw-LOD/helper attempts retained, four HIGH messages preserved.
  No production/build/game/new capture/GPU fixture. Next independent full scene
  composition/expected winner and filled-hole detection, then full region/cost.
  Observed ID attribution is not an expected visibility oracle; R03 incomplete,
  R04 unstarted. Compatibility VK043.

- 2026-10-08: [source-generated complete-scene composition](r03-complete-composition-validation.md)
  computes expected winners from every ordered range before final IDs are read.
  Both views pass4145830 colour/ID/depth/sky pixels;1370 depth/alpha decisions
  remain unaccepted, including277 provisional source/observed-ID differences.
  Forty state/pixel controls reject,14 original files retain recorded hashes.
  Direct1.41 state checks14.406s/13.768s and host oracles42.656s/31.531s exit0;
  four HIGH diagnostics retained. No production/build/game/new capture/GPU fixture.
  Next resolve every pending candidate without relaxing gates, rerun full-scene
  checks, then remaining source/region/silhouette/resources/cost. R03 incomplete/
  R04 unstarted. Compatibility VK044.

- 2026-10-08: [all-candidate composition references](r03-composition-trace-validation.md)
  resolve1370 prior exclusions/277 provisional ID differences with2873 source-
  selected primitive traces/764548 steps. Both existing views pass4147200 colour/
  ID/depth/sky pixels,0 errors/exclusions;1365 traced solid winner D32 values are
  bit-exact and five sky pixels preserve the backdrop.42 controls reject/20
  original hashes intact. The fixed tree UV gate failed on a thin facade;
  derived FP32 compatibility bounds supplement unchanged pixel/artwork/depth
  gates, without an exact/universal interpolation claim. Direct1.41 phases
  163.973s/84.143s exit0 without RPC timeout; four HIGH diagnostics retained.
  No production/build/game/new capture/GPU fixture. Next remaining R03 source/
  region/other views/silhouette/resources/cost. R03 incomplete/R04 unstarted. VK045.
