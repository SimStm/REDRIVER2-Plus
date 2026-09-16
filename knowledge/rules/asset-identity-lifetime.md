---
type: Rule
title: Keep asset identity and instance identity separate
description: Resource records are shared and stable; instances and runtime slots are not, and retained references must detect reuse.
tags: [okf, assets, inspector, identity, lifetime]
---

# Keep asset identity and instance identity separate

**When** adding or changing inspector, export or catalog code that identifies a
model, texture, car or palette, **then** distinguish the resource from the
instance that references it. Two placements of one model share one resource and
remain separately identifiable by the instance (position, yaw, owning
generation); a runtime slot is not an identity.

The contract and its source inventory are in
[the asset catalog discussion](../discussions/asset-catalog-identity/index.md);
the implementation is `src_rebuild/Game/C/assetcatalog.{h,c}`.

- **Stable ids are strings, never pointers.** Models
  `model:<level>:<variant>:<index>`, textures `tex:<name>:<page>:<index>`, cars
  `car:<level>:<modelNumber>`. A raw runtime pointer must never be a persistent
  identifier.
- **Textures key on the manifest triple.** `(name, texturePage, textureIndex)`
  is the mod-manifest identity; do not invent a new key or duplicate a shared
  texture per model. The catalog exposes this triple unchanged.
- **Provenance is explicit.** Mark a source `UNKNOWN`, `DECLARED` (from the
  level name table) or `VERIFIED`; never infer an archive filename from a
  display label.
- **Relationships beat triangle lists.** Model/texture and model/LOD links are
  many-to-many records, so hidden faces, LOD variants and shared materials can
  be enumerated without the submitted triangle set.
- **Retained references carry a generation and revision.** Slot reuse, streaming
  replacement and level/variant changes must invalidate them; reusing a model
  slot or re-registering a record bumps its revision.
- **Bounded and allocation-free.** Fixed capacities, registration during load or
  streaming, and no per-frame work or draw-time allocation. Guard
  desktop-only catalog storage out of the PSX build.
- **Do not couple PsyCross to game formats.** Keep the generic inspector hooks
  generic; game-format identity belongs in the game-owned module.
