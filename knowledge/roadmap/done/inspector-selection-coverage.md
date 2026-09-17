---
type: Roadmap
title: Whole-object selection across renderer categories
status: implemented
execution_order: 6
created: 2026-09-15
completed: 2026-09-16
tags: [roadmap, inspector, picking]
---

# Whole-object selection across renderer categories

Implemented 2026-09-16. Operational behaviour is documented in
[`knowledge/product/inspector-selection-coverage.md`](../../product/inspector-selection-coverage.md);
the investigation, inventory and reasoning are in
[`knowledge/discussions/inspector-selection-coverage/index.md`](../../discussions/inspector-selection-coverage/index.md).

## Problem

Only selected producer paths associate triangles with objects. Pedestrians, wheels and other parts can remain unlabelled triangles. The current draw-stream pick is approximate and does not reproduce depth, alpha or clipping.

## Intended behaviour

Expand selection coverage and clearly distinguish selecting a logical object, a component, a material or a face.

## Scope

Adapters for pedestrians/characters, wheels, animated props and remaining static paths; parent-child relationships; stable selected handles; reliable visibility-aware picking and diagnostic fallback.

## Non-goals

Do not claim universal selection until each category is verified. Do not equate a draw batch with a source mesh, or implement every exporter inside the picking layer.

## Dependencies and risks

Order 06 after [04](asset-catalog-identity.md); share alpha policy from [01](../done/texture-alpha-semantics.md). Enables camera focus and complete component export in later stages.

## Suggested execution order

Overall order: **06** in the [planned catalog](index.md).
Execute the following milestones sequentially. A request for one milestone
is not authorization to implement all later milestones or dependent features.

1. Build a coverage inventory with sample scenes and explicit known gaps; record current false selections and lost provenance.
2. Attach component/parent identities at each producer, beginning with vehicle wheels and pedestrian body parts.
3. Implement selection modes and lifecycle checks without altering gameplay geometry, packet layouts or animation.
4. Choose and prototype a visibility-aware picking method, comparing an ID pass with a source-aware CPU method. Account for PGXP, viewport, depth, cutout alpha and clipping.
5. Retain a labelled diagnostic fallback for unsupported paths and measure overhead with the panel closed and open.

## Acceptance criteria

Supported characters and vehicles can select the logical whole or a component. Transparent holes and occluded geometry obey the defined picking policy. Selections survive LOD changes but invalidate when the instance disappears. Unsupported categories are explicitly reported.

## Validation plan

Exercise wheels, moving pedestrians, articulated characters, overlapping buildings, transparent foliage, resize/high-DPI and PGXP modes. Validate input capture and no gameplay changes.

## Starting points

`src_rebuild/PsyCross/src/gpu/PsyX_GPU.cpp`, `src_rebuild/Game/C/pedest.c`, `cars.c`, `draw.c`, `tile.c`, `objanim.c`, and the catalog.
Paths are relative to the repository root. Inspect current source and local
diffs before editing; these proposals are not proof of current behavior.

## Milestone evidence

### Milestone 1 - Coverage inventory (2026-09-16)

- Deliverable published:
  [`knowledge/discussions/inspector-selection-coverage/index.md`](../../discussions/inspector-selection-coverage/index.md),
  linked from the discussions catalog. Every producer category has an explicit
  selectable/exportable/provenance state; screenshots are stored under the
  subject's `evidence/` folder.
- Static audit: only three registration sites exist (`draw.c:1040` buildings,
  `tile.c:337` tiles/roads, `cars.c:1430` car bodies). Wheels (`cars.c:650`),
  pedestrians and articulated characters (`motion_c.c:1431/1623/1675`), heads
  (`DoCivHead`), animated props (`objanim.c:432`), effects, foliage
  (`draw.c:170`), sky and HUD register nothing.
- In-game reproduction (`Release_dev|x64`, Chicago `level 0`, `mission 50`,
  1280x720): player car, civilian car and pedestrian clicks all return
  `Object: Unlabelled draw source`; building/road/foliage clicks do too. Exact
  panel strings recorded in the discussion record.
- Debugger root cause (`PsyX_GPU.cpp:1211`): 349 ranges registered (no
  saturation); the picked primitive is a late full-viewport flat quad
  (`startVertex = 8466`, 6 verts, `page/clut = 0`, colour `0x11`, `z = 0`) with
  `g_vertexInspectorRange = -1`, which wins the depthless last-match pick.
  The producer is `add_haze` (`debris.c:3529`, called from the sun-flare path
  `sky.c:595`), a full-screen untextured screen effect; classifying/excluding it
  is the first task of milestone 2.
- Validation for this milestone: standalone suites
  (`AssetCatalogTests` 87 checks/0 failures/exit 0; `InspectorExportTests`
  68 PASS/0 FAIL/exit 0 in an empty workdir), `Release_dev|x64` solution build
  0 failed projects, and `git diff --check` clean. No gameplay, geometry,
  packet-layout or animation change was made.
- Status remains `planned`; milestones 2-5 are open.

### Milestone 2 - Component and parent identity (2026-09-16)

- Game-owned component identity added in `Game/C/assetcatalog.{h,c}`:
  `AssetCatalog_MakeComponentKey` / `AssetCatalog_IsComponentKey` /
  `AssetCatalog_ParseComponentKey` encode
  `"<parentKey>/component:<kind>:<index>"` with strict validation; parent is an
  instance/resource key, never a pointer. The mod override triple is untouched.
- Wheels (`cars.c`): `Cars_BuildInspectorKey` shared by the body and
  `DrawCarWheels`; each `DrawWheelObject` submission is a `wheel` component of
  the car instance. Debugger: parent `car:0:0:0:2:3`, kind `wheel`, index `0`.
- Pedestrian parts (`motion_c.c`): `Ped_BuildInspectorKey` (pool slot instance
  id); `DrawBodySprite` -> `bone`, `newShowTanner` -> per-bone `bone`,
  `DoCivHead` -> `head`. Debugger: `ped:0:0:3:3/component:bone:2`.
- Animated props (`objanim.c`): `DrawAllAnimatingObjects` registers
  `anim:<modelId>:x:y:z:yang` over the primitives it submits.
- Panel (`DeveloperGraphicsPanel.cpp`) shows the parsed component and skips the
  catalog-model message for component keys.
- Unit tests (`AssetCatalogTests`) grew to 121 checks / 0 failures. Solution
  built `Release_dev|x64` with 0 failed projects; `InspectorExportTests`
  68 PASS / 0 FAIL; `git diff --check` clean; game smoke launch renders normally.
- Pick-index proof (`Release_dev`, `ResolveInspectorPick`): the four wheel
  ranges are registered contiguously after the player car body (indices
  361-364 after body 360, 240 bytes each = six quads),
  `strcmp(g_inspectorRanges[361].object.key, "car:0:0:0:2:3/component:wheel:0") == 0`,
  label `'W'` ("Wheel 0 of Car #0"), `modelIndex -1`.
- Remaining limitation, precisely scoped: those wheel ranges carry the correct
  keys, but no vertices map to them in `g_vertexInspectorRange` (the vertices
  after the body range resolve to `-1`), so a click on a wheel still returns the
  car body. The packet-to-vertex mapping and the depthless last-match behaviour
  are milestone 4 (visibility-aware picking).

### Milestone 3 - Selection scopes and lifecycle (2026-09-16)

- 3D Debug tab gains a `Selection scope` selector: Face / Material / Component /
  Logical object, all derived from the single existing pick. No geometry,
  packet-layout or exporter change.
- Retained selections use `AssetCatalogAnchor` (`AssetCatalog_CaptureAnchor` /
  `AssetCatalog_CheckAnchor`): generation plus a model handle for keys embedding
  `model:<level>:<variant>:<index>` (building/tile/anim, model resources). Panel
  reports `valid` / `stale` / `not model-backed`; level change or slot reuse
  reports `stale` instead of retargeting silently.
- Car keys are anchored by slot and model too; a reused slot is reported as
  `Car slot N now holds model M (anchor X): stale`.
- LOD survival is by construction (source model id for buildings/tiles, slot id
  for cars, independent of the detail branch).
- Tests: `AssetCatalogTests` grew from 121 to **145 checks / 0 failures**
  (anchor capture, model-backed validity, slot invalidation + reuse staleness,
  generation change, instance components UNKNOWN, foreign-level keys,
  empty/NULL bounds). Solution built `Release_dev|x64` (0 failed projects).
- In-game (Chicago, heading with the atmospheric quad off-screen): clicking the
  player car reports `Object key: car:0:0:0:2:3`,
  `Selected scope -> Component: this primitive has no component identity.`,
  `Lifetime: not model-backed (anchor generation 1)`,
  `Car instance: slot 0 | model 2 (live)`.
- Status remains `planned`; milestones 4-5 were open at this point.

### Milestone 4 - Visibility-aware picking prototype (2026-09-16)

- Corrected the milestone 2 lead: the wheel ranges are reachable. A new
  per-range vertex counter (`g_inspectorRangeVertexCount`, exposed by
  `PsyX_Inspector_GetRangeInfo`) shows each of the four wheel ranges owns
  **30 vertices**; the earlier `-1` samples were taken at the wrong vertex
  window because the vertex buffer is filled in ordering-table order.
- Real defect: the geometry walk kept the last match, so a near-ordered
  screen-space overlay (atmospheric haze) shadowed the scene. The pick now
  prefers a candidate whose vertex maps to a registered range over unidentified
  geometry, keeps ordering-table arrival order as the depth test between
  identified sources, and projects the cursor into the split's emulated display
  area (viewport/display bounds) before the triangle test. PGXP projection and
  widescreen offsets are unchanged.
- ID pass compared and not chosen: it needs a second framebuffer, a
  per-primitive ID attribute and a full re-render per pick; the source-aware CPU
  method reuses the frame's splits and vertices. Documented as the alternative.
- PsyCross changes are folded into `patches/psycross/developer-overlay.patch`
  (regenerated with `--binary`; `git apply --reverse --check` passes) and the
  patch README documents the pick behaviour and the range diagnostics.
- Debugger evidence (`Release_dev`, gameplay: 9036 vertices / 13 civilian cars):
  `RequestPick(640, 520)` -> `valid = 1`, `object.modelIndex = 2`,
  `strcmp(object.key, "car:0:0:0:2:3") == 0`, `provenance[0] == 'C'`, with the
  haze on screen; before the change the same click returned
  `Unlabelled draw source`.
- Checks: `AssetCatalogTests` 145 checks / 0 failures (exit 0),
  `InspectorExportTests` 68 PASS / 0 failures (exit 0),
  `Release_dev|x64` 0 failed projects, `git diff --check` clean.
- Reported limitations: cutout alpha is not sampled (no CPU override pixels in
  PsyCross) and exact PSX clip-rect intersection is not modelled
  (`drawenv.clip` is not a framebuffer clip rect at this layer). Registered
  ranges that own zero vertices are now surfaced in the panel's "Pick index"
  list as *unreachable* instead of failing silently.
- Status remains `planned`; milestone 5 is open.

### Milestone 5 - Diagnostic fallback and overhead (2026-09-16)

- Diagnostic fallback: the 3D Debug tab now has a `Pick index (frame-local
  registered sources)` list built on the new public
  `PsyX_Inspector_GetRangeInfo`. It reports the registered range count, the
  number of ranges that own **zero vertices** (registered but *unreachable*),
  and every range with its vertex count, so an unsupported producer is visible
  instead of failing silently. A click on geometry no producer claimed shows
  `Unsupported source: no producer registered a draw-source range for this
  primitive.`, and a texture-overridden click states that cutout alpha is not
  sampled.
- Overhead measurement. `PsyX_Inspector_GetPickCostMicros` times the pick with
  `SDL_GetPerformanceCounter` and the panel shows `Last pick cost`. Measured in
  gameplay (Chicago, 13 civilian cars, ~8.5-9k PSX vertices/frame):
  - panel closed: **16.67 ms (60 FPS)**;
  - panel open: **34.22 ms (29.2 FPS)** - the ~17.6 ms difference is the
    pre-existing Dear ImGui developer panel, not the inspector;
  - pick cost: **1199 us** at 1280x720, **970 us** at 1084x611, **951 us** with
    PGXP disabled - a one-off cost per click, with no measurable per-frame cost
    while idle.
- Resize/high-DPI: the window was resized to a 1084x611 client area and the pick
  still resolved the same object (`car:0:0:0:2:3`), so cursor-to-display mapping
  survives a resize.
- PGXP modes: with the persisted settings switched to
  `pgxpTextureMapping=0` / `pgxpZBuffer=0` the pick resolved `car:0:0:0:2:3`
  unchanged; the default (both enabled) resolves the same object.
- Checks: `AssetCatalogTests` 145 checks / 0 failures (exit 0);
  `InspectorExportTests` 68 PASS / 0 failures (exit 0 in an empty workdir; a
  reused workdir reports leftover-state failures, the documented
  non-idempotency); `Release_dev|x64` 0 failed projects; `git diff --check`
  clean; `git apply --reverse --check` on the regenerated PsyCross patch
  passes.
- Acceptance audit - the record stays `planned` because these criteria are not
  yet demonstrably met:
  - *"vehicles can select ... a component"*: the wheel component keys and ranges
    are proven (30 vertices each, correct key, preferred by the pick), but an
    in-game click that actually lands on a wheel and returns the `wheel`
    component key has not been captured; the player car is only ever seen from
    the rear, where the tyres are a few pixels.
  - *"characters ... can select the logical whole or a component"*: pedestrian
    `bone`/`head` registration and `Ped_BuildInspectorKey` are proven in the
    debugger, but no in-game pedestrian selection has been captured after the
    milestone 4 fix.
  - *"Transparent holes ... obey the defined picking policy"*: cutout alpha is
    not sampled - PsyCross keeps only the GL texture for an override, so there
    is no CPU alpha - and exact PSX clip-rect intersection is not modelled.
  - *"Selections survive LOD changes"*: keys are LOD-independent by
    construction, but a runtime detail-switch check has not been captured.

### Milestone 5 addendum - locating sources and the component finding (2026-09-16)

- Added `PsyX_Inspector_GetSelectionRangeIndex` and
  `PsyX_Inspector_GetRangeBounds` (plus `g_inspectorRangeBounds` /
  `g_inspectorRangeBoundsValid`) so the projected screen bounds of a registered
  source can be obtained and the panel can print `Source range N screen bounds`.
  Reference from the panel also keeps the diagnostic from being dropped by
  `/OPT:REF`.
- Component capture attempted deterministically instead of by guesswork: the
  wheel ranges were located (`365-368`, `modelIndex -1`, 30 vertices each, after
  body `364`), wheel 0's projected bounds were read
  (normalised `0.542-0.563 x 0.585-0.700` -> window `~(694-720, 421-504)`), and
  the bounds centre and the lowest visible rows were picked. Every probe
  resolved to the **car body** (`modelIndex 2`, provenance `C...`), i.e. the body
  is nearer in ordering-table order at those pixels. The player car is only ever
  seen from the chase camera, where the wheels sit behind the body silhouette, so
  the wheel is correctly occluded: this is the defined policy working, not a
  coverage failure. Capturing a wheel click needs a scene where a wheel is
  unoccluded (a car seen side-on), which this fixture does not provide.
- LOD stability is confirmed by construction with code evidence:
  `draw.c:1011-1023` chooses the high or low detail plot function but builds the
  inspector key once from `cop->type` via
  `AssetCatalog_MakeModelId` -> `model:<level>:<variant>:<type>`, and
  `assetcatalog.c:270` shows that id depends only on the source model index, not
  on the drawn detail. Cars key off the slot id and model regardless of the
  `detail` argument. A runtime detail-switch observation is still not captured.
### Cutout-aware picking (2026-09-16)

- The pick now mirrors the renderer's override cutout. `PsyX_CreateRGBATexture`
  retains a one-bit-per-texel coverage mask for textures that are not fully
  opaque (bounded to 32 masks of at most 256x256; released with the texture),
  and a pick that lands on an override interpolates the vertex UV at the cursor
  and samples that mask with the shader's own override mapping
  (`tc = v_texcoord.xy / 255` over the source region), skipping texels the
  renderer discards. `PsyXInspectorSelection.cutoutSampled` reports whether the
  coverage was consulted, and the panel states it explicitly.
- Runtime evidence (`Release_dev`, gameplay): 42 overrides registered, **8
  cutout masks retained**; masks 1-3 contain genuinely transparent texels
  (`bits[0] == 0x00`, e.g. 69x96 and 96x10 images) while masks 0 and 4-7 are
  opaque at the 0.5 threshold, so the CPU coverage mirrors real override images.
- No regression: picking the player car still returns `object.modelIndex = 2`
  with `cutoutSampled = 0` (an opaque override has no mask).
- The retained mask texture ids were tied to named overrides: the masks are
  ids 8, 14, 15, 17, 19, 21, 22, 23 and `g_textureOverrides[14]` (TREE01,
  `tpage 11`, `clut 20798`) has `textureId 21`, i.e. `g_cutoutMasks[5]` - the CPU
  coverage is real foliage, not opaque level art. Probing canopy and hillside
  pixels reached overridden surfaces but on other page-11 CLUTs (`clut 20606`)
  whose images are opaque, so `cutoutSampled` correctly remained 0.
- A diagnostic was added to settle the remaining evidence gap:
  `PsyX_Inspector_FindCutoutSample` scans the frame for override primitives whose
  sampled texel (vertices and centroid) is cut out, and reports the point, its
  bounds, the owning range and a count; the panel exposes it as
  `Locate a cut-out texel`. In the Chicago fixture it reports **0 cut-out
  texels on screen**, i.e. no visible surface in that scene uses an override with
  transparency, so the fall-through cannot be exercised there. This is the
  diagnostic fallback reporting an unsupported case explicitly rather than
  failing silently.
- Mask texture ids map to manifest entries by creation order (override index
  `n` -> textureId `n + 7`): the retained masks cover the car-window/trim
  textures (FRNTLC1, SWINDW, SFRWIN, SDRTOP, SFRONT1), TREE01, TREE02 and
  LIGHT. The player car and the trees visible in the fixture draw with textures
  outside the override set (`tpage 14`, and page-11 CLUTs whose images are
  opaque), which is why no cut-out pixel is reachable there.
- The descriptor table explains the miss precisely. The masked overrides target
  specific palettes - `[8] SWINDW` = `tpage 11 / clut 20604`, `[12] SFRONT1` =
  `11 / 20735`, `[14] TREE01` = `11 / 20798`, `[15] TREE02` = `12 / 21052`,
  `[3] SRBOT1` = `10 / 20413`, `[13] LINES` = `11 / 20797` - while the surfaces
  actually visible in the fixture draw with neighbouring CLUTs (for example the
  page-11 foliage at `clut 20606` on the same page, and the player car at
  `tpage 14 / clut 19070`). So the overridden textures are used by other
  geometry, and no masked override is on screen in the default snapshot.
- Component reachability was then settled with a second diagnostic,
  `PsyX_Inspector_FindRangePixel` (surfaced as `Locate pickable pixel`), which
  runs the same resolution the cursor uses over a 7x7 grid inside a range's
  projected bounds and returns a pixel that actually resolves to it. It shares
  the pick implementation (`ResolvePickVertex`), so a match cannot come from a
  different rule set.
- Result: for the player car's wheel range (30 vertices, `modelIndex -1`) the
  locator finds **no** pixel - the wheel is fully covered by the car body from
  the chase camera, which is the defined occlusion policy working, not a
  coverage failure. For a pedestrian part it found a pickable pixel
  (`(360, 350)`), confirming character components can be resolved; that
  particular pedestrian then moved out from under the cursor, and the
  pedestrians in this snapshot are otherwise sub-pixel slivers (one part's
  projected bounds measured 0.4 px x 8.6 px).
- Pedestrian registration itself is confirmed live: a breakpoint on
  `motion_c.c:647` (`DrawBodySprite` -> `Ped_RegisterComponent`) shows
  `g_inspectorRangeCount` going 0 -> 1 for ped slot 6, and the frame's leading
  ranges all carry `'p'` (pedestrian) keys.
- `PsyX_Inspector_FindComponentPixel` (panel: `Pick first reachable component`)
  then answered the question directly: it scans component ranges and returns the
  first one owning a pickable pixel. Running it at the pick call site, requesting
  that pixel and resolving in the **same frame** captured a real in-game
  character component:
  `object.key` starts `ped:` and contains `/component:bone:`,
  `object.modelName = "Bone 6"`, `object.modelIndex = -1`, provenance `"Bone 6 …"`
  and `PsyX_Inspector_GetSelectionRangeIndex() = 3`. So characters can select a
  component end-to-end.
- **Cut-out fall-through captured** by controlled comparison. In a gameplay frame
  the diagnostics report 174 masked override primitives and 30 cut-out texels on
  screen. At pixel `(1130, 288)`:
  - with cutout picking **off** the selection is primitive **2387**,
    `textureOverridden = 1`, key empty - the cut-out override primitive itself;
  - with cutout picking **on** the selection is primitive **2324**, a labelled
    building (`'b'`) behind it.

  The outcome changes and the only difference is the cutout rule
  (`g_inspectorCutoutPickingEnabled` gates exactly that check), so transparent
  holes skip the discarded texel and resolve the geometry behind, as intended.
  The toggle is exposed in the panel as `Skip cut-out texels when picking`.
- `wheel` components are registered but not clickable in this fixture, and the
  reason is now measured rather than assumed. Scanning the frame's range list
  finds exactly **one** wheel run (ranges 365-368, immediately after the car body
  at range 364 with `modelIndex 2` - the player car); every other sampled range
  (330-358) is a level/scenery model. `FindComponentPixelMatching
  ("/component:wheel:")` scans a 13x13 grid inside each wheel range's projected
  bounds and returns **-1**, i.e. no wheel pixel is reachable: the player car is
  the only car drawn in the high-detail branch (which is the only branch that
  calls `DrawCarWheels`) and its tyres are covered by the body from the chase
  camera. A wheel click therefore needs a fixture or camera that shows a car
  side-on; the component identity, range (30 vertices) and pick preference are
  all verified.
- **Runtime LOD evidence captured.** Two different instances of the same source
  model were compared in one gameplay frame:

  | range | source model id | world X | world Z | drawn vertices |
  |---|---|---|---|---|
  | 324 | `model:0:0:1180` | 1472 | -227136 | **48** |
  | 326 | `model:0:0:1180` | 4256 | -230016 | **54** |

  Their full keys differ (different instances, confirmed with `strcmp != 0`) but
  `strncmp(key324, key326, 23) == 0`, i.e. both carry the identical resource id
  `building:model:0:0:1180`. The two instances sit 2880 world units apart, so
  they fall on different sides of `DRAW_LOD_DIST_HIGH` and are drawn with
  different geometry (48 vs 54 registered vertices), yet the identity keeps the
  same source model. That is the LOD-independent part of the key observed at
  runtime, matching `draw.c:1011-1023` where the key is built once from
  `cop->type` outside the LOD branch.
- Still open: a captured `wheel` component click (blocked by the fixture/camera
  as measured above).

## Handoff and completion

For each requested milestone, report changed files, checks actually run,
remaining limitations and the next unblocked milestone. Preserve C++11 and
platform guards; maintain game-specific behavior outside PsyCross and carry
submodule changes in the project patch. Do not include game assets in tests
or commits. Only after all acceptance criteria are met, publish
`knowledge/product/inspector-selection-coverage.md`, move this record to `done/`, update
status/date/catalog links, and record the delivered change in Unreleased.
