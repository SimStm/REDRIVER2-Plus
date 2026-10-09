---
type: Change
title: E00 dependency inventory, deterministic baseline instrumentation and isolated regional builds
created: 2026-10-08
tags: [okf, engine, baseline, deterministic, build]
---

# E00 dependency inventory, deterministic baseline instrumentation and isolated regional builds

## Outcome

First E00 implementation block on `feat/new-engine` (parent commit `9c353405`).
No engine behavior changed and PsyCross remains the active backend. The work
records the dependency/timing/replay contracts and adds the tooling that later
stages need to prove determinism and build in isolation.

## Inventory and tooling

- `knowledge/engine-migration/e00/dependency-inventory.md` records every
  required PsyCross/Psy-Q service with file-level evidence, its classification
  (project extraction, modern adapter, temporary compatibility consumer, or
  unused), the NTSC/PAL tick/replay/RNG contracts and draw-time mutation sites,
  and pinned SDK requirements (Diligent Engine v2.5.6, FMOD Engine 2.03.14).
- `scripts/engine_migration/e00_symbol_scan.py` generates the mechanical
  appendix (`e00/symbol-usage.md`, `e00/includes.md`): 3618 declared symbols,
  400 used by game sources, 3218 declared-but-unused.
- `knowledge/engine-migration/e00/baseline-scenarios.md` defines the repeatable
  routes that become the E01/E06 regression matrix.

## Deterministic baseline instrumentation

- `src_rebuild/utils/DeterministicBaseline.{h,cpp}` writes a field-wise
  canonical record once per simulation tick, enabled with
  `-statedump <file>` (`-statedumpevery N` optional). Only explicit gameplay
  fields are serialized and hashed with FNV-1a; `FrameCnt` is diagnostic
  context and is never hashed. `scripts/engine_migration/e00_compare_state.py`
  compares two dumps per tick.
- Hooked from `StepGame` (`Game/C/main.c`) and configured in
  `redriver2_psxpc.cpp`; the CLI options are ignored by `main.c` so the
  command-line popup does not appear. Disabled without the flag.
- Executed evidence (0 mismatches between independent runs of the same command):
  NTSC attract replay 714 shared ticks; PAL attract replay 595; NTSC-vs-PAL 597
  shared ticks identical; NTSC `-mission 1` 726. `-replay` rejects `.R`
  cutscene files explicitly (distinct format from `D2RP` tapes).

## Build isolation

- `premake5.lua` accepts `RED2_BUILD_DIR` (generated project location) and
  `RED2_BIN_DIR` (output folder). `build_e00`/`bin/Release_dev` (NTSC) and
  `build_e00_pal`/`bin/pal` (PAL) were generated and built with MSBuild
  `Release_dev|x64` from this branch's sources and gitlink, leaving the rework's
  `build/` outputs untouched.

## Validation actually run

`premake5 vs2022` (NTSC and PAL), MSBuild `Release_dev|x64` (both), Vulkan
smoke run, four deterministic replay/mission dump pairs plus the `.R` rejection
sample, `python scripts/engine_migration/e00_compare_state.py`, and
`git diff --check`. No GPU capture, audio, or performance budget was measured.
