---
type: Change
title: Scope and correct the modular mod-system foundation
description: Audit the mod/export branch, fix transparent-override rendering, and make texture export append safely before opening a foundation PR.
tags: [okf, mods, textures, rendering, exports, roadmap]
---

# Scope and correct the modular mod-system foundation

## Context

The `codex/modular-mod-system` branch combined mod loading, the 3D inspector,
texture export, a vendored ImGui overlay, and an in-tree PsyCross patch. Build
success alone did not establish runtime correctness. Two known defects
remained: transparent PNG overrides, and a reported `GRASS01C` shimmer. The
working tree also carried unrelated game data, dependency archives, and
generated output.

## Decision

Treat the branch as a bounded foundation and fix only the merge-blocking
correctness defect:

- Give the PsyCross 32-bit RGBA override shader an explicit alpha cutout. A
  new `overrideCutout` uniform discards fragments below 0.5 alpha only while a
  region override is active. Original PSX sampling and the high-resolution
  font/texture path keep their previous alpha behaviour, and disabling
  overrides clears the uniform, so no renderer state leaks.
- Make inspector texture export append a missing
  `(texture, texturePage, textureIndex)` registration into an existing
  `manifest.json` instead of only reporting the entry to add. The merge edits
  the JSON textually, preserving unknown fields and existing mappings, and
  publishes atomically with an intervening-edit check.
- Keep the PsyCross gitlink at upstream `e56e4cd` and regenerate
  `patches/psycross/developer-overlay.patch` from the submodule diff, including
  the vendored ImGui sources.

## Scope inventory

PR base: `master` at upstream `8.0` (`b2d88574`, released 2026-07-02).
Work branch: `codex/modular-mod-system` (`fb752a54`).

Include in a foundation PR:

- `src_rebuild/utils/HdTextureOverrides.*`, `InspectorExport.h`
- `src_rebuild/utils/DeveloperGraphicsPanel.*`, `DeveloperGraphicsSettings.*`
- `src_rebuild/utils/DebugOverlay.cpp`, `redriver2_psxpc.cpp` hooks
- `src_rebuild/Game/C/{cars.c,cars.h,draw.c,main.c,models.c,models.h,texture.c,tile.c}`
- `src_rebuild/tests/InspectorExportTests.cpp`, `src_rebuild/tests/README.md`
- `src_rebuild/premake5_psycross.lua` (scoped ImGui sources)
- `patches/psycross/`, `scripts/apply_psycross_patches.ps1`, `mods/` docs and
  examples, `knowledge/`, `CHANGELOG.md`, `AGENTS.md`

Exclude without deleting: `data/DRIVER2/*` English/texture edits,
`JPEG.zip`/`OPENAL.zip`/`PREMAKE.zip`/`SDL2.zip`, `src_rebuild/bin/**`,
`src_rebuild/build/**`, `src_rebuild/dependencies/**`, and local `mods`
instances under `bin/`. The local `.gitignore` data entries are unrelated to
this change.

## Verification performed

- Standalone export regression tests: 22/22 pass, including new append-merge,
  unknown-field preservation, malformed-manifest, and published-PNG checks.
- Windows `Release_dev` x64 solution build: 2 succeeded, 0 failed. PsyCross and
  REDRIVER2 recompiled; no shader-compilation error was logged.
- `patches/psycross/developer-overlay.patch` applies cleanly to a fresh
  worktree at the pinned `e56e4cd` commit, reverse-applies (idempotent), and
  reproduces the current submodule contents after line-ending normalisation.
- The parent repository gitlink is unchanged (`e56e4cd`, clean).
- Runtime smoke test: the game loads the `remaster-textures` mod, reports
  `Applied SRT0P ... as 1024 x 768 RGBA`, and renders override scenes without
  black cutout regions.

## Flicker classification

`GRASS01C` is fully opaque in both its exported original and its 1024x1024
upscaled override (minimum and maximum alpha are 255 across every pixel), so
alpha is not the cause. Inspected overrides larger than the source, sampled
without mipmaps under `GL_LINEAR`/`GL_NEAREST` minification, produce
minification aliasing. Override binding masks only ABR/dither bits and
requires full UV containment, so a binding failure would fall back to the
low-resolution VRAM texture rather than shimmer. The shimmer was therefore an
aliasing quality issue, not a binding or depth regression.

Fully opaque overrides now use `GR_CreateRGBATextureMipmapped`
(`glGenerateMipmap`) with mipmap minification. Overrides containing any
transparency keep plain filtering so mip averaging cannot bleed the
transparent colour into cutout edges. Runtime captures of the `GRASS01C`
scene with overrides on and off confirm the scene renders without black cutout
regions.

## Follow-up work (same day, later commits)

The debug and correctness work continued after the initial audit:

- Reproducible developer start snapshots (`DeveloperDebugStart.*`,
  `developer_debug_start.ini`) with a panel section, startup application,
  `-gametype`, `-level` and `-startdir` arguments, an intro skip, and
  `scripts/run_debug_start.ps1` for deterministic on/off capture.
- Exported originals now encode the PSX `STP` flag as half alpha, so
  semitransparent texels survive export and re-import.
- Fully opaque overrides use mipmaps (`GR_CreateRGBATextureMipmapped`);
  transparent overrides keep plain filtering to avoid alpha bleed.
- Export tests now cover STP alpha, wildcard entries, duplicate legacy pairs,
  malformed documents and repeated writes (31 checks).
- `git diff --check` is clean, and `Release_dev`/`x64` and `Debug`/`x64` build.
- Disposable-checkout verification (milestone 4): a fresh local clone at the
  branch head, `git submodule update --init --recursive` (pinned `e56e4cde`),
  `scripts/apply_psycross_patches.ps1`, `premake5 vs2022`, and
  `msbuild /p:Configuration=Release_dev /p:Platform=x64` all succeed and
  produce `bin/Release_dev/REDRIVER2_dev.exe`. The patched submodule tree is
  byte-identical to the local one (same `git status --porcelain` set), and
  re-running the applier reports the patch as already applied.
- That checkout exposed two reproducibility defects, both fixed: the patch was
  corrupted by CRLF on Windows (`core.autocrlf=true`) checkouts, and the
  applier hung under Windows PowerShell 5.1 because a failing `git apply` with
  stderr redirected to `$null` deadlocks while `$ErrorActionPreference` is
  `Stop`.
- Runtime alpha verification in the Chicago debug-start scene by overriding
  `GRASS01C` (page 1, index 5) with a flat red PNG: alpha `128` renders an
  opaque ground, alpha `100` is discarded and the ground becomes a hole. This
  confirms the 0.5 cutout. Blend modes come from the primitive's tpage, so an
  override inherits `BM_AVERAGE` (proportional alpha) or the
  additive/subtractive modes (alpha ignored). Product behaviour and limits are
  documented in
  [`knowledge/product/texture-alpha-semantics.md`](../../product/texture-alpha-semantics.md);
  roadmap order 01 is completed.

## Limitations and pending checks

- Linux compilation was not run: the WSL distribution has no SDL2/OpenAL/GL
  development headers and `sudo` requires a password, so dependencies cannot be
  installed. The Linux-specific paths (`_WIN32`-guarded file replacement,
  `<SDL.h>` for `SDL_GetTicks`, desktop-only guards) were reviewed but not
  compiled; record as unavailable coverage rather than passed.
- The F11 panel's own buttons were compile-verified only: automated input does
  not reach the SDL window, so panel interaction was validated manually. Device
  screenshot capture by window title works.
- Smooth alpha below the 0.5 cutout and `BM_ADD`/`BM_SUBTRACT` alpha fading are
  not implemented by design; they are documented as limitations rather than
  regressions.
- Milestone 7 (scoped diff review and PR description): the scoped diff contains
  only source, tests, patch and documentation (the four dependency ZIPs stay
  untracked and out). The draft PR body is in
  [`pr-description.md`](pr-description.md); no PR has been opened and no review
  has been requested yet.
