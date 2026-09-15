---
type: Rule
title: Reconstruct texture manifests from inspector filenames
description: Map original and upscaled inspector export filenames back to explicit texture override identities.
tags: [okf, mods, textures, inspector]
---

# Reconstruct texture manifests from inspector filenames

**When** populating a texture mod manifest from files exported by the 3D
inspector, including upscaled copies, **then** derive the explicit texture
identity from `<texture>_p<page>_i<index>[_processing-suffix].png`.

- `texture`: the complete name before `_p<page>_i<index>`, preserving case and
  embedded underscores. The manifest field is `texture`, not `textureName`.
- `texturePage`: the decimal integer after `_p`.
- `textureIndex`: the decimal integer after `_i`.
- `file`: the actual image path relative to the directory containing
  `manifest.json`, using forward slashes and retaining the processing suffix.
  Processing suffixes do not belong in the texture identity.

For example, `BWINGC1_p58_i16_upscayl_4x_ultrasharp-4x.png` in
`assets/remastered/` produces:

```json
{
  "texture": "BWINGC1",
  "texturePage": 58,
  "textureIndex": 16,
  "file": "assets/remastered/BWINGC1_p58_i16_upscayl_4x_ultrasharp-4x.png"
}
```

A matching pattern is
`^(?<texture>.+)_p(?<page>\d+)_i(?<index>\d+)(?:_.+)?\.png$`.
Use named captures and numeric conversion; do not split the name on every
underscore. Sort entries deterministically, preserve unrelated manifest
metadata and entries, and replace an existing entry for the same
`(texture, texturePage, textureIndex)` rather than adding a duplicate.
If multiple images resolve to that key, ask which variant to use. Report
unmatched names instead of guessing. Validate JSON, identity uniqueness,
file existence, and coverage of all intended images after editing.

This is a manifest-authoring convention, not implicit runtime filename
discovery. The loader still requires explicit manifest entries. Inspector
exports sanitize filename characters, so names that were sanitized or renamed
cannot always reconstruct the original texture name; consult the selection
report or existing manifest for those exceptions. Page/index identify the
export's level texture set, not a globally unique texture across all levels.
