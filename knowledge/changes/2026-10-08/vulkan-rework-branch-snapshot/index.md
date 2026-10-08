---
type: Change
title: Preserve the native Vulkan rework before engine migration
created: 2026-10-08
tags: [okf, git, rendering, handoff]
---

# Preserve the native Vulkan rework before engine migration

The user requested a separate `feat/vulkan-native-rework` branch containing the
existing source, tests, tooling, documentation and archive moves. This is a
preservation snapshot, not acceptance of the unfinished renderer.

R00-R02 remain accepted only in their documented Windows scopes. R03 remains
incomplete and R04 unstarted. The [pause checkpoint](../../../rendering/r03-pause-checkpoint.md)
records the latest investigation, outstanding negative controls and limitations.
No new build, game run, GPU test or stage acceptance accompanies this snapshot.

The PsyCross working tree is clean at
`ae83dd229f2c4a287575fb4cb853006996d9eef8`; that commit is already reachable from
the project fork's remote `master`. The parent snapshot records that gitlink.
Generated Python bytecode is excluded alongside the existing build/data ignores.
Ignored captures, local analysis helpers and receipts under
`src_rebuild/build/native-renderer-r03/` remain local and are not in the commit.
They must be backed up separately before any future cleanup.

The repository baseline is `master`/`origin/master`, commit
`2e93f215c54e61dd1bbb5d26e439c25303466709`; no `main` existed at inspection.
The requested local `main` will name this unchanged baseline, and
`feat/new-engine` will start there, independently of the Vulkan rework.
No remote default branch is renamed or force-pushed.

Validation before preservation: root unstaged/staged `git diff --check`, fork
`git diff --check`, clean fork status and remote gitlink reachability PASS.
Upstream release `8.0` and tag `b2d8857426f815007486def9bd23f9a6007a2c64`
were verified against the GitHub API (published 2026-07-02).
