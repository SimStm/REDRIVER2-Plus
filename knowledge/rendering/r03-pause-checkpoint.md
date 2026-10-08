---
type: Status
title: Native renderer pause and R03 resumption checkpoint
created: 2026-10-08
tags: [okf, rendering, handoff, vulkan]
---

# Native renderer pause and R03 resumption checkpoint

The user cancelled the active goal and paused renderer development on
2026-10-08 to focus on another code area in a separate branch. Goal inspection
returns `goal: null`. This is a work checkpoint, not stage acceptance. No branch,
commit, push or additional implementation was performed for this pause.

## Exact stopping point

R00-R02 are accepted only for their documented Windows scopes. **R03 remains
the first incomplete stage; R04 has not started.** Resume the
[R03 acceptance checklist](stages/03-complete-static-region.md#acceptance-gate),
one item at a time, before advancing in the adopted order R00-R07 -> R10 -> R11
-> R08 -> R09 -> R12. The umbrella roadmap remains planned.

The latest accepted evidence is
[complete composition for two west-road views](r03-composition-trace-validation.md):
4147200 native colour/ID/depth/sky pixels pass with zero errors or exclusions.
All2873 source-selected candidate traces explain the1370 earlier exclusions and
277 provisional ID differences;42 new negative controls reject corruptions.
This is scoped two-view evidence, not all-region or whole-game acceptance.
Four HIGH capture messages and `validationClean=false` remain recorded.

Work stopped during independent original-file geometry provenance verification,
before negative controls or acceptance of that new verification.

## Last investigation: preliminary original topology result

The private helper `check-original-region-topology141.py` independently reads
`data/DRIVER2/LEVELS/CHICAGO.LEV`, without importing the native asset decoder.
It parses permanent/streamed model spans, high-detail LOD selection and shared
parent vertices, then compares exact oriented triangle multiplicities against
all selected native index ranges in the existing normal and wide exports.

The executed host run exited0 in0.879s:530 retained source instances,876 logical
parts and4814 triangles per view matched;43 placement models and43 selected
models. The file has1205 model slots. Seven selected models exist only in
streamed packs:259,261,268,406,407,889,890. The receipt records every matching
pack and original byte span; a matching pack does not establish which pack or
level filename was actually loaded at runtime.

**Still unaccepted:** no corruption/negative controls have been run for this
helper. Placement identity/pose uses the retained live-source ledger, not an
independent raw packed-cell decoder. This does not independently prove UVs,
artwork/material grouping, animated topology, exact runtime file identity,
all camera poses, silhouettes, full-region coverage or performance budgets.
Do not check an R03 gate solely from this preliminary result.

## Evidence location and branch portability

Private captures, raw attachments, source ledger, oracle scripts and receipts
are under `src_rebuild/build/native-renderer-r03/`. This generated directory is
ignored by Git: changing or pushing a branch does not preserve it in Git.
Keep it locally or back it up before cleaning/replacing the checkout. Original
game assets remain local and must not be redistributed. Tracked documentation
preserves the findings and limits, but cannot replace these raw inputs.

SHA256 values checked when saving this checkpoint:

- `data/DRIVER2/LEVELS/CHICAGO.LEV` (recorded input hash):
  `6e45900ed6d89f1cad0783fa38c419810ae4056a9805ceaac5a0a989fdf963cc`.
- `check-original-region-topology141.py`:
  `04f7131422c7c75f1b87f2956d5d9e41c0a3f3bb313208da8c0000eade09c40d`.
- `original-region-topology141.json`:
  `3b33237a75351bcd16647a94b594267171f8051c46145c6c7df1c7decb5d5a68`.
- `west-source-memory.json`:
  `baba5fd6e742a1d1b6df39e957cacd239f8aa8942a6b763e554aabc003973946`.
- `complete-world-normal141/draws.json`:
  `7aca2495da9cfd7e33d123aa79a1532a1053f6f79e44b070153733fb0d591f54`.
- `complete-world-wide141/draws.json`:
  `03f303b30c28c03a8709eef3d1c248b3b90cb1e88ea7056e2da8dbd9f99eea24`.

PsyCross is clean at `ae83dd229f2c4a287575fb4cb853006996d9eef8` in the
project fork; the parent has its staged gitlink and substantial existing tracked
and untracked work. Preserve and inspect both repositories before resuming.
No runtime code, settings or assets were changed by the final topology audit
or this pause. No new build, game run, capture or GPU fixture was executed.

## First actions when renderer work is explicitly resumed

1. Read this checkpoint, CURRENT_STATUS, RECENT_CONTEXT, current source/diffs,
   the R03 checklist and the active rules. Verify private inputs and hashes.
2. Add and run meaningful negative controls for the original topology helper:
   altered source model/LOD/parent/span and native vertex/index/winding/count
   data must fail. Verify original inputs remain unchanged. Publish the actual
   results and scope before accepting the geometry provenance sub-item.
3. Continue source placement/pose and coverage verification for all530 instances;
   earlier captured-camera checks covered ten instances. Establish what remains
   for controlled original-city movement/other-view tree silhouettes, seams,
   auxiliary migration accounting and applicable source categories. The old
   `read-west-sources.py` uses a stale PID/pointers; never rerun it unchanged.
4. Collect actual CPU/GPU and resource budget evidence required by the shared
   gates. Replay duration or a30Hz frame cap is not native GPU cost. Retain the
   warning diagnostics and investigate unresolved validation requirements.

Use direct MSBuild, `rdc-cli` capture and the local **RenderDoc1.41** SDK runner
(`scripts/run_renderdoc_analysis.py`) for routine work. The long composition
replays completed in163.973s/84.143s by observing the same process across tool
yields; avoid the daemon script RPC's30s limit. Use Visual Studio MCP when live
debugger locals/stacks/breakpoints are needed. Follow the
[direct workflow](renderdoc-direct-workflow.md).

The native renderer still aims to replace100% of old rendering. Native failures
must stay explicit; expected loading remains distinct from errors. Preserve
native ownership, synchronization and cleanup, OpenGL backend selection, and
unmigrated R06/R10 consumers. Add no legacy fallback or recovery. This pause
does not change those decisions or authorize advancing to R04.
