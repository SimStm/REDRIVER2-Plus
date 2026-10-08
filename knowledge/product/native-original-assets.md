---
type: Product
title: Native original asset decoding and materials
status: partial
created: 2026-10-07
tags: [okf, product, rendering, assets]
---

# Native original asset decoding and materials

R02 is implemented and accepted for the tested Windows original-import slice.
See [executed evidence](../rendering/r02-validation.md). The original-city fixture
is deliberately incomplete; a complete static region remains R03.

`NativeAssetDecode.*` consumes bounded little-endian MODEL and vertex-source
spans, preserving complete static triangle/quad counts, diagonals, source UVs,
material identity, stored colour bytes and source flags. It owns converted
Y-up vertices, normals, indices and bounds. Unsupported/truncated resources
return an empty output with a reason. PACKED_CELL_OBJECT decoding restores
signed16 coordinate wrap and original model/yaw/Y data without changing loaders.
`Models_GetSourceSize` records loader-declared spans separately from MODEL ABI.

`NativeTextureDecode.*` owns RGBA decoded from bounded original4/8/16-bit VRAM,
including CLUTs, zero transparency and STP metadata. The native PNG adapter
uses existing mod precedence/default and palette-specific CLUT identities.
It copies the selected artwork and checks its content against the legacy
registered version; edited content requires reload. Revision exhaustion is
explicit failure. No manifest, directory or original asset migration occurs.

PsyCross native C ABI v2 has bounded generational materials:32 slots,32 MiB
aggregate copied RGBA and4096 maximum dimension. Meshes reference handles,
never legacy texture IDs. The Vulkan owner uploads sRGB images using its own
existing queue, checks format/filter support, and retires image/view/sampler/
descriptor resources after the last referencing submission. Initial uploads
wait on a dedicated fence in that same queue; asynchronous scheduling and measured performance are
not delivered. Isolated clamp samplers support nearest and linear-light filtering;
opaque/cutout drawing precedes R03 blend-context integration. Analytic meshes
use an internal white texture to retain R01's untextured output.

`NativeAssetCache.*` stores128 immutable geometry records with a32 MiB accounted
payload cap. Identity includes model/vertex-source catalog generation/revision,
decoder version and both original LOD links; copied pre-GTE hashes are evidence,
not a replacement for lifetime identity. Model or parent invalidation blocks
lookup immediately. Cached whole-resource failures retain their reason. The
`-nativeassets` diagnostic option observes buildings/tiles before polygon/GTE
submission without changing legacy geometry or gameplay. Chicago collected69
entries/257972 accounted bytes:68 complete/one unsupported,298 observed and one
rejected at tick2640. Existing modern experimental props remained visible during
the legacy-world probe.

`NativeGpuAssetCache.*` maps catalog-backed immutable geometry to copied indexed
mesh/material handles, remapping byte-coordinate UVs to the original texture
rectangle while preserving artwork resolution independence. Identity includes
page/index, live tpage/CLUT, region, copied RGBA content, mod revision and filter.
Changing content creates a complete replacement before retiring the previous
resource; late creation/allocation failures roll back every new handle. Catalog
and scene generations prevent reused slots from retaining stale references.
Upload hashes compare every copied vertex/index/artwork byte with GPU captures.
Interrupted creation detects scene reset and rolls back new handles; it cannot
drop newly created resources under the assumption that reset already retired them.
`NativeOriginalMaterials.*` resolves the CPU VRAM mirror and winning active PNG
through existing texture metadata. Copied PNGs are retained for the active
registration revision; file edits take effect through the existing reload path.
`-nativeassetview` is a bounded Chicago road37/observed-facade diagnostic import fixture,
at most12 real placements per resource, with complete faces and explicit excluded
object accounting. It is not a complete static region or R03 acceptance.
The direct stabilized sample selects logical road36/source37 (12 original
vertices,3 polygons,6 triangles) and facade273 (30 vertices,20 polygons,40
triangles). Three road/four facade placements use14 indexed draws, zero legacy
world draws and10 original UI draws; four resident meshes/materials own7176 CPU
geometry bytes/4247296 RGBA bytes. Original PNGs and bilinear1 were enabled.
The actual image shows roads/building facades/HUD; direct validation loaded and
has no VUID. RenderDoc matches all four vertex/index/artwork hashes,14 post-VS
transforms and14 MVPs against seven original placements/final camera. Live
filter/PGXP/mod toggles, UI reload, catalog revision and generation reuse pass.
Full level reload recreates four resources and restores the same ownership budgets.
Selection follows source keys after startup stabilization, preserving logical
placement identity separately; the logical and selected-LOD indices differ.

Submitted upload cleanup now uses its own fence. A failed wait cannot free the
command, staging buffer/memory or destination image until completion/loss is
confirmed; retiring material ownership remains charged while unconfirmed. A
real-submission conservative wait-failure test verifies original-world pixels,
legacy draws, retained handles, actual fence recovery and zero remaining owned
bytes. Unconfirmed device-idle failure also preserves shutdown resources for retry.

`NativeModernDecode.*` bridges one already-decoded opaque triangle primitive to
the same owned indexed mesh and RGBA artwork contract, preserving Y-up source
units/indices/UVs and PBR metadata. Explicit bounds reject partial arrays/images,
invalid indices, non-finite values and excessive geometry/image bytes. It does
not parse or validate GLB scene topology; the existing first-primitive importer
was exercised only with the owned single-mesh, single-primitive, identity-node
`assets/modern_fixtures/bollard.glb`. Its2220 vertices/1479 triangles/4437 indices,
source bounds and embedded PNG were checked; original file SHA256 remained
`2e18ee257e1504911c750b831cf59728c555a9c9352ea4f327e93ec6dbb55c8d`.
Full GLB/glTF topology/extensions remain R09 work. No disk-derived cache or new
asset directory exists: CPU cache is disposable/in-memory; game file layout and
manifest paths remain owned by existing loaders.

Executed evidence: asset decoder/cache/GLB1,202 checks, catalog145/export120 checks,
18 scene/C ABI +8 camera +11 material checks, host platform guards, shader
generation and Windows Release_dev x64 build passed. Native GPU readback passed
R01 regression, nearest/linear artwork, cutout/background, filter independence
and eight material upload/retirement cycles plus upload-wait fault recovery with validation loaded and no VUID
output. GPU allocation peaks/timing remain unmeasured. A forced reset during GPU
cache creation exposed orphan road parts; corrected rollback passes CPU regression
and repeated actual Vulkan fault recovery with four resources and original budgets.
See [accepted R02 gate](../rendering/stages/02-original-asset-decoding.md).

Next: R03 complete static-region coverage and picking; R04 long streaming routes.
Other-target runtime acceptance remains unexecuted. Native OpenGL materials
remain [pending and unscheduled](../roadmap/planned/port-vulkan-to-opengl-compatibility.md).
