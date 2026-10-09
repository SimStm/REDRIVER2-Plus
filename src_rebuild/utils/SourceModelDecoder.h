#ifndef SOURCE_MODEL_DECODER_H
#define SOURCE_MODEL_DECODER_H

#include <vector>

/*
 * E03 block 1 source-MODEL decoder (asset-free, no GPU).
 *
 * Decodes the polygon records of a reconstructed `MODEL` into a project-owned
 * `SourceMesh`, without touching the source bytes and without calling the
 * legacy runtime `ConvertPolygonTypes` (which mutates a model in place).
 *
 * The module is deliberately narrow and evidence-backed:
 *  - It mirrors only the textured world layouts the building/tile paths
 *    consume (Game/engine/mdl.h):
 *        ptype 11 -> PL_POLYFT4 (flat quad,     16-byte record, NO RGB)
 *        ptype 20 -> POLYFT3   (flat triangle,  20-byte record)
 *        ptype 21 -> POLYFT4   (flat quad,      20-byte record)
 *        ptype 22 -> POLYGT3   (gouraud tri,    24-byte record)
 *        ptype 23 -> POLYGT4   (gouraud quad,   24-byte record)
 *    Every other polygon type is a named error with no output. The 4..7
 *    textured bank and every untextured/other type are outside the allowlist.
 *    The runtime building loop instead SKIPS unknown types and advances by
 *    PolySizes (draw.c:733-737, 773-777); this decoder fails the whole model
 *    instead. Block 2 must histogram real model ptypes before this skip-vs-fail
 *    policy is final.
 *  - ptype 11's `th` byte is a light/normal index (draw.c:793); it is NEVER
 *    treated as color and no RGB is fabricated for a PL record.
 *  - The span captures the model BEFORE ConvertPolygonTypes runs. The declared
 *    ptypes are decoded as stored: 20/22 decode as triangles (v0,v1,v2). The
 *    runtime mutates them in place (20->21, 22->23) behind the tri_verts & 0x80
 *    guard (draw.c:704-731); reproducing that mutation is a Block 2 concern and
 *    is deliberately not relied on here.
 *  - The tile paths (tile.c) iterate every record with PL_POLYFT4 field
 *    offsets. Types beyond {11,20,21,22,23} are unverified until Block 2 checks
 *    real world data, so they remain hard errors here.
 *  - Record stride is PolySizes[ptype & 0x1f] (draw.c), and the mirrored
 *    layout size must equal it.
 *  - World quad emission order is (v0,v1,v3,v2) with UVs (uv0,uv1,uv3,uv2);
 *    this applies to ptypes 11, 21 and 23. The decoder then applies the same
 *    (0,1,2)/(0,2,3) fan over those emitted corners. The car vertex order
 *    (v0,v1,v2,v3) is NOT used.
 *  - Three-vertex types emit (v0,v1,v2) with UVs (uv0,uv1,uv2).
 *  - The caller supplies an explicit [begin,end) span. Every derived offset and
 *    every count*stride is validated inside the span; every vertex index is
 *    below the declared vertex count; every gouraud normal index is below the
 *    declared point-normal count. No partial success, no clamping, no silent
 *    skips.
 *
 * There are no game types, no PsyCross and no GPU dependency in this header.
 * C++11-compatible; failures are named codes, never exceptions.
 */

/* Named failure codes. */
enum SourceModelDecoderError
{
	SOURCE_MODEL_ERROR_NONE = 0,
	SOURCE_MODEL_ERROR_ARGUMENT,           // no destination mesh
	SOURCE_MODEL_ERROR_RANGE,              // empty or invalid [begin,end)
	SOURCE_MODEL_ERROR_INSTANCE,           // instanced model, not decodable from a span
	SOURCE_MODEL_ERROR_VERTICES_RANGE,     // vertex array outside the span
	SOURCE_MODEL_ERROR_POINT_NORMALS_RANGE,// point-normal array outside the span
	SOURCE_MODEL_ERROR_POLY_BLOCK_RANGE,   // polygon block start outside the span
	SOURCE_MODEL_ERROR_POLY_RECORD_RANGE,  // a polygon record extends past the span
	SOURCE_MODEL_ERROR_VERTEX_INDEX,       // polygon vertex index >= vertex count
	SOURCE_MODEL_ERROR_NORMAL_INDEX,       // gouraud normal index >= point-normal count
	SOURCE_MODEL_ERROR_UNSUPPORTED_PTYPE,  // polygon type outside the allowlist
	SOURCE_MODEL_ERROR_LAYOUT_SIZE,        // mirrored layout size != PolySizes[ptype]
};

/* Result of a decode attempt. On failure `error` is the named code and
   `primitiveIndex`/`primitiveType`/`offset` describe the offending record
   (-1/0 when not applicable). On success `error` is SOURCE_MODEL_ERROR_NONE. */
struct SourceModelDecoderResult
{
	SourceModelDecoderError error;
	int primitiveIndex;      // index of the failing polygon, or -1
	unsigned int primitiveType; // declared polygon type (id & 0x1f), or 0
	long long offset;        // byte offset of the failing polygon, or -1
};

/* POD echo of the MODEL fields the decoder needs. Every offset is a byte
   offset from `begin` (the MODEL base), matching GET_MODEL_DATA on a relocated
   model. `instanceNumber` must be -1: instanced models reference another model
   that is not in this span. */
struct SourceModelView
{
	int instanceNumber;          // MODEL::instance_number
	unsigned int numVertices;    // MODEL::num_vertices
	unsigned int numPolys;       // MODEL::num_polys
	unsigned int numPointNormals;// MODEL::num_point_normals
	int vertices;                // byte offset of the SVECTOR vertex array
	int polyBlock;               // byte offset of the polygon block
	int pointNormals;            // byte offset of the SVECTOR point-normal array
};

/* Coarse class of a decoded primitive. The class plus the source texture
   set/id is the material grouping key. The class distinguishes a PL record
   (no RGB) from an FT4 record even when they share a texture, so a submesh
   never mixes color-bearing and color-less primitives. */
enum class SourcePrimitiveClass : unsigned char
{
	TexturedPlQuad = 0,         // ptype 11 (PL_POLYFT4, no RGB)
	TexturedFlatTriangle,       // ptype 20
	TexturedFlatQuad,           // ptype 21
	TexturedGouraudTriangle,    // ptype 22
	TexturedGouraudQuad,        // ptype 23
};

/* One contiguous, complete run of indices sharing a material key.
   `hasColors` and `hasNormals` are the explicit per-submesh presence states:
   colors come only from layouts that store an RGB face color (ptypes 20/21/22/23,
   never the PL ptype 11), and normals come only from the gouraud layouts
   (ptypes 22/23). A run never mixes a present and an absent attribute. */
struct SourceSubmesh
{
	SourcePrimitiveClass primitiveClass;
	unsigned char textureSet;
	unsigned char textureId;
	unsigned int firstIndex;   // index into SourceMesh::indices
	unsigned int indexCount;   // number of indices in the run
	bool hasColors;            // true when this run stored an RGB face color
	bool hasNormals;           // true when this run stored per-vertex normals
};

/* Project-owned decoded geometry. All arrays are indexed per emitted corner
   (one output vertex per source polygon corner), so UVs are never shared or
   lost.

   `normals` is 3 floats per vertex, filled only for runs whose submesh has
   `hasNormals == true`. It is non-empty exactly when at least one submesh
   stores normals; flat/PL runs hold zero placeholders that must not be read as
   a normal. Flat primitives store no per-vertex normals and none are
   fabricated.

   `colors` is 3 bytes (RGB) per vertex, filled only for runs whose submesh has
   `hasColors == true`. It is non-empty exactly when at least one submesh stores
   color; vertices belonging to a `hasColors == false` submesh hold zero
   placeholder bytes that must not be read as a color. These are the RAW face
   colors stored in the source record; the legacy per-primitive shading
   (combointensity/f4colourTable in draw.c, GetDLightLevel in tile.c) is applied
   at draw time and is NOT baked here.

   `uvs` hold the raw 8-bit texel coordinates as floats, not normalised. */
struct SourceMesh
{
	std::vector<float> positions;      // 3 floats per vertex
	std::vector<float> normals;        // 3 floats per vertex, or empty
	std::vector<float> uvs;            // 2 floats per vertex
	std::vector<unsigned char> colors; // 3 bytes (RGB) per vertex, or empty
	std::vector<unsigned int> indices; // triangle list
	std::vector<SourceSubmesh> submeshes;
};

/* Decodes `model` from the byte span [begin,end). On success fills `out` with a
   complete mesh and returns a result whose error is SOURCE_MODEL_ERROR_NONE. On
   any failure `out` is reset to an empty mesh and the returned result carries a
   named code; `error` receives a human-readable message when it is non-NULL and
   `capacity` is positive. */
SourceModelDecoderResult DecodeSourceModel(const unsigned char* begin, const unsigned char* end,
	const SourceModelView& model, SourceMesh* out, char* error, int capacity);

/* True when `ptype` (masked with 0x1f) is inside the decoder's allowlist. */
bool SourceModelDecoder_IsSupportedType(unsigned int ptype);

/* Human-readable name of a decoder error code. */
const char* SourceModelDecoder_ErrorName(SourceModelDecoderError error);

#endif // SOURCE_MODEL_DECODER_H
