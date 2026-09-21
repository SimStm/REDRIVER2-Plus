---
type: Product
title: Runtime settings GUI
description: The developer panel as the runtime settings surface, its tabs, persistence files and safety rules.
tags: [product, tooling, settings, input, display]
---

# Runtime settings GUI

The developer panel (F11, Dear ImGui, Windows and Linux only) is the runtime
settings surface for the fork. It edits live game settings, the display mode and
the input bindings, and it persists developer state in separate files so
`config.ini` stays the shipped, user-owned configuration.

Delivery history and evidence:
[`knowledge/roadmap/done/runtime-settings-gui.md`](../roadmap/done/runtime-settings-gui.md).

## Ownership and precedence

| File | Owner | Contains |
| --- | --- | --- |
| `config.ini` | the game / the user | shipped defaults for every setting; never written by the panel |
| `developer_graphics.ini` | the developer panel | graphics, display and game-option state edited in the panel |
| `developer_input.ini` | the developer panel | input bindings that differ from `config.ini` |
| `developer_modern_mesh.ini` | modern-mesh module | modern renderer, lights and shadow tuning |
| `developer_debug_start.ini` | debug-start module | the reproducible startup snapshot |

The panel files are applied after `config.ini`, so a developer value wins over
the shipped default. `textureOverrides` is the one deliberate exception: an
explicit `config.ini` key still forces texture overrides for reproducible
captures. Panel edits never write `config.ini`; a setting with no persistence in
`developer_graphics.ini` is session-only by design and says so.

## Surfaces

- **Graphics** - live classic/enhanced renderer controls (bilinear, PGXP, vsync,
  draw distance, field of view, legacy stats, the modern mesh/lighting set), the
  live game options `dynamicLights`, `widescreenOverlays`, `fastLoadingScreens`,
  and the display mode. The **Modern sun and look** group exposes the whole
  `developer_modern_mesh.ini` light set: sun azimuth/height, sun intensity,
  modern ambient, modern exposure, shadow volume size, legacy light strength and
  a shadow debug view, so the lighting can be checked at any angle without
  editing the file or restarting. It reads the live direction vector, so the
  `[`/`]`/`;`/`'` keys and the sliders stay in sync.
- **Display mode** - Fullscreen (desktop) and a window-size combo (current plus
  1280x720, 1600x900, 1920x1080) through `PsyX_ApplyWindowMode`, which resizes
  the SDL window, resets the render device and therefore also moves the viewport,
  aspect handling and primitive picking. A change is provisional for 15 seconds
  with Keep/Revert. The confirmation is **its own ImGui window** anchored to the
  bottom-left of the applied display size, drawn whether or not the panel is
  open and after it, so a mode change cannot hide the buttons that accept or
  reject it; the countdown runs even with the panel closed, so an unusable mode
  always reverts. Keeping it persists to `developer_graphics.ini`. While the
  confirmation is armed the cursor is shown and input stays captured, so the
  click cannot also reach the game.
  - **Minimizing does not stop the countdown.** The decision is deliberate: the
    countdown is a safety timer, so the guarantee "an unconfirmed mode always
    reverts" has to hold whatever the user does, including leaving the window
    minimized. On the Vulkan backend a minimized window stops producing frames
    (its surface has no extent to acquire from), which freezes the timer for as
    long as it lasts; on restore the frames and the remaining countdown resume,
    and the mode reverts then. Verified on Vulkan with an iconized window: the
    timer held its remaining value through the occlusion and reverted after
    frames resumed.
  - A single frame can spend at most `0.25 s` of the budget. ImGui derives
    `DeltaTime` from wall clock, so the frame after a debugger stop, a
    hibernation or any other long stall would otherwise carry the whole gap and
    revert the mode the instant the game resumes. Verified: the first frame
    after a long debugger stop reports `dt=0.2500` and the remaining budget only
    drops by that step.
- **Input** - the game and menu tables for keyboard and controller, kept
  distinct. Click a binding to capture the next key, controller button or stick
  push; Escape, a right click, Cancel, 10 seconds of silence, or closing the
  panel cancels. Each table is `Action | Binding | Also bound to | Reset`: the
  conflict note has its own stretch column so a long list wraps inside the table
  instead of widening the binding column until the table leaves the panel.
  Reset returns an action or a whole table to the `config.ini` value. Input is
  held while capturing, and an edit only reaches the live mapping when its table
  is the active one, so editing gameplay bindings cannot change what an open
  menu answers to.
- **Game Debug** - legacy content and language: content override, Chicago
  bridges, language and Driver 1 music (session values with their reload/restart
  requirement stated), plus the restart-only Free camera shown disabled with the
  reason.

## Help markers

Every checkbox that carries an explanation draws the `(?)` marker at the end of
its label, not on a line below it (`CheckboxWithHelp`). The marker stays on the
control's line while the line has room and moves to its own line when it does
not, so a long label is never clipped by it. The sliders added for the modern
sun follow the same rule (`SliderFloatWithHelp`/`SliderIntWithHelp`). Help text
that describes a paragraph of prose (tab intros, section descriptions) keeps the
marker on its own line.

## Safety rules

- `DeveloperSettingsFile_WriteKeys` writes every developer settings file. It
  preserves lines the caller does not own (unknown keys, comments, blank lines),
  drops keys the owner no longer wants, appends missing keys, writes through
  `<path>.tmp` and rotates the old contents into `<path>.bak`. A failure removes
  the temporary file and leaves the existing file untouched.
- Input bindings persist as overrides only: a binding equal to the `config.ini`
  value is not written, so deleting a line or pressing Reset returns the binding
  to the shipped value and the file stays small.
- Every control states when it takes effect (live, next level load, next music
  start, restart), and the display change is the only one that needs a
  confirmation window.

## Limits

- The panel's own click path cannot be driven by a scripted agent; UI behaviour
  is verified through the shared functions the widgets call, plus captures that
  read the rendered panel. Desktop-control MCP clicks do reach the panel once
  the game window is frontmost, but synthetic clicks on the binding-capture
  button were not observed to start a capture, so that banner is verified from
  the code path rather than a click.
- The fullscreen branch of the display controls is not exercised by the tests
  (it would change the machine's display), and display/DPI changes across
  monitors plus alt-tab are untested.
- Controller bindings apply to every connected pad; the panel does not
  distinguish controller instances, and `pad1device`/`pad2device` are still
  restart-class options that the panel does not expose.
- The language option needs a restart because the locale files load during
  `InitStringMng`.
