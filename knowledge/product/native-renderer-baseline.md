---
type: Product
title: Native renderer baseline diagnostics
description: R00 source coverage counters, read-only device query and measured legacy baseline.
tags: [product, rendering, diagnostics, vulkan]
---

# Native renderer baseline diagnostics

R00 is implemented and validated on Windows Release_dev x64. The scene renderer
itself remains the PSX/experimental path; R01-R12 are not delivered by this stage.
The [evidence and state recipes](../rendering/r00-measured-baseline.md),
[category ownership](../rendering/r00-source-inventory.md) and
[adopted native contracts](../rendering/r00-native-contracts.md) define the baseline.

## Coverage diagnostics

`src_rebuild/utils/DeveloperRenderCoverage.h/.cpp` owns render-thread producer
counts for 17 named categories, from roads to cutscene overlays. `BeginFrame`
resets a producer frame, `EndFrame` makes it inspectable and ignores later calls.
Game views aggregate; frontend/loading/fade/shutter paths declare their own modes.
The public header and struct-return interface are C-compatible.

F11 -> Graphics -> **Rendering migration coverage** displays requested attempts,
nonempty legacy batches, earlier experimental static submissions and native
counts. Native stays0 until the new scene contributes real geometry. Counts are
not faces, visible objects or GPU draws; nested reflections/decals overlap parent
phases. Direct persistent UI packets are counted explicitly. Do not sum the table
into totals or percentages, or treat absent/culled categories as migrated.

Optional `PSYX_COVERAGE_LOG=1` appends `render_coverage.log` in the working
directory on the first and every120 completed producer frames. File failures are
reported to stderr. No new persisted setting or gameplay mutation is introduced.
The log can provide fallback evidence even if the existing saved panel position
is outside the window. These diagnostics do not replace R12's GPU accounting.

## Read-only capabilities tool

Run `pwsh -NoProfile -File scripts/query_vulkan_baseline.ps1` from the repository
root. It locates MSVC, compiles `scripts/query_vulkan_baseline.cpp` against the
configured/repository Vulkan headers, and writes JSON/build/loader logs beneath
ignored `src_rebuild/build/native-renderer-r00/`. Windows-only tooling; no network,
Vulkan import library, logical device, surface or swapchain. The script validates
exit status and JSON. Device/format feature support is distinct from enabled game
features and tested pipelines. Probe memory usage is not game memory usage.

## Actual validation and limits

VS MCP Release_dev x64 build and fresh `-vkpsxtest` passed. The standalone
`scripts/run_render_coverage_tests.ps1` passed 12 invariant/ABI cases, including
a separately compiled C consumer. Original Chicago captures before/after the
diagnostics were RGB-identical and had identical773 draws/pass index counts.
Installed RenderDoc 1.46 captured/replayed city, wide-FOV, original replay and
playground frames with zero first-replay messages. Standalone fixture readback
saved 1280x720 after 120 frames, with 12 instances/draws. F11 table was visually
inspected with all17 rows and keyboard/gamepad capture enabled.

Hardware features, numerical budgets and measured CPU/process memory are in the
evidence note. GPU time/renderer allocation peaks, Linux/OpenGL runs, every mode,
native lighting correctness and whole-city rendering are not claimed. The native
scope starts in R01 with an isolated analytic world using the final game camera,
inside the existing single device/presentation owner and an original-city test.
All five INI files and original VS debugger settings were restored byte-exactly.
