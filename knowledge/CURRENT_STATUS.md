---
type: Status
title: Project status
description: Source of truth for everything implemented in this fork and where each surface stands.
tags: [okf, status]
---

# Current status

> Updated: 2026-09-20
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
- `D32_SFLOAT` depth fallback, macOS/MoltenVK build, raw uncapped GPU
  throughput, the R5 shadow-quality comparison and a fresh `-opengl` parity run
  remain open.

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
  `developer_graphics.ini` (schema 6); Windows and Linux only.
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
