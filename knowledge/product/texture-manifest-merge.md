---
type: Product
title: Append-only texture registration and manifest merging
description: How inspector export adds a missing texture registration without rewriting an existing mod manifest.
tags: [product, mods, exports, manifest]
---

# Append-only texture registration and manifest merging

When the inspector exports a texture into a mod that already has a
`manifest.json`, the export adds a registration only for a missing
`(texture, texturePage, textureIndex)` key and never rewrites unrelated
content.

## Behaviour

- **Identity** is the exact, case-sensitive `(texture, texturePage,
  textureIndex)` triple. The `file` path, model name and any upscale/processing
  suffix are not identity.
- **Missing key:** the export publishes the PNG, then inserts one entry
  textually before the `textures` array's closing `]`. The document is not
  re-serialized, so fields the loader ignores survive.
- **Existing key:** the PNG is replaced and the registration is reported as
  already present; the existing `file` reference is never redirected.
- **Wildcard entries** (an entry without `texturePage`/`textureIndex`) do not
  match a specific export; the specific entry is appended and the wildcard is
  preserved.
- **Duplicate legacy entries** are left untouched; no third entry is added.
- **Malformed `textures` array:** the manifest is left byte-for-byte unchanged
  and the error is reported, while the PNG that was already published is still
  reported so the caller can retry.
- **Atomicity:** the manifest is written through a unique temporary file with
  an intervening-edit check, so a failed write or a concurrent edit preserves
  the previous document.

## Limits

- Automated tests cover repeated exports, wildcard entries, duplicate legacy
  pairs, unknown fields, malformed documents and repeated writes. Escaped
  texture names and an actually denied/destination write are not covered by an
  automated test; publication still preserves the previous file by design.
- The loader ignores unknown metadata, but the merge itself only understands the
  `textures` array; adding new top-level schema fields is out of scope.

## Related

- Rule: [`knowledge/rules/manifest-append-merge.md`](../rules/manifest-append-merge.md)
- Roadmap record: [`knowledge/roadmap/done/texture-manifest-merge.md`](../roadmap/done/texture-manifest-merge.md)
- Tests: [`src_rebuild/tests/InspectorExportTests.cpp`](../../src_rebuild/tests/InspectorExportTests.cpp)
