#ifndef WORLD_MESH_BUILDER_H
#define WORLD_MESH_BUILDER_H

#include "GltfLoader.h"
#include "SourceModelDecoder.h"

/*
 * E03 block 2 item 3: builds one project-owned `GltfMeshData` from a decoded
 * world `SourceMesh`, for the existing E02 single-primitive/one-material GLB
 * pipeline (GltfWriter -> AssetConverter -> GltfLoader).
 *
 * The module is pure data: it has no GPU, no PsyCross, no game headers and no
 * ODT dependency, and it never re-reads the source MODEL bytes. C++11.
 *
 * Adopted invariants (documented so the choices are explicit, not silent):
 *
 *  - Single primitive / one material. The E02 writer emits exactly one mesh,
 *    one primitive and one material. A decoded model whose SourceMesh carries
 *    more than one submesh cannot be represented without dropping geometry, so
 *    the build fails with WORLD_MESH_ERROR_MULTIPLE_SUBMESHES and zero output.
 *    A single-submesh model whose one submesh does not cover every index also
 *    fails (WORLD_MESH_ERROR_UNCOVERED_INDICES); nothing is dropped silently.
 *
 *  - Flat per-triangle normals. World polygon records carry no usable per-vertex
 *    normals (PL/flat types 10/11/20/21 store none; the building runtime shades
 *    per primitive, not per vertex). The build therefore synthesizes one flat
 *    geometric normal per emitted triangle from its three positions, in the
 *    decoder's world winding order, normalized. This applies to every submesh,
 *    including gouraud-class ones: stored point normals are NOT consumed in this
 *    block (a recorded follow-up), and no smooth/averaged normal is ever
 *    invented. A degenerate triangle (zero area) gets the documented fallback
 *    (0,0,1) instead of an undefined normal.
 *
 *  - Every triangle gets three dedicated output vertices (the source per-corner
 *    UV and colour are copied to each), so a flat per-triangle normal is
 *    unambiguous even where the decoder shared a quad's corner between its two
 *    triangles. Output indices are a trivial sequential triangle list.
 *
 *  - Base colour. The one material carries a single `baseColorFactor`. When the
 *    single submesh stored a raw RGB face colour (hasColors == true), the factor
 *    is the arithmetic mean of those raw per-corner colours over [0,1], with no
 *    draw-time intensity/shading applied. When the record stored no RGB (a PL
 *    record, hasColors == false) the documented neutral opaque white
 *    (1,1,1,1) is used. No per-vertex colour attribute exists in this pipeline.
 *
 *  - UVs are the raw 8-bit texel coordinates the decoder produced, left
 *    unnormalised. The multi path can attach a baked base-colour image per
 *    submesh (see `WorldSubmeshImage`); the UV convention is unchanged, so the
 *    renderer scales the raw texels by 1/256 when it samples a full page.
 *
 *  - The node transform stays identity: this builds the source geometry record;
 *    per-instance placement is a later concern.
 *
 *  - Deterministic and bounded: identical input produces byte-identical output;
 *    every array is validated before anything is written and any failure resets
 *    the destination to an empty mesh (no partial output).
 */

/* Named build failures. Zero (NONE) is success. */
enum WorldMeshBuilderError
{
	WORLD_MESH_ERROR_NONE = 0,
	WORLD_MESH_ERROR_ARGUMENT,            /* no destination mesh */
	WORLD_MESH_ERROR_EMPTY,               /* no vertices, indices or submeshes */
	WORLD_MESH_ERROR_MULTIPLE_SUBMESHES,  /* the writer supports one primitive only */
	WORLD_MESH_ERROR_ATTRIBUTE_SIZE,      /* an attribute array disagrees with vertexCount */
	WORLD_MESH_ERROR_INDEX_RANGE,         /* an index is outside the vertex range or not a triangle */
	WORLD_MESH_ERROR_UNCOVERED_INDICES,   /* the single submesh does not cover every index */
	WORLD_MESH_ERROR_IMAGE,               /* a submesh image has invalid dimensions or byte count */
};

/* One optional baked base-colour image for a submesh, in the multi build path.
   `rgba` is RGBA8, row-major, `width * height * 4` bytes (4 bytes per texel).
   A null `rgba` means the submesh is untextured and `width`/`height`/`byteCount`
   are ignored. `byteCount` is the number of valid bytes at `rgba` and must equal
   `width * height * 4`, so the copy is bounded rather than trusting the caller. */
struct WorldSubmeshImage
{
	const unsigned char* rgba;
	int width;
	int height;
	size_t byteCount;
};

/* Outcome detail for the log/report. All counts are zero on failure. */
struct WorldMeshBuilderResult
{
	WorldMeshBuilderError error;
	int submeshCount;
	int vertexCount;
	int triangleCount;
	SourcePrimitiveClass primitiveClass; /* class of the single submesh */
	unsigned char textureSet;
	unsigned char textureId;
	bool hasColors;                      /* the submesh stored a raw RGB face colour */
};

/* Builds `out` from `source`. Returns WORLD_MESH_ERROR_NONE (0) on success and
   fills `out`; otherwise returns a named error and resets `out` to an empty
   mesh. `result` is optional and always zeroed/filled when non-NULL. `error` is
   optional: a human-readable reason is written when it is non-NULL and
   `capacity` is positive. */
int WorldMeshBuilder_Build(const SourceMesh& source, GltfMeshData* out,
	WorldMeshBuilderResult* result, char* error, int capacity);

/*
 * Multi-submesh build path: maps every `SourceMesh` submesh to one
 * `GltfPrimitiveData`, so a model whose decoder produced more than one submesh
 * now converts instead of failing with MULTIPLE_SUBMESHES. It shares the single
 * builder's geometry rules exactly:
 *
 *  - One primitive per submesh, in source order. Each primitive expands its own
 *    submesh's triangles into dedicated per-corner vertices with the same flat
 *    per-triangle normal synthesis (degenerate -> documented (0,0,1)) and the
 *    same raw (unnormalised) per-corner UVs. A triangle in one submesh never
 *    shares a vertex with another.
 *
 *  - Per-primitive base colour follows the single builder's policy for that
 *    submesh: the arithmetic mean of the submesh's stored raw face colours when
 *    the submesh declares colours, otherwise the documented neutral opaque white
 *    (1,1,1,1). A mesh may therefore mix colour-bearing and colour-less (PL)
 *    submeshes; the shared per-vertex colour array is global, so a PL submesh's
 *    placeholder bytes are never read as a colour.
 *
 *  - The submesh index runs must form an ordered, contiguous partition of the
 *    index list (submesh 0 starts at index 0 and each run begins where the
 *    previous ended), covering every index exactly once. A gap or overlap is
 *    UNCOVERED_INDICES with zero output, so no supported geometry is dropped
 *    silently. Every index must still address a real vertex (INDEX_RANGE).
 *
 *  - The node transform stays identity, exactly as the single path.
 *
 *  - Optional per-submesh images. `images` is either null (or `imageCount` zero)
 *    for a purely geometric build, or an array of exactly one `WorldSubmeshImage`
 *    per submesh, parallel to `source.submeshes`. Each non-null entry is copied
 *    verbatim into the matching primitive's `imageRgba`/`imageWidth`/
 *    `imageHeight`; a null entry leaves that primitive untextured. The copy is
 *    bounded: positive dimensions are required and `byteCount` must equal
 *    `width * height * 4`, otherwise the build fails with WORLD_MESH_ERROR_IMAGE
 *    and zero output. The array length must match the submesh count
 *    (WORLD_MESH_ERROR_ARGUMENT otherwise). The UV convention is unchanged: the
 *    raw texel coordinates stay, and the renderer scales them by 1/256.
 *
 *  - Optional per-vertex colours (visual parity P2). `vertexColorsRgb` is either
 *    null (or `vertexColorBytes` zero) for the historical base-colour-factor
 *    behaviour, or one RGB triple per source vertex, in the same vertex order as
 *    `source.positions`, with `vertexColorBytes == vertexCount * 3`. When
 *    supplied, every emitted corner carries the source vertex's colour as an
 *    RGBA8 COLOR_0 attribute (alpha 255) and the primitive's `baseColorFactor` is
 *    the documented neutral white, so the renderer's `texel * vertexColour`
 *    product reproduces the legacy `texColor * v_color` modulation exactly. A
 *    mismatched byte count is WORLD_MESH_ERROR_ATTRIBUTE_SIZE with zero output.
 *    Because every triangle already owns dedicated per-corner vertices, a
 *    per-polygon flat colour is preserved without splitting anything.
 *
 * Deterministic and bounded: identical input produces byte-identical output
 * (via Gltf_WriteFileMulti), and any failure resets the destination to an empty
 * record (no partial output). `result` reports aggregate vertex/triangle counts
 * and `submeshCount` = the primitive count; the single-valued class/texture/
 * hasColors fields describe the first submesh (the log only uses the counts).
 */
int WorldMeshBuilder_BuildMulti(const SourceMesh& source, GltfMultiMeshData* out,
	WorldMeshBuilderResult* result, char* error, int capacity,
	const WorldSubmeshImage* images = NULL, int imageCount = 0,
	const unsigned char* vertexColorsRgb = NULL, size_t vertexColorBytes = 0,
	const float* worldShadeIndices = NULL, size_t worldShadeCount = 0);

/* Human-readable name of a build error code. */
const char* WorldMeshBuilder_ErrorName(WorldMeshBuilderError error);

#endif // WORLD_MESH_BUILDER_H
