---
type: Roadmap
title: Texture flicker diagnosis and sampling stability
status: implemented
completed: 2026-09-15
execution_order: 2
tags: [roadmap, textures, performance]
---

# Texture flicker diagnosis and sampling stability

Completed on 2026-09-15 on the `codex/modular-mod-system` branch. Product
behaviour is documented in
[`knowledge/product/texture-flicker-diagnostics.md`](../../product/texture-flicker-diagnostics.md).

Delivered: the `GRASS01C` shimmer was classified as minification aliasing of a
large opaque override; fully opaque overrides now generate mipmaps with mipmap
minification, and mipmapped overrides use anisotropic filtering (capped at
`4x`) when `GL_EXT_texture_filter_anisotropic` is available. Transparent
overrides keep plain filtering so the 0.5 cutout keeps full alpha coverage;
alpha-coverage-preserving cutout mipmaps were deliberately not added because
they would thin or punch holes in foliage.

## Progress

Milestones 1-4 are covered by the 2026-09-15 audit record at
[`knowledge/changes/2026-09-15/mod-system-pr-readiness/`](../../changes/2026-09-15/mod-system-pr-readiness/index.md).
`GRASS01C` was measured as fully opaque, so the shimmer was classified as
minification aliasing rather than a binding/depth regression. Fully opaque
overrides generate mipmaps with mipmap minification; anisotropic filtering up
to `4x` is applied when the driver supports it (verified at `16x` available on
the test GPU, logged at startup). Transparent overrides keep plain filtering to
preserve alpha coverage exactly. A reproducible capture route exists through
`scripts/run_debug_start.ps1 -Capture`.

## Problem

GRASS01C was reported to shimmer. Inspected original and upscaled files were fully opaque, so its own alpha is not a sufficient explanation. The sample grew from 64x64 to 1024x1024 and the override path lacks mipmaps. Aliasing, Z-fighting, LOD changes and unstable bindings must be distinguished.

## Intended behaviour

Identify the actual failure before changing filtering or depth behavior, then make the smallest evidence-backed correction.

## Scope

Reproducible camera route, diagnostic counters/captures, texture binding and fallback tracking, LOD/subdivision transitions, and sampling improvements when supported by measurements.

## Non-goals

Do not assume alpha or mipmaps alone explain every flicker. Do not apply global polygon offset, disable the Z-buffer or increase draw distance as a workaround.

## Dependencies and risks

Order 02. Diagnostic capture may precede [01](../done/texture-alpha-semantics.md), but assess the final result after its correctness fix. Feed measured budgets into [11](draw-distance-streaming.md) and [12](graphics-quality-profiles.md).

## Suggested execution order

Overall order: **02** in the [planned catalog](index.md).
Execute the following milestones sequentially. A request for one milestone
is not authorization to implement all later milestones or dependent features.

1. Record a repeatable scene, camera motion, configuration and affected textures. Distinguish shimmering detail, alternating surfaces and whole-texture replacement.
2. Compare original versus override, stationary versus moving camera, nearest versus linear, and PGXP/depth settings one variable at a time.
3. Trace logical texture identity, CLUT, UV containment, selected override, fallback reason and LOD across affected frames.
4. If aliasing is confirmed, add mipmap generation and suitable minification filtering; evaluate anisotropy only when supported. Preserve alpha coverage at smaller mip levels.
5. If binding/depth/LOD is responsible, repair that mechanism separately. Re-test reloading and filtering changes for GPU resource/state consistency.

## Acceptance criteria

The diagnosis includes reproducing evidence and a specific root cause or clearly bounded unresolved hypothesis. The identified regression is fixed without hiding it through unrelated settings. Filtering changes have documented visual and memory/performance results.

## Validation plan

Compare matching captures and frame-time measurements; exercise distant roads/grass, foliage edges and moving camera. Check mip chains and alpha coverage if implemented. A confirmed binding regression is a PR blocker even if quality enhancements are deferred.

## Starting points

`src_rebuild/PsyCross/src/render/PsyX_render.cpp`, `src_rebuild/PsyCross/src/gpu/PsyX_GPU.cpp`, the affected local PNG metadata, and developer render statistics.
Paths are relative to the repository root. Inspect current source and local
diffs before editing; these proposals are not proof of current behavior.

## Handoff and completion

For each requested milestone, report changed files, checks actually run,
remaining limitations and the next unblocked milestone. Preserve C++11 and
platform guards; maintain game-specific behavior outside PsyCross and carry
submodule changes in the project patch. Do not include game assets in tests
or commits. Only after all acceptance criteria are met, publish
`knowledge/product/texture-flicker-diagnostics.md`, move this record to `done/`, update
status/date/catalog links, and record the delivered change in Unreleased.
