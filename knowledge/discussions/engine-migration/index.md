---
type: Discussion
title: Engine migration decision
status: adopted
created: 2026-10-08
updated: 2026-10-11
tags: [okf, architecture, engine]
---

# Engine migration decision

## Adopted direction

The user chose Diligent Engine/DiligentFX and FMOD Core with a
project-owned simulation/service boundary and final PsyCross removal. PopcornFX
is now optional final E08, only upon explicit request after the complete game
and cutover pass; existing native effects remain mandatory. Original
assets are converted to readable/native packages using OpenDriver2Tools as format
reference. Physics, gameplay, missions and cutscenes remain Driver 2. Later work
includes FMOD Studio, RmlUi, internal editors and supplementary physics.
The user confirms middleware permitted use and adopts OpenDriver2Tools usage.

The [engine-migration roadmap](../../roadmap/planned/engine-migration.md) owns
the adopted stages. This conversation authorizes branch preservation and planning,
not implementation of the engine. No GitHub issues were requested for publication.

## Verified evidence and limits

- Inspected baseline parent `2e93f215`, PsyCross `37d399df`; current game source
  still uses GTE math in handling, `FilterFrameTime`/`VSync` in the main loop,
  SPU calls in sound and CD/DrawSync callbacks in spooling.
- `CameraCnt` advances on simulation steps; replay records use frame repeats.
  Extracting clock ownership must preserve those tick-indexed semantics and the
  actual regional timer behavior, not make physics display-rate dependent.
- OpenDriver2Tools' [region exporter](https://github.com/OpenDriver2/OpenDriver2Tools/blob/master/DriverLevelTool/exporter/export_regions.cpp)
  writes region OBJ or Unity instantiation scripts with converted placements;
  [model export](https://github.com/OpenDriver2/OpenDriver2Tools/blob/master/DriverLevelTool/exporter/export_models.cpp)
  writes positions/UVs/material page references. This is not gameplay conversion.
  The converter must preserve extra IDs/flags/LOD/animation/damage metadata.
- [DiligentFX](https://github.com/DiligentGraphics/DiligentFX) supplies reusable
  glTF/PBR/shadows/post components. [PopcornFX integration](https://documentation.popcornfx.com/PopcornFX/v2.19/popcornfx-sdk/integration.html)
  recommends engine-owned rendering integration for resource/shader/sort control.
  These capabilities were inspected in documentation, not integrated or tested.

## Recommendations adopted into the plan

Use conversion inventories and deterministic game-state regressions, plus targeted
native rendering tests. New lighting/filtering/effects do not need universal legacy
pixel equivalence. Deliver an early converted driveable slice rather than waiting
for a complete middleware stack. Keep necessary compatibility math/services until
their contracts are independently validated; extraction is not wholesale copying.

Conversion removes repeated source-format interpretation from presentation but
does not solve every overlap/alpha/streaming/dynamic integration problem. No "99%"
reduction or schedule estimate is established. Native failures remain explicit.

## Content decode-base strategy (E02/E03)

User decision (2026-10-09): adopt the hybrid decode-base strategy for
original-content conversion. The project's reconstructed decode stays the
authority for anything simulated or identified; OpenDriver2Tools (ODT) is a
format/placement reference and gap-filler, never a runtime dependency and never
a second long-lived parser. Rationale from the review: a foreign parser would
drift on flags/winding/LOD/damage and cannot supply catalog identities, while a
from-scratch rewrite alone would delay the slice and redo solved format
plumbing.

- Authority: game-reconstructed decode owns world MODEL geometry, sector
  instances/LOD, car damage/vertex mappings and sound IDs; identity and bindings
  reconcile with `assetcatalog.*`.
- One pure decode module (`utils/`, no PsyCross/GPU, cross-platform) serves the
  offline converter and any runtime bridge; one shipped implementation per asset
  category. Runtime consumes published packages via `GltfLoader` +
  `NativeRenderer`, never a parallel decode.
- ODT role: format/export reference (as previously planned) plus gap-filler for
  pure format plumbing (texture palette-to-RGBA, sound containers), lifted into
  the same module or reimplemented from documented behavior. Pin the inspected
  revision and record provenance; adapted code stays in tooling and is not
  reachable from the shipped runtime.
- A bounded, external ODT-to-GLB tooling spike (temporary directory, no repo
  integration) was authorized for a reference GLB and to concretize the
  adaptation surface; it does not authorize a shipped ODT conversion path.

Verified ODT facts (source `master`, head 2025-10-28; the repo has no LICENSE
file - the user accepted reuse for this non-commercial project):

- `DriverLevelTool/driver_routines/` parses standalone: no SDL2/OpenGL/ImGui
  coupling in the parse path; Win32 appears only in shared helpers
  (`core/cmdlib.cpp` console spew under `#ifdef _WIN32`, `util` path handling,
  `math` `fsincos` with a C fallback).
- In-memory representation: `decode_poly` yields per-face positions/UVs/normals/
  colour plus `page`/`detail`; placements are `CELL_OBJECT {pos, yang, type}`
  (the Unity export keeps instances, the region OBJ export bakes them);
  `ConvertIndexedTextureToRGBA` decodes palette variants.
- No glTF/GLB or PNG writer exists (TGA/TIM/BMP only). A GLB layer must add
  index de-duplication, node/TRS synthesis, handedness conventions, `TEXINF`
  sub-rect material mapping, normals for polygons without them and alpha
  semantics.
- Audio: `DriverSoundTool` yields decoded 16-bit PCM (`PCMSample` with loops);
  XM is emitted as raw bytes, not a parsed module; MP3 would need an encoder.

Adopted acceptance gates (on top of E02's existing criteria):

- Cross-interpretation agreement fixture: offline decode and the game's live
  interpretation of the same source bytes reconcile in positions/UVs/indices/
  texture id/flags.
- Identity/binding fixture: damage, LOD and `source_object_index` reconcile with
  the asset catalog; missing bindings are explicit errors.
- Convergence ledger per category (`game-authority | otd-derived | not-covered`)
  with one owning implementation per category.
- Portability/isolation gate: the pure module builds on all declared targets
  without SDL2/OpenGL/ImGui.
- Named diagnostics with no partial output; unsupported categories remain
  explicit in the coverage report.

Recommended sequencing for the first visible native world content (continues
E03 Block 2; ODT-independent):

1. Capture the real MODEL byte span at load (`models.c:131-139`) into the
   decoder's `[begin,end)` contract.
2. Extend `SourceModelDecoder` to the full world ptype set, settling the
   skip-vs-fail policy once against a real-data ptype histogram.
3. Decode one real building/tile MODEL into `SourceMesh` and publish a GLB
   through the existing `GltfWriter`/`AssetConverter`.
4. Add a bounded per-instance draw API plus sector/instance transforms in
   `NativeRenderer` and draw the converted region with the game camera.

Open questions:

- Which categories stay `otd-derived` permanently (texture palettes? sound
  containers?) versus converging to project decode.
- The runtime skip-vs-fail ptype divergence (`draw.c` skips unknown ptypes; the
  decoder fails the model) must be decided once in the shared module.
- ODT code-adaptation provenance and confinement details.
- Remaining ODT gaps stay explicit: character/skeleton geometry, video/FMV,
  event models, mission tool stub.

### ODT-to-GLB spike results (2026-10-09)

Bounded external spike in the approved temp directory (repo read-only;
artifacts and command logs under `odt-glb-spike/`). Local `data\DRIVER2\` is a
full retail NTSC tree; `MLEVELS\CHICAGO.LEV` (1.73 MB, 10 regions) served as
the small level. ODT v1.10 and master (2025-10-28) were both exercised; GLBs
were produced for car clean/low and the whole mini level with embedded PNG
textures (Khronos validator 0 errors / 0 warnings, hints only, on every
artifact).

Confirmed facts for the future GLB writer and package contract:

- Instance placements are baked into vertices by both OBJ paths (`g regNN`
  only); the Unity mode is the only placement-preserving path and was not
  exercised. Package instances must come from the project decode/catalog.
- `damaged` car models are deformation data, not geometry: most faces are
  degenerate (`f 1 1 1`) and the game uses only the vertex array for denting
  (`CleanVert + (DamVert - CleanVert) * damage/2`). Packages must carry damage
  as vertex mappings/arrays, never as renderable meshes.
- UVs address full 256x256 pages; `PAGE_N.ini` sub-rect metadata is ignored and
  per-face `texture_id`/CLUT selection is dropped. The writer layer must
  recover both from project decode.
- World exports contain zero `vn` lines; car exports have normals only on
  smooth polygons. Conversion must synthesize normals or use flat shading.
- TGA alpha is binary from the STP bit with a magenta transparency key; real
  per-primitive ABE blending is not representable in OBJ/MTL.
- Missing texture references occur in practice (`usemtl page_89` with no MTL
  entry/TGA); conversion must report them explicitly.
- Master writes `regions\<lev>_regN.obj`; v1.10 writes one merged
  `<lev>_LEVELMODEL.obj`.

Reference GLBs stay in the temp spike folder (not copied into the repository).

## Temporary legacy backend recommendation

2026-10-08 analysis on parent `9c353405` with existing local migration edits:
the user reports OpenGL reaching 360 FPS (~3ms) while PsyCross Vulkan rarely
exceeds 45 FPS. Earlier isolated Vulkan logs show ~40-48 FPS, including runs
with modern meshes disabled, so that feature alone does not explain the gap.
Source inspection identifies serialized frame reuse, full-frame readback,
CPU framebuffer-to-VRAM conversion and complete 4MiB VRAM uploads ending in
`vkQueueWaitIdle` (`PsyX_Vk.cpp::GameBeginFrame/UploadVramMirror/EndOneShot`,
`PsyX_render.cpp::GR_VkMirrorFrameToVRAM`). Available validation layers are
enabled automatically, including Release_dev. These are concrete cost candidates;
their individual contributions to the observed frame time were not measured.

Adopted interim direction: use legacy OpenGL for everyday gameplay
and simulation regression checks; defer performance work on the retiring PsyCross
Vulkan renderer unless it blocks a specifically required consumer. Validate native
Diligent rendering independently, including its own CPU/GPU and high-FPS budgets.
E03 still selects Diligent's Vulkan backend; `NativeRenderer.cpp` creates its
device through `GetEngineFactoryVk`, separately from PsyCross. The legacy result
does not establish Diligent Vulkan performance or justify changing that API scope.
OpenGL-to-Diligent describes replacement of presentation ownership, not a port of
legacy GL commands into Diligent or a required intermediate Diligent GL backend.

Remove the retired Vulkan implementation after migrated consumers, build targets,
tools and shared interfaces are accounted for; complete PsyCross removal remains
E07 after E06 acceptance. Keep the historical rework branch. No renderer deletion,
default-backend change, performance fix or migration-stage acceptance is authorized
by this recommendation. See [E03](../../engine-migration/stages/03-diligent-playable-slice.md)
and [E07](../../engine-migration/stages/07-remove-psycross.md).

## Native material follow-up (2026-10-10; updated 2026-10-11)

Follow-up to the visibility checkpoint identified two distinct E03 material contracts:

- Building orientation shading is now implemented in the working tree. Normal-shaded
  converted vertices preserve `_RED2_WORLD_SHADE`; each placement applies its original
  yaw group to a live 32-entry palette, matching `draw.c::PlotBuildingModel` and the
  subdivided path's `r = (rot >> 3) * 4`, `f4colourTable[(r - th) & 31]` lookup. Shared
  geometry stays immutable, and decoded polygon corners do not alias shade metadata
  between combo and normal-shaded primitives. Inspector, asymmetric RGB mapping and
  native smoke tests pass. The mission log confirms scene submission, but a RenderDoc
  capture and visual inspection remain pending; no full-image parity claim follows.
- `texturebake.c::TextureBake_BakeModel` reads original CPU VRAM and embeds whole
  page/CLUT images. It does not resolve HD manifest pixels.
  `HdTextureOverrides.cpp::EnsureImageLoaded` couples PNG loading to
  `PsyX_CreateRGBATexture`, which is unavailable in window-only mode; the saved
  native startup log contains named compatibility-texture errors. A native path
  needs GPU-independent owned pixels, palette/mod precedence, sub-rectangle UV
  mapping, bounded memory and invalidation when enabled mods/pixels change.
  `ResidentRegionState` currently tracks sectors/models/pages, not override state.
  The inspected local developer setting has `hdTextureOverrides=0`; this does not
  establish HD overrides as the cause of the last original-texture differences.

User decision (2026-10-11): defer HD texture overrides and mods until the original-
asset first slice has functional static-world coverage, including Chicago. The
desired later authoring route is export to GLB/glTF, edit externally, then validate
and reimport. Its data contract, validation rules and safe reimport behavior are
undefined and unimplemented; this is not an authorization to start that work.

For the next original-only world investigation, the Gate 4A Chicago inventory
reports 6740 unique placements, 227 region straddlers, 150 referenced model slots,
13 referenced textures and 114 models without a catalog material link, explicitly
classified as unknown. It reports no missing model references; the unknown material
classification is not evidence that runtime textures are absent. `assetcatalog_game.c`
defines the inspected world primitive allowlist, `SourceModelDecoder.cpp` decodes
supported textured/flat polygon classes into submeshes, and `texturebake.c` maps each
submesh's texture set/id through `texture_pages`/`texture_cluts` to original CPU VRAM.
Recommended bounded task: trace material identity and original page/CLUT resolution
for the referenced static Chicago model slots, then fix and measure the most common
confirmed unresolved original-material class. Keep the four eligible Chicago day
barrels as the bounded fixture; report resolved/unresolved counts and named causes.
Do not infer a defect from the 114 unknown links alone. This task is separate from
the player car and pedestrian analyses.

Remaining original textures/materials, static coverage, player vehicle/on-foot and
city expansion stay open. Neither shading correction nor the visibility checkpoint
accepts E03, original-texture correctness or full-game parity.

History (2026-10-11): user deferred HD/mod consumption pending functional original
world coverage and named external GLB/glTF edit/reimport as a later desired path.

## Open implementation decisions

E00 pins SDKs/compiler/targets and inventories remaining consumers; E02 chooses
the compiled package/GLB boundary and readable schema; E01 establishes tested
regional/replay compatibility and backlog policy. Later physics changes require
separate game/replay versioning. No unresolved choice authorizes a silent fallback.

## History

2026-10-11 follow-up: per-instance building shading was implemented with retained
shade metadata, yaw palette lookup and RGB mapping regression coverage. Native
mission log reached world draws; capture/visual inspection remains pending. The
HD texture override contract remains separate and unresolved.

2026-10-10 native material diagnosis: cell/event visibility was published as
`34e104ed`; source inspection separated fixed-zero building shading from missing
native HD override consumption and recorded their different constraints.

2026-10-09 ODT-to-GLB spike follow-up: bounded external spike produced
validator-clean GLBs (car clean/low, mini-level) from ODT exports of the local
retail data and confirmed the writer-layer gaps plus damaged-model semantics
(deformation data, not renderable geometry); results recorded under the
decode-base section. No code changed in this task.

2026-10-09 decode-base follow-up: user adopted the hybrid content conversion
strategy (project-reconstructed decode stays the authority; OpenDriver2Tools is
a format/placement reference and gap-filler, never a runtime dependency) after
a modularity inspection of ODT's reader layer and an oracle review of the
alternatives. One pure decode module serves the offline converter and runtime
consumers; E03 Block 2 and a bounded external ODT-to-GLB tooling spike are the
authorized next steps. No runtime ODT dependency was authorized. No code changed
in this task.

2026-10-09 effects/replay follow-up: complementary updates moved legacy effect
emission/lifecycle, light phases and input-gating title time to regional ticks;
local drawing interpolates poses. Complete ATTRACT NTSC 30/360 and PAL 25/360
comparisons agree, including recorded effect state. The bad original trajectory
remains open: b2d8857 plus its historical PsyCross reproduces it on local assets,
with equal Win32/x64 reference state. Matching schedules is not route acceptance;
a working reference with matching data remains necessary. E00-E03 status is
unchanged. [Evidence](../../changes/2026-10-09/effect-timing-replay-regression/index.md).

2026-10-08: user adopted migration, requested preservation/push of current work
and a fresh branch from baseline. Rework `e68b6ea6` published; local `main` aliases
existing `origin/master` because no main existed, without remote renaming.
The new branch contains planning changes only; all E00-E07 implementations pending.

2026-10-08 follow-up: PopcornFX was originally included in mandatory E05. The user
deferred it to optional final E08 to prioritize services/ticks, Diligent, FMOD
and the complete working game. E05 retains native effects/UI; E07 removal and
required migration completion do not depend on PopcornFX. No runtime change.

2026-10-08 motion regression follow-up: the user authorized a bounded correction
of car flick/jumps without advancing migration status. Live temporal evidence
identified mixed car/camera sample times and camera control tied to display FPS.
Camera/car snapshots now share a tick interval; original controllers run at
regional service cadence and tick camera globals are restored before simulation.
Exact regional clock phase and render-local car rotation/shadows were corrected.
Moving-route tick-state comparison and NTSC/PAL timing tests corroborate this
scope; E01 remains in progress. See [fix evidence](../../changes/2026-10-08/e01-presentation-motion-fix/index.md).

2026-10-08 backend follow-up: user requested an opinion on temporarily focusing
on OpenGL before Diligent and later discarding legacy Vulkan. Recorded the
recommendation and retirement boundaries above; the user's subsequent agreement
adopts the interim testing focus.
Only documentation changed; native backend selection and stage status remain.

2026-10-11 native actor follow-up: the default-off `RED2_NATIVE_VISUAL_DEBUG=1` overlay reports
world/car/wheel/pedestrian lifecycle counters without capturing input. Owned CPU snapshots,
generation/revision invalidation, finite/count-checked dynamic wheel uploads and preserved black
wheel quads bound the actor data path. The fresh inspector runner passed 20 suites, GPU smoke
passed 1,274 checks, and native/reference x64 builds succeeded. Correct Chicago mission 50 showed
`playerCar=0`, 14 cars and 56 wheels submitted, and four original `CIVILIAN` meshes ready over the
session. The captured frame submitted one civilian and the PNG shows it at the sidewalk. Loading
recorded five dropped catch-up ticks total; steady windows recorded 30 ticks with zero interval
drops. This is bounded evidence, not complete pedestrian/city coverage, player input, lighting,
damage or E03 acceptance. A separate story launch loaded `CUT1.R` and showed a cutscene type-0
actor (`pad=-2`), not controlled Tanner on foot. See the [native actor change record](../../changes/2026-10-11/native-original-car-bodies/index.md).
