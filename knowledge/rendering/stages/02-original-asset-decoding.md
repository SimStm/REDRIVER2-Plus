---
type: ImplementationStage
title: R02 - Decode original files into persistent native assets
status: implemented
created: 2026-10-06
tags: [okf, rendering, stage, vulkan]
---

# R02 - Decode original files into persistent native assets

**Status:** implemented and accepted on the tested Windows original-import slice.
**Dependency:** R01 accepted. See [executed evidence](../r02-validation.md) and
[product](../../product/native-original-assets.md). Full region coverage is R03;
other-target runtime, long streaming routes and timing/peaks are not claimed.

## Executed progress (2026-10-07)

- Bounded game-owned MODEL/placement and 4/8/16-bit texture decoders now own
  their results; whole-resource rejection covers corrupt/unsupported records.
  Original loader byte spans are recorded separately from legacy MODEL layouts.
  `run_native_asset_tests.ps1`: 1,202 checks passed, including GPU cache and GLB cases.
- Existing manifest precedence/CLUT matching supplies copied native PNG artwork,
  with a monotonic revision and edited-source rejection until reload. Existing
  schemas/directories remain unchanged. Fresh inspector runner: catalog145 and
  export120 checks passed, including17 added native override cases.
- Native C ABI v2 adds copied, bounded material handles. Vulkan owns sRGB images,
  samplers/descriptors and fence retirement in the existing device/queue. CPU
  runner:18 scene/C ABI +8 camera +11 material cases and host guard checks passed.
  Shader generation and VS Release_dev x64 build passed. Direct validation-enabled
  `-vknativetest` passed analytic regression and material readback: expected188
  at black/white linear-light midpoint, cutout/background, independent legacy
  filter, eight upload/retirement cycles. Its report buffer was enlarged after
  discovering that4096 bytes truncated late-cycle evidence; the complete rerun
  passed, as did fresh legacy PSX readback. No VUID messages in inspected VS
  debug outputs; validation DLL was loaded.
- `NativeAssetCache.*` keys immutable geometry by model AND vertex-source catalog
  generation/revision, decoder version and original high/lower-detail links.
  Actual catalog tests cover invalidation, reuse, city generation and budgets.
  `-nativeassets` currently collects read-only before legacy polygon/GTE processing:
  Chicago probe cached69 resources/257972 accounted payload bytes,68 complete and
  one unsupported model1176; tick2640 observed298/rejected1. No GPU original draw
  is claimed. Cached rejection accounting was found/fixed by this actual run.
- `NativeModernDecode.*` bridges one already-decoded primitive to the same owned
  mesh/artwork contract. The existing owned bollard GLB matched2220 vertices,
  1479 triangles/4437 indices, source bounds/UVs and copied embedded PNG. Source
  hash unchanged. Existing importer still reads first primitive, does not validate
  full topology and is not production R09 support.
- GPU geometry/material cache now owns complete indexed parts with catalog/scene,
  texture-content, live palette/page, mod revision and filter identity. Tests
  cover rollback of late creation failure, reuse, invalidation and zero ownership.
  Actual submitted-upload fault injection passes original-world pixel/draw
  fallback, retained command/staging/image/fence, recovery and zero ownership;
  full native GPU suite PASS with validation loaded/no VUID. Shutdown no longer
  destroys resources after an unconfirmed device-idle failure.
- Original MODEL UVs can exceed named texture rectangles. Original VRAM crop
  expands to source coverage; active PNG regions retain explicit clamp sampling.
  Chicago1180 page0/id28 width24 vs UV extent32 exposed the issue; tests added.
- `-nativeassetview` is an incomplete road/facade import fixture, not R03. Actual
  city road plus1180 traffic-light/1150 street-lamp samples drew copied resources
  with PNGs/filter1 and original UI; source bounds/visual inspection prevent
  classifying these props as buildings. Final sample selects observed facade273
  after startup stabilization and retains logical placement/real source LOD keys.
  Actual city shows road36/source37 and facade273:3 roads/4 facades,14 native
  draws/world0/UI10, four meshes/materials,7176 CPU geometry/4247296 RGBA bytes,
  PNGs/bilinear1 enabled. Khronos loaded/no VUID. RenderDoc confirms four complete
  vertex/index/artwork byte hashes and14 post-VS/MVP comparisons, with zero messages.
- Live filter/PGXP/mod toggles and UI reload preserve native drawing. Actual level
  reload exercises catalog slot revisions and context generation; resources are
  rebuilt with unchanged counts/budgets. Forced reset during creation exposed an
  orphan-resource bug; fixed rollback passes CPU regression and repeated actual
  GPU fault. Five INIs/debugger settings restored byte-exactly. See full evidence.

## Objective

Build native geometry from existing Driver 2 files without requiring conversion of the installation.

## Steps

1. Implement game-owned read-only decoders for static model families, road data, cell placements, instanced vertex sources and original material/texture references before GTE submission.
2. Create owned immutable CPU resources and indexed GPU resources, keyed by catalog identity/generation/revision. Preserve winding, quad diagonals, UVs, face colours, palette context and original LOD links.
3. Decode original textures and active PNG/CLUT overrides through an explicit material adapter, preserving mod precedence and invalidation. Remove native geometry's dependency on nearest filtering or disabled overrides.
4. Specify one shared import contract for originals and glTF/GLB. Exercise an existing owned modern fixture, but distinguish the bounded existing importer from production R09 support.
5. Add decoder integrity checks for bounds/lengths, corrupt data and unsupported records. Define derived-cache versioning and actual visual file layout access.

## Deliverables

Read-only original static mesh/material adapters, stable asset cache, initial GLB contract and identity diagnostics.

## Acceptance gate

- [x] Representative road/building resources match source counts, placements, UVs and texture identities.
- [x] Changing/reusing region slots cannot leave stale resource handles or pointers.
- [x] Original files and mods load without directory/schema migration; bilinear/native geometry coexist.
- [x] Unsupported complete resources are reported explicitly; no silent missing faces. Historical old-scene recovery is superseded by the R03 explicit native failure policy.

## Planned validation

Targeted meaningful decoder tests using synthetic/owned inputs, inspector/catalog tests when their contracts change, VS build, RenderDoc mesh/material inspection in an original city and no-edit source comparisons. Preserve data assets.

## Risks and fallback

Original model variants and blend-context decoding are easy to misclassify. Use category coverage reports, not only a visually plausible building.

Original loader and unmigrated consumers remain required; new cache is disposable.
Native scene failure cannot select the old world. Existing OpenGL backend choice
remains separate; preserve ownership during native failure/recovery.

## Handoff

Record changed files, actual checks/results, unresolved limits and the next stage
in the [active plan](../implementation-plan.md), current status and recent context.
Every Vulkan change also updates the unscheduled OpenGL compatibility backlog.
Pass the [shared validation gates](../validation-and-completion.md); a planned
checklist is not evidence. Update the matching implemented product surface before
claiming delivery. This stage request does not authorize executing later stages.
