---
type: Change
title: E02 reproducible conversion - canonical content hash in exported manifests
created: 2026-10-08
tags: [okf, engine, e02, conversion, assets, determinism]
---

# E02 reproducible conversion - canonical content hash in exported manifests

## Outcome

The texture export path now records a canonical content identity. A 64-bit
FNV-1a hash is computed over the converted pixel data (encoder-independent) in
`ExportTextureCore` and written as a `"contentHash"` field on each new manifest
entry by `BuildManifestEntry`. Two identical conversions of the same VRAM region
therefore produce equal entries, satisfying the E02 acceptance rule "two
identical conversions produce equal canonical manifests/content hashes". The
manifest parser already skips unknown keys, so the field is forward-compatible;
a re-export that merges references or backfills type/level preserves the
existing hash.

### Native asset package manifest

`Game/C/assetpackage.*` adds a pure-data package manifest contract: a readable
JSON manifest (`schemaVersion`, package `id`, `entries[]` with `id`, `type`,
`file`, `contentHash` and `dependencies[]`). `AssetPackage_LoadFromText` parses
and validates it (schema version, required fields, duplicate ids, unknown
dependencies, trailing data), `AssetPackage_SerializeCanonical` writes a
deterministic manifest, and failures reset the package and return a named error
with a human-readable diagnostic. The module never touches PsyCross, the GPU or
game state, so a package loads and is compared without them (E02 acceptance #5)
and a stale schema is an explicit error (E02 acceptance #4).

### Conversion cache contract

`Game/C/conversioncache.*` adds the cache-key contract: a converted resource is
keyed by the source content hash, the converter identity, the converter/schema
version, the converter options and the active mod set. It provides FNV-1a
hashing, a canonical key string, parsing with malformed-key diagnostics,
staleness reasons (`stale-schema`/`stale-source`/`stale-converter`/
`stale-options`/`stale-mods`) and a deterministic cache path. A stale schema or
a changed mod is therefore an explicit rebuild reason (E02 acceptance #4), and
the module has no PsyCross/GPU/game dependency.

### Native mesh record

`utils/GltfWriter.*` adds a bounded glTF/GLB writer for the conversion path
(one mesh/primitive, POSITION/NORMAL/TEXCOORD_0, 32-bit indices, one
pbrMetallicRoughness material, one node matrix), matching exactly what
`utils/GltfLoader.*` reads back. The importer now also retains the first node's
placement as a column-major matrix (`GltfMeshData::transform`), so source
geometry, UVs, transform and material flags are one record. Output is
deterministic, so identical conversions produce byte-identical GLB.

### Transactional converter

`utils/AssetConverter.*` is the publish step of the pipeline: it turns a source
mesh record into a cached native GLB keyed by source hash/converter/schema/
options/active mods. A fresh cached GLB is reused without rewriting; a stale one
is rebuilt at a new path. The new GLB is written to a temporary file, verified by
reading it back, then renamed into place, so a partial conversion is never
published as ready. It returns the published GLB's content hash (the same
identity the package manifest records).

### Package assembly

`Game/C/assetpackage.*` gained a construction API (`AssetPackage_SetId`,
`AssetPackage_AddEntry`) so a converted resource's published content hash becomes
a manifest entry with dependencies. Entries may be added in any order; the parser
still validates dependency resolution, so a package with a dangling reference
fails to load.

### Sound metadata record

`Game/C/soundasset.*` adds a sound record that is more than an anonymous WAV:
sample rate, channels, bit depth, frame count, loop points, a pitch reference,
the source bank/voice ids and, for music, bank/program/sequence ids. It is
validated (field bounds, loop range, music completeness) and serialized
canonically, so a package can reference a converted sound's metadata and two
identical conversions produce equal output.

## Evidence (executed)

`pwsh -NoProfile -File scripts/run_inspector_tests.ps1`:

- `AssetCatalogTests`: 145 checks, 0 failures.
- `AssetPackageTests`: 39 checks, 0 failures (canonical serialization is
  reproducible; every diagnostic path returns its named error).
- `ConversionCacheTests`: 38 checks, 0 failures (hashing, key round trip,
  malformed-key diagnostics, every staleness reason, deterministic paths).
- `GltfRoundTripTests`: 28 checks, 0 failures (write→read agreement on
  positions/normals/UVs/indices/material/transform, byte-identical repeated
  writes, non-GLB and truncated-GLB diagnostics).
- `AssetConverterTests`: 28 checks, 0 failures (cache reuse, stale rebuild on
  changed source/schema, content hash equals the published bytes, identical
  conversions are byte-identical, empty-mesh diagnostic).
- `AssetPipelineTests`: 25 checks, 0 failures (converted hashes become package
  entries, canonical round trip preserves hashes and dependencies, identical
  builds are byte-identical, a dangling dependency fails to load).
- `SoundAssetTests`: 32 checks, 0 failures (defaults, id/field/loop/music
  validation, canonical serialization).
- `InspectorExportTests`: 107 checks passed (3 new: the manifest records a
  `contentHash`, an identical export into a second mod succeeds, and the two
  hashes are equal).
- The game still builds: MSBuild `Release_dev|x64` (NTSC) rebuilt clean, with
  `Game/C/soundasset.c` regenerated into the project.

## Limits and next steps

- The hash is recorded for new entries; the re-export path preserves an existing
  entry's hash rather than recomputing it, which is enough for reproducibility
  but does not yet detect a changed source region on re-export.
- The pipeline takes an already-extracted mesh record; the game-side extraction
  (for example the car serializer in `Game/C/cars.c`) and the loader that
  materializes packages into the game are not wired to it yet.
- E02 still needs: the Chicago slice instance/model/material inventory
  reconciliation, and a fixture that drives the pipeline from a real source
  geometry (the car model).

## Files

- `src_rebuild/utils/HdTextureOverrides.cpp`: `ComputeContentHash`,
  `BuildManifestEntry`, `ExportTextureCore`.
- `src_rebuild/Game/C/assetpackage.{h,c}`: package manifest load/validate/
  serialize.
- `src_rebuild/Game/C/conversioncache.{h,c}`: cache key, staleness, paths.
- `src_rebuild/utils/GltfWriter.{h,cpp}`: bounded GLB writer.
- `src_rebuild/utils/GltfLoader.{h,cpp}`: node transform retention.
- `src_rebuild/utils/AssetConverter.{h,cpp}`: transactional cached mesh
  conversion.
- `src_rebuild/Game/C/assetpackage.{h,c}`: package construction API.
- `src_rebuild/Game/C/soundasset.{h,c}`: sound metadata record.
- `src_rebuild/tests/InspectorExportTests.cpp`: content-hash reproducibility
  checks.
- `src_rebuild/tests/AssetPackageTests.cpp`,
  `src_rebuild/tests/ConversionCacheTests.cpp`,
  `src_rebuild/tests/GltfRoundTripTests.cpp`,
  `src_rebuild/tests/AssetConverterTests.cpp`,
  `src_rebuild/tests/AssetPipelineTests.cpp`,
  `src_rebuild/tests/SoundAssetTests.cpp` and
  `scripts/run_inspector_tests.ps1`: the new suites.
