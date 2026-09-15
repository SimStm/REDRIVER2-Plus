---
type: Roadmap
title: Append-only texture registration and safe manifest merging
status: planned
execution_order: 3
tags: [roadmap, mods, exports]
---

# Append-only texture registration and safe manifest merging

## Problem

Exporting a new texture into an existing mod does not append its registration. Re-exporting originals must also not redirect an existing remastered mapping back to the dump.

## Intended behaviour

Append missing texture entries without duplicates while preserving existing mappings, metadata, unknown fields and user edits.

## Scope

Use the exact case-sensitive `(texture, texturePage, textureIndex)` identity within the current schema. Define explicit handling for wildcard selectors, duplicate legacy entries, malformed manifests and external edits. Retain transactional export behavior.

## Non-goals

Do not overwrite an existing entry's `file` on ordinary export. Do not treat a model name or processing suffix as texture identity, or silently repair invalid JSON by replacing the document.

## Dependencies and risks

Order 03 after export correctness review in [00](mod-system-pr-readiness.md). Required before [05](object-texture-batch-export.md). Consult the existing filename reconstruction rule.

## Suggested execution order

Overall order: **03** in the [planned catalog](index.md).
Execute the following milestones sequentially. A request for one milestone
is not authorization to implement all later milestones or dependent features.

1. Define merge semantics with fixtures: missing key, existing exact key, wildcard entry, duplicate key, unknown fields and modified upscale path.
2. Use a JSON representation that can preserve unknown fields; validate the current hand-written parser before relying on it for document rewriting.
3. Publish the PNG, then append only a missing exact registration. If metadata publication fails, report the already exported image and make retry idempotent.
4. Write the manifest atomically and detect intervening edits instead of losing them. Preserve metadata and existing file references.
5. Add regression tests for repeated exports, malformed JSON, escaped names, duplicates, write failure and existing remastered entries.

## Acceptance criteria

Exporting ROAD1M twice for the same page/index yields one entry. The same name at another page/index can coexist. Existing remastered paths and custom metadata remain unchanged. Every failure reports which artifacts were published, and retry does not duplicate entries.

## Validation plan

Test synthetic mod directories, interrupted/denied writes, two sequential exports, reloading the generated mod, and round-tripping unrelated JSON fields. No user assets are required for automated tests.

## Starting points

`src_rebuild/utils/HdTextureOverrides.cpp`, `src_rebuild/utils/InspectorExport.h`, `src_rebuild/tests/`, `knowledge/rules/texture-manifest-from-filenames.md`.
Paths are relative to the repository root. Inspect current source and local
diffs before editing; these proposals are not proof of current behavior.

## Handoff and completion

For each requested milestone, report changed files, checks actually run,
remaining limitations and the next unblocked milestone. Preserve C++11 and
platform guards; maintain game-specific behavior outside PsyCross and carry
submodule changes in the project patch. Do not include game assets in tests
or commits. Only after all acceptance criteria are met, publish
`knowledge/product/texture-manifest-merge.md`, move this record to `done/`, update
status/date/catalog links, and record the delivered change in Unreleased.
