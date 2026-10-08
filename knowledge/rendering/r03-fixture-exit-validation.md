---
type: Validation
title: R03 fixture process exit propagation
created: 2026-10-08
tags: [okf, rendering, validation, tooling]
---

# R03 fixture process exit propagation

The direct workflow exposed a false-success gate: `RunVulkanSelfTest` returns
1 on unavailable Vulkan or a failed GPU suite, but the command-line handler
discarded that result and game `main` returned0 for any requested fixture.

`DeveloperVkFixtureResult` now distinguishes `NotRequested`, `Success` and
`Failure`. The fixture handler preserves the existing self-test/fixture return
value; game startup returns1 for `Failure`,0 for `Success`, and continues normal
startup only for `NotRequested`. Unsupported-platform guards retain unrelated
startup. No renderer, shader, resource ownership, synchronization, game logic,
backend selection or scene-fallback policy changes.

## Executed verification

- Direct incremental MSBuild `Release_dev|x64`: exit0,4.25s; warnings retained.
- A missing driver manifest supplied through process-local `VK_DRIVER_FILES`
  and `VK_ICD_FILENAMES`: native fixture exit1 in0.23s, fresh log reports Vulkan
  unavailable. This is a real loader failure, not a mocked self-test result.
- Both environment variables restored, then a new native fixture: exit0 in
  24.83s, fresh16922-byte report ends with native PASS. Existing native pixel,
  clipping, winding, mixed-height/filter, failure/loading/native recovery and
  retirement checks pass; no legacy-world success is substituted.
- Existing PSX fixture: exit0 in5.30s, fresh1568-byte report ends with PSX PASS.
- Host compilation of `DeveloperVkFixture.cpp` under `PSX`, `__ANDROID__` and
  `__EMSCRIPTEN__`: all exit0. This verifies excluded branches, not full target
  builds or an OpenGL runtime test.
- Ordinary game arguments remain unhandled by the fixture. A fresh configured
  RenderDoc1.41 Vulkan frame500 capture and local SDK inspection (14.88s total)
  prove actual native startup:80 backdrop/1023 physical world/876 logical draws,
  44 cutouts,148320 geometry bytes and61 material images/49926792 bytes. All
  geometry/full-mip payload multisets and876 ordered bindings/ranges match the
  earlier direct capture. Actual1280x720 attachments contain768299 native ID>0/
  depth<1 pixels. Restored settings differ from earlier1920x1080, so frame hashes
  differ and no pixel-equivalence claim is made. Four HIGH replay warnings and
  the auxiliary14-versus10 count failure remain explicit. Six settings hashes
  match their backups and no owned game process remains.

The first test harness restored absent environment variables as empty strings;
the next process still had no driver and correctly failed. Removing absent
variables explicitly fixed the harness. The failed report is preserved. A fresh
separate shell also ran the native suite successfully. This is recovery across
process launches; in-session native failure/recovery remains covered by the
existing GPU suite, not by the loader negative control.

Private evidence stays in ignored
`src_rebuild/build/native-renderer-r03/fixture-exit-validation/`: `verify.ps1`,
`receipt.json`, fresh per-case logs/stdout/stderr, rejected restoration report
and platform-guard objects. `fixture-exit-msbuild.log` records the build.
The sibling `fixture-exit-native141_frame500.rdc`, inspection/summary and
`game-startup-resources.json` record ordinary startup/resource presence.
Log offsets isolate this run because the fixture log appends. Require both the
current process exit and a fresh completed PASS report; exit0 alone proves no
whole-city pixel/resource criterion.

## Scope still pending

This closes the test process-status defect, not R03. Actual auxiliary producer
attribution, negative-w city geometry, excluded edges/silhouettes, complete
region coverage and GPU cost remain pending in the
[west evidence](r03-west-road-validation.md). R04 remains unstarted.
OpenGL compatibility tracking: VK030; existing OpenGL choice is preserved.
