---
type: Plan
title: E07 - Remove PsyCross and validate the cutover
status: planned
created: 2026-10-08
tags: [engine, psycross, removal, build]
---

# E07 - Remove PsyCross and validate the cutover

## Objective and dependencies

After accepted E06, remove the PsyCross submodule and all project build/runtime
dependencies on it. The removal is explicitly adopted by the user, but is not
authorized as a premature shortcut before consumer migration.

## Implementation blocks

- Reconcile E00's checklist: math/GTE, ticks/input, streaming/files, audio, native
  scene/effects/UI/present, tools/tests, diagnostics and vendored dependencies.
- Move independently needed libraries such as ImGui into their own clearly
  versioned locations; do not move all PsyCross source wholesale to hide dependency.
  Small extracted compatibility math/data/API adapters may stay with provenance.
- Remove gitlink/`.gitmodules` entry, PsyCross includes/link targets, Premake
  references, setup/CI/scripts/docs assumptions and generated shader/tool coupling.
  Update source build definitions and generate fresh projects; do not edit stale
  generated solutions to make a local build appear successful.
- Finalize the supported-target/backend matrix. Preserve currently supported
  OpenGL/platform behavior until replacement or an explicit user scope decision;
  no automatic scene fallback. Cross-platform SDK support is not proof of a build.
- Keep the independent rework branch/commits available for historical reference.
  Do not remove local user data or ignored evidence while cleaning build metadata.

## Acceptance criteria

- [ ] A fresh clone can acquire declared SDK dependencies and build/run without
  initializing PsyCross; no source, include, link, runtime or tool dependency remains.
- [ ] No loaded library/file/resource comes from PsyCross during supported runs;
  scans and build/dependency reports agree with runtime observations.
- [ ] All required platform/backend configurations and integration suites pass
  from fresh isolated outputs; unsupported targets are explicitly documented.
- [ ] Deterministic mechanics/replays, native graphics/effects, FMOD Core, UI/input,
  mod/export paths and resource cleanup still pass after actual removal.
- [ ] Ownership/provenance of extracted code and third-party dependencies is
  documented, with no SDK/game asset redistribution introduced by the migration.
- [ ] Product/status/changelog and setup guides reflect the shipped state;
  the umbrella moves to done only after every required acceptance gate passes.

## Validation and limits

Use fresh-clone or equivalent clean isolated builds, symbol/include/link scans,
module/file traces and the E06 full integration matrix. Run a negative dependency
test with PsyCross unavailable. Compilation alone or renaming its directory does
not prove removal. Full cutover must pass without PopcornFX; deferred E08 cannot
justify retaining any required PsyCross consumer. No submodule removal is
performed by this planning task.
