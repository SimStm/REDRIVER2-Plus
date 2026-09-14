---
type: Rule
title: Regenerate build files after source additions
description: Premake-generated IDE projects must be regenerated before a newly added source file can be compiled.
tags: [okf, build, premake]
---

# Regenerate build files after source additions

**When** a C, C++, or header file is added to a target whose project files were
already generated, **then** rerun the appropriate Premake generator before
building or debugging from the IDE.

Premake expands source globs when it generates the project. A Visual Studio
project that predates a new `utils/*.cpp` file, for example, will compile a
caller but omit that file from the link, causing unresolved external symbols.

For the Windows VS2022 setup, run `premake5.exe vs2022` from `src_rebuild` with
the configured `SDL2_DIR`, `OPENAL_DIR`, and `JPEG_DIR` paths. Do not hand-edit
the generated `.sln` or `.vcxproj` files.
