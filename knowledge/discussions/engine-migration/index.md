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
