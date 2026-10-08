---
type: Product
title: Current rendering baseline before native replacement
description: Source-backed implemented baseline and open issues; does not describe the planned replacement as shipped.
updated: 2026-10-07
tags: [okf, rendering, baseline, vulkan]
---

# Current rendering baseline before native replacement

Inspection baseline: parent HEAD `2e93f215c54e61dd1bbb5d26e439c25303466709`
with substantial existing local changes; PsyCross
`446b96daaf74b0aeb6a68338aac7e82d7fd799c5`, pushed to the project fork's
`origin/master`, with the parent gitlink already staged. Read the live source
and both repository diffs before implementation. This document summarizes
the inspected baseline. Subsequent delivered surfaces are R00 diagnostics and
R01 opt-in analytic native world, copied resource/snapshot API, final game camera
and separate D32/native pass with original UI. See
[R01 product](../product/native-renderer-bootstrap.md) and
[validation](r01-validation.md):26 CPU cases, actual GPU pixels/reload/resize,
PGXP independence, city capture and explicit legacy/GL fallback. Fork99fa641 is
pushed/gitlink staged. R02 now has bounded decoders, immutable catalog-backed CPU
cache, copied native materials, indexed GPU/content cache and a pre-GTE city probe
with an incomplete import fixture. R02 is accepted on its declared Windows slice:
four vertex/index/artwork hashes and14 source/GPU transforms match; live filter/
PGXP/mod reload and catalog slot/context reuse pass. Submitted-upload fault and
interrupted-creation rollback recover without orphan resources. Original atlas/
PNG clamp semantics have targeted checks. See [R02 product](../product/native-original-assets.md)
and [executed evidence](r02-validation.md).
Latest R03 partial native static region has530 logical placements/876 world parts,
80 native backdrop draws, complete mip chains, retained per-instance sampling and
GPU logical picking. ABI v9 retains the v8 policy disabling old-scene recovery: native loading/
failure clears world pixels and reports useful diagnostics, while required original
UI/frontend/VRAM consumers remain pending migration. Existing OpenGL is separate.
Picking, explicit-failure and controlled camera/settings gates pass; region/surface
visual acceptance remains pending and R04-R12 are unstarted. Near road-marking
strips found during a133-step physics-input drive were diagnosed as coplanar D32
rounding and fixed with bounded authored terrain layers. Actual normal/wide native
pixels/IDs and unchanged original/fixed resources pass; a separate widened ground
seam remains unaccepted. Subsequent double CPU view/model-first matrix composition
fixes large-city cancellation; native GPU occlusion/IDs and paint coverage pass,
and canonical horizontal fragment depth subsequently removes the reproduced
equal-height overlap stippling. Ordinary native ties use submitted order, authored
paint retains bounded bias, and physical nearer/cutout/ID/retirement checks pass.
Actual normal/wide ownership masks and unchanged bound buffers/mips pass; source
artwork boundaries and full R03 surface coverage remain pending.
See [terrain evidence](r03-terrain-validation.md).
See [current product](../product/native-static-region.md)
and [executed R03 evidence](r03-validation.md). Earlier fallback evidence above is
historical and superseded by the adopted policy. Existing experiments below are
baseline dependencies, not acceptance evidence for the new renderer.

## Backend and original graphics

Desktop defaults to Vulkan in `src_rebuild/redriver2_psxpc.cpp`; `-opengl`
selects the existing fallback. `GR_*` dispatches to `PsyX_Vk_Game*` in
`src_rebuild/PsyCross/src/render/PsyX_Vk.cpp`. The Vulkan backend still consumes
PSX/GTE primitives: changing the API did not replace the scene representation.
Non-Vulkan target guards remain. No macOS/MoltenVK runtime build is established.

The Vulkan PSX shaders implement CLUT palettes, texture windows, dithering,
bilinear sampling, blend modes and mask-bit stencil. Capacity is 65,536 submitted
vertices and 512 game texture slots. CPU VRAM and a GPU mirror support uploads,
offscreen effects, minimap/overhead-map ordering, framebuffer effects and export.
Queued rectangle uploads replay in draw-generation order. Depth/stencil copies,
white untextured primitives and resumed-pass state have dedicated fixes/tests.

Resize, minimize/restore and out-of-date acquisition/fence handling have been
fixed. `LOAD_OP_LOAD` preserves each acquired swapchain image separately; it
does not guarantee a copy of the last presented frame. D24/S8 is preferred;
the selectable D32 fallback has no PSX mask-bit stencil. World composition uses
a game-provided OT boundary before overlays; split-screen retains the older
end-of-frame fallback. These are reuse constraints, not a new scene architecture.

## Modern experiments and their limits

`PsyX_ModernMesh_*`, `DeveloperModernMesh.*`, `GltfLoader.*` and
`DeveloperVkFixture.*` provide persistent indexed fixtures, a bounded glTF/GLB
importer and reference metallic/roughness PBR. The importer is not full glTF
coverage. Owned Meshy fixtures and analytic spheres exist under
`assets/modern_fixtures/`; preserve their provenance and hashes.

The game modern path still shares legacy projection/depth. The light set has
eight slots; legacy compositing consumes up to four point lights from headlights,
police sirens or a probe. Legacy lighting/shadows reconstruct positions from
the emulated depth and approximate normals from neighbours. Ambient occlusion
is an approximation, not a complete scene AO implementation. F9/F10 and the F11
panel control experiments, persisted separately from `config.ini`.

`DeveloperStaticScene.cpp` converts a narrow subset of original opaque models:
converted polygon types 11/21/23, 8-256 polygons, at most four materials. Eight
source resource keys and sixteen textures are shared across at most 128
placements (default 64); draws are per material/placement, not GPU instancing.
Catalog generation/revision and VRAM changes invalidate resources. Unsupported
whole models retain legacy drawing; roads, cutouts, tree sprites, transparency,
water, cars, pedestrians, damage and event geometry are outside this adapter.

`PolicyAllowsMigration()` requires Vulkan, enhanced rendering, single player,
both PGXP options, nearest sampling, and no enabled registered overrides.
Consequently an enabled native checkbox can still be ineffective with bilinear
or texture mods. The user's last saved bilinear setting was enabled; the sampled
log then reported only imported fixtures. The new renderer must expose effective
coverage and work with filtering/mods independently of PGXP.

## Existing resident shadows and distance

`DeveloperWorldShadow.*` collects resident opaque world models independently of
camera/PVS/backface submission. Supported source/converted polygon families are
10/11, 20/21 and 22/23. Capacity is 65,536 triangles. Actual CPU light matrices and
source vertex bounds conservatively reject unrelated cells/objects/triangles;
crossing triangles stay whole. No GPU-mapped CPU selection reads or retained raw
model pointers. Missing regions are counted; this collector never requests loads.

The game shadow atlas is D32, 4096x2048, two 2048-square tiles: 32 MiB allocated
even when far shadows are disabled. Player-centred, independently snapped near
and far volumes are not camera-fitted cascades. Optional far range follows the
native scenery setting (default 18,000, clamped 10,000-40,000 original units).
Complete overflow retries use requested/75%/50%/near-only coverage, with a
one-tier recovery probe after 120 successful frames. Capacity failure can still
remove coverage; requested range and effective range differ. Separate optional
legacy casters see only camera-submitted depth-valid opaque/cutout primitives.

`UpdateSpoolPC` caches raw spool data in a fixed 16 MiB desktop buffer. This is
not a completely decoded native city cache or a fully GPU-resident city. Model
and texture slots still belong to original region loading. Extending camera
draw distance cannot create missing resources, off-camera blockers or genuine
3D buildings where the source contains only painted horizon art.

## Evidence and unresolved visual issues

Previous milestone: VS MCP `Release_dev|x64` build and `-vkpsxtest` passed;
fresh installed RenderDoc 1.46 capture/first replay reported zero messages.
The MCP's 1.41 replay retained ten instrumentation warnings; duplicate capture
layers remain an environment issue. These results predate this docs-only task.

At 18k, sampled spawn collection fell from 21,766 to 8,751 triangles and a route
frame from 28,632 to 8,535, with identical colour and raw near/far maps. A 40k
low-sun route fit both volumes at 48,477 triangles. This proves sampled parity
and collection savings, not correct shadows for every angle, city or category.
Isolated actual-function MSVC tests with mocked capacity covered tier transitions;
real dense-scene recovery, native GPU time and peak memory remain unvalidated.

User screenshots show sky artifacts, facade artifacts and limited useful reach.
Shader review: `psx_composite.frag` can classify backdrop depths as world pixels;
the stricter threshold gates added sun lighting but does not gate all shadow
receiving. Enlarging the far volume can therefore shade reconstructed sky
positions. This is a source-supported defect candidate; the exact screenshot
still needs a frame/pixel-history diagnosis. Facade artifacts may involve depth
quantization/offsets, reconstructed normals, bias or prelit textures; do not claim
an exact cause without a capture. Screenshot 2 has far disabled, one volume and
zero capacity failures, so its cutoff is not evidence of buffer overflow.

Physics/AI/replays remain unchanged. Presentation is coupled to the native game
cadence (NTSC 30 Hz, PAL 25 Hz); rendering functions also contain counter-driven
effects and state changes. Do not unlock FPS by removing the frame gate.

## R00 baseline diagnostics (2026-10-07)

17 source categories now expose producer fallback/experimental/native counters
through a C-compatible API, F11 table and optional log. Windows read-only Vulkan
query and measured city/replay/wide-FOV/playground/fixture states are implemented;
see [product](../product/native-renderer-baseline.md) and
[evidence](r00-measured-baseline.md). The original-city frame stayed RGB-identical
after instrumentation. At R00 acceptance native counts were zero. Subsequent
R01 analytic native drawing is described above; original migration remains
in progress. No default renderer or original asset format changed.
