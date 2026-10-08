---
type: Roadmap
title: Engine migration with preserved Driver 2 simulation
status: planned
created: 2026-10-08
updated: 2026-10-08
tags: [okf, roadmap, engine, timing, assets, diligent, fmod, popcornfx]
---

# Engine migration with preserved Driver 2 simulation

## Decision and current state

This is the user-adopted direction for `feat/new-engine`. All E00-E07 stages
below are **planned, not implemented**. This request prepares documentation;
implementation begins only through subsequent explicitly requested work.

The branch starts from `2e93f215c54e61dd1bbb5d26e439c25303466709`, the existing
`master` baseline, through a local `main` alias. Its PsyCross gitlink remains
`37d399dfd4584df746647cba4d2d832bc27f0922`. The independent native Vulkan rework
is preserved at `e68b6ea6b1e30543af77d6244c109f7993ff65cf` on
[`feat/vulkan-native-rework`](https://github.com/SimStm/REDRIVER2-Plus/tree/feat/vulkan-native-rework).
That branch has partial Windows R00-R02 evidence, unfinished R03 and unstarted
R04; none of its native APIs or acceptance claims exists automatically here.
Ignored local captures/builds do not travel with a branch and are not build inputs.

## Problem

The game owns mechanics but also calls Psy-Q-compatible math, clocks, input,
audio, file/CD and graphics services implemented by PsyCross. The current loop
couples presentation to simulation through `FilterFrameTime`, `VSync`,
`StepGame`, `DrawGame`, `FrameCnt` and `CameraCnt`. Replays encode tick-like
frame repeats rather than independent display timestamps. Rendering source
commands through a new API alone cannot remove those dependencies.

## Intended behaviour

Preserve Driver 2 physics, collision, AI, missions, cutscenes, camera intent,
damage and replay semantics in a project-owned simulation. Extract required
PsyCross services into focused project modules or verified replacement adapters.
Use Diligent Engine/DiligentFX for presentation, FMOD Core for audio and
PopcornFX for VFX. Remove PsyCross after its last supported consumer migrates.
Higher display FPS and faster CPUs must not speed up game mechanics.

Original installation files remain the source of truth for original content.
A reproducible converter, informed by OpenDriver2Tools, produces versioned
native visual/audio packages. Human-readable manifests and metadata accompany
machine-efficient geometry, textures and sound data. The game runs these new
representations without requiring original graphics command generation.

## Scope and architecture

- **Simulation:** fixed-step game logic, stable tick IDs, deterministic input,
  RNG and replay timelines; original fixed-point math semantics where required.
- **Project platform services:** clock, window, input, filesystem/streaming,
  callbacks, diagnostics and lifecycle. Reuse suitable SDL services directly;
  do not copy all of PsyCross into a renamed monolith.
- **Asset compiler:** original assets -> shared meshes/materials, sector
  instances and audio, plus provenance, stable IDs and gameplay bindings.
- **Visual scene:** immutable snapshots of transforms, animation and damage;
  independent visual residency, bounds, LOD and interpolation.
- **Presentation:** one Diligent device/swapchain owner, explicit materials,
  depth, lighting/shadows, VFX, UI and resource retirement.
- **Audio:** FMOD Core voices/events driven by simulation state, with a stable
  interface suitable for later FMOD Studio banks and events.

The user confirms FMOD/PopcornFX use is permitted for this project and adopts
OpenDriver2Tools as a format reference. Do not reopen that product choice as an
approval gate. Record concrete SDK versions, acquisition/build requirements,
attribution and any terms relevant to code actually reused. Keep proprietary
SDKs and original/converted game content out of public source commits.

## Sequence and issue mapping

1. [E00 — Dependency inventory and behavioral baseline](../../engine-migration/stages/00-dependency-baseline.md).
2. [E01 — Project services, fixed ticks and replay independence](../../engine-migration/stages/01-services-ticks-replays.md).
3. [E02 — Reproducible original-content conversion](../../engine-migration/stages/02-asset-conversion.md).
4. [E03 — Diligent renderer and first playable slice](../../engine-migration/stages/03-diligent-playable-slice.md).
5. [E04 — FMOD Core audio](../../engine-migration/stages/04-fmod-core.md).
6. [E05 — PopcornFX and complete presentation consumers](../../engine-migration/stages/05-vfx-and-presentation.md).
7. [E06 — Mechanics and full-game integration acceptance](../../engine-migration/stages/06-game-integration.md).
8. [E07 — Remove PsyCross and validate the cutover](../../engine-migration/stages/07-remove-psycross.md).

The [issue briefs](../../engine-migration/issue-briefs.md) map the user's five
requested implementation issues onto these stages. Asset conversion is an
explicit prerequisite of rendering; PopcornFX and all still-required UI/effects
are explicit prerequisites of final removal. Issue names do not prove completion.

Deliver coherent functional blocks. E00/E01 protect the simulation; E02/E03 aim
at a driveable converted region early. Define and pass each block's essential
contracts before its dependents, without requiring explanation of every legacy
pixel or implementing unrelated optional features first. Preparation may overlap
when independent, but partial delivery must remain labeled and bounded.

## Non-goals and later extensions

The migration does not immediately change car physics, normalize PAL to NTSC,
increase simulation frequency, rebuild every editor, infer PBR from old artwork,
or redistribute game assets. "Converted models" means a new representation of
the originals; higher-detail authored replacements are a separate content layer.

FMOD Studio authoring, RmlUi menus/HUD, in-game map/model/sound/mission/cutscene
editors and a supplementary physics framework are adopted future directions.
They consume the same IDs, package metadata and service boundaries. They are not
prerequisites for the first playable slice. Basic native UI is required before
PsyCross removal even if RmlUi remains future work. A new physics solver initially
owns visual-only bodies; never let two solvers author the same gameplay body.
Changed simulation rates/physics require versioned replay compatibility and a
separate gameplay acceptance decision. See [future extensions](../../engine-migration/future-extensions.md).

## Dependencies and risks

Audit GTE math used by handling/collision/camera, vblank clocks, SPU state,
CD/spool callbacks, pad sampling, draw-time state/RNG mutations and vendored
dependencies. Preserve bounded memory, synchronization, error reporting and
cleanup while moving ownership. Native failures are explicit; old scene drawing
cannot conceal them. Expected loading, pause and minimized states are distinct.

Fix a SDK/compiler/platform matrix in E00: this project currently has C++11 and
Windows/Linux/web/Android/PSX paths. A modern dependency's requirements must be
scoped intentionally; cross-platform marketing is not tested project support.
Preserve existing OpenGL/platform paths until their consumers migrate. Backend
selection is separate from scene fallback. Keep [OpenGL tracking](port-vulkan-to-opengl-compatibility.md)
independent, with no obligation to implement an OpenGL port before Vulkan work.
Final removal requires a supported-target decision and no hidden PsyCross path.

## Acceptance criteria

- Fixed tick inputs reproduce approved baseline states independently of display
  FPS, present mode and CPU speed; regional behavior and existing replays survive.
- Converted packages retain content coverage, instance identities and source
  linkage; streamed/dynamic/destructible visuals follow game state correctly.
- Diligent owns actual supported rendering; FMOD and PopcornFX own their adopted
  consumers, with useful diagnostics and bounded resource lifetimes.
- Full modes, missions, cutscenes, replays, sound, VFX, menus/HUD and loading work
  with no dependency on PsyCross in the final supported application/build.
- Performance, memory and platform claims have executed evidence. No stage is
  accepted merely because it compiles, does not crash or displays legacy pixels.

## Validation plan and lifecycle

Follow [the validation policy](../../engine-migration/validation.md): deterministic
tick/state tests for mechanics, conversion inventories and corruption tests,
targeted native pixel/resource checks, full integration routes and measured budgets.
Legacy graphics are a diagnostic reference, not a mandatory final visual target.
Use direct build/run/capture and local RenderDoc1.41 where appropriate; observe
the same process across tool yields, with explicit progress and real failure exits.
Use debugger tools for stacks/state rather than adding long RPC-dependent loops.

Each implementation updates its product note, cumulative status/handoff and
Unreleased changelog. Keep this umbrella planned until all required stages pass;
then publish `knowledge/product/engine-migration.md` and move this record to done.

2026-10-08: architecture and stages adopted; source inspection and branch
preservation completed. No engine, tick, asset, audio or VFX implementation run.
