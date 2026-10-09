---
type: Change
title: Preserve the renderer rework and adopt engine migration planning
created: 2026-10-08
tags: [okf, git, engine, roadmap, documentation]
---

# Preserve the renderer rework and adopt engine migration planning

## Outcome and branch provenance

The user requested preservation of existing work followed by an independent
engine-migration branch and roadmap. The existing repository used `master`,
not `main`. Created/pushed `feat/vulkan-native-rework` at
`e68b6ea6b1e30543af77d6244c109f7993ff65cf`; the remote branch SHA was verified.
The snapshot contains250 changed files, including source/tests/tooling/docs and
archive moves. No new renderer acceptance was declared; R03 remains incomplete.

PsyCross was clean at `ae83dd229f2c4a287575fb4cb853006996d9eef8`, already published
on its fork remote `master`; the snapshot records that gitlink. Created local
`main` at unchanged `origin/master` `2e93f215c54e61dd1bbb5d26e439c25303466709`,
then `feat/new-engine` from it. No remote default branch was renamed or force-pushed.
Updated the clean submodule checkout to the baseline gitlink
`37d399dfd4584df746647cba4d2d832bc27f0922` with `git submodule update --init --recursive`.

Generated Python bytecode was excluded from both branches. Ignored local captures,
analysis helpers/receipts and build/runtime data remain local; they were neither
committed nor deleted. Old generated executables/projects may belong to the other
branch and must not validate new-engine source without isolated regeneration.

## Adopted planning changes

[engine-migration](../../../roadmap/planned/engine-migration.md) now owns E00-E07:
dependency/behavior baseline, project-owned services/fixed ticks/replays, converted
original content, Diligent playable slice, FMOD Core, PopcornFX/native presentation,
full mechanics/mode acceptance and final PsyCross removal. Five local issue briefs
map this sequence to the user's intended implementation tasks. No external issues
were created and no stage implementation was started.

Future Studio/RmlUi/editors/supplementary physics are recorded separately inside
the same scope without blocking the first playable slice. The user confirms
permitted middleware use. Mechanics preserve their regional tick/input/math/RNG
contracts; presentation FPS and CPU clock cannot drive physics or replay timing.
Native graphics can differ intentionally, with structural conversion checks,
deterministic game-state tests and targeted visual/resource/audio acceptance.

Agent guidance, rules, catalogs, status/handoff and eight former overlapping plans
were aligned. Historical plans/discussions remain in place with explicit
supersession; unrelated product/completion evidence remains available. Legacy
bridge/composite rules apply only to their current compatibility consumers.
OpenGL backend support stays independent; no graphics backend was changed.

## Validation scope

Executed root staged/unstaged whitespace checks during preservation, fork clean/
remote reachability checks and snapshot remote SHA equality. Seven new stage files
had extra EOF blank lines and were normalized before the snapshot's cached check.
Upstream release8.0/tag `b2d8857426f815007486def9bd23f9a6007a2c64` was verified
through GitHub API, published2026-07-02. This new branch changes documentation
and Python cache ignores only; no new engine/gameplay/audio/VFX code was changed.

Documentation checks PASS:42 changed files limited to Markdown/MDC and Python
cache ignores;20 new/rewritten documentation surfaces with70 valid local links;
eight planned stage files each with deliverables, at least five acceptance gates
and validation; five issue briefs; eight old plans explicitly superseded with
obsolete execution-order metadata removed. Status files are147/99 lines, under350.
No premature done/product engine-migration record exists. Root `git diff --check`
PASS, fork clean, and `main`/`master`/`origin/master` all equal the unchanged
baseline. No `src_rebuild`, `.gitmodules` or data changes exist on the new branch.

All94 newly added local links across changed documentation also PASS. No game
build/run, GPU capture or audio/VFX test was run in this task. Baseline
September evidence and separate rework evidence were not rerun.

## Follow-up: defer optional PopcornFX

The user changed the sequence: required E00-E07 now cover services/ticks,
conversion/Diligent, FMOD Core, existing native effects/UI, full-game acceptance
and PsyCross removal. PopcornFX moved from mandatory E05 to optional final E08,
only upon a separate explicit request after E07. It does not block completion;
no SDK acquisition or implementation is required now. The original planning
summary above records the initial decision, superseded by this follow-up.

Updated stage/issue/roadmap contracts, catalogs, agent guidance, status/handoff
and changelog. Checked documentation-only scope, local links, E00-E08 ordering,
planned stage acceptance/validation contracts, status line limits and whitespace;
all passed. Reverified upstream release8.0 and tag baseline through GitHub API.
No engine code, dependency, Git branch or runtime behavior was changed, and no
build, GPU/audio/VFX runtime test was needed or claimed.
