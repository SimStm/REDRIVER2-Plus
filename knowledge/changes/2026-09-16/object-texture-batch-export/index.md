---
type: Change
title: Batch texture export with model references (roadmap item 05, M1-M5)
description: One-click export of the identified texture list or the catalog's source-model materials, identity deduplication, catalog-derived model references and filename suffixes, re-export that preserves the mapped file and merges references, and a cooperative batch with progress, cancel and retry.
tags: [okf, mods, exports, manifest, inspector, batch]
---

# Batch texture export with model references (roadmap item 05, M1-M5)

## Context

The 3D inspector exposed identified textures one export at a time. A scan or
"remaster set" needed many clicks, a texture shared by several models could be
written several times, and the manifest carried no model information. Roadmap
item 03 supplies safe append-only registration and item 04 supplies the
source-aware catalog; this change delivers milestones 1-5 of item 05 on top of
both and keeps item 05 `planned`.

## Decision

- **Batch export (M1)** in `HdTextureOverrides_ExportTextureBatch`
  (`src_rebuild/utils/HdTextureOverrides.{h,cpp}`): one action exports the
  currently identified texture list from the 3D Debug tab, deduplicating by the
  exact `(texture, texturePage, textureIndex)` identity, publishing one PNG per
  identity and reporting exported, duplicate-omitted and failed counts with the
  last failure named. A per-resource failure never aborts the batch.
- **Model metadata (M2)**: appended entries gain a backward-compatible
  `modelReferences` array and an optional readable, sanitized model filename
  suffix. `DeveloperGraphicsPanel.cpp` derives both from the catalog:
  `AssetCatalog_EnumerateTextureModels` (new) keeps every model that shares a
  texture, the reference is the catalog's stable `model:<level>:<variant>:<index>`
  id, and the suffix is the catalog model name. Missing catalog data falls back
  explicitly to `slot<index>` / `unknown:model-slot:<index>`; a label with no
  alphanumeric character has no token. `modelReferences` is never part of the
  override key.
- **Preserve and merge on re-export (M3)**: the export locates the existing
  registration before writing, so the PNG goes to the manifest's mapped `file`
  path instead of a new name, and new references are merged into the same entry
  textually (stored order preserved, duplicates skipped). The `textures` array
  span is validated first, so a malformed or missing array leaves the document
  untouched; unknown fields survive because the document is never re-serialized.
- **Source-model scope (M4)**: `AssetCatalog_CollectExportModels` defines the
  LOD policy (the selected model plus its high-detail sibling; lower-detail
  siblings are not linked and are never included), and
  `HdTextureOverrides_GetKnownTextureRegion` adapts a catalog texture identity to
  its registered VRAM region so a hidden material can be exported.
  `BuildSourceModelBatchItems` in `DeveloperGraphicsPanel.cpp` collects every
  catalog material of the collected models (hidden faces included), names any
  missing adapter, and states that palette variants and cross-model child parts
  are not enumerated. The existing identified-list batch stays the visible-only
  action.
- **Cooperative batch (M5)**: `HdTextureOverrideBatchJob` plans identities at
  `BeginBatchJob` (invalid and duplicate items are marked, not queued),
  `StepBatchJob` exports a bounded number of resources per frame, `CancelBatchJob`
  stops while keeping published files, `RetryFailedBatchJob` re-queues only
  failures, and `GetBatchJobStatus` plus the per-item `outcomes`/`messages` report
  each resource. The 3D Debug tab shows a progress bar, cancel, a per-resource
  results list and retry; `ExportTextureBatch` now drives the same job to
  completion for programmatic callers, and there is no all-or-nothing
  transaction.

## Impact and evidence

- `src_rebuild/tests/InspectorExportTests.cpp`: **68 checks, 0 failures**,
  adding identity deduplication, model-reference metadata, sanitized suffixes,
  per-resource failure reporting, mapped-file replacement, reference merging
  (including inserting the field into a legacy entry), the known-texture region
  adapter, and cooperative stepping, cancellation, duplicate planning and retry.
- `src_rebuild/tests/AssetCatalogTests.cpp`: **87 checks, 0 failures**, adding
  `AssetCatalog_EnumerateTextureModels` (all shared references, true count into a
  zero-capacity buffer, unknown record) and `AssetCatalog_CollectExportModels`
  (selected model plus high-detail sibling, capacity handling, invalid record).
- Windows `Release_dev` x64 solution builds with 0 failed projects.
- Documentation: rule `knowledge/rules/model-reference-metadata.md`, amended
  rule `manifest-append-merge.md`, roadmap progress kept `planned`, and an
  Unreleased changelog entry.

## Limitations

- Palette variants and cross-model child parts are not modelled by the catalog
  and are not enumerated; hidden faces within the listed models are included.
- A catalog material whose page is not currently registered is reported as a
  missing adapter rather than exported.
- All five milestones are delivered, but the roadmap record stays `planned` until
  the acceptance evidence is complete: a local police car / school bus / tree /
  road count comparison and palette-variant coverage, both partly gated by the
  inspector selection work in items 06 and 07.
- References are merged up to a fixed per-entry bound, carry no LOD/palette
  qualifier, and provenance is still `declared`/`unknown`, never `verified`.
- An existing mapped `file` that is not a safe relative asset path is left
  unchanged and the PNG falls back to the default inspector path.
- The panel wiring is validated by build only; the catalog and adapter logic is
  covered by the standalone suites.

## Validation

- `InspectorExportTests.exe` exits 0 with 68 checks and 0 failures.
- `AssetCatalogTests.exe` exits 0 with 87 checks and 0 failures.
- `Release_dev` x64 solution build: 0 failed projects.
- `git diff --check` passed for the source and documentation changes.
