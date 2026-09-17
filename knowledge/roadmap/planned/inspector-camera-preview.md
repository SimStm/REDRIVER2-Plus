---
type: Roadmap
title: Inspector navigation and independent previews
status: planned
execution_order: 7
tags: [roadmap, camera, inspector]
---

# Inspector navigation and independent previews

## Problem

Free Camera and GUI selection compete for mouse capture, and current previews are limited to loaded overrides. Inspecting all sides of an object is cumbersome.

## Intended behaviour

Provide predictable navigation/selection modes, focus on a selected object, and clearly labelled original/override texture and model previews.

## Scope

Integrate existing Free Camera/F7 behavior; optional focus/orbit/follow controls; preview resource lifetime; source versus currently rendered model/LOD labels; optional pause while inspecting.

## Non-goals

Do not change ordinary driving controls or require a second game simulation. An independent model preview does not imply arbitrary model re-import.

## Dependencies and risks

Order 07 after the handle contract in [04](../done/asset-catalog-identity.md) and enough coverage in [06](../done/inspector-selection-coverage.md). Source model adapters can be delivered with [09](model-export-import-roundtrip.md).

## Suggested execution order

Overall order: **07** in the [planned catalog](index.md).
Execute the following milestones sequentially. A request for one milestone
is not authorization to implement all later milestones or dependent features.

1. Define transitions among game control, free navigation and selection, including F11/F7, cursor visibility, relative mouse and input capture.
2. Add focus/follow controls using validated selected handles; cancel safely on unload or slot reuse.
3. Add original texture versus active override previews, preserving aspect ratio and labelling alpha mode, dimensions and palette.
4. Implement an isolated model preview with orbit, zoom, bounds, wireframe and material selection for supported adapters.
5. Test render target, GPU state and resource cleanup after previews and window changes; persist only deliberate user settings.

## Acceptance criteria

Clicking the GUI does not steer the game or rotate the camera. Navigation can be exited reliably. Previews identify their data source and do not mutate live geometry or leak renderer state.

## Validation plan

Test open/close, alt-tab, lost focus, pause, resized windows, object destruction and repeated preview changes. Check independent preview and game frame rendering together.

## Starting points

`src_rebuild/redriver2_psxpc.cpp`, `src_rebuild/utils/DebugOverlay.cpp`, `DeveloperGraphicsPanel.cpp`, and PsyCross input/overlay APIs.
Paths are relative to the repository root. Inspect current source and local
diffs before editing; these proposals are not proof of current behavior.

## Handoff and completion

For each requested milestone, report changed files, checks actually run,
remaining limitations and the next unblocked milestone. Preserve C++11 and
platform guards; maintain game-specific behavior outside PsyCross and carry
submodule changes in the project patch. Do not include game assets in tests
or commits. Only after all acceptance criteria are met, publish
`knowledge/product/inspector-camera-preview.md`, move this record to `done/`, update
status/date/catalog links, and record the delivered change in Unreleased.
