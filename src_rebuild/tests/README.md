# Inspector export regression tests

`InspectorExportTests.cpp` tests the production WIC encoder, original-texture
export path (including PSX transparent and STP semi-transparent coverage),
report writer, shared atomic text writer, and append-only manifest merging
(unknown fields, wildcards, duplicate legacy pairs, malformed documents), the batch exporter's identity deduplication, model-reference metadata, sanitized filename suffixes, per-resource failure reporting, a re-export that keeps the manifest's mapped file and merges references into one entry, the known-texture region adapter used to export hidden materials, and the cooperative batch job's stepping, cancellation, duplicate planning and retry
without game assets or an OpenGL context. The renderer is stubbed with
synthetic VRAM. It does not test mesh picking, the game car serializer, or
visual highlighting.

From an x64 Visual Studio Native Tools command prompt, in `src_rebuild`:

```bat
cl.exe /nologo /EHsc /std:c++14 /O2 /Gy /I dependencies\SDL2-2.30.2\include /I PsyCross\include /I PsyCross\include\psx tests\InspectorExportTests.cpp /Fo:build\InspectorExportTests.obj /Fe:build\InspectorExportTests.exe /link /OPT:REF ole32.lib uuid.lib windowscodecs.lib
```

Run `build/InspectorExportTests.exe` by absolute path with an empty temporary
directory as the working directory. Tests create `test.png`, `car.obj`, and a
`mods/regression` directory there; do not run them against a real mod directory.
Exit code zero means all checks passed. Artifacts are retained for inspection.

## Asset catalog tests

`AssetCatalogTests.cpp` tests the source-aware asset catalog (roadmap item 04)
in isolation: generation and context lifetime, model/slot revision handles,
texture manifest-triple deduplication, many-to-many model/texture relationships,
explicit provenance, palette relationships, the source-export LOD policy
(`AssetCatalog_CollectExportModels`), and graceful bounds handling. It
needs no game data or graphics context. Because the catalog is a `.c` file that
the project compiles as C++, pass `/TP` when building it by hand:

```bat
cl.exe /nologo /TP /EHsc /std:c++14 /O2 /Gy tests\AssetCatalogTests.cpp Game\C\assetcatalog.c /Fo:build\ /Fe:build\AssetCatalogTests.exe /link /OPT:REF
build\AssetCatalogTests.exe
```

It prints a check/failure count and exits non-zero on failure. It creates no
files, so any working directory is safe.

## Asset package tests

`AssetPackageTests.cpp` tests the E02 native asset package manifest
(`Game/C/assetpackage.*`) in isolation: schema/id/entry parsing, dependency
resolution, canonical serialization reproducibility (two identical packages and
a serialized→reloaded round trip produce identical bytes), and explicit
diagnostics for syntax errors, unsupported schema versions, missing fields,
duplicate ids, unknown dependencies and trailing data. It is pure data and needs
no game assets, PsyCross or a GPU context. Because the manifest is a `.c` file
the project compiles as C++, pass `/TP` when building it by hand:

```bat
cl.exe /nologo /TP /EHsc /std:c++14 /O2 /Gy tests\AssetPackageTests.cpp Game\C\assetpackage.c /Fo:build\ /Fe:build\AssetPackageTests.exe /link /OPT:REF
build\AssetPackageTests.exe
```

## Conversion cache tests

`ConversionCacheTests.cpp` tests the E02 conversion cache contract
(`Game/C/conversioncache.*`) in isolation: FNV-1a hashing, canonical key
formatting and parsing (with malformed-key diagnostics), the staleness reasons
(schema, source, converter, options, active mods) and deterministic cache paths.
It is pure data and needs no game assets, PsyCross or a GPU context.

```bat
cl.exe /nologo /TP /EHsc /std:c++14 /O2 /Gy tests\ConversionCacheTests.cpp Game\C\conversioncache.c /Fo:build\ /Fe:build\ConversionCacheTests.exe /link /OPT:REF
build\ConversionCacheTests.exe
```

## glTF/GLB mesh record tests

`GltfRoundTripTests.cpp` tests the E02 native mesh record: the project-owned
`Gltf_WriteFile` writer (`utils/GltfWriter.*`) and `Gltf_LoadFile` importer
(`utils/GltfLoader.*`) must agree on positions, normals, UVs, indices, material
factors and the node transform, and identical input must produce byte-identical
GLB. It also checks that a non-GLB and a truncated GLB are rejected with a
diagnostic. The importer's WIC image path is not exercised, so no game assets or
GPU context are needed. The importer links Windows Imaging Component:

```bat
cl.exe /nologo /TP /EHsc /std:c++14 /O2 /Gy tests\GltfRoundTripTests.cpp utils\GltfLoader.cpp utils\GltfWriter.cpp /Fo:build\ /Fe:build\GltfRoundTripTests.exe /link /OPT:REF windowscodecs.lib ole32.lib
```

## Asset converter tests

`AssetConverterTests.cpp` tests the E02 converter (`utils/AssetConverter.*`) that
turns a source mesh into a cached native GLB: it keys output by source
hash/converter/schema/options/mods, reuses a fresh cached GLB, rebuilds a stale
one at a new path, verifies the written GLB by reading it back, returns the
content hash of the published bytes, and rejects an empty mesh with a
diagnostic. It writes into a local `cache*` directory and needs no game assets
or GPU context. The importer links Windows Imaging Component:

```bat
cl.exe /nologo /TP /EHsc /std:c++14 /O2 /Gy tests\AssetConverterTests.cpp utils\AssetConverter.cpp utils\GltfLoader.cpp utils\GltfWriter.cpp Game\C\conversioncache.c /Fo:build\ /Fe:build\AssetConverterTests.exe /link /OPT:REF windowscodecs.lib ole32.lib
```

## Car mesh serializer tests

`CarMeshSerializerTests.cpp` tests the E02-B car mesh serializer
(`utils/CarMeshSerializer.*`) that adapts raw car geometry and an active-mods
list to the conversion pipeline: identical inputs reuse one cached GLB, changed
positions rebuild it, the cache key and path are sensitive to the mods list, the
documented canonical key derivation is pinned, and invalid arguments or a missing
cache directory report named diagnostics while leaving a zeroed result. A valid
conversion also succeeds when the caller supplies no diagnostic buffer. It writes
into a local `cache` directory and needs no game assets or GPU context. The
importer links Windows Imaging Component:

```bat
cl.exe /nologo /TP /EHsc /std:c++14 /O2 /Gy tests\CarMeshSerializerTests.cpp utils\CarMeshSerializer.cpp utils\AssetConverter.cpp utils\GltfWriter.cpp utils\GltfLoader.cpp Game\C\conversioncache.c /Fo:build\ /Fe:build\CarMeshSerializerTests.exe /link /OPT:REF windowscodecs.lib ole32.lib
```

It asserts a first conversion, so it is not idempotent: run it in a fresh working
directory, the way `scripts/run_inspector_tests.ps1` gives each suite its own.
Re-running it in the same directory would find the earlier artifacts and is not
supported.

## Source MODEL decoder tests

`SourceModelDecoderTests.cpp` tests the E03 block 1 source-MODEL decoder
(`utils/SourceModelDecoder.*`), a pure, asset-free module with no game/PSX
headers and no GPU dependency. Synthetic byte-exact MODEL spans pin the narrow
textured world allowlist (`PL_POLYFT4` ptype 11 plus `POLYFT3`/`POLYFT4`/
`POLYGT3`/`POLYGT4` ptypes 20-23), the world quad emission order
`(v0,v1,v3,v2)` and UV order `(uv0,uv1,uv3,uv2)`, the triangle order, the
`POLYGT4` normal/UV offsets (the documented non-subdiv suspicion), per-submesh
color presence for mixed PL/FT models, material grouping by
`(primitiveClass,textureSet,textureId)`, unsupported-type rejection with zero
output, span/index/normal validation, the `yang*64` rotation convention gate
and determinism. It creates no files:

```bat
cl.exe /nologo /TP /EHsc /std:c++14 /O2 /Gy tests\SourceModelDecoderTests.cpp utils\SourceModelDecoder.cpp /Fo:build\ /Fe:build\SourceModelDecoderTests.exe /link /OPT:REF
build\SourceModelDecoderTests.exe
```

## Model polygon-type probe tests

`ModelPtypeProbeTests.cpp` tests the E03 block 2a polygon-type probe
(`Game/C/modelptypeprobe.*`), a pure, asset-free module with no game/PSX headers
and no GPU dependency. Synthetic byte spans pin the declared-type walk with the
real `PolySizes` stride table, the quad/triangle and textured 4..7 bank counts,
the draw-path classification inputs (building-drawn vs catalog-material), the
zero-stride finding (recorded and never advanced past), out-of-span records and
polygon blocks, argument/empty-span handling and multi-model accumulation. It
creates no files:

```bat
cl.exe /nologo /TP /EHsc /std:c++14 /O2 /Gy tests\ModelPtypeProbeTests.cpp Game\C\modelptypeprobe.c /Fo:build\ /Fe:build\ModelPtypeProbeTests.exe /link /OPT:REF
build\ModelPtypeProbeTests.exe
```

`AssetCatalogTests.cpp` additionally covers the model byte-span lifecycle
(`AssetCatalog_RegisterModelWithSpan` / `AssetCatalog_GetModelSpan`: register,
re-register revision bump, invalidation, stale handle) and that a placement's
`sourceObjectIndex` is provenance that does not participate in the duplicate
identity key.

## CPU texture decoder tests

`TextureDecodeTests.cpp` tests the E03 block 2.4 CPU texture decoder
(`utils/TextureDecode.*`), a pure, asset-free module with no game/PSX headers,
no GPU and no FMOD. Synthetic byte-exact fixtures pin the 4-bit nibble order
(low nibble leftmost), the 8-bit byte order (low byte leftmost), the BGR555
expansion `E5TO8(c) = ((c*527)+23)>>6`, the alpha rules (`0000h` transparent,
`8000h` black STP -> 128, opaque otherwise), an explicit tight/padded/VRAM row
stride, the palette table (one RGBA per CLUT entry, multiple variants), bounded
validation with named errors and zero partial output, and determinism. The
texture window is a sample-time concern and is not applied. An optional
read-only probe decodes one local 8bpp TIM when `REDRIVER2_REPO_ROOT` is set and
otherwise reports SKIP; no asset is committed. It creates no files:

```bat
cl.exe /nologo /TP /EHsc /std:c++14 /O2 /Gy tests\TextureDecodeTests.cpp utils\TextureDecode.cpp /Fo:build\ /Fe:build\TextureDecodeTests.exe /link /OPT:REF
build\TextureDecodeTests.exe
```

## Asset pipeline tests

`AssetPipelineTests.cpp` tests the E02 conversion → package pipeline: source
meshes are converted to cached native GLBs and their published content hashes
become package manifest entries (`AssetPackage_SetId`/`AssetPackage_AddEntry`),
one entry depending on another. The assembled package must round-trip through
canonical serialization and the parser with its hashes and dependencies intact,
two identical builds must produce identical bytes, and a package whose
dependency was never added must fail to load. It writes into a local `cache`
directory and needs no game assets or GPU context. The importer links Windows
Imaging Component:

```bat
cl.exe /nologo /TP /EHsc /std:c++14 /O2 /Gy tests\AssetPipelineTests.cpp utils\AssetConverter.cpp utils\GltfLoader.cpp utils\GltfWriter.cpp Game\C\conversioncache.c Game\C\assetpackage.c /Fo:build\ /Fe:build\AssetPipelineTests.exe /link /OPT:REF windowscodecs.lib ole32.lib
```

## Sound asset tests

`SoundAssetTests.cpp` tests the E02 sound asset record (`Game/C/soundasset.*`):
explicit defaults, id handling, field validation (sample rate, channels, bit
depth, frames, pitch reference), loop range handling, music bank/program/sequence
completeness and canonical serialization. It is pure data and needs no game
assets, PsyCross or a GPU context. Because the record is a `.c` file the project
compiles as C++, pass `/TP` when building it by hand:

```bat
cl.exe /nologo /TP /EHsc /std:c++14 /O2 /Gy tests\SoundAssetTests.cpp Game\C\soundasset.c /Fo:build\ /Fe:build\SoundAssetTests.exe /link /OPT:REF
build\SoundAssetTests.exe
```

## Audio contract tests

`AudioContractTests.cpp` tests the E04 audio contract
(`Game/C/audiocontract.*`): canonical event id packing/unpacking (kind, primary,
secondary) with reserved-id handling, play flag independence, play request
validation (event id, kind mismatch, flags, volume, pitch, spatial distances),
voice handle lifecycle in the fixed pool with per-slot generations (stale
handles, full table, 16-bit wraparound) and inclusive loop-point validation. It
is pure data and needs no FMOD SDK, game assets, PsyCross or a GPU context.
Because the contract is a `.c` file the project compiles as C++, pass `/TP` when
building it by hand:

```bat
cl.exe /nologo /TP /EHsc /std:c++14 /O2 /Gy tests\AudioContractTests.cpp Game\C\audiocontract.c /Fo:build\ /Fe:build\AudioContractTests.exe /link /OPT:REF
build\AudioContractTests.exe
```

## Presentation timing and snapshots

`PresentationTimingTests.cpp` compiles the production `ProjectClock.cpp` and
`VisualSnapshot.cpp` directly, substituting only the SDL counter and stateful
camera controller. A counter frequency indivisible by 30/25 Hz verifies exact
NTSC/PAL cadence at 25, 30, 60, 120, 144 and 1000 presentation FPS and irregular
frame intervals. It checks car/camera temporal alignment, unchanged car/player
state, tick-camera restoration, paused poses, replay-editor servicing, cuts,
teleports, slot/session resets, split views, rotation size/wrap/aspect and bounded
stall phase. It requires Windows Visual Studio C++ tools and the existing SDL2
headers, with no game assets, running game or GPU. From the repository root:

```powershell
pwsh -NoProfile -File scripts/run_presentation_timing_tests.ps1
```

The suite also checks rational FPS pacing at 25/30/60/120/144/360, coarse timer
wakeups, unchanged simulation phase after a live FPS edit and startup-only region
activation. `FrameSettingsTests.cpp` links production graphics settings/file
persistence, verifying defaults, reload, atomic backups, invalid ranges, unknown
keys/comments, unchanged `config.ini` and unsaved provisional resolution.

Each invocation builds both regions into fresh ignored directories and reports
compile/test failures explicitly. The stub camera tests the scheduling contract;
actual camera/gameplay compatibility also needs controlled live replay comparisons.

`EffectTimingTests.cpp` also compiles the production debris pool and mission-title
fade updates. It checks lifetime/displacement through sparse pool slots, reused
slot history, pause and input unlock at 15/25/30/60/120/360 presentation FPS for
both regional tick rates. Wind/camera/hubcap dependencies are substitutes; rain
projection, GPU rendering and complete replay trajectories require live checks.
The suite also links production palette cycling and verifies that catch-up ticks
do not enqueue packets, repeated draws use independent packets, drawing preserves
palette data and loading invalidates readiness. Palette VRAM/PGXP are substitutes.
The state comparator has independent parser/inventory/mutation regression tests:

```powershell
python scripts/engine_migration/test_compare_state.py
```

Use `e00_compare_state.py --strict a.txt b.txt` for complete same-format replay
runs: it requires equal tick/entity/field inventories as well as equal values.
Without `--strict`, older formats can be compared on shared fields. An empty
shared tick set or a recorded presentation mutation always fails comparison.

## Native renderer smoke

`NativeRendererSmoke.cpp` exercises the E03 `native/NativeRenderer.*` service:
it creates a hidden Win32 window, creates the Diligent Vulkan device and swap
chain on it, clears and presents three frames, loads a converted GLB and checks
its vertex positions and indices round-trip through immutable Diligent buffers
and a staging readback, renders two overlapping triangles through a
depth-tested graphics pipeline into an offscreen target and checks the nearer
one wins the centre pixel, samples a base-colour texture on a textured quad and
checks a translated constant-buffer MVP moves it off the centre pixel, resizes
the swap chain, presents again and shuts down.
It needs the Diligent libraries from `scripts/fetch_diligent.ps1`, a
Vulkan-capable GPU/driver, and no game assets. Run it with:

```powershell
pwsh -NoProfile -File scripts/run_native_renderer_smoke.ps1
```

Manual in-game verification:

1. Select a texture and export it twice to the same mod id. Both operations
   should succeed. Check that the PNG opens and has the registered dimensions.
2. Export a car twice and a selection report twice; check nonzero output.
3. Select separate buildings sharing a model: only the chosen position should
   highlight. Move the camera across LOD distances and inspect the object key.
4. Check the selected-object texture list against different clicked faces.
5. Change levels: the old selection must clear. Unlabelled paths and depth/
   alpha/scissor picking remain known diagnostic limitations.
