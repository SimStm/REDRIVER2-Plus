---
type: ImplementationStage
title: R01 - Native renderer bootstrap and camera/depth
status: implemented
created: 2026-10-06
tags: [okf, rendering, stage, vulkan]
---

# R01 - Native renderer bootstrap and camera/depth

**Status:** implemented and accepted on the tested Windows desktop slice.
**Dependency:** R00 accepted. See [product](../../product/native-renderer-bootstrap.md)
and [executed evidence](../r01-validation.md); other-target runtime not claimed.

## Objective

Render genuine native 3D geometry with a conventional camera and reliable depth inside the game.

## Steps

1. Introduce the agreed renderer boundary, reusing/extracting Vulkan resource and presentation helpers with exactly one device/swapchain owner. Implement explicit frame/upload/retirement ownership.
2. Render an unlit indexed analytic scene using native world/view/projection, source normals/UV attributes and chosen depth conventions. Bind the final game camera snapshot without GTE vertex encoding.
3. Provide an isolated native world mode or an explicitly tested temporary composition boundary; do not assume old PSX depth can occlude a new projection.
4. Preserve native UI/debug overlay composition, resize, minimize/restore and screenshots. Report native versus legacy draw counts and effective mode.

## Deliverables

An opt-in native analytic scene, reusable resource handles, camera/depth contract and presentation diagnostics.

## Acceptance gate

- [x] Intersecting near/far geometry, winding, clipping, wide FOV and camera movement are correct and stable.
- [x] No dependence on PGXP settings for native geometry; no sky/UI depth contamination.
- [x] No duplicate presentation/device ownership or use-after-free during reload/resize.
- [x] The existing renderer remains selectable and platform guards compile.

## Planned validation

VS MCP Release_dev x64 build; RenderDoc vertex output, depth and render-target inspection; Vulkan validation on upload, resize and destruction; controlled clipping/occlusion screenshots and legacy self-test regression.

## Risks and fallback

Cross-projection composition is a temporary risk. Use explicit native loading/
failure diagnostics without old-world recovery. Historical acceptance above
predates the adopted no-fallback policy, validated by R03's new regression tests.

Opt-in mode; existing OpenGL/platform choice remains separate.

## Handoff

Record changed files, actual checks/results, unresolved limits and the next stage
in the [active plan](../implementation-plan.md), current status and recent context.
Every Vulkan change also updates the unscheduled OpenGL compatibility backlog.
Pass the [shared validation gates](../validation-and-completion.md); a planned
checklist is not evidence. Update the matching implemented product surface before
claiming delivery. This stage request does not authorize executing later stages.
