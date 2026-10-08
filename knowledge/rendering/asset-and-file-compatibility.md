---
type: Design
title: Original assets and modern asset compatibility
status: planned
created: 2026-10-06
tags: [okf, rendering, assets, gltf, mods]
---

# Original assets and modern asset compatibility

## Original installation remains usable

Read the existing city/model/texture/car/pedestrian/animation/event data through
game-owned adapters; preserve filenames, directory conventions, region/variant
selection and existing `overrideContent` consumers. No installation conversion,
replacement of original archives, mandatory HD pack or redistribution of `data/`.
An optional disposable derived cache can accelerate loading: version it by decoder,
source identity/revision and mod state; invalidate safely and rebuild from originals.

R00 inventories actual source formats and entry points; R02 decodes originals
before GTE/PSX primitive generation. Decode positions/indices, winding/quad diagonal,
UVs, face modulation, palettes, LOD links, bounds and material categories explicitly.
Validate unsupported records and corrupt lengths; do not silently drop faces or
make partially decoded models appear complete. Support all categories by their
assigned stage, with counted fallback during development.

## Shared scene representation

Both original adapters and glTF import produce the same owned mesh/material/instance
contracts. Stable catalog resource IDs stay distinct from instance IDs and runtime
slots. Retained handles carry generation/revision; GPU lifetimes extend through the
last submission that uses them. Mesh sharing does not merge logical-object identity.

Static originals need reliable face/vertex normals and material defaults. Dynamic
cars retain wheel/component/damage relationships; pedestrians retain animation
poses, hierarchy and palettes. Visual replacement changes neither collision nor
original deformation/skeleton/game state. If topology cannot follow the existing
damage/animation contract, reject it with a reason or use a documented component
mapping; never overwrite live source game buffers to make a model fit.

## Texture and mod contracts to preserve

Keep `mods/<id>/manifest.json`, `mods/enabled.json` order, existing PNG paths and
matching `(texture, texturePage, textureIndex)` with optional `clut`. Keep
`modelReferences` as metadata; preserve palette-specific precedence and existing
manifest merge/export behaviour. Do not replace catalog identity with a transient
GPU address or glTF node number.

Resolve CLUT colours, transparent zero texels, STP context, texture windows and
animated palette updates in an original-material adapter. Cutouts and semitransparent
effects need different explicit modes. Do not interpret every STP texel as soft
opacity or every exported alpha-128 texel as transparent on an opaque draw.
The current [alpha contract](../product/texture-alpha-semantics.md),
[proportional mode](../product/semi-transparent-texture-overrides.md) and
[palette variants](../product/texture-palette-variants.md) remain required inputs.

Native filtering, mipmaps/aniso and texture overrides must coexist with native
geometry; no dependency on PGXP or the current nearest/no-override migration gate.
Generate cutout-aware mipmaps and prevent atlas-border bleed. Preserve original
material/face tint as an explicit input, with a tested policy for prelit art.
Keep selection/export of original versus active resources distinctly labelled.

## glTF 2.0 / GLB support contract

GLB packages glTF content in a binary container; neither format defines gameplay.
R02 specifies the supported subset; R09 delivers production import/replacement.
Required baseline: multiple mesh primitives, indices/accessors with correct strides
and offsets, node transforms, base colour, metallic/roughness, normals/tangents/UVs,
normal/occlusion/emissive textures, declared alpha modes, double-sided materials
and material/texture colour spaces. Validate sparse or other unsupported features
explicitly. External glTF resources stay confined to their declared asset root.

Rigged/animated assets must either satisfy the R06 pose/component contract or be
reported unsupported; static import alone is not animated-object support. Define
extension support individually, including compressed geometry/textures; unknown
required extensions reject the asset. Do not claim universal glTF support.
Make coordinate/scale conversion and tangent conventions explicit. Original world
units need a verified scale; the engine must not guess a metre conversion.

Add a versioned optional sidecar model mapping only if the current mod schema has
no suitable field, with stable original catalog IDs and clean fallback. Its exact
schema is an R02/R09 design deliverable, not an existing runtime capability.
No-edit export/import must preserve counts, bounds, UVs, material identity and
component transforms within declared tolerance. Keep OBJ/MTL tooling compatible
where useful; glTF supplies the richer runtime contract for modern materials.

Meshy can provide additional owned fixtures through the configured MCP when needed.
Reuse existing fixtures first. Record generation provenance, accepted formats,
hashes and relevant asset terms. Runtime and repeatable tests consume local files
and do not depend on an online generation service.

## Primary reference

The [Khronos glTF 2.0 specification](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html)
defines buffers/accessors, transforms and metallic/roughness material semantics.
Consult the actual selected loader's documentation and validate exports during
implementation; the existing bounded importer is not proof of these planned capabilities.
