# Modern static fixtures — provenance

Owned test assets generated with Meshy (Text to 3D) on 2026-09-18 for renderer
roadmap item 14 (milestone R3/R4). These are generated props, not original game
assets. Use them only as local regression fixtures.

Generation settings for every entry:

- Model: `meshy-6` (`text-to-3d-preview`) then `text-to-3d-refine`
  (texture_richness `high`, `remove_lighting: true`).
- Topology: triangle, remeshed during preview to the per-asset polycount below.
- Export: GLB (binary glTF 2.0) with an embedded base-colour PNG.
- Credits: 20 (preview) + 10 (refine) per asset = 180 total for these six.
- Signed download URLs are intentionally not stored here.

| File | Preview task | Refine task | Verts | Tris | SHA-256 (GLB) |
| --- | --- | --- | --- | --- | --- |
| `bollard.glb` | `01a0b307-99d1-71a3-8880-f2ab47f41e9a` | `01a0b30c-91a9-7497-8c4d-ff5dd197656a` | 2220 | 1479 | `2e18ee257e1504911c750b831cf59728c555a9c9352ea4f327e93ec6dbb55c8d` |
| `jersey_barrier.glb` | `01a0b307-dda2-7388-8983-1fc09737ff7b` | `01a0b30c-92b7-7230-8d5c-df0c22c15b9f` | — | — | `bd96686668144263cc9730f6f86e5f839c10c4fa768f03a715f4111152193e6e` |
| `wooden_crate.glb` | `01a0b307-dfa6-752b-87c1-3ea566c83e7a` | `01a0b30c-93c4-71ef-b2e5-f2c1b78e7bf1` | — | — | `f47530ae60681d0469d7fbf938ebe01cc0a530552aa5be96b91ea3cb4e0b178e` |
| `traffic_cone.glb` | `01a0b307-e0ef-7226-af07-5f9c8efb94a2` | `01a0b30c-9382-70f6-9d40-1df9144a1203` | — | — | `a10618cb8738f5b342ba0bf2a94cb7e698e43106f41c74504249bbff00900501` |
| `oil_barrel.glb` | `01a0b307-e2cc-7622-ab6b-9a3ee3ab6781` | `01a0b30c-949e-7125-bde2-ab5f8ed2cf8d` | — | — | `e896667911dadaf793c8092f5cd93c37cd59be12fd25159ab95a1c8a5ff97201` |
| `street_lamp.glb` | `01a0b307-e49f-7725-a988-2ed468cb93dd` | `01a0b30c-95c9-736e-af7f-3d69dd6553b9` | — | — | `06d1577268d81c80bee7b8b17397b0774e5def693b5878cdc54f076f9acbc25f` |

Prompts:

- `bollard.glb`: "A single freestanding painted steel bollard, simple cylindrical silhouette, matte yellow paint with small exposed metal areas, no base scene, no text"
- `jersey_barrier.glb`: "A single concrete road Jersey barrier, simple trapezoidal silhouette, weathered grey concrete with chipped white paint, no base scene, no text"
- `wooden_crate.glb`: "A single wooden shipping crate, cubic box with visible planks and metal corner brackets, weathered brown wood, no base scene, no text"
- `traffic_cone.glb`: "A single orange traffic cone with a reflective white stripe, simple conical silhouette, no base scene, no text"
- `oil_barrel.glb`: "A single dented steel oil barrel, cylindrical with two rims, weathered blue paint and rust patches, no base scene, no text"
- `street_lamp.glb`: "A single street lamp post, tall slender pole with a curved arm and a lamp head, dark green painted metal, no base scene, no text"

Each download also produced a `*_base_color.png` next to the GLB; the GLB
already embeds that image, so the importer reads it from the binary chunk. The
sidecar PNGs exist only for inspection and are not required at runtime.

## PBR re-textures (roadmap R4)

`bollard_pbr.glb` and `oil_barrel_pbr.glb` re-run `retexture` on the same
geometry with `enable_pbr: true` (10 credits each). The GLBs embed
`baseColorTexture`, `metallicRoughnessTexture` and `normalTexture`; the
importer binds all three. Sidecar maps live under
`*_pbr_textures/` (`base_color.png`, `metallic.png`, `roughness.png`,
`normal.png`, `emission.png`); emission is not yet consumed.

| File | Retexture task | SHA-256 (GLB) |
| --- | --- | --- |
| `bollard_pbr.glb` | `01a0b31c-b991-73b0-9a6d-d9b08e7ba5af` | — |
| `oil_barrel_pbr.glb` | `01a0b31c-b82a-77a9-8124-6e576cc03173` | — |

Total Meshy spend for all fixtures: 6 x (20 + 10) + 2 x 10 = 200 credits.
