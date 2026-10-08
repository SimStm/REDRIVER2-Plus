---
type: Plan
title: E02 - Reproducible original-content conversion
status: planned
created: 2026-10-08
tags: [engine, assets, conversion, opendriver2tools]
---

# E02 - Reproducible original-content conversion

## Objective and dependencies

Use E00 identities/content inventory to compile original files into independent
native resources. OpenDriver2Tools is a reference for formats, placements and
exporting; pin the source revision inspected and test its supported variants.
The original simulation/world loading remains authoritative for mechanics.

## Package contract and implementation

- Define units, handedness, winding, origins and hierarchy once. Export shared
  local meshes plus sector instance transforms; do not flatten the city into one
  mesh or lose object identity. Retain model/slot/area/sector and LOD relationships.
- Use GLB/glTF or an explicitly documented compiled mesh format plus readable
  JSON manifests for provenance, flags, IDs, bindings, bounds and dependencies.
  Distinguish geometry identity from gameplay instance identity/generation.
- Decode texture/palette variants, alpha/blend/billboard/decal/animation semantics
  and existing PNG mod precedence. Preserve information that OBJ/MTL omits.
  Original artwork can stay unlit/prelit; author PBR overrides separately.
- Preserve vehicle component and vertex/damage mappings and pedestrian animation
  metadata. Unsupported dynamic categories are explicit, not exported as "done".
- Export sounds with sample rate, loops, pitch reference, bank/voice IDs and music
  metadata, rather than only producing anonymous WAVs.
- Cache locally by source hash, converter/schema/options and active mods. Convert
  on explicit import/first use and rebuild changed packages transactionally.
  Temporary output is never published as ready. Keep assets out of Git.

## Acceptance criteria

- [ ] Original Chicago slice inventory reconciles unique instances, selected
  models/materials and sector placements with no unexplained missing/duplicate data.
- [ ] Source geometry/UV/transform/flags and native package records agree in
  focused fixtures; other cities/variants remain explicitly unaccepted until tested.
- [ ] Two identical conversions produce equal canonical manifests/content hashes.
- [ ] Truncated files, invalid spans/indices, missing references, cancellation,
  changed mods and stale cache schemas fail or rebuild with useful diagnostics.
- [ ] A package loads without calling PsyCross or creating a GPU device.
- [ ] Static source IDs link to game objects, including moved/destroyed states;
  original simulation activation/collision data is not replaced by visual meshes.

## Validation and limits

Test the converter independently using bounded synthetic and local original
fixtures. Compare structural inventories, coordinates, topology/material metadata
and package reload behavior; preview selected content, not every legacy pixel.
Retain a readable report for omitted source types. Export-to-Unity scripts do not
prove runtime integration or complete editors. E03 uses the accepted static slice;
E06 completes dynamic and multi-city category coverage.
