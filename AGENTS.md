# Agent Guide

## Project knowledge (OKF)

Canonical project knowledge lives in [`knowledge/`](knowledge/index.md). Read `knowledge/index.md` and `knowledge/rules/` before changing lint, tokens, or booking naming. When you learn a repeating constraint, add a rule under `knowledge/rules/` (see `knowledge/rules/self-learning.md`).

Two status documents complement each other:

- [`knowledge/CURRENT_STATUS.md`](knowledge/CURRENT_STATUS.md) is the **source of
  truth for everything implemented in this fork** - a detailed, cumulative
  summary with a short comment on each surface (renderer, textures/mods,
  inspector, playground, tooling). **Update it every time you finish
  implementing something**, before ending the turn: add or amend the entry for
  the affected surface so a new session can see the whole current state without
  reading every change record. Keep it a summary, not a chat log.
- [`knowledge/RECENT_CONTEXT.md`](knowledge/RECENT_CONTEXT.md) is the
  **session-to-session handoff** - what the last interactions changed, the
  evidence gathered, techniques that worked, environment notes, and the next
  recommended action.

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

- Read [`knowledge/CURRENT_STATUS.md`](knowledge/CURRENT_STATUS.md) and
  [`knowledge/RECENT_CONTEXT.md`](knowledge/RECENT_CONTEXT.md) at the start of
  any non-trivial task, session continuation, bug investigation, refactor, or
  multi-file feature, before editing code. `CURRENT_STATUS.md` answers "what
  exists today"; `RECENT_CONTEXT.md` answers "what just happened and what is
  next".
- Treat them as auxiliary context only. Confirm the real state from the current
  source, `git status`, `git diff`, build configuration, and tests.
- Update `knowledge/CURRENT_STATUS.md` **whenever you finish implementing
  something** - a feature, a fix, a backend change, a tooling or documentation
  surface - before ending the turn. Update `knowledge/RECENT_CONTEXT.md` before
  ending a long session or after a substantial milestone: multiple changed
  files, an architectural refactor, an API/ABI change, an
  ownership/lifetime or concurrency/synchronization decision, a change to
  error/exception guarantees, a meaningful performance change, an
  inconclusive investigation, a known blocker, or a significant build, test,
  sanitizer, benchmark, or static-analysis result.
- Record only verifiable facts: objective and acceptance criteria, decisions
  and reasons, relevant C++ invariants, changed files, completed work, pending
  work, risks or blockers, the next concrete step, and validation commands
  actually executed with their real results.
- Never claim validation that was not executed.
- Keep each file under 350 lines. Consolidate or remove obsolete entries
  instead of turning them into a chat log.
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

## MCP tooling and interactive debugging

Check which MCP servers the session has available before documentation,
debugging, or run-the-game work, and use them when they are configured:

- **context7** - query current third-party API documentation instead of relying
  on memory when touching SDL2, Dear ImGui, Vulkan, OpenAL, or other libraries.
- **visual-studio-ide-mcp** - drive the solution already open in Visual Studio:
  select the configuration, build, launch or debug
  `src_rebuild/build/REDRIVER2.sln`, and inspect breakpoints, the call stack,
  locals, and the error list. Use `Release_dev_gl` for the OpenGL renderer and
  `Release_dev` for the default Vulkan renderer.
- **computer-control-mcp**, **windows-mcp**, or another desktop-control MCP -
  interact with the game window during a debug session: navigate the frontend
  menus, drive, press keys, take screenshots, and read on-screen text. These
  servers are the reliable way to drive the game; do not assume scripted input
  injection reaches a window without focus.

Avoid launching the game executable from the terminal. It is an interactive
foreground application, so a normal terminal invocation blocks until the window
closes and looks like a hang. Prefer the MCP launch paths above, and when a
terminal launch is unavoidable, make it fire-and-forget (for example
`Start-Process` without `-Wait`), wait with `Start-Sleep` or a timeout loop,
then read the game log (`<executable>.log` next to the executable) and
screenshots to learn the state instead of waiting on the process handle.

## PsyCross and submodules

- Initialise the PsyCross submodule before editing or building it:
  `git submodule update --init --recursive`.
- The PsyCross submodule **must come from the project fork**
  `git@github.com:SimStm/PsyCross.git` (`origin`, tracked branch
  `origin/master`), which carries this project's Vulkan backend,
  MoltenVK/portability work and
  other renderer changes that upstream does not have. Never check out
  `OpenDriver2/PsyCross` in `src_rebuild/PsyCross`, never point `.gitmodules` or
  the gitlink at it, and never build against an upstream copy. `upstream`
  (`https://github.com/OpenDriver2/PsyCross.git`) exists only as a read-only
  source for integrating upstream commits into the fork.
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

## Code quality and structure

Write new code and maintenance edits so the next reader can follow them without
reconstructing your reasoning. These rules govern new code and the parts of
existing files you touch; they are not a licence to rewrite reconstructed game
logic or working code that is outside the task.

- Prefer simple, readable, idiomatic C++ that is easy to modify, and match the
  style already used in the file you are editing.
- Do not produce spaghetti code: avoid long functions that mix
  responsibilities, deeply nested control flow, unnecessary global state, and
  implicit dependencies between distant parts of the code.
- Separate responsibilities: game logic, user input, rendering/presentation,
  domain rules, persistence, and external integration stay decoupled whenever
  it is viable. Call the owning system instead of reaching across layers.
- Prefer small, cohesive functions, explicit names, and types or structures
  that express domain intent instead of raw primitives.
- Avoid premature abstractions and unnecessary architectural patterns. Do not
  create classes, interfaces, layers, or generic systems without a concrete
  need in the project.
- Before changing existing code, understand the current flow, reuse the local
  conventions, and make the smallest coherent change that solves the problem.
- When a function or file is already too complex, do not add to that
  complexity. Extract cohesive parts only when the split clearly improves
  readability, testability, or maintenance.
- Do not duplicate logic. Extract a shared helper or component when repetition
  is real, and keep the execution flow visible instead of hiding it behind
  indirection.
- Reduce deeply nested `if`/`switch` blocks with early returns, helper
  functions, or explicit state modeling when the result is clearer.
- Avoid `bool` flags and ambiguous positional parameters when an `enum class`,
  an options structure, or a named type makes the intent explicit.
- Keep error handling explicit and close to the point that can fail. Never
  swallow an error silently.
- Preserve determinism and predictability in game logic: side effects must be
  visible, controlled, and limited to the system that owns them.
- Do not perform broad refactors outside the task scope. When a larger
  structural improvement is warranted, explain why and propose it as a separate
  change instead of folding it in.
- When you finish a change, review the result for mixed responsibilities,
  duplication, vague names, unnecessary coupling, shared mutable state, and
  execution paths that are hard to follow.

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
- Every finished implementation also updates
  [`knowledge/CURRENT_STATUS.md`](knowledge/CURRENT_STATUS.md) in the same
  change, so the cumulative status never lags behind the source.

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
