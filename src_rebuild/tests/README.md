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

Manual in-game verification:

1. Select a texture and export it twice to the same mod id. Both operations
   should succeed. Check that the PNG opens and has the registered dimensions.
2. Export a car twice and a selection report twice; check nonzero output.
3. Select separate buildings sharing a model: only the chosen position should
   highlight. Move the camera across LOD distances and inspect the object key.
4. Check the selected-object texture list against different clicked faces.
5. Change levels: the old selection must clear. Unlabelled paths and depth/
   alpha/scissor picking remain known diagnostic limitations.
