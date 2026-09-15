---
type: Product
title: Modular mod system foundation
description: What the merged mod-system foundation provides, how it was validated, and what remains.
tags: [product, mods, textures, rendering, release]
---

# Modular mod system foundation

The mod-system foundation was merged to `master` as PR
[#2](https://github.com/SimStm/REDRIVER2-Plus/pull/2) (merge commit
`5f1da8a2`, 2026-09-15). This document summarizes the shipped surface; the
audit trail is in
[`knowledge/changes/2026-09-15/mod-system-pr-readiness/`](../changes/2026-09-15/mod-system-pr-readiness/index.md).

## What ships

- **External RGBA texture mods** discovered under `mods/<id>/manifest.json`,
  ordered by `mods/enabled.json`, identified by TEXINF name plus
  `(texturePage, textureIndex)`, with the original TIM/VRAM texture as
  fallback. The legacy `mods/hd_textures/manifest.ini` still loads.
- **Override rendering** in PsyCross: a 0.5 alpha cutout for active override
  regions, PSX `STP` exported as half alpha, mipmaps for fully opaque
  overrides, and anisotropic filtering up to `4x` when the driver supports
  `GL_EXT_texture_filter_anisotropic`.
- **3D inspector** and **developer panel** (`F11`) with diagnostics, texture
  export as PNG, car-body export as OBJ, and selection-detail export as TXT.
  Selection is diagnostic (draw-stream approximation), and OBJ export is
  geometry-only with no re-import.
- **Reproducible debug start** (`-mission`/`-gametype`/`-level`/`-playercar`/
  `-startpos`/`-startdir`/`-players`/`-chase`, `-replay`), the panel
  "Reproduce this state" snapshot in `developer_debug_start.ini`, and
  `scripts/run_debug_start.ps1` with self-driven capture.
- **Patch-based PsyCross integration**: the gitlink stays at the pinned
  upstream `e56e4cd`; changes live in `patches/psycross/developer-overlay.patch`
  applied by `scripts/apply_psycross_patches.ps1`.

## Validation

- Windows `Release_dev|x64` and `Debug|x64` build.
- Export regression tests: 31 checks, 0 failures.
- Disposable checkout: submodule init, patch apply (byte-identical tree),
  Premake, `Release_dev|x64` build.
- Linux `release_dev_x64`/`debug_x64` build and start under WSLg.
- In-game: override on/off captures, alpha cutout, STP blend semantics,
  deterministic start.

## Remaining work

- Roadmap orders 04-12 (asset catalog, batch export, inspector coverage,
  previews, tool interoperability, model round trip, runtime settings, draw
  distance, quality profiles) and the playground/renderer track (13-14) remain
  planned.
- Untested edges: escaped texture names and a denied manifest write; Linux was
  exercised only up to startup and level load; Android/Emscripten are
  unvalidated.

## Related

- Roadmap record: [`knowledge/roadmap/done/mod-system-pr-readiness.md`](../roadmap/done/mod-system-pr-readiness.md)
- Build and run guide: [`BUILDING.md`](../../BUILDING.md)
