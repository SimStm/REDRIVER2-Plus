---
type: Rule
title: Build and run the Linux target (including via WSL)
description: Prerequisites, patch step, premake generation and run directory for the Linux target.
tags: [okf, build, linux, wsl, psycross]
---

# Build and run the Linux target (including via WSL)

**When** validating or building the Linux target, including from Windows,
**then** install the system dependencies, apply the PsyCross patch, generate
with Premake, and build in `src_rebuild/build`.

- Install dependencies once on Ubuntu/Debian:
  `build-essential libsdl2-dev libopenal-dev libgl1-mesa-dev libjpeg-dev pkg-config`.
  Linux links the **system** `jpeg`, `SDL2`, `openal` and `GL`; the bundled
  `jpeg` project is Windows-only, and `libjpeg-dev` provides `-ljpeg`.
- In WSL, the default user cannot `sudo` without a password, but
  `wsl -u root -- <command>` gives root directly. Clone into the Linux
  filesystem (for example `/root/...`), not `/mnt/g`, because building across
  the 9p mount is much slower.
- For a clone created from a Windows drive, run
  `git config --global --add safe.directory '*'` or git aborts with "detected
  dubious ownership".
- Apply the patch before generating: the PowerShell applier is Windows-only, so
  use `git -C src_rebuild/PsyCross apply "$PWD/patches/psycross/developer-overlay.patch"`.
  `linux_dev_prepare.sh` does the download, patch and generation steps and is
  idempotent.
- Generate with `src_rebuild/premake5 gmake2` (output in `src_rebuild/build`)
  and build with `make -j"$(nproc)" config=release_dev_x64` (or `debug_x64`).
  The binaries are `src_rebuild/bin/<Configuration>/REDRIVER2_dev` and
  `REDRIVER2_dbg`; `FontTool` and `libPsyCross.a` are also produced.
- To run under WSLg, link or copy the game data as a `DRIVER2/` folder into the
  executable directory, since the game resolves `DRIVER2\` from its working
  directory. WSLg provides `DISPLAY=:0` and reports a D3D12/Mesa core context;
  set `SDL_AUDIODRIVER=dummy` when audio should not initialize.

See [Keep PsyCross changes as patches](psycross-patches.md) for the patch
lifecycle and [Regenerate build files after source additions](generated-build-files.md)
for when to re-run Premake.
