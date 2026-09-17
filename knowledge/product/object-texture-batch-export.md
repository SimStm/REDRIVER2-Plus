---
type: Product
title: Batch texture export with model references
description: How the 3D Debug tab exports a texture set in one action, deduplicates identity, derives model references and filename suffixes from the asset catalog, preserves mappings on re-export, and reports progress, cancellation and failures.
tags: [product, textures, mods, exports, manifest, inspector, batch]
---

# Batch texture export with model references

The 3D Debug inspector can export one texture at a time. Batch export adds
actions that publish a whole set in one step, write catalog-derived metadata, and
report what happened per resource instead of failing silently or as an
all-or-nothing transaction.

## Actions

The Export section of the 3D Debug tab offers three entry points:

- **Export all identified textures (batch)** — the submitted-geometry list, i.e.
  every texture the current selection actually draws with. Unregistered regions
  are omitted. This is the visible-only scope.
- **Export all source-model textures (catalog)** — every material the asset
  catalog links to the selected source model, including hidden faces, plus its
  high-detail LOD sibling. A material whose page is not currently registered is
  reported rather than dropped.
- **Export this texture** — the single-resource action, unchanged in spirit; it
  runs through the same batch engine with a one-item plan.

The scope text under each action states what was collected and what was not:
the model count, the material count, the number of missing VRAM regions with an
example, and the standing note that palette variants and cross-model child parts
are not enumerated by this action.

## Identity and deduplication

The unit of work is the override identity `(texture, texturePage, textureIndex)`,
extended with `clut` for palette variants. `HdTextureOverrides_BeginBatchJob`
plans the set up front and marks an item invalid or duplicate instead of
queueing it, so a texture shared by several models is published once and a bad
item never consumes an export slot. A per-resource failure does not abort the
rest of the batch.

## Model references and file names

A newly appended entry gains two pieces of catalog-derived metadata:

- `modelReferences`, a backward-compatible array of stable catalog ids
  (`model:<level>:<variant>:<index>`) that keeps every model sharing the texture.
  It is never part of the override key and never duplicates the image.
- a readable, sanitized model filename suffix. When no catalog record exists the
  suffix falls back to an explicit `slot<index>` / `unknown:model-slot:<index>`
  form; a label with no alphanumeric character has no token.

With **Organize exports by type and level** enabled, files land under
`assets/inspector/<type>/<level>/NAME_p<page>_i<index>[_slot<N>].png`, alongside
the descriptive `type` and `level` fields.

## Re-export preserves and merges

Re-exporting an existing identity does not append a second registration:

- the PNG is written to the manifest's existing mapped `file` when that path is
  a safe relative asset path, so a remastered mapping is not lost;
- new `modelReferences` are merged into the same entry textually, preserving the
  stored order and skipping duplicates;
- a pre-existing entry that predates the `type`/`level` fields gains them on the
  next re-export with a context, still without a duplicate entry;
- unknown fields survive because the document is never re-serialized, and a
  malformed or missing `textures` array leaves the document untouched.

## Cooperative batch, progress and failures

Large batches run across frames rather than blocking:

- `HdTextureOverrides_StepBatchJob` exports a bounded number of resources per
  frame, so the panel keeps drawing a progress bar;
- `HdTextureOverrides_CancelBatchJob` stops further work and keeps every file
  already published;
- `HdTextureOverrides_RetryFailedBatchJob` re-queues only the failures;
- `HdTextureOverrides_GetBatchJobStatus` plus the per-item `outcomes` and
  `messages` arrays report each resource as exported, duplicate-omitted or
  failed, with the last failure named.

The panel shows the progress bar, a cancel control, a dismiss control, a retry
action, a scope line and a per-resource result list.
`HdTextureOverrides_ExportTextureBatch` drives the same job to completion for
programmatic callers, so the panel and a caller share one code path.

## Limits

- Palette variants and cross-model child parts are not enumerated by the batch
  actions. Palette variants are exported by the separate **Export all palette
  variants (PNG)** action for car and pedestrian selections; the batch notes the
  limitation in its scope text.
- Source-model scope needs a labelled catalog model. Cars are a separate pack and
  streamed/unlabelled sources have no model slot, so those selections report that
  source-model export is unavailable and only the identified-list scope applies.
- A material whose page is not currently registered is a missing adapter, not an
  export.
- References are merged up to a fixed per-entry bound, carry no LOD or palette
  qualifier, and provenance stays `declared`/`unknown`, never `verified`.
- An existing mapped `file` that is not a safe relative asset path is left
  unchanged and the PNG falls back to the default inspector path.
- The panel batch actions are Windows-only; the engine and catalog paths are
  platform-neutral and covered by the standalone suites.

## Validation

Measured in the Chicago debug start (in-game, `Release_dev`), selecting tile
`GRASS01C` (source model 76):

- one click of the catalog-scoped batch reported `total=1 exported=1 dup=0
  failed=0` and wrote
  `assets/inspector/tiles/chicago/GRASS01C_p1_i5_slot76.png` with `"type":
  "tiles"` and `"level": "chicago"`;
- a repeat run of the same plan reported `exported=1 dup=0 failed=0` and left the
  manifest at 80 entries / 80 distinct identities / 0 duplicate groups;
- the entry kept all 8 shipped model references
  (`model:0:0:76`, `78`, `79`, `80`, `88`, `100`, `103`, `118`), showing a shared
  texture retains multiple references;
- the scope text named the missing-VRAM count and the un-enumerated palette
  variants.

Standalone: `AssetCatalogTests` 145 checks / 0 failures and
`InspectorExportTests` 104 checks / 0 failures (`scripts/run_inspector_tests.ps1`),
covering deduplication, model references, sanitized suffixes, per-resource
failure reporting, mapped-file replacement, reference merging, type/level
backfill, the known-texture region adapter, and cooperative stepping,
cancellation, duplicate planning and retry.

## Related

- Manifest merging: [`knowledge/product/texture-manifest-merge.md`](texture-manifest-merge.md)
- Asset catalog: [`knowledge/product/asset-catalog-identity.md`](asset-catalog-identity.md)
- Palette variants: [`knowledge/product/texture-palette-variants.md`](texture-palette-variants.md)
- Selection coverage: [`knowledge/product/inspector-selection-coverage.md`](inspector-selection-coverage.md)
- Tests: [`src_rebuild/tests/InspectorExportTests.cpp`](../../src_rebuild/tests/InspectorExportTests.cpp)
