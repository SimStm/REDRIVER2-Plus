---
type: Discussion
title: Engine migration decision
status: adopted
created: 2026-10-08
updated: 2026-10-09
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

## Open implementation decisions

E00 pins SDKs/compiler/targets and inventories remaining consumers; E02 chooses
the compiled package/GLB boundary and readable schema; E01 establishes tested
regional/replay compatibility and backlog policy. Later physics changes require
separate game/replay versioning. No unresolved choice authorizes a silent fallback.

## History

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
