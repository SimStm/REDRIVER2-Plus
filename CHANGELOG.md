# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog 1.1.0](https://keepachangelog.com/en/1.1.0/),
and this project follows [Semantic Versioning](https://semver.org/spec/v2.0.0.html)
where release policy permits it.

## [Unreleased]

### Added

- The in-game modern-mesh system now runs on the Vulkan backend (renderer
  roadmap item 14, defect 3, Option A), so it is live in the default
  configuration instead of OpenGL-only. `PsyX_ModernMesh.cpp` dispatches every
  call to a new `PsyX_Vk_GameModernMesh*` module that owns its own load render
  pass, UBO, descriptor sets and pipelines; `vk_shaders/psx_modern.vert/.frag`
  reproduce the GTE-encoded vertex path (GL-to-Vulkan clip conversion, xyScale,
  camera transform) and the GGX/Schlick PBR fragment path, and
  `vk_shaders/fullscreen.vert` plus `psx_composite.frag` composite the modern
  pass over the PSX main pass with the scene-depth shadow term. The developer
  gallery no longer disables itself on Vulkan, so the F10 toggle and the eight
  GLB fixtures behave as on OpenGL. The shared `ModernUBO` layout must be
  declared identically in all three shaders; the vertex shader had been missing
  `ambientExposure`, which shifted `cameraPos` onto the wrong std140 slot and
  clipped every mesh away.
- An experimental native-Vulkan backend for the modern scene and a standalone
  developer window (renderer roadmap item 14, milestone R7, partial):
  `PsyX_Vk_*` in PsyCross owns an SDL window, swapchain, depth buffer, shadow
  map, PBR/depth pipelines and descriptor sets, with texture uploads, resize
  handling, RGBA readback and an ImGui overlay. It negotiates the newest API
  the loader supports (Vulkan 1.4.0 here) and resolves the loader at runtime
  through SDL, so the build needs the Vulkan headers only - no SDK, import
  library or validation layers. Run it with `-vkfixture` (add `-vkcapture N`,
  `-vkshot <path>` or `-vknogui`); it renders the same eight Meshy GLBs plus
  four analytic spheres as the OpenGL fixture. SPIR-V is embedded and
  regenerated with `scripts/compile_vk_shaders.ps1`. The fixture renders
  correctly with zero Khronos validation errors, and `-vkshot` writes an
  upright BMP, and the main render pass is created only after the surface
  format is negotiated (before that fix every draw in the pass was silently
  discarded). Work in progress towards rendering the game itself with Vulkan:
  the emulated PSX path is ported (`vk_shaders/psx.vert`, `vk_shaders/psx.frag`
  with the in-shader CLUT, texture window, dither and bilinear filter), the
  `R32G32_SFLOAT` VRAM image, the RG8 decode table and the five PSX blend
  pipelines are created, and `REDRIVER2_dev.exe -vkpsxtest` renders synthetic
  PSX quads and asserts the readback pixels (`(252,0,0,255)` for a 16-bit
  `0x001F` word, `(0,0,252,255)` for a 4-bit texture resolving to CLUT entry 5)
  with zero validation errors. The game still renders through OpenGL; the GR_*
  contract is mapped onto this path in the next phase.
  On macOS SDL resolves MoltenVK, so the same source is the portability path,
  but no macOS build has been produced or verified.
- The game window can now render through the Vulkan backend (renderer roadmap
  item 14/R7b, partial): `REDRIVER2_dev.exe -vulkan` maps the `GR_*` contract
  onto the native backend, so the in-game scene, HUD and minimap are presented
  by Vulkan while OpenGL stays the default and is run without the flag
  (`Release_dev_gl`). The PSX vertex capacity was raised to 65536 vertices (the
  previous 256 KiB buffer truncated roughly 11500 vertices per frame), the game
  texture-slot limit went from 64 to 512, and `GR_StoreFrameBuffer` now mirrors
  the presented frame into the PSX VRAM mirror so framebuffer-reading effects
  such as the sky lens flare sample it. Verified in an original Chicago launch
  under the Khronos validation layer with zero validation errors and a
  screenshot matching the OpenGL image; the `-playground` fixture and the
  `-vkpsxtest` PSX self-test still pass.
- A fixed fixture gallery for the experimental modern path (renderer roadmap
  item 14): all eight owned Meshy GLBs (six base-colour, two PBR) plus the four
  analytic material spheres are now placed in the same scene, each grounded on
  its own bounding box at the terrain height and laid out once from the spawn
  pose. The update now runs with the frame's final scene camera, which removes
  the slight swimming the imported mesh showed while the camera moved, and
  `PsyX_ModernMesh_SetInstanceWorld` gives the shadow pass the instance world
  transform so casters are rendered where they stand.
- Modern shadows now reach the existing scenery (renderer roadmap item 14,
  milestone R5 completion of the receiver side): the modern shadow map is
  projected over the already rendered legacy frame, so modern objects cast onto
  the road, walls and trees as well as onto themselves. The pass copies the
  scene depth (`GL_DEPTH24_STENCIL8`), reconstructs world positions through the
  inverse `Projection3D` and the inverse camera view, applies a 3x3 PCF term,
  and is gated by the existing shadow toggle. Diagnostic modes for the pass are
  selectable with the `7` key or `shadowdebug=` in `developer_modern_mesh.ini`
  (`1` depth, `2` shadow-volume membership, `3` shadow map, `4` receiver vs
  caster depth). The sun default now faces the follow camera so cast shadows are
  visible without rotating the light.
- A classic/enhanced renderer switch and the R6 Forward-path decision (renderer
  roadmap item 14): the developer Graphics panel gains an **Enhanced renderer
  (modern meshes/PBR)** checkbox plus **Modern shadows** and **Modern ambient
  occlusion** toggles, persisted in `developer_graphics.ini` (schema 6) and also
  reachable with the F10 key live without reloading. The reference Forward path
  is retained as the initial production choice with the recorded rationale.
- Lighting effects for the experimental modern path (renderer roadmap item 14,
  milestone R5, partial): a light set of up to eight directional/point lights
  with colour, intensity and range, ambient and exposure; a 2048x2048
  directional shadow map whose casters are the modern meshes and whose receivers
  are both the modern meshes and the legacy scene (see the shadow-projection
  entry above); glTF emissive factors and maps; and a normal-based
  ambient-occlusion approximation as its own toggle.
  In game `[`/`]` rotate the sun, `;`/`'` change elevation, `-`/`=` change
  exposure, `0` toggles shadows and `9` toggles ambient occlusion; values are
  persisted to `developer_modern_mesh.ini`.
- A reference Forward PBR path for the experimental modern meshes (renderer
  roadmap item 14, milestone R4, partial): the new shader shades
  metallic/roughness with GGX + Schlick Fresnel, one directional light, an
  ambient term and an exposure multiplier, decoding base colour sRGB->linear and
  re-encoding on output. It binds the imported normal map (tangent frame
  reconstructed from screen-space derivatives) and the metallic/roughness map
  (G/B). Two Meshy PBR fixtures (`bollard_pbr.glb`, `oil_barrel_pbr.glb`, 20
  credits) plus four procedural analytic material spheres render with visible
  metal/dielectric response; light direction, intensity, ambient and exposure
  are set from `developer_modern_mesh.ini`. Real-time light control and
  shadows/AO remain R5.
- A bounded glTF 2.0 / GLB static-asset import path (renderer roadmap item 14,
  milestone R3, partial): `utils/GltfLoader.*` reads one mesh primitive
  (float positions/normals/UVs, 16/32-bit indices) and one
  `pbrMetallicRoughness` material with an embedded base-colour image decoded
  through WIC, and the new `PsyX_ModernMesh_CreateEx` draws it through the
  shared-depth modern path. `DeveloperModernMesh` selects a fixture from
  `developer_modern_mesh.ini` (`asset=...`) and keeps the synthetic cube as the
  fallback. Six owned Meshy fixtures (Meshy 6 preview + texture, 180 credits)
  are retained under `assets/modern_fixtures/` with `provenance.md`; verified in
  the playground with `bollard.glb` (2220 verts) and `wooden_crate.glb`
  (1822 verts). PBR maps and lighting remain R4.
- An experimental hybrid modern-mesh path (renderer roadmap item 14, milestones
  R1/R2, partial): PsyCross now exposes a small C-compatible persistent
  VAO/VBO mesh API (`PsyX_ModernMesh_*`) that draws unlit vertex-coloured
  geometry through the legacy `Projection3D` in the shared colour/depth buffer,
  and the game owns a synthetic static fixture
  (  `utils/DeveloperModernMesh.*`). In the playground and in an original Chicago
  launch the fixture mutually occludes with the legacy scene (car, floor,
  skyline and street geometry); a persisted `developer_modern_mesh.ini` flag
  (toggle with `F10`) restores the exact legacy-only baseline. The path is a
  no-op on non-OpenGL targets and is carried by the project PsyCross fork. R3/R4
  asset/PBR work and the initial hardware budgets remain open.
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
  guards (Windows and Linux), and the renderer changes live in the project
  PsyCross fork.
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

- The native Vulkan backend is now the default renderer for the game (renderer
  roadmap item 14/R7b). It had been opt-in behind `-vulkan` while OpenGL stayed
  the default "until parity is proven"; that condition now holds - same image,
  resize, readback, primitive-mask stencil, offscreen render-to-VRAM and VRAM
  export, zero validation errors, and measured 30 FPS equality with OpenGL - so
  the default is flipped. OpenGL remains fully selectable as the fallback with
  the new `-opengl` flag, `-vulkan` is still accepted, and a machine whose
  driver exposes no usable Vulkan device falls back to OpenGL automatically at
  startup. PSX, Android and Emscripten builds keep OpenGL as their default
  because the backend is compiled out there.
- The emulated-PSX primitive mask bit is now implemented on the Vulkan backend
  (renderer roadmap item 14/R7b): the main pass depth attachment prefers a
  stencil-capable `D24_UNORM_S8_UINT` format, cleared to zero every frame, and
  each PSX blend mode gains a mask-set pipeline variant. `GR_SetStencilMode`'s
  OpenGL semantics are reproduced exactly: the mask-bit draw (`DrawPrim`,
  `overmap.c` map tiles, `loadview.c`) always passes and writes stencil bit 4,
  while every other draw only passes where that bit is clear. On a driver
  without a combined depth-stencil format the mask degrades to a no-op, as
  before. Verified in an original-city Vulkan launch with zero validation
  errors and the minimap/map overlay drawn correctly.
- The emulated-PSX offscreen render-to-VRAM target is now implemented on the
  Vulkan backend (renderer roadmap item 14/R7b): `PsyX_Vk_GameSetOffscreen` /
  `PsyX_Vk_GameResolveOffscreen` render the draws queued by
  `GR_SetOffscreenState(enable)` into a colour-only target, read them back and
  pack them into the PSX VRAM mirror before the frame is submitted, so effects
  that sample a `dfe=0` region in the same frame (the Tanner pedestrian shadow)
  see it. Groups are bound to the double-buffered frame index, matching the
  game's one-frame-ahead display-list build. The `-vkpsxtest` PSX self-test now
  also covers the offscreen path (a 32x32 quad rendered offscreen and resolved
  into VRAM at (64,64)).
- The PsyCross submodule now tracks this project's own fork
  (`git@github.com:SimStm/PsyCross.git`) instead of the upstream repository, so
  project-specific changes are committed and reviewed there and recorded by the
  parent gitlink. The `patches/psycross/` delta and
  `scripts/apply_psycross_patches.ps1` were removed, and the Windows and Linux
  prepare scripts now initialise the submodule instead of applying a patch.
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

- Image-streaming UI (the overhead map above all) drew from the wrong data on the
  Vulkan backend. The backend records the whole frame's PSX draws at present
  time while the OpenGL renderer executes each `DrawSync` flush immediately, so
  (1) every `GR_UpdateVertexBuffer` wrote from offset 0 and overwrote the
  previous flush's vertices, and (2) every draw sampled the frame's final VRAM
  contents, making all sixteen recycled map tile slots hold the last batch.
  Vertex uploads now append (the buffer keeps four flushes, each draw is offset
  by its upload's base) and `GR_CopyVRAM` queues its rectangle with the pixels so
  the writes are replayed in flush order while the draws are recorded. The map
  screen now draws its tiles and labels; full parity with OpenGL is tracked in
  `knowledge/roadmap/planned/vulkan-ui-image-parity.md`.
- The loading screen was black on the Vulkan backend and its progress bar was
  lost. The PSX loading path draws the art once (`ShowLoadingScreen`) and then
  redraws only the bar (`ShowLoading`) while the level streams in; the OpenGL
  renderer keeps the art because it only clears when the draw environment has
  `isbg` set, but the Vulkan main pass always used `LOAD_OP_CLEAR` and the
  `clearRequested` flag written by `GR_Clear` was never read. The main pass
  colour attachment now uses `LOAD_OP_LOAD` with a `PRESENT_SRC` initial layout
  and clears with `vkCmdClearAttachments` only when the game asked for it (or
  when a swapchain image is used for the first time), so the loading art
  persists exactly as on OpenGL. `-vkpsxtest` still passes with unchanged
  pixel assertions.
- The HD-texture override preview in the developer graphics panel showed
  nothing on the Vulkan backend because the ImGui Vulkan backend needs a
  `VkDescriptorSet`, while the texture id is a slot index there. A new
  backend-aware accessor (`PsyX_GetOverlayTextureId`,
  `PsyX_GetRGBATextureSize`) returns a cached
  `ImGui_ImplVulkan_AddTexture` descriptor set on Vulkan and the `GLuint` on
  OpenGL, and the panel uses it with the texture's real aspect ratio.
- The Vulkan game path recreated only the swapchain handed to the compositor on a
  window resize; the framebuffers, swapchain image views and depth attachment
  built for the old extent were left alive and then shadowed by the new ones, so
  every resize leaked them and left the newly created framebuffers referencing
  stale depth/colour attachments. Resize and out-of-date acquire now run a single
  `RecreateSwapchain()` that waits idle, tears down the framebuffer/view/depth
  resources before the swapchain, and rebuilds at the new extent; the readback
  buffer and the new extent are logged (`swapchain recreated WxH images=N`). A
  live session resized through 1024x768, 800x600, 1600x900 and 1280x720 keeps
  presenting at ~30 FPS with zero validation errors.
- The Vulkan game path generated mip chains for PSX `GR_CreateRGBATextureMipmapped`
  textures with whole-image layout barriers. Uploading left mip 0 as
  `TRANSFER_DST`, then a single image-wide barrier forced every level to
  `TRANSFER_SRC` before the next level was written as `TRANSFER_DST`; the
  generated mips were also never moved to `SHADER_READ_ONLY`. Validation
  reported `VUID-VkImageMemoryBarrier-oldLayout-01197`,
  `VUID-vkCmdBlitImage-srcImageLayout-00221` and `VUID-vkCmdDraw-None-09600`
  (30 messages, capped by the duplicate limit) on every run that loaded such a
  texture. Barriers are now per mip level and the final transition covers every
  level; a Vulkan game run reports zero validation errors.
- The VRAM TGA export (`GR_SaveVRAM`, bound to F10 and used by the debug VRAM
  dump) wrote a 18-byte header-only file on the Vulkan build: the pixel loop was
  compiled out unless `USE_OPENGL`, and the Vulkan build reports that as 0. The
  writer only reads the CPU VRAM mirror, which both backends keep current, so it
  is now backend-agnostic (18 + 1024x512x2 = 1,048,594 bytes). The `-vkpsxtest`
  self-test exports the image and asserts the exact byte count so this cannot
  silently regress.
- The Vulkan game path used the PSX clip rectangle as-is for its scissor, but
  `GR_SetupClipMode` hands over GL's bottom-left convention. Every clipped HUD
  element therefore tested against a vertically mirrored region; the minimap
  (a 63x60 clip in the bottom-right corner) was dropped entirely. The scissor Y
  is now mapped to Vulkan's top-left origin like the viewport.
- The experimental Vulkan backend drew nothing: the main render pass was
  created before the swapchain negotiated its surface format, so its colour
  attachment format was `VK_FORMAT_UNDEFINED` and every recorded draw was
  silently discarded. The surface format is now negotiated before the render
  passes. `PsyX_Vk_ReadbackRgba` also flipped rows despite documenting a
  top-left origin, which made `-vkshot` screenshots upside down.
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
