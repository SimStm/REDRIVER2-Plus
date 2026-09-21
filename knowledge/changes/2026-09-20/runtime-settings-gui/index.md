---
type: Change
title: Runtime settings GUI milestones 2-4: live controls, display mode, bindings
description: Expose live game options, a recoverable display mode and a binding editor in the developer panel, with executed evidence for each.
tags: [tooling, developer-panel, settings, input, display]
---

# Runtime settings GUI milestones 2-4

Implementation of [`runtime-settings-gui.md`](../../roadmap/done/runtime-settings-gui.md)
milestones 2-4. The primary record with the settings inventory, per-milestone
evidence and remaining limits is that roadmap document; this record covers the
changed files, the techniques and one environment incident.

## Changed files

- `src_rebuild/utils/DeveloperGraphicsSettings.{h,cpp}` - `schemaVersion` 6 -> 8:
  `dynamicLights`, `widescreenOverlays`, `fastLoadingScreens` (milestone 2) and
  `fullscreen`, `windowWidth`, `windowHeight` (milestone 3), plus display-state
  tracking that keeps the last windowed size while fullscreen and applies the
  persisted mode through `PsyX_ApplyWindowMode` on load and on restore-defaults.
- `src_rebuild/utils/DeveloperGraphicsPanel.cpp` - three live checkboxes, the
  display controls with the 15 s keep/revert banner (`UpdateDisplayRevert` runs
  before the visibility check), and the Input tab with `BuildOverlayWidgets`
  capture handling.
- `src_rebuild/utils/DeveloperInputMapping.{h,cpp}` - new module: action lists
  per device, game/menu table get/set, default snapshot, reset, conflict lookup,
  name formatting and active-table-aware apply.
- `src_rebuild/redriver2_psxpc.cpp` - includes the new header, reports the active
  table from `SwitchMappings`, and captures the `config.ini` bindings as the
  reset defaults before the panel can edit them.
- `src_rebuild/PsyCross` (fork working tree) - `PsyX_ApplyWindowMode` in
  `PsyX_public.h`/`PsyX_main.cpp`; no other renderer change for these
  milestones.

## Techniques that worked

- Both modules were validated by temporary in-loop probes driven by environment
  variables (`PSYX_DISPLAY_PROBE`, `PSYX_INPUT_PROBE`), removed and rebuilt
  afterwards. The input probe fed synthetic `SDL_Event`s into
  `HandleSDLEvent`, which is the same path a real SDL event takes, so the
  capture, cancellation and conflict logic was exercised without a working
  scripted keyboard path.
- The display probe recorded `SDL_GetWindowSize` before, during and after a
  provisional change, which is what proved both the timeout revert and that an
  unconfirmed mode is never persisted.
- `PsyX_Inspector_RequestPick` at a fixed fraction of the window resolved a
  primitive before and after a resize, which demonstrated the picking path
  follows the window instead of caching a size.

## Incident: generated Visual Studio configurations

Regenerating the projects with `src_rebuild/premake5.exe vs2022` (needed so the
new `utils/DeveloperInputMapping.cpp` would compile) rewrote every `.vcxproj` and
deleted the hand-added `Release_dev_gl` and `Release_dev_playground`
configurations, which exist only in the generated files. `REDRIVER2.vcxproj` and
`REDRIVER2.sln` were restored from the backups taken first; the other three
projects were repaired by cloning every `Release_dev` block
(`ProjectConfiguration Include`, `Configuration` `PropertyGroup`, `ImportGroup`,
property `PropertyGroup`, `ItemDefinitionGroup`) for both configurations and
platforms, with `visual-studio-ide-mcp build_configuration_get` confirming all
ten configuration/platform pairs are back. The convention and the repair steps
are recorded in
[`visual-studio-project-regeneration.md`](../../rules/visual-studio-project-regeneration.md).

## Not done

- Binding persistence, legacy-content state and the "unrelated fields" hardening
  are milestone 5.
- The fullscreen branch of the display controls was not exercised at runtime, and
  the fullscreen/DPI/alt-tab cases of the validation plan remain unvalidated.

## Incident: the panel body was deleted and reconstructed

While removing a temporary probe from `DeveloperGraphicsPanel.cpp` with a
pattern-matched line range, the end-of-block heuristic matched a much later
closing brace and deleted about 220 lines: the body of `BuildOverlayWidgets`
(the tab bar and the whole Graphics tab), `RenderOverlay` and the
anonymous-namespace close. The file still compiled - the panel body had been
replaced by `RenderOverlay`'s ImGui frame calls with `BuildOverlayWidgets()`
calling itself - so only inspection caught it.

The region was reconstructed from the committed version of the file, which
contained the original panel body, plus the session's additions (the display
banner and controls, the live game options, the legacy-lighting controls and the
Input tab). Verification: every function and every `g_*` global from the
committed version is present in the reconstructed file, the build is green, the
PSX self-test passes, and a desktop capture of the restored Graphics tab shows
the tab bar and every control. The committed version's `RenderOverlay` Vulkan
branch was preserved.

Lesson, now in [`mcp-agent-tooling.md`](../../rules/mcp-agent-tooling.md): remove
a temporary probe by replacing its exact text and then checking the new line
count, never by deleting a pattern-matched range.
