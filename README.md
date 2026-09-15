# REDRIVER2-Plus

> A community-maintained fork of [OpenDriver2/REDRIVER2](https://github.com/OpenDriver2/REDRIVER2), focused on practical graphical improvements, quality-of-life work, and developer-facing diagnostics for the PC port of *Driver 2: Back on the Streets*.

[![Upstream: OpenDriver2/REDRIVER2](https://img.shields.io/badge/upstream-OpenDriver2%2FREDRIVER2-6f42c1)](https://github.com/OpenDriver2/REDRIVER2)
[![Fork: REDRIVER2-Plus](https://img.shields.io/badge/fork-REDRIVER2--Plus-2ea44f)](https://github.com/SimStm/REDRIVER2-Plus)

## Contents

- [What this project is](#what-this-project-is)
- [Legal game data](#legal-game-data)
- [Getting started on Windows](#getting-started-on-windows)
- [Running and debugging](#running-and-debugging)
- [Developer panel](#developer-panel)
- [Technology](#technology)
- [Project layout](#project-layout)
- [Contributing and AI agents](#contributing-and-ai-agents)
- [Credits](#credits)

## What this project is

REDRIVER2-Plus builds on the clean-room C/C++ reimplementation maintained by the OpenDriver2 team. It is **not an emulator**: the original PlayStation game code was reconstructed into portable code, while PsyCross provides the compatibility and rendering layer needed by the port.

This fork keeps that foundation and aims to make the game more pleasant to play, test, and extend. Current fork work includes:

- graphics controls and visual-quality experimentation;
- quality-of-life improvements for development and testing;
- an in-game Dear ImGui developer panel with live renderer controls and game telemetry;
- reproducible, project-owned PsyCross integration changes without maintaining a full PsyCross fork;
- durable project knowledge and change history for contributors and AI agents.

REDRIVER2-Plus is an independent, unofficial modification. It is not affiliated with or endorsed by the original game rights holders or the upstream OpenDriver2 team.

## Legal game data

This repository contains code and tooling only. It does **not** contain the commercial game data, nor does it grant a license to use it.

To run the game, you need your own legally obtained copies of both *Driver 2* CDs and must prepare the game data as described in the upstream [installation instructions](https://github.com/OpenDriver2/REDRIVER2/wiki/Installation-instructions). Keep the extracted data under the repository's `data/` directory, or configure a valid data location through `RED2_DIR`.

## Getting started on Windows

### Prerequisites

- Git with submodule support
- Visual Studio 2022 with the **Desktop development with C++** workload
- A legal *Driver 2* game-data installation (see above)
- PowerShell

### Clone and prepare

```powershell
git clone --recurse-submodules https://github.com/SimStm/REDRIVER2-Plus.git
Set-Location REDRIVER2-Plus
```

If you already cloned without submodules:

```powershell
git submodule update --init --recursive
```

Install the Windows development dependencies and generate the Visual Studio solution:

```powershell
.\windows_dev_prepare.ps1
```

The script downloads the pinned build dependencies and invokes Premake. Apply the project-owned PsyCross integration after the submodule is initialized:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\apply_psycross_patches.ps1
```

Open [src_rebuild/build/REDRIVER2.sln](src_rebuild/build/REDRIVER2.sln), select the `Release_dev` configuration and `x64` platform, then build the `REDRIVER2` project. The development executable is produced at:

```text
src_rebuild/bin/Release_dev/REDRIVER2_dev.exe
```

> The PsyCross patch script is safe to run again. It validates the expected upstream submodule revision and does nothing when the patch has already been applied.

### Linux and other platforms

The upstream project also targets Linux, WebAssembly, and Android. For the current platform-specific prerequisites and generation commands, follow the upstream [contributor guide](https://github.com/OpenDriver2/REDRIVER2/wiki/Contributing-to-project). The Plus-specific PsyCross patch must still be applied after initializing submodules.

## Running and debugging

### Run from Visual Studio

1. Open `src_rebuild/build/REDRIVER2.sln`.
2. Make `REDRIVER2` the startup project.
3. Choose `Release_dev | x64`.
4. The generated project sets the debugger **Working Directory** to the executable folder (`src_rebuild/bin/<configuration>`), where `config.ini`, `mods/`, and `developer_debug_start.ini` live. Set `RED2_DIR` only when your game data lives elsewhere.
5. Press `F5` to start under the debugger. If `developer_debug_start.ini` is enabled, the game starts directly at the saved session.

For a non-debug launch, run `src_rebuild/bin/Release_dev/REDRIVER2_dev.exe` from a context where its data path resolves, or provide `RED2_DIR` explicitly.

### Common setup checks

- `data/` is populated from a legal installation and is readable.
- Git submodules are initialized.
- `scripts/apply_psycross_patches.ps1` completed successfully.
- Visual Studio is building `Release_dev | x64`, not an incompatible platform/configuration pair.
- The working directory is `src_rebuild` when launching from Visual Studio unless `RED2_DIR` overrides it.

### Deterministic debug launch and capture

`Debug` and `Release_dev` are built with `DEBUG_OPTIONS`, so they accept direct-start arguments and skip both the frontend and the intro:

```text
REDRIVER2_dev.exe -nointro -mission <N> -gametype <G> -level <L> -playercar <C> -startpos <x> <z> -startdir <A> -players <P> [-chase <H>]
REDRIVER2_dev.exe -nointro -replay "DRIVER2/REPLAYS/ATTRACT.400"
```

- `-mission`, `-gametype`, `-level`, `-playercar`, `-startpos`, `-startdir`, `-players`, and `-chase` start a specific session. They exist only in `Debug`/`Release_dev`.
- `-startdir` is the player heading as a 12-bit PlayStation angle (`0..4095`). It is captured with the position, so the vehicle faces the same way.
- `-gametype` and `-level` are required for a faithful reproduction: `GAME_TAKEADRIVE` recomputes the mission number from `GameLevel`, so `-mission` alone does not select the map.
- `-replay <file.d2rp>` starts a recorded replay deterministically and also works in a plain `Release` build. The attract replays in `data/DRIVER2/REPLAYS/` are reproducible scenes; `-replay` is the best choice when comparing the same frame with a setting on and off. Scripted campaign ("Undercover") missions reload through the mission ladder and are not exactly restored by the direct-start arguments.

The **Game Debug** tab of the developer panel has a **Reproduce this state** section. It captures the current mission, level, vehicle, position, players, and chase; copies the matching command line; and saves it to `developer_debug_start.ini`. When that file has `enabled=1`, a debug build applies it at startup unless `-mission` or `-replay` was given.

[`scripts/run_debug_start.ps1`](scripts/run_debug_start.ps1) launches from explicit arguments, the saved snapshot, or a replay:

```powershell
# Start a mission directly.
pwsh -File scripts/run_debug_start.ps1 -Mission 1 -Car 0 -X 0 -Z 0

# Start from the panel snapshot.
pwsh -File scripts/run_debug_start.ps1 -FromSnapshot

# Capture the same session with texture overrides on and off.
pwsh -File scripts/run_debug_start.ps1 -Mission 1 -Car 0 -X 0 -Z 0 -Capture
```

Capture mode writes `src_rebuild/build/debug-start-captures/overrides-1.bmp` and `overrides-0.bmp`. It sets a temporary config so the game saves `SCREENSHOT.BMP` itself after the requested number of rendered seconds; no window focus or key injection is required. Relevant config keys:

- `[game] captureAfterSeconds=<seconds>` — save `SCREENSHOT.BMP` once after entering gameplay (`0` disables it).
- `[render] textureOverrides=0|1` — initial HD texture override state (default `1`).

## Developer panel

Press `F11` while the game is running to toggle the Dear ImGui developer panel. It has four tabs:

- **Graphics** — live renderer and visual controls.
- **Game Debug** — explained, live telemetry for primitive-table usage, streaming/spooling, civilian traffic, police, mission limits, vehicle state, and road-state data, plus the reproducible-start snapshot tools.
- **3D Debug** — mod diagnostics and primitive/texture inspection and export.
- **About** — fork attribution, repository links, and upstream credits.

While the panel is open, disable **Capture game input while panel is open** if you want to keep controlling the game during testing. Hover the information markers in the Game Debug tab for explanations of individual values.

## Technology

| Area | Upstream foundation | REDRIVER2-Plus additions |
| --- | --- | --- |
| Language | C and C++ (with C++11-compatible project code) | C++ developer-panel and diagnostics code |
| Build | Premake 5 and Visual Studio 2022 / platform toolchains | Reproducible post-submodule patch script |
| Platform layer | PsyCross, SDL2, OpenGL | Dear ImGui 1.91.9b with SDL2/OpenGL3 backends |
| Audio and assets | OpenAL Soft and libjpeg | No replacement for proprietary game data |
| Game origin | Clean-room reconstruction of the PlayStation release | Graphics, QoL, and developer-experience work |
| Documentation | Upstream wiki and source comments | `AGENTS.md`, OKF knowledge catalog, and Keep a Changelog history |

PsyCross remains an upstream submodule. This fork carries only the small integration delta in [`patches/psycross/`](patches/psycross/), applied by [`scripts/apply_psycross_patches.ps1`](scripts/apply_psycross_patches.ps1).

## Project layout

| Path | Purpose |
| --- | --- |
| [`src_rebuild/`](src_rebuild/) | Game source, Premake project definitions, generated build output, and PsyCross submodule |
| [`src_rebuild/utils/`](src_rebuild/utils/) | Cross-cutting utilities, including the developer graphics panel |
| [`data/`](data/) | Local game data prepared from legally owned media; do not commit proprietary files |
| [`patches/psycross/`](patches/psycross/) | Versioned fork-specific PsyCross delta |
| [`scripts/`](scripts/) | Repeatable development and patch-application scripts |
| [`knowledge/`](knowledge/index.md) | Project rules, product notes, and shipped-change records in Open Knowledge Format |
| [`CHANGELOG.md`](CHANGELOG.md) | User-facing change history following Keep a Changelog |

## Contributing and AI agents

Read [AGENTS.md](AGENTS.md) before making a change. It describes the architecture, build workflow, submodule patch flow, test expectations, and documentation rules used in this fork.

Canonical project knowledge lives in [`knowledge/`](knowledge/index.md). Read [`knowledge/index.md`](knowledge/index.md) and [`knowledge/rules/`](knowledge/rules/index.md) before changing linting, tokens, or booking naming. When a verified constraint will recur, add a rule under [`knowledge/rules/`](knowledge/rules/index.md), following [`self-learning.md`](knowledge/rules/self-learning.md).

All durable documentation is written in English. Update [`CHANGELOG.md`](CHANGELOG.md) under **Unreleased** for every user-visible, build, or developer-workflow change, and add a change record under `knowledge/changes/YYYY-MM-DD/<name>/` when work ships. Never add secrets, proprietary game assets, or unverified product behavior to project knowledge.

For upstream coding conventions and general contribution guidance, also consult the [OpenDriver2 contributor guide](https://github.com/OpenDriver2/REDRIVER2/wiki/Contributing-to-project).

## Credits

REDRIVER2-Plus modifications are implemented and maintained by [Lucas Sims](https://github.com/SimStm). The fork exists because of the original OpenDriver2/REDRIVER2 project and its contributors.

- **SoapyMan** — lead reverse engineer and programmer
- **Fireboyd78** — code refactoring and improvements
- **Krishty** and **someone972** — early format decoding
- **Gh0stBlade** — HLE emulator code that formed the basis of PsyCross
- **Ben Lincoln** — [This Dust Remembers What It Once Was](https://www.beneaththewaves.net/Software/This_Dust_Remembers_What_It_Once_Was.html)
- **Stohrendorf** — [Symdump](https://github.com/stohrendorf/symdump)

See the [upstream repository](https://github.com/OpenDriver2/REDRIVER2) and its [release history](https://github.com/OpenDriver2/REDRIVER2/releases) for the original project, its complete history, and additional contributors.
