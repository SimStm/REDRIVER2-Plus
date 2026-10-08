---
type: Status
title: Recent engineering context
description: Vulkan rework preserved; new engine migration planned from the existing baseline.
tags: [okf, status, agents]
---

# Recent engineering context

> Updated: 2026-10-08.
> Confirm branch/source/status before acting; implementation has not begun.

## Latest user decision

The user adopted Diligent Engine/DiligentFX, FMOD Core (Studio later), PopcornFX,
OpenDriver2Tools-informed original-content conversion, project-owned services/
simulation ticks and final PsyCross removal. Future directions include RmlUi,
internal map/model/sound/mission/cutscene editors and supplementary physics.
The user confirms permitted middleware use. Do not ask again about adopting it.

Preserve Driver 2 mechanics, missions/cutscenes and replay timing; high display
FPS or CPU clocks must not accelerate simulation. Initial fixed ticks retain
actual NTSC/PAL behavior. Conversion produces modern representations, not
automatically new high-detail art or complete gameplay packages.

## Branch preservation actually completed

Original checkout was `master`, parent `2e93f215`, with extensive dirty source/
tests/tooling/docs and a staged fork gitlink. No local/remote `main` existed.

Created and pushed `feat/vulkan-native-rework` at
`e68b6ea6b1e30543af77d6244c109f7993ff65cf`; remote SHA matched after push.
All versionable existing work, including archive moves, is preserved there.
Generated Python caches were excluded; data/build paths remain ignored.
Seven stage docs had extra EOF blank lines; normalized before cached whitespace
validation and commit. No renderer test was rerun or acceptance claim added.

Fork was clean at `ae83dd229f2c4a287575fb4cb853006996d9eef8`, already reachable
from fork remote `master`; no fork commit/push was necessary.
Rework R03 remains incomplete. Local ignored captures/private scripts/receipts
under `src_rebuild/build/native-renderer-r03/` do not travel with Git; preserve
them separately before cleanup. No local assets or generated directories deleted.

Created local `main` from unchanged `origin/master`, without renaming/pushing
the remote default branch, then created `feat/new-engine`. Initialized its
matching fork gitlink `37d399dfd4584df746647cba4d2d832bc27f0922`; fork clean.
This new branch contains the older working baseline and new planning docs,
not the independent native rework. Do not merge that branch wholesale.

## Documentation delivered

The [umbrella](roadmap/planned/engine-migration.md) is the sole new execution owner.
[Stage catalog](engine-migration/index.md), [validation](engine-migration/validation.md)
and [five issue briefs](engine-migration/issue-briefs.md) define:

1. E00 inventory/baseline and E01 project services/fixed ticks/replay contracts.
2. E02 conversion and E03 Diligent first driveable original-content slice.
3. E04 FMOD Core voice/event/sample/resource migration.
4. E05 PopcornFX/UI/maps/effects and E06 full mechanics/city/mode acceptance.
5. E07 remove PsyCross only after all required consumers pass.

All stages remain planned. No external GitHub issues were created.
Earlier hybrid/high-FPS/export/legacy-lighting roadmaps remain available with
superseded execution notes. Rules/AGENTS/catalogs now route agents to this plan;
legacy bridge rules apply only to repairs of their existing consumers.
Status was consolidated below350 lines without adopting other-branch results.
[Future extensions](engine-migration/future-extensions.md) capture Studio/RmlUi/
editors/physics without making them prerequisites of the first playable slice.

## Evidence and next concrete action

Read this branch's source: `FilterFrameTime`/`VSync` gate `StepGame` and drawing;
`CameraCnt`/replay repeats are simulation-indexed; `FrameCnt` is draw-coupled.
Handling calls GTE; sound calls SPU; spooling relies on CD/DrawSync callbacks.
Do not remove PsyCross just because mechanics source files are outside it.

The next separately requested issue is **E00/E01**. Inventory every contract and
capture deterministic NTSC/PAL baselines; then extract services and implement
independent tick scheduling/replay-compatible snapshots. No existing executable
validates that change. Fresh isolated projects must use this branch's sources/
gitlink; ignored old generated solutions/binaries can still contain rework code.

Use canonical tick-state/event comparisons and structural conversion checks.
Native perspective/lighting/AA/VFX may differ intentionally. Use targeted pixel/
resource/audio tests for actual contracts/defects; universal legacy pixel matching
is not an acceptance prerequisite. Keep loading/errors explicit and ownership,
synchronization, bounded allocations and cleanup intact.

Direct MSBuild/project CLI/local RenderDoc1.41 are permitted for routine checks;
SDK APIs must be verified in primary docs. Observe one owned process across yields
instead of relaunching on RPC response limits; debugger tools remain useful for
stacks/state. Live input injection alone is not a reliable route fixture.

This task changed documentation/branch preservation only. No engine build,
gameplay/GPU capture/audio/VFX test ran. Upstream8.0/tag
`b2d8857426f815007486def9bd23f9a6007a2c64`/2026-07-02 release was verified via API.
[Change record](changes/2026-10-08/engine-migration-plan/index.md) records executed
documentation/Git checks. Baseline historical September evidence remains linked
in current status/product/change records; it was not rerun here.
