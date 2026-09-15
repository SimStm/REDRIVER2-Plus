---
type: Roadmap
title: Batch texture export with model references
status: planned
execution_order: 5
tags: [roadmap, inspector, exports]
---

# Batch texture export with model references

## Problem

A selected object can use many textures, but exports currently require individual clicks. A visible-triangle list is incomplete for hidden faces and omitted parts, and dumps lack useful model grouping.

## Intended behaviour

Offer one action to export a supported model's texture set with stable references and a clear partial/completed result.

## Scope

Separate 'export identified visible textures' from 'export all source-model textures'. Add readable filename suffixes and backward-compatible model reference metadata, preferably a list for shared resources.

## Non-goals

Do not label a partial capture as complete. Do not make `modelReference` part of the texture override key or duplicate an image just because multiple models use it.

## Dependencies and risks

Order 05. [03](../done/texture-manifest-merge.md) is required for safe registration. A bounded visible-list milestone can precede [04](asset-catalog-identity.md); complete source export depends on that catalog.

## Suggested execution order

Overall order: **05** in the [planned catalog](index.md).
Execute the following milestones sequentially. A request for one milestone
is not authorization to implement all later milestones or dependent features.

1. Implement batch export for the current identified list, deduplicating by identity and reporting omitted/failed resources.
2. Specify `modelReferences` and safe filename suffixes; derive references from verified source metadata or use an explicit fallback.
3. Preserve an existing mapped `file` during re-export and merge reference lists without duplicating the texture entry.
4. Use catalog material relationships to include hidden faces and supported child parts; define whether all LODs and palettes are included.
5. Add progress, cancellation, per-resource results and retry. Do not require one all-or-nothing transaction for a large batch, but publish each file safely.

## Acceptance criteria

One click exports all resources in the explicitly stated scope. Repeating it does not duplicate registrations or lose remastered mappings. Shared textures retain multiple references. The report names missing adapters, palette variants and failed files.

## Validation plan

Use a police car, school bus, tree and road locally; compare counts against catalog materials rather than visible faces alone. Test duplicate references, cancellation, partial failure and filename reconstruction with a model suffix.

## Starting points

`src_rebuild/utils/DeveloperGraphicsPanel.cpp`, `HdTextureOverrides.cpp`, `InspectorExport.h`, and the catalog introduced in order 04.
Paths are relative to the repository root. Inspect current source and local
diffs before editing; these proposals are not proof of current behavior.

## Handoff and completion

For each requested milestone, report changed files, checks actually run,
remaining limitations and the next unblocked milestone. Preserve C++11 and
platform guards; maintain game-specific behavior outside PsyCross and carry
submodule changes in the project patch. Do not include game assets in tests
or commits. Only after all acceptance criteria are met, publish
`knowledge/product/object-texture-batch-export.md`, move this record to `done/`, update
status/date/catalog links, and record the delivered change in Unreleased.
