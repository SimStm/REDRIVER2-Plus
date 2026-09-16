# REDRIVER2-Plus

> A community-maintained fork of [OpenDriver2/REDRIVER2](https://github.com/OpenDriver2/REDRIVER2), focused on practical graphical improvements, quality-of-life work, and developer-facing diagnostics for the PC port of *Driver 2: Back on the Streets*.

[![Upstream: OpenDriver2/REDRIVER2](https://img.shields.io/badge/upstream-OpenDriver2%2FREDRIVER2-6f42c1)](https://github.com/OpenDriver2/REDRIVER2)
[![Fork: REDRIVER2-Plus](https://img.shields.io/badge/fork-REDRIVER2--Plus-2ea44f)](https://github.com/SimStm/REDRIVER2-Plus)

## Contents

- [What this project is](#what-this-project-is)
- [Legal game data](#legal-game-data)
- [Building and running](#building-and-running)
- [Command-line parameters](#command-line-parameters)
- [Configuration keys](#configuration-keys)
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
- external RGBA texture mods with deterministic precedence and original-asset fallback;
- reproducible, project-owned PsyCross integration changes without maintaining a full PsyCross fork;
- durable project knowledge and change history for contributors and AI agents.

REDRIVER2-Plus is an independent, unofficial modification. It is not affiliated with or endorsed by the original game rights holders or the upstream OpenDriver2 team.

## Legal game data

This repository contains code and tooling only. It does **not** contain the commercial game data, nor does it grant a license to use it.

To run the game, you need your own legally obtained copies of both *Driver 2* CDs and must prepare the game data as described in the upstream [installation instructions](https://github.com/OpenDriver2/REDRIVER2/wiki/Installation-instructions). Keep the extracted data under the repository's `data/` directory, or configure a valid data location through `RED2_DIR`.

## Building and running

REDRIVER2-Plus builds with Premake 5 on Windows (Visual Studio 2022) and on Linux, either natively or on Windows through WSL/WSLg.

- **Windows:** run `.\windows_dev_prepare.ps1`, then build the `Release_dev | x64` configuration in Visual Studio. The executable is produced at `src_rebuild/bin/Release_dev/REDRIVER2_dev.exe`.
- **Linux:** install the SDL2, OpenAL, OpenGL and libjpeg development packages, run `./linux_dev_prepare.sh`, then `make -j"$(nproc)" config=release_dev_x64`.

The game resolves a `DRIVER2/` game-data folder from its working directory, so keep the prepared data next to the executable or set `RED2_DIR` (see [Legal game data](#legal-game-data)).

**[BUILDING.md](BUILDING.md)** has the complete instructions: prerequisites and how to install them, per-platform build and run steps (including WSL), the deterministic debug-start arguments and capture workflow, the standalone export tests, screenshots, and troubleshooting.

## Command-line parameters

Every build accepts the base arguments. The **debug arguments** exist only in
`Debug` and `Release_dev`, because those configurations define
`DEBUG_OPTIONS`; plain `Release` ignores them (see
[Configurations](BUILDING.md#configurations)). Run the executable from a folder
that contains the `DRIVER2/` game data (or point `RED2_DIR` at it).

### Base arguments (all builds)

| Argument | Meaning |
| --- | --- |
| `-ini <file.ini>` | Load a specific configuration file instead of `config.ini`. |
| `-cdimage <file.iso\|file.bin>` | Read game data from a CD image instead of the `DRIVER2/` folder. |
| `-nointro` | Skip the intro movie and splash screens that play when launched with no arguments. |
| `-nofmv` | Disable all FMV playback. |
| `-replay <file.d2rp>` | Start the attract/user replay from a `.d2rp` file. Works in every build. |

### Debug arguments (`Debug` and `Release_dev`)

| Argument | Meaning |
| --- | --- |
| `-mission <n>` | Start the given mission number directly, skipping the frontend. |
| `-gametype <n>` | Override the game type for the started session (see `GAMETYPE` in `game/dr2types.h`; `GAME_TAKEADRIVE` is `1`). |
| `-level <n>` | Select the city/level for the started session (`0` Chicago, `1` Havana, `2` Vegas, `3` Rio). For `GAME_TAKEADRIVE` the level determines the derived mission number. |
| `-playercar <n>` | Force the player-1 car model. |
| `-player2car <n>` | Force the player-2 car model. |
| `-players <1\|2>` | Set the player count. |
| `-startpos <x> <z>` | Override the player start position in game units. |
| `-startdir <0..4095>` | Override the player start heading as a 12-bit PlayStation angle. |
| `-chase <n>` | Force a specific chase number instead of a random one. |
| `-playground` | Start the dedicated generated playground scene (roadmap item 13). Reuses a Chicago take-a-ride level for car/texture/sound resources and replaces world geometry, roads and collisions with a regenerated flat fixture. |
| `-exportxasubtitles` | Export the strings embedded in XA audio to an SBN subtitle file. |
| `-recordcutscene <file.ini>` | Start a scripted cutscene-recording session from an INI file. |
| `-chaseautotest <file.ini>` | Start a scripted chase auto-test from an INI file. |

Reproducible single-session launches combine these; the canonical examples are:

```text
REDRIVER2_dev -nointro -mission <N> -gametype <G> -level <L> -playercar <C> -startpos <x> <z> -startdir <A> -players <P> [-chase <H>]
REDRIVER2_dev -nointro -replay "DRIVER2/REPLAYS/ATTRACT.400"
REDRIVER2_dev -nointro -nofmv -playground
```

The **Game Debug** tab of the developer panel captures the current session into
that command line and can persist it to `developer_debug_start.ini` (applied
automatically at startup when `enabled=1`). See
[Deterministic debug start](BUILDING.md#deterministic-debug-start) for the full
workflow and the `scripts/run_debug_start.ps1` helper.

## Configuration keys

`config.ini` is read from the working directory (or from `-ini <file>`). Keys
outside `[game]`/`[render]` are commented in the shipped file. The keys that
matter most when testing are:

| Section | Key | Meaning |
| --- | --- | --- |
| `[fs]` | `dataFolder` | Game-data folder relative to the executable (default `DRIVER2`). |
| `[game]` | `drawDistance` | World draw distance (441..1800). |
| `[game]` | `fieldOfView` | Camera field of view (128..384, 256 default). |
| `[game]` | `freeCamera` | `1` enables the F7 free-fly camera. |
| `[game]` | `captureAfterSeconds` | Save `SCREENSHOT.BMP` once, this many seconds after gameplay begins (`0` disables). |
| `[game]` | `overrideContent` | Enable modded textures and car models (HD overrides). |
| `[game]` | `languageId` | `0` English, `1` Italian, `2` German, `3` French, `4` Spanish. |
| `[game]` | `unlockAll` | Debug builds only: unlock all missions and cheats. |
| `[render]` | `textureOverrides` | Initial HD-texture override state (`0`/`1`); lets a capture run with overrides disabled. |
| `[render]` | `vsync`, `fullscreen`, `windowWidth`/`windowHeight` | Presentation controls. |
| `[render]` | `pgxpTextureMapping`, `pgxpZbuffer`, `bilinearFiltering` | PSX-accurate or smoothed rendering toggles. |
| `[pad]` | `pad1device`, `pad2device` | Controller device index (`-1` for automatic). |
| `[cdfs]` | `image`, `mode` | CD image path and sector mode (do not change `mode`). |

In-game control bindings live under `[kbcontrols_game]`, `[kbcontrols_menu]`,
`[controls_game]` and `[controls_menu]`; the developer panel can capture and
persist its own settings separately in `developer_graphics.ini`.

## Developer panel

Press `F11` while the game is running to toggle the Dear ImGui developer panel. It has four tabs:

- **Graphics** — live renderer and visual controls.
- **Game Debug** — explained, live telemetry for primitive-table usage, streaming/spooling, civilian traffic, police, mission limits, vehicle state, and road-state data, plus the reproducible-start snapshot tools.
- **3D Debug** — mod diagnostics and primitive/texture inspection and export.
- **About** — fork attribution, repository links, and upstream credits.

While the panel is open, disable **Capture game input while panel is open** if you want to keep controlling the game during testing. Hover the information markers in the Game Debug tab for explanations of individual values.

`Debug` and `Release_dev` also accept direct-start arguments that skip the frontend and intro; see [Deterministic debug start](BUILDING.md#deterministic-debug-start).

## Technology

| Area | Upstream foundation | REDRIVER2-Plus additions |
| --- | --- | --- |
| Language | C and C++ (with C++11-compatible project code) | C++ developer-panel and diagnostics code |
| Build | Premake 5 and Visual Studio 2022 / platform toolchains | Reproducible post-submodule patch script |
| Platform layer | PsyCross, SDL2, OpenGL | Dear ImGui 1.91.9b with SDL2/OpenGL3 backends |
| Audio and assets | OpenAL Soft and libjpeg | No replacement for proprietary game data |
| Game origin | Clean-room reconstruction of the PlayStation release | Graphics, QoL, and developer-experience work |
| Documentation | Upstream wiki and source comments | `AGENTS.md`, `BUILDING.md`, OKF knowledge catalog, and Keep a Changelog history |

PsyCross remains an upstream submodule. This fork carries only the small integration delta in [`patches/psycross/`](patches/psycross/), applied by [`scripts/apply_psycross_patches.ps1`](scripts/apply_psycross_patches.ps1).

## Project layout

| Path | Purpose |
| --- | --- |
| [`src_rebuild/`](src_rebuild/) | Game source, Premake project definitions, generated build output, and PsyCross submodule |
| [`src_rebuild/utils/`](src_rebuild/utils/) | Cross-cutting utilities, including the developer graphics panel |
| [`data/`](data/) | Local game data prepared from legally owned media; do not commit proprietary files |
| [`patches/psycross/`](patches/psycross/) | Versioned fork-specific PsyCross delta |
| [`scripts/`](scripts/) | Repeatable development and patch-application scripts |
| [`docs/`](docs/) | Screenshots and images used by the documentation |
| [`knowledge/`](knowledge/index.md) | Project rules, product notes, and shipped-change records in Open Knowledge Format |
| [`BUILDING.md`](BUILDING.md) | Build, run and troubleshooting instructions |
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
