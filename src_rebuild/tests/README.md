# Inspector export regression tests

`InspectorExportTests.cpp` tests the production WIC encoder, original-texture
export path (including PSX transparent and STP semi-transparent coverage),
report writer, shared atomic text writer, and append-only manifest merging
(unknown fields, wildcards, duplicate legacy pairs, malformed documents)
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

Manual in-game verification:

1. Select a texture and export it twice to the same mod id. Both operations
   should succeed. Check that the PNG opens and has the registered dimensions.
2. Export a car twice and a selection report twice; check nonzero output.
3. Select separate buildings sharing a model: only the chosen position should
   highlight. Move the camera across LOD distances and inspect the object key.
4. Check the selected-object texture list against different clicked faces.
5. Change levels: the old selection must clear. Unlabelled paths and depth/
   alpha/scissor picking remain known diagnostic limitations.
