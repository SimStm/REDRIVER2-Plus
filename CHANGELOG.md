# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog 1.1.0](https://keepachangelog.com/en/1.1.0/),
and this project follows [Semantic Versioning](https://semver.org/spec/v2.0.0.html)
where release policy permits it.

## [Unreleased]

### Added

- A resident procedural playground scene (roadmap item 13, milestones P1-P5)
  that reuses the existing renderer and original vehicle simulation: a
  generated flat drivable surface, collidable box obstacles, disabled donor
  traffic/police/missions and camera events, a car reset control, safe return
  to the frontend, a `-playground` developer launch path, and a **Playground**
  choice on the Take a Ride city screen that starts the same launcher from every
  build. A loaded original level only supplies car, texture and sound resources;
  its world geometry, roads and collisions are replaced. The stable fixture
  identity is `playground.flatpad.v1`.
- Batch texture export from the 3D Debug tab (roadmap item 05, milestones 1-5): one action exports the currently identified texture list, deduplicating by the exact `(name, texturePage, textureIndex)` identity so a texture shared by several models is published once, and reports exported, duplicate-omitted and failed counts with the last failure named. New manifest entries carry a backward-compatible `modelReferences` array and a readable, sanitized model filename suffix derived from the asset catalog, falling back to an explicit `slot<index>` / `unknown:model-slot:<index>` form when no catalog record exists; model references are metadata and never change the override key. Re-exporting writes the PNG to the manifest's existing mapped `file` and merges new references into the same entry, preserving unknown fields and never appending a duplicate registration, and a separate catalog-scoped action exports every material linked to the selected source model, including hidden faces and its high-detail LOD sibling, resolving the VRAM region of materials that are not on a submitted triangle and naming any missing adapter (palette variants and cross-model child parts are not enumerated yet). Large batches run cooperatively across frames with a progress bar, cancel control, per-resource results and retry of failed resources, publishing each file independently so cancelling keeps what was already written.
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
- Exported textures encode the PSX STP semi-transparency flag as half alpha
  (`128`) instead of opaque, so semitransparent texels survive the
  export/re-import round trip and blend on semitransparent primitives.
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

### Fixed

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
