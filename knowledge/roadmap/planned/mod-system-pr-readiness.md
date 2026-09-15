---
type: Roadmap
title: Mod-system branch PR readiness
status: planned
execution_order: 0
tags: [roadmap, release, validation]
---

# Mod-system branch PR readiness

## Milestone checklist

Base `b2d88574` (`master`); branch `codex/modular-mod-system`. The narrative
evidence is in the audit record at
[`knowledge/changes/2026-09-15/mod-system-pr-readiness/`](../../changes/2026-09-15/mod-system-pr-readiness/index.md).

- [x] **1. Scope inventory / include-exclude list.** Audit record produced;
  unrelated game data and dependency archives stay out of the branch.
  `711935f8` ignores new `data/DRIVER2`/`data/_DRIVER2` files instead of
  committing local assets.
- [x] **2. Loader and exporter review.** Malformed JSON, duplicate ids, path
  traversal, limits, partial writes and repeated exports are covered by the
  export tests and the append-only merge; invalid configuration cannot
  silently enable unintended mods.
- [x] **3. Transparency and flicker classification.** Override cutout and PSX
  `STP` export (`29ba80d0`, `4aff8e56`); `GRASS01C` classified as minification
  aliasing and fixed with opaque-override mipmaps (`423cc8a7`) plus anisotropic
  filtering (`8058c807`).
- [x] **4. Disposable-checkout build.** Fresh clone, submodule init, patch
  apply, Premake and `Release_dev|x64` build succeeded. Fixed the CRLF patch
  corruption (`e9ebce62`) and the PowerShell 5.1 applier hang (`d3560908`).
- [x] **5. Tests and platforms.** 31 export checks pass; Windows `Release_dev`
  and `Debug` build; Linux `release_dev_x64`/`debug_x64` build and start under
  WSLg (`ff87f0b9`); manual reload, repeated export, precedence, alpha and
  level-change scenarios checked.
- [x] **6. Documentation reconciliation.** README, `BUILDING.md`
  (`c1ae5736`), `mods/README.md`, product documents and the Unreleased
  changelog match observed capabilities; inspector selection is marked
  diagnostic and OBJ export as limited.
- [ ] **7. Scoped diff review and PR.** PR body drafted in
  [`pr-description.md`](../../changes/2026-09-15/mod-system-pr-readiness/pr-description.md).
  Pending: open the PR, review the scoped diff in a clean sub-agent, and merge.
  On merge, add `knowledge/product/mod-system-pr-readiness.md` and move this
  record to `done/`.

## Problem

The current modular-mod branch combines mod loading, export, inspector hooks and renderer patches. Known PNG transparency defects remain; build success alone does not establish runtime correctness. The working tree also contains unrelated game-data changes, dependency archives and untracked implementation files.

## Intended behaviour

Prepare a bounded, reproducible foundation PR with truthful feature claims and an explicit merge gate. Do not require completing the entire editor roadmap.

## Scope

Audit the intended base branch and full PR diff, including untracked files; identify required source, tests, patches, templates and documentation. Exclude local game assets, upscaled textures, generated output, dependency ZIPs and unrelated edits from staging without deleting or reverting them. Check licensing and provenance of any distributed examples.

## Non-goals

Do not implement general model import, new graphics effects or every inspector category to make this PR ready. Do not automatically stage everything, commit, push or create a PR without the user's authorization.

## Dependencies and risks

Run this audit first. Complete the correctness portion of [01](../done/texture-alpha-semantics.md) before merge. Use [02](../done/texture-flicker-diagnostics.md) to classify flicker: a binding/depth regression blocks merge; broader quality improvements can be deferred. [03](../done/texture-manifest-merge.md) is recommended before promising a complete export-to-mod workflow, but may be explicitly excluded from a foundation PR.

## Suggested execution order

Overall order: **00** in the [planned catalog](index.md).
Execute the following milestones sequentially. A request for one milestone
is not authorization to implement all later milestones or dependent features.

1. Record the intended PR base and inventory staged, unstaged and untracked changes in both repositories. Produce a proposed include/exclude list; preserve all user changes.
2. Review new loader/exporter code for malformed JSON, duplicate IDs, path traversal, limits, allocation failures, partial writes and repeated exports. Verify invalid configuration cannot silently enable unintended mods.
3. Resolve transparent-PNG rendering and classify the reported flicker. Test disabling overrides restores original rendering, and confirm normal gameplay/input remains unaffected with the panel closed.
4. Regenerate Premake output in a disposable checkout; initialize PsyCross and apply the maintained patch against its pinned upstream commit. Build from that checkout, not only the existing local environment.
5. Run export tests plus manual import/reload, repeated export, mod precedence, alpha, selection and level-change checks. Check Windows Release_dev and Debug; validate Linux compilation or clearly record unavailable coverage.
6. Reconcile README, mods documentation, product documentation and Unreleased changelog with observed capabilities. Mark inspector selection as diagnostic and OBJ export as limited.
7. Summarize passed checks, unresolved blockers and accepted limitations. Request review of the scoped diff; do not claim approval or merge readiness from compilation alone.

## Acceptance criteria

The intended diff contains all required untracked implementation files and no unrelated assets or generated dependencies. The upstream PsyCross gitlink is unchanged and patch application is reproducible. No known alpha/binding correctness regression remains. Export failure preserves existing files, repeated exports work, and the PR description names unsupported platforms/features and validation gaps.

## Validation plan

Record commands, configurations, test outputs and concrete in-game scenarios. Verify clean patch application and idempotence, not only reverse checking in an already modified checkout. Treat any unperformed check as pending, not passed.

## Starting points

`src_rebuild/tests/`, `scripts/apply_psycross_patches.ps1`, `patches/psycross/`, `mods/README.md`, `CHANGELOG.md`, and the complete base-to-branch plus working-tree diff.
Paths are relative to the repository root. Inspect current source and local
diffs before editing; these proposals are not proof of current behavior.

## Handoff and completion

For each requested milestone, report changed files, checks actually run,
remaining limitations and the next unblocked milestone. Preserve C++11 and
platform guards; maintain game-specific behavior outside PsyCross and carry
submodule changes in the project patch. Do not include game assets in tests
or commits. Only after all acceptance criteria are met, publish
`knowledge/product/mod-system-pr-readiness.md`, move this record to `done/`, update
status/date/catalog links, and record the delivered change in Unreleased.
