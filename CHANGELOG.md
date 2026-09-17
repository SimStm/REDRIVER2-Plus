# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog 1.1.0](https://keepachangelog.com/en/1.1.0/),
and this project follows [Semantic Versioning](https://semver.org/spec/v2.0.0.html)
where release policy permits it.

## [Unreleased]

### Added

- Palette variant texture exports: a manifest entry may now carry a `"clut"`
  that is part of the override identity, so a car or pedestrian texture drawn
  through several runtime palettes gets one PNG and one registration per
  palette instead of collapsing into a single `(texture, texturePage,
  textureIndex)`. The 3D Debug tab exposes **Export all palette variants (PNG)**
  for car and pedestrian selections, writing `NAME_pP_iI_clutN.png` files. Legacy
  entries without `clut` keep the previous single-override behaviour, and a load
  registers one override per declared CLUT. Measured with two `clut` entries for
  `GRASS01C`: 2 loaded images and 2 active renderer mappings.
- Type/level metadata backfill on re-export: a pre-existing manifest entry that
  predates the descriptive `type`/`level` fields gains them on the next
  re-export with a context, without a duplicate registration, alongside the
  existing model-reference merge.
- A live page-registration cost diagnostic on the Mods tab. Measured on the
  Chicago debug start: 0.176 ms of name registration for 18 pages with no mods,
  versus 927.6 ms when the `inspector-export` mod's PNGs are decoded during the
  pass, so the spool cost is override-image decoding rather than the
  registration walk.
- Opt-in proportional texture-override alpha (roadmap item 16, complete): the
  new persisted `Proportional override alpha` developer setting (Mods tab,
  default off) makes `BM_AVERAGE` override draws blend the imported PNG alpha
  through `SRC_ALPHA, ONE_MINUS_SRC_ALPHA` instead of the binary 0.5 cutout, so
  `alpha 0/64/128/192/255` render five steps while `alpha 0` still punches
  through and `alpha 128` still matches the original `STP=1` blend. Opaque,
  additive and subtractive draws keep the compatibility cutout, so existing mods
  are unaffected until the flag is enabled; the CPU picker mirrors whichever
  mode is active.
- Whole-object selection across renderer categories (roadmap item 06,
  complete): the 3D Debug inspector now resolves a clicked primitive to the
  source that produced it and reads it at four scopes - **Face**, **Material**,
  **Component** and **Logical object** - with game-owned identity for buildings,
  tiles, animated props, car bodies, wheels and pedestrian parts. Keys are
  resource-based and LOD-independent, retained selections carry a catalog
  generation plus a model handle (and a car slot for vehicles) and report
  `valid` / `stale` / `not model-backed` instead of retargeting silently.
  Picking is visibility-aware: ordering-table arrival order is the depth test, a
  registered source is preferred over unidentified overlays so screen-space
  effects no longer shadow the scene, the cursor is projected into the split's
  emulated display area, PGXP is honoured with texture mapping and the Z-buffer
  either way, and override cutout coverage is mirrored on the CPU so transparent
  texels are skipped and the geometry behind resolves. New diagnostics report
  the frame-local pick index, sources that are registered but unreachable,
  masked and cut-out override texels, a   locator for any reachable source or
  component, and the measured pick cost. The identity and picking paths are
  platform-neutral; the diagnostics UI follows the existing developer-panel
  guards (Windows and Linux), and the renderer changes are carried by
  `patches/psycross/developer-overlay.patch`.
- A resident procedural playground scene (roadmap item 13, milestones P1-P5)
  that reuses the existing renderer and original vehicle simulation: a
  generated flat drivable surface, collidable box obstacles, disabled donor
  traffic/police/missions and camera events, a car reset control, safe return
  to the frontend, a `-playground` developer launch path, and a **Playground**
  choice on the Take a Ride city screen that starts the same launcher from every
  build. A loaded original level only supplies car, texture and sound resources;
  its world geometry, roads and collisions are replaced. The stable fixture
  identity is `playground.flatpad.v1`.
- Batch texture export from the 3D Debug tab (roadmap item 05, complete): one action exports the currently identified texture list, deduplicating by the exact `(name, texturePage, textureIndex)` identity so a texture shared by several models is published once, and reports exported, duplicate-omitted and failed counts with the last failure named. New manifest entries carry a backward-compatible `modelReferences` array and a readable, sanitized model filename suffix derived from the asset catalog, falling back to an explicit `slot<index>` / `unknown:model-slot:<index>` form when no catalog record exists; model references are metadata and never change the override key. Re-exporting writes the PNG to the manifest's existing mapped `file` and merges new references into the same entry, preserving unknown fields and never appending a duplicate registration, and a separate catalog-scoped action exports every material linked to the selected source model, including hidden faces and its high-detail LOD sibling, resolving the VRAM region of materials that are not on a submitted triangle and naming any missing adapter (palette variants and cross-model child parts are not enumerated yet). Large batches run cooperatively across frames with a progress bar, cancel control, per-resource results and retry of failed resources, publishing each file independently so cancelling keeps what was already written. Validated in-game on the Chicago debug start: selecting tile `GRASS01C` (source model 76), the catalog-scoped batch exported 1/1 with 0 duplicates, a repeat run left the manifest at 80 distinct entries with no duplicate identity, and the entry kept all 8 shared model references.
- A source-aware asset catalog (roadmap item 04, complete): a game-owned,
  allocation-free module that separates stable resource identity (models,
  textures, cars, palettes) from runtime instances, keeps the exact
  `(texture, texturePage, textureIndex)` identity the mod manifest matches on,
  records explicit `UNKNOWN`/`DECLARED`/`VERIFIED` provenance, links models to
  textures and LODs many-to-many, and invalidates retained handles when a
  runtime slot is reused, streamed away, or the level changes. It is populated
  from the level loaders (models with names and LOD links, texture sets, car
  resident slots, model material links), re-populated as regions stream, and
  includes the playground's generated assets; the building/tile/car inspector
  keys carry catalog source identity and the city variant. The 3D Debug tab
  shows catalog context, counts vs capacity, the fixed footprint, and the
  selected building/tile's stable id, provenance and source-material list
  (including hidden faces). Covered by the standalone `AssetCatalogTests`.
- Linked playground and renderer-modernization roadmaps, with a validated
  minimum playground preceding modern scene experiments and original-city
  regression checks retained alongside the dedicated test environment.
- A planned Meshy MCP workflow for prompt-generated PBR test assets, with
  export validation and reproducible local fixtures for renderer development.
- A source-backed discussion of a procedural driving playground, declarative
  scene options, collision requirements, and Take a Ride integration.
- An evolving knowledge discussion catalog, with automatic topic maintenance
  guidance and an illustrated renderer-modernization exploration kept separate
  from adopted roadmap work and implemented features.
- Thirteen ordered roadmap records for mod-system PR readiness, texture
  correctness, manifest merging, asset identity, batch exports, inspector
  coverage, camera/previews, tool interoperability, model round trips,
  runtime settings, draw distance, and measured graphics improvements.
- A knowledge roadmap lifecycle for planned and completed features, with
  required product documentation for completed roadmap records.
- Object-level building inspection, LOD-independent world-object keys, bounded
  model-name lookup, and a list of registered textures on the selected object
  with individual PNG export actions.
- Standalone Windows export regression tests covering channel fidelity,
  repeated writes, source validation, and preservation after failed writes.
- Added configurable diagnostic selection highlighting, draw-source labels,
  live car-slot positions, loaded override previews, selection-detail TXT
  export/clipboard copying, and explicit export paths
  and disabled-action explanations in the 3D inspector.
- Added discoverable JSON texture mods under `mods/<id>/manifest.json`, with a
  deterministic `mods/enabled.json` load order and a `3D Debug` inspector for
  their declared texture resources.
- Added click-based PSX primitive inspection, including texture page, CLUT,
  UV, level texture identity, active override, and provenance for car bodies
  and city tiles; selected textures export as PNG and selected cars as OBJ.

- A Windows desktop HD-texture vertical slice: external RGBA PNG replacements
  can be mapped by `TEXINF` name with original TIM/VRAM and CLUT fallback
  preserved, plus manifest documentation and Developer Graphics Panel
  diagnostics.
- A **Reproduce this state** section in the developer panel's Game Debug tab
  that generates and copies the direct-start command line for the current
  mission, vehicle, position, players, and chase, and persists it to
  `developer_debug_start.ini` for automatic startup.
- `scripts/run_debug_start.ps1`, which launches a debug build at a saved or
  explicit session and can capture the same scene with texture overrides on and
  off. Scripted captures use the new `[game] captureAfterSeconds` setting and
  `[render] textureOverrides` instead of injecting keyboard input.
- Added `-gametype <n>`, `-level <n>`, and `-startdir <angle>` debug arguments
  so a direct start can restore the game mode, city, and vehicle heading;
  `GAME_TAKEADRIVE` derives the mission from the level, so `-mission` alone was
  not sufficient, and the position override previously discarded the heading.
- The generated project debugger working directory is now the executable
  folder, so `F5` finds `config.ini`, `mods/`, and `developer_debug_start.ini`.
- Documented deterministic launch arguments: `-mission`, `-gametype`, `-level`,
  `-playercar`, `-startpos`, `-players`, `-chase`, `-replay`, `-nointro`, and
  `-nofmv`.
- Dear ImGui developer panel, opened with F11, with live graphics controls and
  an explained game-debug tab for renderer, streaming, traffic, police,
  mission, vehicle, and road-state telemetry.
- Durable agent guidance, OKF knowledge catalog, and build-file regeneration
  rule for contributor and AI-agent workflows.
- `BUILDING.md`, a dedicated build and run guide, with per-platform
  prerequisites and dependency installation, Windows/Linux/WSL steps, the
  deterministic debug-start workflow, the standalone export tests, screenshots
  in `docs/images/`, and troubleshooting. The README now links to it instead of
  inlining the build steps.
- Inspector exports record what they belong to: every manifest entry now carries
  a `type` (`buildings`, `sprites`, `cars`, `pedestrians`, `tiles`, `props`)
  and the `level` it is used on, and the developer panel gained an opt-in
  **Organize exports by type and level** setting that writes textures under
  `assets/inspector/<type>/<level>/`. The flag is off by default; the manifest
  always stores the real relative path.
- The developer panel can export textures in **base colours** (enabled by
  default). Cars and pedestrians are drawn through runtime palettes, so the
  choice is explicit at export time: enabled, the PNG uses the palette the level
  registered for the texture - the original artwork colours that keep working
  with CLUT recolouring; disabled, it uses the palette of the clicked primitive,
  so each instance exports the colours it currently shows. The tooltip explains
  both cases.
- `scripts/run_inspector_tests.ps1` builds and runs `AssetCatalogTests` and
  `InspectorExportTests` in fresh directories, so the export-path verification
  is reproducible. `InspectorExportTests` is not idempotent and must never run
  twice in the same directory; `AGENTS.md` records the command.
- Double-clicking in pick mode selects the whole object instead of the part: the
  selection keeps the parent instance of the clicked component (a car, a
  pedestrian) and highlights and exports every part together. A single click
  keeps selecting the individual part.

### Changed

- Reorganised the developer panel: modding information (override toggle, mod
  diagnostics, mods root, reload, enabled mod order, declared override list)
  moved from the Graphics and 3D Debug tabs into a dedicated **Mods** tab, and
  the **3D Debug** tab is now render-debug only (picking, highlight, selection
  details, asset-catalog diagnostics, and model/texture export plus the mod
  reload action). The **Graphics** tab keeps renderer controls.
- Fully opaque texture overrides now generate mipmaps and use mipmap
  minification, which reduces the minification shimmer reported for large
  upscales such as `GRASS01C`. Overrides that contain transparency keep plain
  filtering so mip averaging cannot bleed the transparent colour into cutout
  edges.
- Mipmapped overrides now use anisotropic filtering (capped at 4x) when the
  driver exposes `GL_EXT_texture_filter_anisotropic`, reducing grazing-angle
  shimmer on roads and ground. The PSX nearest-filtered path is unchanged.
- Exported texture alpha now follows the PSX transparency rules and the
  consuming draw's blend context (roadmap item 15, complete). `0000h` always
  exports as a cutout (`0`), because PSX treats it as fully transparent in every
  context and Driver 2 writes opaque black as `8000h`. The `STP` flag exports
  `128` on a semi-transparent draw and `255` on an opaque draw, where PSX
  ignores it, so an opaque texture now exports `255` everywhere instead of a
  half-alpha black that could be mistaken for a blend. Batch and catalog exports
  have no primitive context and keep the conservative `128`. The 3D Debug tab
  shows the effective rule for the current selection.
- An enabled `developer_debug_start.ini` now also skips the intro movie, as if
  `-nointro` had been passed.

- Documented how to reconstruct explicit texture manifest entries from
  inspector-export filenames, including renamed upscaled variants.
- Developer-panel text wraps within the available window width. Texture
  exports use the full registered texture region, retain up to 4096 known
  textures independently of the mod-entry limit.
- Inspector PNG, TXT and car OBJ exports can be repeated at the same path.
  Completed nonempty temporary files replace the previous export; PNG output
  is decoded and dimension-checked before publication.
- Kept the previous `mods/hd_textures/manifest.ini` mapping as a compatibility
  fallback when no JSON mod manifests are present.
- Exporting a texture into an existing texture mod now appends a missing
  `(texture, texturePage, textureIndex)` registration instead of only reporting
  the entry to add. Unknown manifest fields, existing file references and user
  edits are preserved, repeated exports stay idempotent, and an unreadable
  `textures` array is reported without rewriting the manifest.
- Texture manifests and PNG overrides can now be reloaded for texture pages
  already present in the current level without changing original game assets.

- Rewrote the project README with fork scope, legal-data guidance, setup,
  build, run, debug, technology, and agent-workflow documentation.
- The F11 developer-panel hotkey now activates on key press and supports both
  SDL F11 identifiers.
- Project-owned PsyCross changes are distributed as an idempotent patch applied
  after submodule initialisation, avoiding a fork solely for this integration.
- The panel's texture-lookup explanation (`Explain texture lookup`) refreshes
  automatically when the selection changes, instead of describing the previous
  pick until the button is pressed again.

### Fixed

- Inspector model names are sanitized. Some name-table entries, notably
  sprite-only models, are not text; those characters used to reach inspector
  labels, catalog records and export file names and are now reported as an
  unnamed model.
- Car and pedestrian textures are now nameable in the inspector, so their
  textures export normally, through the batch export and in the catalog. The
  lookup required an exact texture-page/CLUT match against the level's named
  textures, but cars and pedestrians are drawn with runtime palettes that are
  never registered (measured: the car page holds 30 named textures and the
  player car's CLUT matched none of them). A named texture on the same page
  whose VRAM region contains the selection is now used as a fallback; the PNG
  still resolves from VRAM with the primitive's own CLUT, so the palette is
  preserved.
- Sprite scenery (trees and other billboards) now carries identity. It is drawn
  by `DrawSprites` as a subdivided mesh, which registered nothing, so a click
  resolved a single mesh triangle, reported an unsupported source and left the
  trunk unselectable. Sprites are registered as resource-backed objects
  (`sprite:` keys) with their model slot and placement, and the ground shadow
  stays outside the range.
- Buildings drawn from a streamed texture page now resolve a texture name. Only
  the level texture loaders registered page names, so a page assigned by the
  texture spool had none and every primitive from it reported an unregistered
  texture (measured: a facade on page 8 matched nothing at all, and the named
  texture count rose from 300 to 363 once the spool registered its pages).
- Clicking a source no producer registered no longer selects whatever is behind
  it. The registered-source preference that stops the atmospheric haze from
  shadowing the scene only outranks the nearest candidate when that candidate is
  a screen-space overlay, that is, carries no depth; real geometry keeps
  draw-order priority.
- The developer panel's Export section reports why a texture lookup succeeded or
  failed (`Explain texture lookup`), separating a page/CLUT mismatch, a region
  mismatch and the palette fallback.
- Debug-start snapshots now honour the saved `gameType` instead of forcing
  `GAME_TAKEADRIVE`, and a snapshot captured during a replay is not applied at
  startup (reproduce it with the generated `-replay` command).
- An absent `[render] textureOverrides` key no longer forces HD texture
  overrides on, so the developer panel toggle persists across launches.
- Timed screenshots are taken before the buffer swap, so `captureAfterSeconds`
  records the frame that was just rendered instead of the undefined back buffer.
- Inspector export detects a truncated output path and refuses to replace an
  existing but unreadable `manifest.json`; WIC initialisation no longer
  unbalances a pre-existing COM apartment.
- Fixed zero-byte PNG exports caused by rejecting the WIC encoder's BGRA
  pixel-format negotiation after opening the destination file. RGBA pixels
  are now explicitly converted to BGRA, retaining alpha and colour channels.
- Transparent PNG texture overrides no longer render black or incorrectly
  occlude geometry. Override draws now discard fragments below 0.5 alpha; the
  cutout is limited to active region overrides, so original PSX sampling,
  high-resolution fonts, and the fallback path keep their previous behaviour.
- Screenshots saved by `PsyX_TakeScreenshot` (F12 or `captureAfterSeconds`) are
  no longer vertically mirrored. `glReadPixels` rows are flipped before
  `SDL_SaveBMP`, matching the displayed orientation.
- The PsyCross `developer-overlay.patch` now checks out with LF line endings
  (`.gitattributes`), so a fresh Windows clone with `core.autocrlf=true`
  applies it instead of failing on the corrupted binary payload.
- `scripts/apply_psycross_patches.ps1` no longer hangs under Windows PowerShell
  5.1. With `$ErrorActionPreference='Stop'`, redirecting the stderr of a failing
  `git apply` to `$null` deadlocked; the combined output is captured instead.
- `windows_dev_prepare.ps1` skips dependencies that are already present,
  applies the PsyCross patch, and opens the solution at the path Premake
  actually generates (`src_rebuild/build/REDRIVER2.sln`).
- `linux_dev_prepare.sh` applies the PsyCross patch and changes into the
  directory Premake actually generates (`src_rebuild/build`), so a clean Linux
  checkout configures and builds. The Linux target (`release_dev_x64`,
  `debug_x64`) was verified to compile and to start and load a level with the
  WSLg D3D12/Mesa core context.

## [8.0] - 2026-07-02

Upstream baseline for this fork: OpenDriver2/REDRIVER2 commit `b2d8857`.

### Added

- High-resolution fonts and dynamic lighting from street lights.
- An additional car-model slot so all base, non-special car models can be
  visible at the same time.
- Restored lights for spooled objects and the birds previously present in
  Driver 1.
- 64-bit platform support and direct Linux execution on a 64-bit ABI.

### Fixed

- Cutscene car deviation in Car Bomb and Tanner's vertical position on Chicago
  lifting bridges.
- A film-director crash when changing the camera from a car to event objects.
- Civilian cars spawning outside roads, missing sirens, high-pitched horns,
  missing skid sounds, and selected texture artifacts.
- High CPU load in the main menu, plus minor bugs and regressions.

[Unreleased]: https://github.com/SimStm/REDRIVER2-Plus/compare/8.0...HEAD
[8.0]: https://github.com/OpenDriver2/REDRIVER2/releases/tag/8.0
