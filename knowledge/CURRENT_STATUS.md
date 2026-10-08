---
type: Status
title: Project status
description: Current branch implementation, preserved rework and adopted engine migration.
tags: [okf, status]
---

# Current status

> Updated: 2026-10-08.
> Source of truth for implemented surfaces on the current branch; verify source/Git.
> [RECENT_CONTEXT](RECENT_CONTEXT.md) records the next task and session handoff.

## Branch and adopted plan

`feat/new-engine` starts from parent `2e93f215c54e61dd1bbb5d26e439c25303466709`,
the existing `master` baseline, through a newly created local `main` alias.
Remote `master`/default branch was not renamed. PsyCross is clean at the baseline
gitlink `37d399dfd4584df746647cba4d2d832bc27f0922`, from the project fork.

The former dirty native Vulkan work is committed/pushed separately at
`e68b6ea6b1e30543af77d6244c109f7993ff65cf` on
[`feat/vulkan-native-rework`](https://github.com/SimStm/REDRIVER2-Plus/tree/feat/vulkan-native-rework).
That snapshot includes source/tests/tooling/knowledge/archive moves and the fork
gitlink `ae83dd229f2c4a287575fb4cb853006996d9eef8`, already published in the fork.
R00-R02 Windows scopes are documented there; R03 is incomplete/R04 unstarted.
Those native APIs/results are not part of this branch's current implementation.
Ignored captures/private helpers/builds remain local and do not travel with Git.

Implemented here: [engine-migration planning](roadmap/planned/engine-migration.md),
E00-E07 stage contracts, five issue briefs, validation policy and updated agent/
catalog guidance. **No new engine behavior is implemented.** Current source still
requires PsyCross. Diligent/DiligentFX, FMOD Core, PopcornFX, converted packages,
independent ticks and eventual removal are planned. Earlier hybrid graphics/
timing plans are superseded execution directions here.

## Project and current services

REDRIVER2 is reconstructed Driver 2 game logic, not an emulator. Physics,
collision, AI, missions and replays live primarily under `src_rebuild/Game/`,
but still call PsyCross math/GTE, clocks, pad, SPU/CD/streaming and GPU services.
Main-loop simulation and drawing share `FilterFrameTime`/`VSync`; replay repeats
and counters remain frame/tick indexed. Independent high-FPS presentation is not
implemented. Future E01 preserves regional semantics before changing clock owners.

PsyCross remains a separate submodule from `git@github.com:SimStm/PsyCross.git`.
Do not substitute upstream or infer removal from a roadmap. Existing C++11 and
Windows/Linux/web/Android/PSX guards remain. SDK/compiler/target compatibility
for the new stack is pending E00, not established by cross-platform advertising.

## Existing renderer baseline

Desktop defaults to Vulkan through PsyCross, with separately selected OpenGL.
This is the existing PSX primitive/hybrid rendering architecture, not Diligent.
[Product behavior](product/vulkan-game-renderer.md) and corresponding source
govern actual support; earlier dated validation is not rerun by this summary.

- Vulkan PSX shaders implement CLUT/texture windows/dither/filtering, VRAM mirror,
  blend/stencil/depth, white untextured primitives and queued VRAM copies.
  `-vkpsxtest` contains readback/CLUT/offscreen/export/blend regression fixtures.
- Existing modern mesh API/importer/PBR/shadow composite shares the PSX projection
  and depth. Its legacy lighting receptivity is optional, with copied scene color
  and reconstructed normals. PGXP dependencies remain on that old composite.
- World/overlay boundary splits gameplay composition from flare/fade/UI tails;
  split-screen uses its existing limited composition path. This does not prove
  native mode/category coverage under the new architecture.
- Prior minimize/restore swapchain and early-return fence deadlock fixes are
  recorded in [minimize evidence](changes/2026-09-21/vulkan-minimize-swapchain-crash/index.md)
  and [fence evidence](changes/2026-09-21/vulkan-frame-fence-deadlock/index.md).
- Persistent swapchain attachments preserve each image independently; general
  previous-presented-frame history is not implemented.
- Shadow-volume centers snap to texels while following the vehicle. Controlled
  historical aligned GL/Vulkan shadow-receive comparison reached IoU 0.985.
  Legacy city geometry still lacks the planned caster path; only imported modern
  meshes cast in the baseline shadow map. One published sun is not local-light coverage.
- Opt-in `PSYX_PERF_LOG` records submission CPU time. Historical 1280x720 Vulkan
  submission was about6.4ms; a30FPS gate remained after uncapping present. GPU
  timestamps, peak memory and broad platform measurements remain pending.
- Standalone `-vkfixture` renders modern GLB/reference fixtures. Its images do not
  accept whole-game behavior or the new engine.

Existing UI parity/blend/offscreen details remain in
[UI product](product/vulkan-ui-image-parity.md) and
[lighting product](product/legacy-lighting-receptivity.md). New migration uses
targeted native contracts, not universal pixel equality with this old pipeline.

## Textures, mods and inspector

- Ordered JSON mods and `mods/enabled.json` retain deterministic precedence.
  Texture keys are name/page/index with optional palette identity; model references
  remain metadata, separate from override keys.
- Existing override alpha/cutout/blend semantics and CPU selection mirror the
  active compatibility policy. Do not silently change these during conversion.
- `AssetCatalog.*` separates stable resources from transient instances, records
  provenance and many-to-many model/texture/LOD links, and invalidates reused slots.
  [Catalog product](product/asset-catalog-identity.md) and standalone tests exist.
- Inspector supports face/material/component/logical-object scopes, source/override
  previews, TXT and single/batch PNG exports with deduplication, progress/retry,
  selection highlights, source labels and car-slot/texture diagnostics.
  [Selection product](product/inspector-selection-coverage.md) and
  [batch export](product/object-texture-batch-export.md) record the shipped scope.
- Versioned offline visual/audio packages and OpenDriver2Tools-based native
  conversion are not implemented. Existing bounded `GltfLoader.*`/owned modern
  fixtures can be evaluated for reuse, not assumed to supply the new city pipeline.

## Gameplay and developer tooling

- Resident procedural playground includes flat surface/collidable boxes, disabled
  donor traffic/missions, reset/frontend return, `-playground` and Take a Ride
  entry. [Playground product](product/playable-testing-playground.md) records P1-P5.
- Debug start settings preserve mission/city/vehicle/position/heading/player/chase;
  the panel generates reproducible command lines. Existing script:
  `scripts/run_debug_start.ps1`.
- F11 Developer Graphics Panel uses vendored ImGui1.91.9b (Windows/Linux) and
  `developer_graphics.ini`, independently from main `config.ini`.
- Schema8 display controls implement provisional window/fullscreen selection,
  confirmation/revert and a visible independent confirmation window/input capture.
  Window sizing/revert was tested historically; fullscreen remains untested.
  Countdown behavior under minimized frames/debugger stops has documented limits.
- Input editor captures/cancels keyboard/controller bindings, marks conflicts,
  supports reset and persists overrides in `developer_input.ini`; game/menu
  mapping ownership and UI input capture remain required.
- Content/language/loading/dynamic-light/widescreen controls describe reload/
  restart requirements. Shared `DeveloperSettingsFile_WriteKeys` preserves
  unknown keys/comments and uses transactional temporary/backup files.
  [Runtime settings product](product/runtime-settings-gui.md) has full details.
- FMOD/PopcornFX, RmlUi, supplementary physics and complete in-game authoring editors
  are not implemented. ImGui/input/window dependencies must migrate before E07.

## Knowledge and validation state

English documentation separates planned intent, product behavior, completion
records and dated change evidence. [Migration validation](engine-migration/validation.md)
requires deterministic mechanics, conversion inventories and targeted native
resources/pixels/audio, with actual budgets and platform scope.

This planning change ran Git branch/fork/remote verification and documentation
hygiene/link/contract checks as recorded in
[the change record](changes/2026-10-08/engine-migration-plan/index.md).
No fresh game build, gameplay run, GPU capture or audio/VFX test was executed.
Previously generated projects/executables may belong to the other branch and
must be regenerated in isolated outputs before any implementation validation.

Existing commands include `git diff --check`, Premake/generated builds,
`-vkpsxtest`, `-vkfixture`, backend flags and
`pwsh -NoProfile -File scripts/run_inspector_tests.ps1`. They prove their stated
baseline scopes only. Inspector export tests require a fresh working directory.
