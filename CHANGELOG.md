# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog 1.1.0](https://keepachangelog.com/en/1.1.0/),
and this project follows [Semantic Versioning](https://semver.org/spec/v2.0.0.html)
where release policy permits it.

## [Unreleased]

### Added

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

### Changed

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
