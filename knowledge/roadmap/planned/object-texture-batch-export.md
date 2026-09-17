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

Order 05. [03](../done/texture-manifest-merge.md) is required for safe registration. A bounded visible-list milestone can precede [04](../done/asset-catalog-identity.md); complete source export depends on that catalog.

## Suggested execution order

Overall order: **05** in the [planned catalog](index.md).
Execute the following milestones sequentially. A request for one milestone
is not authorization to implement all later milestones or dependent features.

1. Implement batch export for the current identified list, deduplicating by identity and reporting omitted/failed resources.
2. Specify `modelReferences` and safe filename suffixes; derive references from verified source metadata or use an explicit fallback.
3. Preserve an existing mapped `file` during re-export and merge reference lists without duplicating the texture entry.
4. Use catalog material relationships to include hidden faces and supported child parts; define whether all LODs and palettes are included.
5. Add progress, cancellation, per-resource results and retry. Do not require one all-or-nothing transaction for a large batch, but publish each file safely.

## Delivery progress

Partial delivery only; the record stays `planned`.

- **Milestone 1 — delivered (2026-09-16).** The 3D Debug tab's identified-texture
  list has one batch action that deduplicates by the exact `(name, texturePage,
  textureIndex)` identity, publishes each resource once, and reports exported,
  duplicate-omitted and failed counts with the last failure named. Evidence:
  `HdTextureOverrides_ExportTextureBatch` in
  `src_rebuild/utils/HdTextureOverrides.{h,cpp}` and the new cases in
  `src_rebuild/tests/InspectorExportTests.cpp`.
- **Milestone 2 — delivered (2026-09-16).** Newly appended manifest entries carry
  a backward-compatible `modelReferences` array and a readable, sanitized model
  filename suffix derived from the asset catalog's stable model id and declared
  name, falling back to an explicit `slot<index>` / `unknown:model-slot:<index>`
  form when no catalog record exists. `modelReference` is not part of the
  override key and a texture shared by several models is still one image.
  Evidence: `AssetCatalog_EnumerateTextureModels` in
  `src_rebuild/Game/C/assetcatalog.{h,c}`, `HdTextureOverrides.{h,cpp}`,
  `DeveloperGraphicsPanel.cpp`, and the new cases in `AssetCatalogTests.cpp`
  and `InspectorExportTests.cpp`.
- **Milestone 3 — delivered (2026-09-16).** A re-export locates the existing
  registration first and writes the PNG to its mapped `file` path instead of a
  new name, then merges new `modelReferences` into the same entry textually
  (stored order preserved, duplicates skipped) without appending a second
  registration; unknown fields are still preserved. The array span is validated
  before merging so a malformed or missing `textures` array leaves the document
  untouched. Evidence: `ManifestEntryLocation`/`LocateTextureEntry`,
  `ParseModelReferencesValue`, `MergeReferenceLists`,
  `ReplaceReferencesInManifest` and `CreateDirectoriesForFile` in
  `src_rebuild/utils/HdTextureOverrides.cpp`, covered by the mapped-file cases
  in `src_rebuild/tests/InspectorExportTests.cpp`.
- **Milestone 4 — delivered (2026-09-16).** The 3D Debug tab separates
  "export identified visible textures" from a new catalog-scoped action that
  exports every material the catalog links to the selected source model,
  including hidden faces, plus its high-detail LOD sibling. LOD policy: the
  selected model and its high-detail sibling are included; lower-detail siblings
  are not linked by the loader and are never included. Palette variants and
  cross-model child parts are not modelled and the report states that. A catalog
  material with no registered VRAM region is named as a missing adapter instead
  of being dropped. Evidence: `AssetCatalog_CollectExportModels` in
  `src_rebuild/Game/C/assetcatalog.{h,c}` (LOD policy, covered by
  `AssetCatalogTests.cpp`) and `HdTextureOverrides_GetKnownTextureRegion` plus
  `BuildSourceModelBatchItems` in `src_rebuild/utils/HdTextureOverrides.{h,cpp}`
  and `DeveloperGraphicsPanel.cpp` (adapter, covered by `InspectorExportTests.cpp`).
- **Milestone 5 — delivered (2026-09-16).** `HdTextureOverrideBatchJob` turns a
  batch into a cooperative job: `BeginBatchJob` builds the identity plan
  (marking invalid and duplicate items without queueing them), `StepBatchJob`
  exports a bounded number of resources per frame, `CancelBatchJob` stops further
  work while keeping already-published files, `RetryFailedBatchJob` re-queues only
  failures, and `GetBatchJobStatus` plus the per-item `outcomes`/`messages`
  arrays report each resource. The 3D Debug tab shows a progress bar, a cancel
  control, a per-resource results list and a retry action; there is no
  all-or-nothing transaction, and `HdTextureOverrides_ExportTextureBatch` still
  drives the same job to completion for programmatic callers. Evidence:
  `src_rebuild/utils/HdTextureOverrides.{h,cpp}`,
  `src_rebuild/utils/DeveloperGraphicsPanel.cpp`, and the stepping, cancellation,
  duplicate-plan and retry cases in `src_rebuild/tests/InspectorExportTests.cpp`.
- **Completion status:** all five milestones are delivered, but the record stays
  `planned` until the acceptance criteria and validation plan are evidenced —
  notably the local police car / school bus / tree / road comparison against
  catalog material counts and palette-variant coverage. Selection of cars and
  pedestrians is limited by the inspector coverage work in items 06 and 07.
  Known limits: palette variants and cross-model child parts are not enumerated;
  references are merged up to a fixed per-entry bound and carry no LOD/palette
  qualifier; provenance is still `declared`/`unknown`, never `verified`; a
  texture whose page is not currently registered is reported as a missing
  adapter; an existing mapped `file` that is not a safe relative asset path is
  left unchanged and the PNG falls back to the default inspector path.

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
