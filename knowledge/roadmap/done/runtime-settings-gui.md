---
type: Roadmap
title: Runtime settings GUI and safe persistence
status: implemented
completed: 2026-09-20
execution_order: 10
tags: [roadmap, settings, gui, input, display]
---

# Runtime settings GUI and safe persistence

Status: implemented 2026-09-20. Product behaviour is documented in
[`knowledge/product/runtime-settings-gui.md`](../../product/runtime-settings-gui.md).
This record is the delivery history; the product document and the source are the
authoritative evidence for current behaviour. Change record:
[`knowledge/changes/2026-09-20/runtime-settings-gui/index.md`](../../changes/2026-09-20/runtime-settings-gui/index.md).

## Objective

Expose the settings that currently live outside the developer GUI with explicit
live/apply/reload/restart behaviour and clear persistence ownership, and make
persistence failure-safe.

## Scope and non-goals

Inventory `config.ini` consumers; camera toggle, dynamic lighting, display
mode/resolution, game/menu/controller bindings and legacy `overrideContent`
diagnostics. Preserve `developer_graphics.ini` ownership for existing developer
settings and define new fields deliberately. Do not treat every setting as an
immediate flag, overwrite `config.ini` wholesale, change default controls, or
expand the supported ImGui platforms implicitly.

Acceptance criteria: every control states when it takes effect; display changes
can be recovered; binding edits do not leak into gameplay; unsupported settings
are disabled with an explanation; saved settings round-trip without damaging
unrelated configuration.

## Settings inventory

Every setting below was traced to its reader in the source. `live` means the
owning system reads the variable each frame or each use, so a GUI write takes
effect immediately; `apply` needs one call into the owning system; `reload` needs
data re-read (level, language, mappings); `restart` needs the window/device/audio
initialised again.

### `config.ini` (shipped game config; read once in `redriver2_psxpc.cpp:594-703`, then `ini_free`)

| Key | Reader | Class |
| --- | --- | --- |
| `render.windowWidth` / `windowHeight` / `fullscreen` | `PsyX_Initialise` window size | restart |
| `render.screenWidth` / `screenHeight` | fullscreen size | restart |
| `render.vsync` | `g_cfg_swapInterval` | live |
| `render.bilinearFiltering` | `g_cfg_bilinearFiltering` | live |
| `render.pgxpTextureCorrection` / `pgxpZBuffer` | PGXP globals | live |
| `render.textureOverrides` | `HdTextureOverrides_SetEnabled` (`:715`, only when present) | live, overrides the panel |
| `game.captureAfterSeconds` | scripted capture timer | once at start |
| `game.languageId` | `gUserLanguage`, loads `*_GAME.LTXT` (`dr2locale.c:35`) | restart |
| `game.drawDistance` | `gDrawDistance` | live |
| `game.dynamicLights` | `gEnableDlights`, read per car draw (`cars.c:974`) | live |
| `game.fieldOfView` | `gCameraDefaultScrZ` | live |
| `game.disableChicagoBridges` | `gDisableChicagoBridges`, level road setup (`dr2roads.c:175`) | reload |
| `game.freeCamera` | installs debug key/mouse handlers (`:677`) | restart |
| `game.fastLoadingScreens` | loading-screen waits | live |
| `game.widescreenOverlays` | `gWidescreenOverlayAlign` | live |
| `game.driver1music` | `gDriver1Music`, music start (`main.c:1307`) | live (next music start) |
| `game.overrideContent` | `gContentOverride`, frontend car availability (`FEmain.c:2289`) | reload |
| `game.userChases` | chase-car list | once at start |
| `kbcontrols_game` / `kbcontrols_menu` | `ParseKeyboardMappings` into fixed arrays (`:696`) | reload (`SwitchMappings`) |
| `controls_game` / `controls_menu` | `ParseControllerMappings` (`:699`) | reload |
| `pad.pad1device` / `pad2device` | controller assignment | restart |

### `developer_graphics.ini` (developer panel; `DeveloperGraphicsSettings.cpp`)

`bilinearFiltering`, `pgxpTextureMapping`, `pgxpZBuffer`, `vsync`,
`drawDistance`, `fieldOfView`, `showLegacyStats`, `hdTextureOverrides`,
`organizeTextureExports`, `exportBaseColours`, `overrideProportionalAlpha`,
`modernRenderer`.

### `developer_modern_mesh.ini` (modern-mesh module)

`enabled`, `shadows`, `ambientOcclusion`, `legacyLighting`,
`legacyLightReceptivity`, `lightdir`, `shadowdebug`, `pointLights`,
`shadowBias`. All are **live** (`ApplyLightSet` + per-frame reads).

### `developer_debug_start.ini` (debug start snapshot)

Mission, city, vehicle, position, heading, players, chase. Applied at startup
only, and only when no `-mission`/`-replay` argument is present.

### Findings that constrained later milestones

- `config.ini` and the developer files already have separate ownership; nothing
  in the panel writes `config.ini`, so persistence keeps that split rather than
  merging files.
- `textureOverrides` is the only `config.ini` key that deliberately overrides a
  developer setting, and only when the key is present (`:715`).

## Delivered milestones

All five milestones were delivered on 2026-09-20. Each was validated on Windows
`Release_dev|x64` with 0 failed projects, and the final build passed
`REDRIVER2_dev.exe -vkpsxtest` (`psx self-test: PASS`).

1. **Inventory** - the tables above.
2. **Live controls** - `dynamicLights`, `widescreenOverlays` and
   `fastLoadingScreens` as Graphics-tab checkboxes, persisted in
   `developer_graphics.ini` (`schemaVersion` 6 -> 7). Apply path verified by a
   capture A/B of the overlay corners (`meanAbs 21.3/255`, 41% of samples above
   8); writer verified by a settings save that rewrote the file.
3. **Display mode** - Fullscreen (desktop) and a window-size combo through the
   new `PsyX_ApplyWindowMode`, with a 15 s Keep/Revert countdown that also runs
   while the panel is closed, persisted at `schemaVersion` 8. Verified: a
   persisted 1600x900 produced a 1600x900 capture; a provisional 1024x600 kept
   through `ConfirmDisplayMode` persisted that size; the same change left
   unconfirmed reverted to 1600x900 without touching the file; a pick at the same
   relative point resolved a primitive at both sizes.
4. **Binding editor** - the Input tab with click-to-capture, Escape/right
   click/Cancel/timeout cancellation, conflict markers, per-action and
   per-device reset, distinct game/menu tables, input held while capturing, and
   an edit that only reaches the currently active table. Verified with synthetic
   SDL events through the panel's own event handler: `capture-key consumed=1
   active=0 binding=20 (Q=20)` while the live menu mapping stayed `RETURN`,
   `conflicts=2 first=Cross second=Circle`, both cancels left the value at 44,
   and Reset restored `Up`.
5. **Legacy content and persistence** - a Content and language section
   (content override, Chicago bridges, language, Driver 1 music, with the
   restart-only Free camera disabled and explained), the shared
   `DeveloperSettingsFile_WriteKeys` writer that preserves foreign lines and
   writes through `.tmp`/`.bak`, and bindings persisted as overrides in
   `developer_input.ini`. Verified: a hand-added comment and unknown key survived
   a graphics save while missing keys were appended; `input-save=1 input-load=1
   cross-default=82 cross-after-load=20 (Q=20)` proved the round-trip; after a
   reset the override line was removed; and with the file seeded the Input tab
   rendered `Cross | Q`.

## Limits

Recorded in full in the product document. The main ones: the panel's own click
path cannot be driven by a script, so UI behaviour is verified through the
shared functions the widgets call; the fullscreen branch of the display controls
is not runtime-tested, nor are multi-monitor/DPI changes or alt-tab; controller
bindings apply to every connected pad and `pad1device`/`pad2device` stay
unexposed; the shared writer's failure branch is inspection-verified.
