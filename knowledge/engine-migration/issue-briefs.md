---
type: Plan
title: Engine migration implementation issue briefs
status: planned
created: 2026-10-08
tags: [engine, issues, implementation]
---

# Engine migration implementation issue briefs

These are local briefs for the five user-requested implementation issues, not
published GitHub issues or an instruction to implement the whole roadmap now.
Each task begins by reading [the umbrella](../roadmap/planned/engine-migration.md),
current status and its stage contracts, then inspecting source/diffs.

## 1. Move required services into REDRIVER2 and decouple simulation ticks

Scope: [E00](stages/00-dependency-baseline.md) and
[E01](stages/01-services-ticks-replays.md). Inventory callers, capture deterministic
regional baselines, extract required platform/math/stream/input contracts and
implement fixed ticks, presentation snapshots and replay compatibility.
Preserve initial NTSC/PAL semantics; changing a simulation rate is separate work.
Deliver evidence of equal tick states across display schedules, not just a higher
FPS counter. Keep rendering/audio dependencies explicit for their later owners.

## 2. Implement Diligent Engine and the converted-content pipeline

Scope: [E02](stages/02-asset-conversion.md) and
[E03](stages/03-diligent-playable-slice.md); consumes accepted E01 snapshots.
Build reproducible local packages from original content, using OpenDriver2Tools
format knowledge. Integrate DiligentCore/DiligentFX and deliver a driveable Chicago
slice with actual native resources, original mechanics and an explicit material
policy. No requirement to finish legacy R03 first or reproduce every old pixel.

## 3. Implement FMOD Core

Scope: [E04](stages/04-fmod-core.md); depends on E01 events/E02 audio metadata.
Migrate required original voice/bank/music/listener behaviors with correct tick
timing and resource ownership. Provide an interface suitable for later Studio.
Acceptance includes recorded/mixed output, loop/pitch/event and lifecycle tests.

## 4. Guarantee mechanics and complete presentation through Diligent

Scope: [E05](stages/05-vfx-and-presentation.md) and
[E06](stages/06-game-integration.md), consuming accepted rendering/audio contracts.
Replace UI/maps/offscreen and existing effects with native consumers and finish
dynamic/city coverage. Verify full gameplay, missions, cutscenes and replay matrix
by canonical tick states and event timelines plus native visual/audio evidence.
"Mechanics through Diligent" means original game mechanics feeding the renderer;
physics and mission authority do not move into the graphics library. PopcornFX
is not part of this issue or a requirement for full-game acceptance.

## 5. Remove PsyCross from the project

Scope: [E07](stages/07-remove-psycross.md), after all required consumers pass E06.
Remove submodule/build/includes/link/vendor assumptions; retain necessary extracted
math/service contracts with provenance. Fresh builds/runs must work without PsyCross,
with full integration regressions and an explicit supported-target matrix.
Do not delete still-needed menus/audio/streaming/code before their replacements.

## Optional final follow-up: PopcornFX

[E08](stages/08-optional-popcornfx.md) is outside the five required issues. Begin
only after accepted E00-E07 and a separate explicit user request. It must not
block the working game, PsyCross removal or migration completion.

## Delivery discipline

Implement coherent blocks and report their real evidence and pending consumers.
Reuse automated fixtures; reserve detailed pixels/traces for defects and specified
rendering contracts. Update product/status/handoff/changelog at each completed
block. Future physics changes, FMOD Studio, RmlUi and full authoring editors remain
in [future extensions](future-extensions.md), not hidden requirements of issue 1.
