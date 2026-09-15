---
type: Roadmap
title: Runtime settings GUI and safe persistence
status: planned
execution_order: 10
tags: [roadmap, settings, gui]
---

# Runtime settings GUI and safe persistence

## Problem

Free Camera, dynamic lights, input mappings, resolution and legacy content options exist outside the developer GUI. Not every config.ini option can safely apply to already loaded resources.

## Intended behaviour

Expose relevant settings with explicit live/apply/reload/restart behavior and clear persistence ownership.

## Scope

Inventory config.ini consumers; camera toggle, dynamic lighting, display mode/resolution, game/menu/controller bindings and legacy overrideContent diagnostics. Preserve developer_graphics.ini ownership for existing developer settings and define new fields deliberately.

## Non-goals

Do not treat all settings as immediate flags, overwrite config.ini wholesale, change default controls, or expand the supported ImGui platforms implicitly.

## Dependencies and risks

Order 10. Coordinate camera behavior with [07](inspector-camera-preview.md), legacy content semantics with [08](modding-toolchain-integration.md), and renderer correctness with [01](texture-alpha-semantics.md). Independent settings can ship as separate milestones.

## Suggested execution order

Overall order: **10** in the [planned catalog](index.md).
Execute the following milestones sequentially. A request for one milestone
is not authorization to implement all later milestones or dependent features.

1. Inventory each setting's reader, runtime API, platform guard and persistence file; classify live, apply, reload or restart.
2. Add low-risk live controls first, such as dynamic-light and camera controls where the build supports them.
3. Add display-mode application with timeout/revert and synchronized viewport/picking updates.
4. Add binding capture, cancellation, conflicts and reset-to-default behavior, keeping game/menu/controller mappings distinct.
5. Expose legacy content state with explicit reload requirements, then implement failure-safe persistence without losing unrelated fields.

## Acceptance criteria

Every control states when it takes effect. Display changes can be recovered, binding edits do not leak into gameplay, unsupported settings are disabled with an explanation, and saved settings round-trip without damaging unrelated configuration.

## Validation plan

Test apply/cancel, restart, malformed config, read-only files, display failure, alt-tab, input capture and controller disconnect. Confirm Windows/Linux guards and document unavailable platform tests.

## Starting points

`data/config.ini`, `src_rebuild/redriver2_psxpc.cpp`, `DeveloperGraphicsPanel.cpp`, `DeveloperGraphicsSettings.cpp`, PsyCross window/input APIs.
Paths are relative to the repository root. Inspect current source and local
diffs before editing; these proposals are not proof of current behavior.

## Handoff and completion

For each requested milestone, report changed files, checks actually run,
remaining limitations and the next unblocked milestone. Preserve C++11 and
platform guards; maintain game-specific behavior outside PsyCross and carry
submodule changes in the project patch. Do not include game assets in tests
or commits. Only after all acceptance criteria are met, publish
`knowledge/product/runtime-settings-gui.md`, move this record to `done/`, update
status/date/catalog links, and record the delivered change in Unreleased.
