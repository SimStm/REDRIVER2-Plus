# Agent Guide

## Project knowledge (OKF)

Canonical project knowledge lives in [`knowledge/`](knowledge/index.md). Read `knowledge/index.md` and `knowledge/rules/` before changing lint, tokens, or booking naming. When you learn a repeating constraint, add a rule under `knowledge/rules/` (see `knowledge/rules/self-learning.md`).

Roadmap records live in [`knowledge/roadmap/`](knowledge/roadmap/index.md). Read
them when planning work, but do not treat them as implementation evidence:
`planned/` documents intent only, while `done/` is a completion record that must
be corroborated by its matching `knowledge/product/` document and the source.

Substantive architecture and design discussions live in
[`knowledge/discussions/`](knowledge/discussions/index.md). Before continuing
an existing topic, read its record; after a material discussion, update the
same subject with evidence, recommendations, open questions, and a dated
history entry. Follow [`discussion-records.md`](knowledge/rules/discussion-records.md).
Exploration belongs there until the user adopts a concrete roadmap scope;
a discussion does not authorize implementation or establish shipped behaviour.

## Persistent Engineering Context

- Read [`docs/ai/RECENT_CONTEXT.md`](docs/ai/RECENT_CONTEXT.md) at the start
  of any non-trivial task, session continuation, bug investigation, refactor,
  or multi-file feature, before editing code.
- Treat it as auxiliary context only. Confirm the real state from the current
  source, `git status`, `git diff`, build configuration, and tests.
- Update it before ending a long session or after a substantial milestone:
  multiple changed files, an architectural refactor, an API/ABI change, an
  ownership/lifetime or concurrency/synchronization decision, a change to
  error/exception guarantees, a meaningful performance change, an
  inconclusive investigation, a known blocker, or a significant build, test,
  sanitizer, benchmark, or static-analysis result.
- Record only verifiable facts: objective and acceptance criteria, decisions
  and reasons, relevant C++ invariants, changed files, completed work, pending
  work, risks or blockers, the next concrete step, and validation commands
  actually executed with their real results.
- Never claim validation that was not executed.
- Keep the file under 350 lines. Consolidate or remove obsolete entries
  instead of turning it into a chat log.
- Never record secrets, credentials, tokens, `.env` contents, or long logs.

## Project purpose

REDRIVER2 is a clean-room C/C++ reimplementation of the original PlayStation
game *Driver 2*. It is not an emulator. The game code was reconstructed from
symbols, disassembly, and reverse-engineering work, then adapted to run on
desktop and other platforms through PsyCross.

Preserve the reverse-engineered behaviour unless a task explicitly asks to
change gameplay. Small-looking changes to game logic, memory layouts, fixed
point math, timing, or rendering can affect compatibility.

## Repository map

- `src_rebuild/Game/`: reconstructed game source. `C/`, `engine/`, and
  `Frontend/` contain the main game systems; `ASM/` contains assembly-related
  material.
- `src_rebuild/redriver2_psxpc.cpp`: desktop/PsyCross entry point and platform
  integration for the game executable.
- `src_rebuild/utils/`: desktop utilities and developer-facing helpers.
- `src_rebuild/PsyCross/`: Git submodule that implements PlayStation/Psy-Q
  compatibility APIs, rendering, input, audio, CD, and GTE functionality.
- `src_rebuild/PsyCross/include/PsyX/`: public PsyCross interfaces. Keep
  reusable platform-layer APIs here rather than coupling PsyCross to this game.
- `src_rebuild/PsyCross/src/`: PsyCross implementation; key areas include
  `gpu/`, `gte/`, `render/`, `pad/`, and `psx/`.
- `src_rebuild/platform/`: platform-specific application and web files.
- `src_rebuild/premake5.lua`: workspace and game build definition.
- `src_rebuild/premake5_psycross.lua`: PsyCross static-library build
  definition.
- `data/`: runtime game data. It may contain local game assets; do not modify,
  delete, or redistribute it unless the task explicitly covers those assets.
- `PSXToolchain/`: tooling and materials for the PlayStation target.

## Technology and build model

- The codebase is primarily C and C++, compiled as C++ where Premake declares
  it. Linux explicitly uses C++11; avoid introducing newer language features
  unless all target generators are updated intentionally.
- Premake 5 generates project files. Generated output lives below
  `src_rebuild/build/`, `src_rebuild/project_*`, and `src_rebuild/bin/`; edit
  Premake scripts or source, not generated IDE files.
- Desktop rendering uses OpenGL through PsyCross. SDL2 provides the window and
  input layer; OpenAL Soft provides audio; libjpeg is used by the game.
- Supported build targets include Windows, Linux, Emscripten/web, and Android
  NDK. A change that is desktop-only must be guarded in Premake and source so
  it does not break the other targets.
- `GAME_REGION` must be either `NTSC_VERSION` or `PAL_VERSION`; it defaults to
  `NTSC_VERSION`. `RED2_DIR` and `WEBDEMO_DIR` configure data locations.
- `SDL2_DIR`, `OPENAL_DIR`, and `JPEG_DIR` can override the default dependency
  directories when generating a build.

## Building locally

- Windows: `windows_dev_prepare.ps1` downloads Premake 5 beta1, SDL2 2.30.2,
  OpenAL Soft 1.23.1, and JPEG 9d, prepares their paths, generates a VS2022
  solution, and opens it. It downloads archives to the repository root and
  expands dependencies, so do not run it in a dirty tree without checking the
  expected file changes.
- Linux: `linux_dev_prepare.sh` downloads Premake 5 beta1, runs the `gmake2`
  and VS Code generators, then enters the generated Linux project directory.
- Prefer regenerating a project after changing a Premake file. Build the
  generated project in the desired configuration (`Debug`, `Release`, or
  `Release_dev`) rather than editing the solution by hand.
- Check the existing toolchain before claiming a build was run. The setup
  scripts require network access and can create large dependency directories.

## PsyCross and submodules

- Initialise the PsyCross submodule before editing or building it:
  `git submodule update --init --recursive`.
- The PsyCross submodule is wired to the project fork
  `git@github.com:SimStm/PsyCross.git` (`origin`) with upstream
  `https://github.com/OpenDriver2/PsyCross.git` as `upstream`.
- Commit project-specific PsyCross changes directly in the fork, push them to
  `origin`, and record the commit by staging `src_rebuild/PsyCross` in the
  parent. The parent gitlink intentionally tracks the fork, not the upstream
  base commit.
- Do not reintroduce `patches/psycross/` or
  `scripts/apply_psycross_patches.ps1`; the fork is the single source of truth
  for PsyCross changes. See `knowledge/rules/psycross-fork.md`.
- PsyCross is a separate Git repository. Review its status independently with
  `git -C src_rebuild/PsyCross status`.
- Keep generic hooks in PsyCross and game-specific UI or behaviour in
  `src_rebuild`. Public PsyCross changes must retain C-compatible declarations
  where they are consumed by C or mixed C/C++ game code.

## Developer graphics panel

- The desktop developer graphics panel lives in
  `src_rebuild/utils/DeveloperGraphicsPanel.*`; F11 toggles it.
- Runtime settings live in `DeveloperGraphicsSettings.*` and are deliberately
  persisted to `developer_graphics.ini` (with `.tmp` and `.bak` handling), not
  the main `config.ini`.
- The panel uses vendored Dear ImGui 1.91.9b at
  `src_rebuild/PsyCross/third_party/imgui`. Premake excludes the broad ImGui
  glob and explicitly adds only the four core sources plus the SDL2 and
  OpenGL3 backends. Do not add ImGui examples, test programs, or unrelated
  backend implementations to the game build.
- Its generic PsyCross extension points are declared in
  `PsyX_public.h`: an SDL event consumer, a post-frame overlay renderer,
  desktop input-capture state, SDL window access, and render statistics.
  Event consumption and input capture are required so visible UI cannot also
  control the game.
- ImGui is enabled only for Windows and Linux. Preserve these platform guards
  unless Emscripten/Android support is deliberately implemented and tested.

## Change discipline

- Read the relevant files and current local diff before modifying code. This
  repository often has large local data changes that are unrelated to the
  requested work.
- Do not use destructive Git commands, reset generated/data directories, or
  overwrite local assets to make a build convenient.
- Keep edits narrow. Update the matching Premake definition when adding a
  source file that is not already covered by the existing glob rules.
- Run the smallest relevant validation available: `git diff --check` for text
  changes, then the appropriate generated build when the toolchain is present.
  For the asset catalog or the inspector export path, run
  `pwsh -NoProfile -File scripts/run_inspector_tests.ps1`, which builds and runs
  `AssetCatalogTests` and `InspectorExportTests` in fresh directories.
  `InspectorExportTests` is not idempotent, so never run it twice in the same
  working directory.
- All new documentation and developer-facing prose must be written in English.
- Capture durable conventions in `knowledge/rules/` at the time they are
  learned; use `knowledge/changes/` for the context of shipped changes.

## Changelog

- Maintain the root [`CHANGELOG.md`](CHANGELOG.md) in the
  [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) format. It is the
  release-oriented history for this fork; the historical `changelog.txt` is
  game-era source material and must not be rewritten.
- Before adding or changing a release entry, verify the current upstream
  version and baseline commit against the
  [OpenDriver2 releases](https://github.com/OpenDriver2/REDRIVER2/releases).
  The current baseline is upstream `8.0` at `b2d8857` (released 2026-07-02).
- Record every user-visible, developer-facing, build, tooling, or behavioural
  change under `## [Unreleased]` before opening a PR. Use only the applicable
  Keep a Changelog categories: `Added`, `Changed`, `Deprecated`, `Removed`,
  `Fixed`, and `Security`.
- Move entries from `Unreleased` to a dated, immutable release section only
  when that version is actually released. Do not fabricate release dates,
  versions, or historical changes.

## Roadmap records

- Create one `knowledge/roadmap/planned/<slug>.md` record for each newly
  planned feature, using the lifecycle and required fields documented in
  `knowledge/roadmap/index.md`.
- Roadmap records are planning metadata, not a source of truth for currently
  implemented behaviour. Do not report a `planned/` entry as a shipped feature
  during repository analysis.
- When implementing a planned feature, move its record to
  `knowledge/roadmap/done/`, update its status to implemented, and add or
  update the matching `knowledge/product/<slug>.md` document in the same
  change. A record must not enter `done/` without that product document.
