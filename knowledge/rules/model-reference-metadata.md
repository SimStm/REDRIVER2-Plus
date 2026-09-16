---
type: Rule
title: Keep model references as metadata outside the texture override key
description: Batch and re-export metadata must not change texture identity, duplicate shared images, or redirect a mapped file.
tags: [okf, mods, exports, manifest, inspector]
---

# Keep model references as metadata outside the texture override key

**When** exporting inspector textures (single or batch), **then** treat the
exact `(texture, texturePage, textureIndex)` triple as the only identity and
keep model information as non-key metadata.

- Deduplicate a batch by that triple, not by filename, model, or VRAM region: a
  texture shared by several models is published once and lists every model
  reference.
- Write `modelReferences` as a list on the manifest entry. The override loader
  must keep ignoring it; never include it in the override matching key.
- Derive references and the readable filename suffix from the source-aware asset
  catalog (stable model id and declared name). When no catalog record exists,
  state the fallback explicitly (`slot<index>` / `unknown:model-slot:<index>`)
  instead of inventing an archive name from a display label.
- A model suffix may extend the PNG filename, but it is not identity: on
  re-export keep the manifest's mapped `file` and write to that path.
- Distinguish a visibility-scoped export (submitted triangles) from a
  source-model export (every catalog material, hidden faces included, plus the
  high-detail LOD sibling). Name a catalog material whose VRAM region is not
  registered as a missing adapter; never drop it silently. Palette variants and
  cross-model child parts are not enumerated until the catalog models them.
- Sanitize every label used in a filename so it cannot introduce a path
  separator, drive letter, or extension. A label with no alphanumeric character
  has no usable token.

See [Reconstruct texture manifests from inspector filenames](texture-manifest-from-filenames.md),
[Append texture registrations without rewriting manifest data](manifest-append-merge.md)
and [Keep asset identity and instance identity separate](asset-identity-lifetime.md).
