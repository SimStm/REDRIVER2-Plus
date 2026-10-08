---
type: ImplementationStage
title: R10 - Native effects, UI and every game mode
status: planned
created: 2026-10-06
tags: [okf, rendering, stage, vulkan]
---

# R10 - Native effects, UI and every game mode

**Status:** pending. **Dependency:** R07 accepted; world coverage R03-R06 established.

Execution precedes R11, R08 and R09 under the 2026-10-08 adopted order. Use
original content and accepted native material/composition/target contracts;
production modern imports and final AA are not prerequisites. Later R08/R09
must regression-test these accepted consumers rather than postpone their migration.

## Objective

Complete the graphics path beyond world meshes, so the native mode no longer relies on the PSX GPU for presentation.

## Steps

1. Migrate decals/skid marks, particles/smoke/debris, flares, lens effects, shadows used by gameplay presentation and other R00 effect categories to explicit native resources/material operations.
2. Render HUD/minimap/overhead map, fonts, pause/fades/loading/frontend and developer overlays as native UI/images/quads using original content. A semantic original-command adapter is allowed; running PSX GPU emulation is not final native UI.
3. Replace render-to-VRAM/framebuffer-read effects with explicit native render targets/history. Verify rear-view/other secondary views where the current game exposes them.
4. Cover split-screen, frontend previews, replays/director, cutscenes, menus and loading transitions; define per-view camera/light/AA/UI ownership and avoid shared mutable viewport state.
5. Keep existing supported product scope honest: desktop FMV playback is not currently implemented; preserve existing playback/content paths and record unsupported modes rather than promise restored movie features.

## Deliverables

Complete native effects/UI and mode coverage, plus a source-category checklist without remaining native-mode PSX GPU draw paths.

## Acceptance gate

- [ ] Every currently functional graphical surface in supported modes renders through native Vulkan, including UI and auxiliary targets.
- [ ] No world shadows, tone mapping or AA corrupt HUD; alpha/add/subtract and pause/fade ordering are correct.
- [ ] Split-screen/views, loading, resize and map updates preserve content and state.
- [ ] Coverage counters report zero legacy GPU draws in accepted native frames, not only zero legacy buildings.
- [ ] Effects/UI and multiple views pass with original native resources and the current sampling profile, without requiring R08 AA or R09 production imports; retain cases for their later regression checks.

## Planned validation

VS build and RenderDoc main/auxiliary/overlay inspection; gameplay, menus, loading/map, pause/fades, split-screen, on-foot, replay/director and representative cutscene transitions; input capture and display restore checks.

## Risks and fallback

Current effects assume VRAM persistence/order. Each needs an explicit native target/history contract; do not assume the last presented swapchain image is a history texture.

Native mode remains opt-in until complete functional acceptance; legacy platform path preserved.

## Handoff

Record changed files, actual checks/results, unresolved limits and the next stage
in the [active plan](../implementation-plan.md), current status and recent context.
Every Vulkan change also updates the unscheduled OpenGL compatibility backlog.
Pass the [shared validation gates](../validation-and-completion.md); a planned
checklist is not evidence. Update the matching implemented product surface before
claiming delivery. This stage request does not authorize executing later stages.
