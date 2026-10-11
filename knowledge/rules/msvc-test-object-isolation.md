---
type: Rule
title: Isolate MSVC object files in test runners
description: Keep compiler intermediates separate when scripts build multiple test targets.
tags: [okf, tooling, validation]
---

# Isolate MSVC object files in test runners

**When** a Windows test script compiles multiple executables or suites from source files
that may share basenames, **then** create an object-output directory for each suite/build
and pass it to `cl.exe` with `/Fo` before compiling. Keep each executable's working
directory separate as needed; do not rely on the caller's current directory to own `.obj`
files. This prevents one suite's object from being reused or overwritten by another.
