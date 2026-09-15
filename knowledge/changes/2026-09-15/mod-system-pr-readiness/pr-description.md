---
type: Change
title: Foundation PR description for the modular mod system
description: Draft pull-request body for the modular texture mods, inspector and developer panel foundation.
tags: [okf, mods, textures, rendering, release]
---

# Foundation PR description

Base: `master` at upstream `8.0` (`b2d88574`). Head:
`codex/modular-mod-system`. This is the text to use when opening the PR; a PR
is not opened yet.

## Title

Add modular texture mods, a 3D inspector and a developer graphics panel

## Body

### Summary

Foundation for external RGBA texture mods on the desktop port: a JSON mod
loader with deterministic precedence and original VRAM/TIM fallback, a
diagnosing 3D inspector with PNG/OBJ/TXT export, an F11 developer panel,
reproducible debug-start tooling, and the PsyCross override rendering path
carried as a maintained patch instead of a private submodule fork.

### What changed

- **Mod system** (`src_rebuild/utils/HdTextureOverrides.*`): discoverable
  `mods/<id>/manifest.json` mods with `mods/enabled.json` ordering; identity by
  TEXINF name plus page/index; last enabled mod wins; original textures remain
  the fallback; legacy `mods/hd_textures/manifest.ini` still loads; exporting a
  texture into a mod appends a missing registration atomically.
- **Rendering** (PsyCross patch): 32-bit RGBA override sampling with a 0.5 alpha
  cutout limited to active overrides; PSX `STP` exported as half alpha (`128`);
  fully opaque overrides use mipmaps, transparent ones keep plain filtering.
- **Inspector** (`DeveloperGraphicsPanel`, `Cars_ExportInspectorModel`,
  `InspectorExport.h`): click selection with texture page, CLUT, UV, level
  texture identity and provenance; selected textures export as PNG and selected
  car bodies as OBJ; selection details export as TXT.
- **Developer panel**: vendored Dear ImGui 1.91.9b, F11, live graphics controls
  and game-debug telemetry, persisted to `developer_graphics.ini` (Windows and
  Linux only).
- **Debug tooling** (`DeveloperDebugStart.*`, `scripts/run_debug_start.ps1`):
  reproducible start at a saved mission, vehicle, position and heading, an
  intro skip, and self-driven screenshot capture.
- **Tests** (`src_rebuild/tests/InspectorExportTests.cpp`): standalone Windows
  checks for the PNG encoder, export paths and manifest merging.
- **Patch delivery**: `patches/psycross/developer-overlay.patch` plus
  `scripts/apply_psycross_patches.ps1`; the gitlink stays at the pinned upstream
  `e56e4cd`.

### Validation

- Windows `Release_dev|x64` and `Debug|x64` build.
- Fresh disposable clone: submodule init, patch apply (idempotent,
  byte-identical submodule tree), Premake generation, `Release_dev|x64` build.
- Export regression tests pass (31 checks).
- In-game: override on/off captures, alpha cutout (`128` kept, `100`
  discarded), STP blend semantics, and direct start with
  `-gametype`/`-level`/`-startdir`.

### Known limitations and unsupported platforms

- Linux, Android and Emscripten compilation were not validated; the WSL
  distribution lacks the development headers. The ImGui panel builds only on
  Windows and Linux.
- Inspector selection is a diagnostic draw-stream approximation, not a
  depth-tested editor selection; OBJ export is geometry-only and has no
  re-import.
- Overrides use a hard 0.5 alpha cutout, so there is no soft alpha below 0.5,
  and `BM_ADD`/`BM_SUBTRACT` effects ignore texel alpha.
- Panel buttons were verified manually because automated input does not reach
  the SDL window.
- Night variants and scripted Undercover missions are not reproducible through
  direct-start arguments.

### Out of scope

Model import, additional inspector categories, new graphics effects and
draw-distance changes; these remain in `knowledge/roadmap/planned/`.
