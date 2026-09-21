---
type: Status
title: Project status
description: Source of truth for everything implemented in this fork and where each surface stands.
tags: [okf, status]
---

# Current status

> Updated: 2026-09-21
> Role: **source of truth for what has been implemented in this project.**
> Add a short, factual entry here whenever you finish implementing something, so a
> new session can see the whole surface without reading every change record.
> [`RECENT_CONTEXT.md`](RECENT_CONTEXT.md) complements this file: it holds the
> last interactions in detail, while this file holds the cumulative picture.
> Confirm anything you act on against the source and `git status` - this is a
> summary, not evidence.

## What this project is

REDRIVER2 is a clean-room C/C++ reimplementation of the PlayStation game
*Driver 2*, not an emulator. Game logic was reconstructed from symbols and
disassembly; PsyCross provides the PSX/Psy-Q compatibility layer (rendering,
input, audio, CD, GTE) and the desktop platform integration.

- Game code: `src_rebuild/Game/` (`C/`, `engine/`, `Frontend/`, `ASM/`).
- Backend/platform layer: `src_rebuild/PsyCross/` - a Git **submodule pinned to
  the project fork** `git@github.com:SimStm/PsyCross.git` (tracked branch
  `origin/master`). The fork, never `OpenDriver2/PsyCross`, is the build
  source: it carries the Vulkan backend and MoltenVK/portability work upstream
  lacks. Project-specific renderer changes are committed there and recorded by
  staging the gitlink in the parent (`knowledge/rules/psycross-fork.md`).
- Developer tools: `src_rebuild/utils/`, `scripts/`, `PSXToolchain/`.
- Runtime data: `data/` (local game assets; do not modify or redistribute).
- Build generation: Premake 5 (`src_rebuild/premake5.lua`,
  `premake5_psycross.lua`); generated output is not tracked.
- Targets: Windows, Linux, Emscripten/web, Android NDK, PSX. `GAME_REGION` is
  `NTSC_VERSION` by default.

## Renderer

### Vulkan game renderer (default on desktop)

`redriver2_psxpc.cpp` defaults `useVulkan = 1` on desktop; `-opengl` opts out,
`-vulkan` is still accepted, and PSX/Android/Emscripten keep OpenGL. The
`GR_*` contract maps onto `PsyX_Vk_Game*` in `PsyX_Vk.cpp`.

Delivered in order, each verified on the game window:

- PSX path: `psx.vert`/`psx.frag` reproduce the emulated PSX pipeline (GL-to-
  Vulkan clip conversion, in-shader CLUT, texture window, dither, bilinear
  filter), with an `R32G32_SFLOAT` VRAM image over the CPU `unsigned short vram`
  mirror and five blend pipelines. `-vkpsxtest` renders synthetic PSX quads and
  asserts the readback (`16-bit worst=0`, `4-bit CLUT worst=0`, offscreen
  samples, `vram export 1048594/1048594`).
- Game path: vertex capacity raised to 65536, game texture slots 64 -> 512, the
  scissor Y mapped to Vulkan's top-left origin (restored the minimap).
- Framebuffer persistence: the main pass colour attachment uses `LOAD_OP_LOAD`
  with a `PRESENT_SRC` initial layout and clears via `vkCmdClearAttachments`
  only when `GR_Clear` asked for it. This preserves each acquired image; it
  does not explicitly copy the immediately preceding presented image.
- Depth/stencil: `D24_UNORM_S8_UINT` preferred; the PSX primitive mask bit is
  implemented as per-blend-mode stencil-write pipeline variants.
- Depth writes require the depth test, matching OpenGL. Stencil capability now
  survives PSX resource initialization; reference/write/compare masks and
  operations match `GR_SetStencilMode`, including depth-disabled draws. Both
  main and resumed passes store stencil, with compatible dependencies.
- Untextured PSX primitives use an explicit white texture selector, reproducing
  GL's decoded 0xffff colour/alpha independently of VRAM. Game rendering prefers
  UNORM presentation for display-space blending and matches GL's constant alpha.
- Expanded `-vkpsxtest` covers the white-texture bridge, all five blend modes,
  stencil across two pass restarts and several partial presentations. The latest
  Windows run passed; the partial test does not guarantee every swapchain image
  was acquired, so it is not proof of general previous-frame persistence.
- Offscreen render-to-VRAM (`GR_SetOffscreenState`) closes the Tanner shadow;
  `GR_StoreFrameBuffer`/`GR_VkMirrorFrameToVRAM` feed framebuffer-reading
  effects such as the sun lens flare; `GR_SaveVRAM` (F10) works on both backends.
- Flush-order correctness: vertex uploads append (each draw is offset by its
  upload's base) and `GR_CopyVRAM` queues its rectangle plus pixels
  (`PsyX_Vk_GameCopyVRAM`), replayed in generation order while draws are
  recorded, closing/reopening the main pass around each replay. This is what
  makes the streaming overhead map draw correctly.
- Modern mesh on Vulkan: `PsyX_ModernMesh.cpp` dispatches to
  `PsyX_Vk_GameModernMesh*`; `psx_modern.vert/.frag`, `fullscreen.vert` and
  `psx_composite.frag` reproduce the GTE-encoded vertex path and the GGX/Schlick
  PBR path, composited over the PSX pass with the scene-depth shadow term. The
  shared `ModernUBO` (std140) layout must match in all three shaders.
- Modern scene-depth copies transition both depth/stencil aspects and prepare
  the destination transfer layout. A later enhanced-path run exposed VUIDs
  03320/09600; commit `647ea0b` corrects the missing transitions. The subsequent
  inspected enhanced run had the validation layer enabled and no error/VUID.
- Resize: both the resize and out-of-date paths go through
  `RecreateSwapchain()` (framebuffers, views and the depth image are rebuilt).
- Backend-agnostic opt-in perf log (`PSYX_PERF_LOG` -> `psyx_perf.log`) measured
  OpenGL and Vulkan at 30.0 FPS / 33.4 ms with identical vertex and draw counts,
  i.e. the game is PSX-timestep-bound rather than GPU-bound.
- Renderer-debt closure (2026-09-20): the stencil-less `D32_SFLOAT` fallback is
  selectable with `PSYX_VK_DEPTH_FORMAT=d32` and the self-test then asserts the
  mask-bit no-op degradation; a fresh classic-renderer `-opengl` parity capture
  differs from Vulkan by mean 4.08/255 with 0.22 % of channels > 16; the R5
  shadow comparison measured Vulkan 7722 vs OpenGL 2264 changed pixels in the
  sampled ground band (IoU 29.2 %), with OpenGL additionally darkening the
  legacy tree canopy. See
  [`changes/2026-09-20/vulkan-renderer-debt`](changes/2026-09-20/vulkan-renderer-debt/index.md).
- Previous-frame history was investigated, not implemented: `LOAD_OP_LOAD`
  preserves each swapchain image individually, and sampled natural
  partial-update frames (loading art + bar via a `ShowLoading` breakpoint)
  showed complete, identical art with a progressing bar. No natural corruption
  reproduced.
- Experimental standalone Vulkan developer window: `-vkfixture` (with
  `-vkcapture N`, `-vkshot <path>`, `-vknogui`) renders eight Meshy GLBs plus
  four analytic spheres; SPIR-V is embedded and regenerated with
  `scripts/compile_vk_shaders.ps1`.

### Modern mesh / renderer modernization

- Persistent C-compatible mesh API (`PsyX_ModernMesh_*`) drawn through the
  legacy `Projection3D` in the shared colour/depth buffer, so modern meshes
  occlude with the legacy scene; F10 toggles it.
- Reference Forward PBR (R4), up to eight lights, 2048x2048 shadow map whose
  casters and receivers include the legacy scene (R5), normal-based ambient
  occlusion, and the classic/enhanced switch persisted in
  `developer_graphics.ini` (`developer_modern_mesh.ini` holds the mesh tuning).
- Legacy lighting receptivity (2026-09-20, refined 2026-09-21): the modern light
  set now also shades the already-rendered legacy scene on both backends. The
  composite samples a copy of the framebuffer colour (a blend cannot brighten
  past the fixed-point clamp), reconstructs a normal from a five-tap cross of
  the neighbour world positions with an edge test, and applies
  `colour * (1 + strength * N·L * light)`, gated to `depth < 0.999` and faded
  out with distance at `2x`-`4x` the shadow volume half-size. The shadow volume
  and fade centre follow the **player vehicle**, not the spawn; the light set is
  pushed every frame and falls back to the camera when `hd.where.t` is absent
  during level/mission transitions. `legacyLighting` (default 0) and
  `legacyLightReceptivity` (default `0.350`) persist in
  `developer_modern_mesh.ini`; the Graphics panel, its **Modern sun and look**
  group (azimuth, height, intensity, ambient, exposure, shadow size, debug view)
  and F9 control it. Measured `+10%..+34%` on legacy surfaces depending on the
  sun angle, sky unchanged; see
  [`product/legacy-lighting-receptivity.md`](product/legacy-lighting-receptivity.md)
  and [`changes/2026-09-21/lighting-and-panel-followup`](changes/2026-09-21/lighting-and-panel-followup/index.md).
- The composite needs both PGXP options on: PGXP texture mapping writes the
  per-vertex depth (otherwise every legacy vertex takes the 2D path at a
  constant depth) and PGXP Z-buffer gates depth writes. The Graphics tab states
  this on both checkboxes.
- World/overlay composition (2026-09-21): the earlier forced 2D-depth fix was
  replaced after user testing exposed opaque holes behind translucent menus,
  flare rectangles and angle-dependent scene loss. The game marks OT bucket 10
  as the final-overlay boundary; PsyCross splits even a state batch crossing
  that exact vertex, composes the legacy world and modern meshes, then blends
  lens flare, fades and UI over the result. No draw is classified by its first
  vertex's `scr_h`, and legacy depth behaviour is preserved. OpenGL restores
  texture bindings on units 0..4 and the active unit before legacy drawing
  resumes; Vulkan preserves VRAM generations and resumes its load pass for the
  overlay tail. This boundary is supplied by the single-view gameplay path;
  split-screen retains the previous end-of-frame fallback.
  Both Windows builds and the expanded Vulkan blend/readback test pass; live
  pause transparency and four Vulkan camera orientations were inspected.
  See [composition regression](changes/2026-09-21/modern-overlay-composition/index.md).
- Bounded glTF 2.0/GLB importer (`utils/GltfLoader.*`) with six owned Meshy
  fixtures under `assets/modern_fixtures/` plus `provenance.md`.

### Unresolved renderer items

- **UI follow-up (2026-09-20)**: corrected white primitives, stencil and blend
  state after the user's repeat tests disproved the previous completion claim.
  Live Vulkan map no longer shows world shadows over it; loading bar, compass,
  Damage/Felony and police flash/cones were observed. Controlled GL/Vulkan HUD
  captures and natural CloseShutters at h=96 confirm restored elements; small
  pixel differences remain (not pixel-exact parity). Fork commit `d9d8628`; see
  [`product/vulkan-ui-image-parity.md`](product/vulkan-ui-image-parity.md).
- Khronos validation is installed (`G:\VulkanSDK\1.4.357.0`) and automatically
  enabled by the backend. It exposed incompatible resumed render passes
  (`VUID-vkCmdDraw-renderPass-02684`), now fixed. The subsequent inspected game
  debug output confirmed the layer was enabled and contained no VUID/error.
- Open after the 2026-09-20 closure: macOS/MoltenVK build, raw uncapped GPU
  throughput and CPU/GPU frame-time profiling. The `D32_SFLOAT` fallback,
  fresh `-opengl` parity run and R5 shadow comparison now have executed evidence
  (see above); the shadow receive still differs between backends and is an R5
  follow-up. Legacy lighting receptivity is implemented
  ([`product/legacy-lighting-receptivity.md`](product/legacy-lighting-receptivity.md));
  it adds one full-screen colour copy plus draw only while its composite is
  active, and no frame-time delta on the minimum GPU class is recorded yet.

## Textures, mods and the inspector

- HD texture mods: discoverable JSON mods under `mods/<id>/manifest.json` with a
  deterministic `mods/enabled.json` load order; entries match on
  `(texture, texturePage, textureIndex)`, may carry a `clut` for palette
  variants, and `modelReferences` as metadata that never changes the key.
- Opt-in proportional override alpha (default off) makes `BM_AVERAGE` blend the
  imported PNG alpha instead of the binary 0.5 cutout; opaque, additive and
  subtractive draws keep the compatibility cutout and the CPU picker mirrors the
  active mode.
- Source-aware asset catalog (`AssetCatalog.*`): stable resource identity
  separate from runtime instances, explicit provenance, many-to-many model to
  texture/LOD links, and invalidation when slots are reused or the level
  changes. Covered by the standalone `AssetCatalogTests`.
- 3D Debug tab: click-based primitive inspection with Face/Material/Component/
  Logical-object scopes, catalog context, override previews, detail TXT export,
  single and batch PNG export (deduplicated by identity, cooperative across
  frames with progress and retry), and a live page-registration cost diagnostic.
- Diagnostic tooling: configurable selection highlighting, draw-source labels,
  live car-slot positions, zero-TIM flicker diagnostics and alpha-semantics
  notes in `knowledge/product/`.

## Gameplay and developer tooling
- Resident procedural playground (roadmap item 13, P1-P5): generated flat
  surface, collidable boxes, donor traffic/police/missions disabled, car reset,
  safe return to the frontend, a `-playground` launch path and a **Playground**
  entry on the Take a Ride city screen. Fixture id `playground.flatpad.v1`.
- Debug start snapshots: `developer_debug_start.ini` restores mission, city,
  vehicle, position, heading, player count and chase at startup; the panel's
  **Reproduce this state** generates the command line; `scripts/run_debug_start.ps1`
  captures scenes with overrides on and off.
- Developer Graphics Panel (F11) with Dear ImGui, persisted to
  `developer_graphics.ini` (schema 8); Windows and Linux only.
  - Schema 7 added live checkboxes for `dynamicLights`, `widescreenOverlays` and
    `fastLoadingScreens` (runtime-settings-gui milestone 2, 2026-09-20). Each
    help marker names the equivalent `config.ini` key; `config.ini` stays the
    shipped default and the panel file, applied afterwards, wins. Reader/apply
    verified by an overlay-corner capture A/B, writer verified by a settings
    save.
  - Schema 8 added the **display mode** (milestone 3): a Fullscreen (desktop)
    checkbox and a Window size combo (current, 1280x720, 1600x900, 1920x1080)
    through the new `PsyX_ApplyWindowMode`, with a 15 s keep/revert countdown
    that survives closing the panel (`UpdateDisplayRevert` runs before the
    visibility check), so a bad mode always reverts. Verified: a persisted
    1600x900 mode captured a 1600x900 frame; a provisional 1024x600 change kept
    through `ConfirmDisplayMode` persisted 1024x600, and the same change left
    unconfirmed reverted to 1600x900 without touching the file; a pick at the
    same relative point resolved a primitive at both sizes (the viewport and
    picker read the live window size). The fullscreen branch is not
    runtime-tested (it would change the machine's display).
  - Panel follow-up (2026-09-21, after user testing): the display confirmation is
    now its **own ImGui window** anchored to the bottom-left of the applied
    display size, drawn whether or not the panel is open and after it, with the
    cursor shown and input captured while armed - the old in-panel banner was
    invisible when the panel was closed or clipped by the mode change.
    `CheckboxWithHelp` puts the `(?)` at the end of every checkbox label (its
    own line only when the label fills the line), the new sun sliders follow the
    same rule, the Input tab table is `Action | Binding | Also bound to | Reset`
    with the conflict note in its own wrapping column, and the Graphics tab
    exposes the whole `developer_modern_mesh.ini` light set under **Modern sun
    and look**. See
    [`changes/2026-09-21/lighting-and-panel-followup`](changes/2026-09-21/lighting-and-panel-followup/index.md).
- Input-binding editor (runtime-settings-gui milestone 4, 2026-09-20): an
  **Input** tab in the same panel edits the game and menu keyboard/controller
  tables with click-to-capture, Escape/right-click/Cancel/10 s cancellation,
  conflict markers and per-action/per-device reset. `DeveloperInputMapping.*`
  owns the model; `SwitchMappings` reports the active table so an edit only
  reaches the live mapping when it belongs to it, and input is held for the
  duration of a capture. Verified with synthetic SDL events through the panel's
  event handler: a captured `Q` bound game Cross while the live menu mapping
  stayed `RETURN`, conflicts reported both actions, Escape and timeout
  cancelled, and reset restored the `config.ini` value (`Up`).
- Runtime settings GUI completed (milestone 5, 2026-09-20): the Game Debug tab
  gained a **Content and language** section (content override, Chicago bridges,
  language, Driver 1 music - each stating its reload/restart requirement - and
  the restart-only Free camera disabled with the reason via the new
  `gFreeCameraConfiguration`). Persistence moved onto the shared
  `DeveloperSettingsFile_WriteKeys` writer, which preserves unknown keys and
  comments, removes keys the owner no longer wants and writes through
  `.tmp`/`.bak`, and bindings now persist in `developer_input.ini` as overrides
  only. Verified: a hand-added comment and unknown key survived a graphics save
  while missing keys were appended; bindings round-tripped
  (`input-save=1 input-load=1 cross-default=82 cross-after-load=20`), a reset
  removed the override line, and the Input tab rendered the seeded override.
- MCP-assisted agent workflow (2026-09-20): `AGENTS.md` now requires checking
  for configured MCP servers first - context7 for library documentation,
  visual-studio-ide-mcp for build/debug of `src_rebuild/build/REDRIVER2.sln`
  (`Release_dev_gl` OpenGL, `Release_dev` Vulkan), and desktop-control MCPs
  (computer-control-mcp, windows-mcp) for navigating the running game and
  screenshots. Terminal game launches are fire-and-forget only, with log and
  screenshot polling instead of waiting on the process handle.
- Structural code-quality rules (2026-09-20): a mandatory **Code quality and
  structure** section in `AGENTS.md` governs new code and maintenance edits -
  readable idiomatic C++, no spaghetti code or unnecessary global state,
  separated responsibilities, small cohesive functions, no premature
  abstractions, no out-of-scope refactors, explicit error handling, and a
  self-review pass for mixed responsibilities and coupling.

## Knowledge process

- `knowledge/` is an OKF bundle: `rules/` (repeatable constraints), `product/`
  (how a surface works), `discussions/` (evolving topics with evidence),
  `roadmap/planned|done` (planning vs completion records), `changes/<date>/`
  (why a change shipped).
- This file (`CURRENT_STATUS.md`) is the cumulative implementation summary;
  `RECENT_CONTEXT.md` is the session-to-session handoff.
- `CHANGELOG.md` holds release-oriented `## [Unreleased]` entries; the baseline
  is upstream `8.0` at `b2d8857`.

## Validation commands that actually exist

| Command | What it proves |
| --- | --- |
| VS build `Release_dev\|x64` (or `premake5` + `gmake2`) | the tree compiles |
| `REDRIVER2_dev.exe -vkpsxtest` | Vulkan PSX readback, CLUT, offscreen, VRAM export |
| `REDRIVER2_dev.exe -vkfixture -vkcapture N -vkshot <bmp>` | modern fixture render + capture |
| `REDRIVER2_dev.exe -opengl` / `-vulkan` | backend selection still works |
| `pwsh -NoProfile -File scripts/run_inspector_tests.ps1` | `AssetCatalogTests`, `InspectorExportTests` |
| `git diff --check` | whitespace-level hygiene |
| `git -C src_rebuild/PsyCross status` | fork and gitlink state |
