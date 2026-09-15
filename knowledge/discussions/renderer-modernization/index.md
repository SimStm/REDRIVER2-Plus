---
type: Discussion
title: Renderer modernization while retaining PsyCross compatibility
status: exploring
created: 2026-09-15
updated: 2026-09-15
tags: [discussions, architecture, rendering, psycross, pbr, modding]
---

# Renderer modernization while retaining PsyCross compatibility

## Context and decision status

The user wants to explore modern rendering pipelines, modern meshes, PBR
materials, custom lighting, Vulkan/Metal, custom models/maps, and menu changes.
The question is whether PsyCross must be removed to enable this direction.

**Current recommendation:** preserve useful PsyCross compatibility services and
introduce a richer scene submission path alongside the legacy graphics path.
Refine graphics backend boundaries incrementally. Replacing the whole
compatibility layer is not a prerequisite for these goals.

The user authorized documenting this discussion and maintaining such records
in future conversations. The user requested a recommended sequence **before**
creating a roadmap entry. No rendering implementation, experiment execution,
backend selection, pipeline selection, schedule, or platform expansion has
been authorized by this record. The sequence below remains a recommendation.

## Reading guide

- [Recommended investigation and development sequence](exploration-sequence.md)
- [Proposed architecture image](diagrams/proposed-architecture.png)
- [Exploration sequence image](diagrams/exploration-sequence.png)
- [Editable diagram generator](diagrams/render_diagrams.py)

## Evidence from the current checkout

Inspected on 2026-09-15: parent HEAD `8058c807`, PsyCross HEAD `e56e4cd`.
The PsyCross working tree contains local integration changes and vendored
ImGui. The parent tree also has unrelated data and ignore-file changes.
These findings describe inspected files, not a clean upstream checkout or
new runtime validation. Recheck the relevant source when implementation starts.
All source links below are relative to this subject directory.

- [Premake](../../../src_rebuild/premake5.lua) compiles game C sources as C++,
  links PsyCross and platform dependencies, and defines platform paths.
  Linux explicitly selects C++11. Windows/Linux, web, and Android paths must
  not be assumed equally validated; macOS is not established as a tested
  game target merely because Metal is a candidate.
- [PsyCross](../../../src_rebuild/PsyCross/README.md) provides Psy-Q-compatible
  graphics, geometry, audio, controller and CD interfaces. Desktop graphics
  use OpenGL, window/input use SDL2, and SPU-AL audio uses OpenAL.
- [Game drawing](../../../src_rebuild/Game/C/draw.c), including
  `PlotBuildingModel`, transforms geometry through GTE and builds PSX
  primitives before the renderer sees the result.
- [GPU translation](../../../src_rebuild/PsyCross/src/gpu/PsyX_GPU.cpp),
  `ParsePrimitivesLinkedList` / `DrawAllSplits`, turns primitive streams into
  batches and ultimately calls `GR_UpdateVertexBuffer` / `GR_DrawTriangles`.
- [GrVertex and GR interfaces](../../../src_rebuild/PsyCross/include/PsyX/PsyX_render.h)
  provide position/projection data, UVs, colours and texture-page/palette
  information. `GrVertex` has no normal, tangent, or PBR material attribute.
  Headers, shader compilation, resource IDs and context setup remain coupled
  to OpenGL; the existing functions are a starting seam, not a backend-neutral
  rendering abstraction ready for Vulkan.
- [MODEL](../../../src_rebuild/Game/engine/mdl.h) includes normal and collision
  fields. Absence of normals in the final vertex stream does not mean the
  original asset has none. Recoverability and quality vary by source path.
- [PGXP data](../../../src_rebuild/PsyCross/include/PsyX/common/pgxp_defs.h)
  retains precision/projection information. It is not a complete world scene
  with persistent objects, materials, lights and off-camera shadow casters.
- [ImGui integration](../../../src_rebuild/utils/DeveloperGraphicsPanel.cpp),
  `DeveloperGraphicsPanel_Initialise`, uses SDL2/OpenGL3 backends; context and
  presentation changes affect it. [PsyX_main.cpp](../../../src_rebuild/PsyCross/src/PsyX_main.cpp)
  also contains direct OpenGL framebuffer readback for screenshots.
- [LoadCarModelFromFile](../../../src_rebuild/Game/C/cars.c) loads external
  `.MDL` car variants. This does not establish arbitrary glTF model import.
  [HD overrides](../../../src_rebuild/utils/HdTextureOverrides.cpp) load
  manifests and PNG texture replacements; image replacement is not a scene
  or material system.
- [Map code](../../../src_rebuild/Game/C/map.c),
  [streaming](../../../src_rebuild/Game/C/spool.c), and
  [level lumps](../../../src_rebuild/Game/C/main.c) carry world/region/road
  assumptions beyond rendering. [Frontend](../../../src_rebuild/Game/Frontend/FEmain.c)
  belongs to the game and can evolve independently of a full PsyCross removal.

## Main architectural distinctions

### Compatibility layer versus graphics backend

Removing PsyCross requires replacing or adapting many interfaces the game
already calls. Replacing its OpenGL backend is narrower, although still a
substantial resource/shader/context/synchronization integration task.
Audio and input need not change just because the graphics backend changes.

### Graphics API versus rendering technique

OpenGL, Vulkan and Metal are graphics APIs. Forward, Forward+ and Deferred
describe how rendering/lighting work is organized. PBR describes material and
lighting models. An API migration does not itself deliver PBR, new content,
or a guaranteed performance improvement.

Forward evaluates lighting during geometry rendering. Forward+ adds spatial
light assignment, often with compute. Deferred stores surface properties in
a G-buffer and lights them later. All can support PBR. Deferred also brings
bandwidth, material representation, transparency and antialiasing tradeoffs.
Compare techniques against actual scenes and supported hardware; do not
implement all of them as an initial goal.

### Scene information versus final primitives

Modern rendering benefits from persistent meshes, object transforms, normals,
materials, lights and camera data. The legacy stream is optimized around PSX
commands and carries less semantic information. Capture richer information
before CPU projection/culling reduces objects to that stream. Normals can
sometimes be recovered; material intent generally needs authored metadata.
Shadow visibility must include relevant off-camera objects, not just visible
triangles submitted for the main camera.

## Proposed direction

![Proposed hybrid rendering architecture; not implemented](diagrams/proposed-architecture.png)

Keep game-specific scene adapters, asset identities, materials and lighting
policy in `src_rebuild`. Keep generic compatibility/backend hooks reusable.
The physical home of a modern renderer remains open: a project-owned module
may be preferable to expanding PsyCross's game-specific responsibilities.
Maintain focused [PsyCross patches](../../rules/psycross-patches.md); this
discussion does not approve a private fork or a changed gitlink.

Use one graphics backend/context for the first hybrid experiment. Legacy and
modern rendering must agree on projection, depth, viewport and frame ownership.
Drawing two images on top of each other is insufficient: a modern object must
correctly occlude and be occluded by legacy geometry. PSX ordering and special
blend behaviour need explicit compatibility treatment; shared depth may require
an adapter or a prepass rather than direct reuse. This is an open technical risk.

The first modern slice should use a synthetic static mesh, one material and one
controllable light inside a reproducible game scene. Preserve original assets
and simulation. Use OpenGL for this first slice if its available capabilities
suffice; a future backend should exercise a meaningful, tested interface.

## Feature implications

- **PBR:** requires material definitions, normals, linear-light evaluation and
  colour-space/output handling. Author suitable base colour/roughness/metallic
  data. Legacy vertex colours and textures may already encode lighting;
  blindly applying new lighting can double-darken or over-light them.
- **Ambient occlusion:** screen-space AO needs usable depth and, depending on
  the algorithm, stored or reconstructed normals. Treat sky, transparent edges
  and HUD separately. A later lighting-aware implementation should affect the
  appropriate indirect component rather than indiscriminately darkening all light.
- **Custom models:** either compile into legacy formats/limits or add modern
  asset loading. Static visuals precede skinned characters and deforming cars.
  Rendering a vehicle is distinct from integrating wheels, collision and damage.
- **Custom maps:** distinguish a visual test scene from a playable city.
  Collision, road topology, streaming, AI, missions and activation ranges are
  game systems requiring dedicated work; glTF geometry alone cannot supply them.
- **Menus:** can change independently. Preserve input focus/capture and
  existing menu actions; the debug ImGui panel does not prescribe the player UI.
- **Vulkan/Metal:** backend choice follows platform goals. Metal implies Apple
  build/platform work as well as rendering. Vulkan through MoltenVK is another
  candidate with capability/portability constraints, not native Metal feature parity.

## Alternatives still open

1. Extend only the legacy renderer: useful for bounded post-processing and
   sampling improvements, but limited for full scene/material semantics.
2. Keep PsyCross services with a parallel modern scene path: recommended
   direction for investigation; hybrid depth and object identity are key risks.
3. Replace the compatibility layer or integrate another full engine: possible,
   but significantly expands migration scope. Reconsider only if evidence shows
   the bridge cannot satisfy the desired product and maintenance constraints.

## Questions to settle before roadmap adoption

- What is the first visible goal: original assets with optional enhancements,
  or new PBR assets in a more remastered art direction?
- What hardware, resolution, frame-time and memory budgets matter first?
- Are Windows/Linux the first experimental targets? When is Apple support needed?
- Is initial custom content static scenery, cars, characters, or a playable map?
- Which legacy behaviours must remain visually exact in compatibility mode?
- Is a small shared OpenGL scene/depth prototype the desired first milestone?

These are pending product decisions, not assumed user preferences. Provisional
defaults for discussion are static scenery, a faithful fallback and a small
OpenGL experiment on an existing desktop target.

## Related project records

- [Model round trips](../../roadmap/planned/model-export-import-roundtrip.md)
- [Asset identity](../../roadmap/planned/asset-catalog-identity.md)
- [Tool interoperability](../../roadmap/planned/modding-toolchain-integration.md)
- [Draw distance and streaming](../../roadmap/planned/draw-distance-streaming.md)
- [Graphics quality profiles](../../roadmap/planned/graphics-quality-profiles.md)
  explicitly excludes arbitrary PBR conversion and major renderer replacement.
- [Implemented texture alpha semantics](../../product/texture-alpha-semantics.md)

The planned records are context only. They neither implement nor authorize
this broader modernization, and need not all be completed before a synthetic
experiment. Add links to adopted scopes here if that decision is made later.

## External technical references

Consulted during the 2026-09-15 conversation. These establish general technical
capabilities, not features of REDRIVER2-Plus.

- [Khronos glTF 2.0 specification](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html):
  mesh attributes and material/asset conventions; a candidate import contract.
- [Khronos PBR overview](https://www.khronos.org/gltf/pbr): metallic/roughness materials.
- [AMD Forward+ example](https://github.com/GPUOpen-LibrariesAndSDKs/ForwardPlus11/):
  per-tile light assignment; its Direct3D sample is not a project dependency.
- [Apple deferred lighting example](https://developer.apple.com/documentation/Metal/rendering-a-scene-with-deferred-lighting-in-c%2B%2B):
  G-buffer lighting and platform-specific implementation considerations.
- [MoltenVK](https://github.com/KhronosGroup/MoltenVK): Vulkan over Metal and
  supported capability subsets; evaluate versions at implementation time.

## Discussion history

### 2026-09-15 - Technology inventory and compatibility question

Reviewed C/C++, PsyCross, SDL2, OpenGL/GLSL/GLAD, GTE/PGXP, OpenAL/SPU-AL,
ImGui, JPEG/AVI, WIC image handling and build tooling. Explained that replacing
PsyCross is possible but not required for graphics modernization. Identified
the final vertex stream's missing modern material/normal data and recommended
investigating a richer scene path. No new renderer was selected or implemented.

### 2026-09-15 - Persistent discussion and recommended sequence

User requested a durable discussion folder, image diagrams and automatic
updates as the same subject develops. Created this synthesis and the staged
exploration note. Refined the recommendation: prove shared camera/depth and
legacy compatibility before PBR complexity; evaluate pipelines and APIs through
separate gates; custom playable maps remain a distinct gameplay/tooling effort.
The user explicitly requested exploration before a new roadmap entry.
