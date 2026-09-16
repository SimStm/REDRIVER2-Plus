---
type: Rule
title: Append texture registrations without rewriting manifest data
description: Add missing texture entries to an existing mod manifest while preserving unknown fields and existing mappings.
tags: [okf, mods, manifest, exports]
---

# Append texture registrations without rewriting manifest data

**When** exporting a texture into a mod that already has a `manifest.json`,
**then** add a registration only if the exact case-sensitive
`(texture, texturePage, textureIndex)` key is missing, and publish the change
atomically.

- Preserve the existing document. Insert the new entry textually immediately
  before the `textures` array's closing `]`; do not re-serialize the JSON,
  because hand-edited or third-party fields that the loader ignores must
  survive the edit.
- Never redirect an existing entry's `file`. A re-export writes the PNG to that
  mapped path, merges new `modelReferences` into the same entry (stored order
  preserved, duplicates skipped), and never appends a second registration.
- Treat the manifest `file` path, model name, and any upscale/processing
  suffix as non-identity. Only `(texture, texturePage, textureIndex)` decides a
  duplicate.
- When the `textures` array cannot be parsed, report it and leave the manifest
  untouched; still report the PNG that was already published so the caller can
  retry.
- Publish through a unique temporary file, verify the destination has not
  changed since it was parsed, then replace it. A failed write, encoder, or
  intervening edit must preserve the previous manifest and identify which
  artifacts were produced.

See [Reconstruct texture manifests from inspector filenames](texture-manifest-from-filenames.md)
for the identity convention and [Validate inspector exports before publication](inspector-export-validation.md)
for the atomic-publication requirements this extends.
