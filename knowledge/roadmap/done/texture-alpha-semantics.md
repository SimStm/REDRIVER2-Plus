---
type: Roadmap
title: Texture alpha and PSX blending correctness
status: implemented
completed: 2026-09-15
execution_order: 1
tags: [roadmap, textures, rendering]
---

# Texture alpha and PSX blending correctness

Completed on 2026-09-15 on the `codex/modular-mod-system` branch. Product
behaviour is documented in
[`knowledge/product/texture-alpha-semantics.md`](../../product/texture-alpha-semantics.md).

Delivered: the override cutout discard (`color.a < 0.5`), PSX `STP` export as
alpha `128` with `0` for the transparent colour, and documented blend-mode
semantics for `BM_AVERAGE` versus the additive/subtractive modes. Verified in
the Chicago debug-start scene (`GRASS01C` alpha `128` opaque, alpha `100`
discarded) and by the export tests. The optional sub-0.5 soft-alpha enhancement
was deliberately left out as a non-goal.

- `29ba80d0` — override alpha cutout limited to active region overrides.
- `4aff8e56` — PSX `STP` exported as half alpha, intro skip, STP regression
  test.
- `9e72044a` — product document, rule and completed roadmap record.

## Problem

The RGBA override shader samples alpha but does not discard transparent fragments. Opaque PSX primitives disable blending, so transparent texels can render black and occlude geometry. Exported PNG alpha also does not encode all original PSX semitransparency semantics.

## Intended behaviour

Support reliable cutout transparency while preserving original PSX blending behavior and the ability to disable overrides immediately.

## Scope

Define opaque, cutout and semitransparent behavior, defaults and any versioned manifest metadata needed. Preserve the distinction between PNG coverage, PSX transparent colour and the original STP/blend mode.

## Non-goals

Do not enable conventional alpha blending globally, change original game assets, or redesign lighting. Smooth-alpha enhancement is optional and must not block the basic cutout fix.

## Dependencies and risks

First rendering implementation after the [PR audit](mod-system-pr-readiness.md). Coordinate with [02](texture-flicker-diagnostics.md) and carry PsyCross changes in the maintained patch.

## Suggested execution order

Overall order: **01** in the [planned catalog](index.md).
Execute the following milestones sequentially. A request for one milestone
is not authorization to implement all later milestones or dependent features.

1. Capture opaque, cutout and semitransparent examples with and without overrides; inspect original CLUT/STP, primitive blend mode and PNG alpha.
2. Specify the compatibility default and alpha threshold. Decide how partially transparent upscaled edges behave without losing original semitransparency.
3. Implement minimal cutout discard and correct depth/blend handling for overrides. Keep original shader behavior unchanged.
4. Add optional explicit alpha metadata only if needed; preserve backward compatibility for manifests without it.
5. Test straight versus premultiplied alpha, colour fringes and renderer state transitions; document remaining soft-alpha limitations.

## Acceptance criteria

Transparent pixels no longer render black or incorrectly occlude the background. Opaque grass and car paint remain opaque. Original semitransparent effects retain their intended blending. Toggling overrides does not leak GPU state.

## Validation plan

Use synthetic alpha ramps plus locally owned foliage/fence/glass examples, PGXP Z-buffer on/off, bilinear on/off and overlapping geometry. Include runtime visual verification; encode/decode tests alone are insufficient.

## Starting points

`src_rebuild/PsyCross/src/render/PsyX_render.cpp`, `src_rebuild/PsyCross/src/gpu/PsyX_GPU.cpp`, `src_rebuild/utils/HdTextureOverrides.cpp`.
Paths are relative to the repository root. Inspect current source and local
diffs before editing; these proposals are not proof of current behavior.

## Handoff and completion

For each requested milestone, report changed files, checks actually run,
remaining limitations and the next unblocked milestone. Preserve C++11 and
platform guards; maintain game-specific behavior outside PsyCross and carry
submodule changes in the project patch. Do not include game assets in tests
or commits. Only after all acceptance criteria are met, publish
`knowledge/product/texture-alpha-semantics.md`, move this record to `done/`, update
status/date/catalog links, and record the delivered change in Unreleased.
