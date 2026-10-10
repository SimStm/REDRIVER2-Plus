#ifndef GLTF_LOADER_H
#define GLTF_LOADER_H

#include <vector>

/*
 * Bounded glTF 2.0 / GLB static-mesh importer (renderer roadmap R3).
 *
 * Scope: one mesh. Attributes POSITION/NORMAL/TEXCOORD_0 as float accessors,
 * unsigned 16/32-bit indices, an optional COLOR_0 attribute (ubyte VEC3/VEC4,
 * normalised or not, expanded to RGBA8), and a pbrMetallicRoughness material
 * with a base-colour factor and (optionally) one embedded image. Images are
 * decoded on Windows through WIC (PNG/JPEG), the same decoder the override path
 * uses. Anything outside that subset fails with a clear message instead of
 * importing silently wrong geometry.
 *
 * Two records exist. `Gltf_LoadFile` keeps the historical single-primitive
 * contract (first primitive only, images retained through WIC) unchanged for
 * existing callers. `Gltf_LoadFileMulti` reads every primitive of the first mesh
 * into `GltfMultiMeshData`, each with its own base-colour material and an
 * optional embedded base-colour image; it is strict (vertex/index bounds,
 * attribute count agreement, triangle completeness) and rejects a texture
 * reference the multi record cannot hold rather than dropping it silently. On
 * failure the destination is reset to an empty record (no partial output).
 *
 * The multi record's image is the writer's embedded PNG: it is decoded from the
 * writer's stored-block (uncompressed deflate) subset. A PNG outside that subset
 * (dynamic/fixed Huffman, non-RGBA colour type, interlacing, non-None filter) is
 * a named error, never a silent drop.
 *
 * The importer never mutates game state; the caller owns the returned data.
 */

struct GltfMeshData
{
	std::vector<float> positions;			// 3 floats per vertex
	std::vector<float> normals;				// 3 floats per vertex (may be empty)
	std::vector<float> uvs;					// 2 floats per vertex (may be empty)
	std::vector<unsigned char> colors;		// RGBA8, 4 bytes per vertex (may be empty)
	std::vector<unsigned int> indices;		// triangle list (may be empty -> non-indexed)

	std::vector<unsigned char> baseColorRGBA;	// width*height*4, may be empty
	int baseColorWidth;
	int baseColorHeight;

	std::vector<unsigned char> normalRGBA;		// tangent-space normal map, may be empty
	int normalWidth;
	int normalHeight;

	std::vector<unsigned char> metallicRoughnessRGBA;	// G=roughness, B=metallic
	int metallicRoughnessWidth;
	int metallicRoughnessHeight;

	std::vector<unsigned char> emissiveRGBA;	// emissive map, may be empty
	int emissiveWidth;
	int emissiveHeight;

	float baseColorFactor[4];
	float emissiveFactor[3];
	float metallicFactor;
	float roughnessFactor;

	// Node transform of the first node that references the mesh, column-major
	// (identity when the node has none). Retained so a source placement is not
	// lost when geometry and its transform are compared as one record.
	float transform[16];

	int vertexCount;
	int triangleCount;

	GltfMeshData()
		: baseColorWidth(0), baseColorHeight(0),
		  normalWidth(0), normalHeight(0),
		  metallicRoughnessWidth(0), metallicRoughnessHeight(0),
		  emissiveWidth(0), emissiveHeight(0),
		  metallicFactor(1.0f), roughnessFactor(1.0f),
		  vertexCount(0), triangleCount(0)
	{
		baseColorFactor[0] = baseColorFactor[1] = baseColorFactor[2] = baseColorFactor[3] = 1.0f;
		emissiveFactor[0] = emissiveFactor[1] = emissiveFactor[2] = 0.0f;
		for (int i = 0; i < 16; i++)
			transform[i] = (i % 5 == 0) ? 1.0f : 0.0f;
	}
};

/* One primitive: geometry plus its own base-colour material factors and an
   optional baked base-colour image.

   Image contract (E03 texture phase 1): `imageRgba` is RGBA8, row-major, exactly
   `imageWidth * imageHeight * 4` bytes with 4 bytes per texel. It is empty (and
   `imageWidth`/`imageHeight` are 0) when the primitive is untextured. The writer
   embeds a non-empty image as one deterministic PNG per material and the multi
   loader decodes that same stored-block subset back; the single loader keeps its
   historical WIC path. The UVs stay raw 8-bit texel coordinates (see the
   builder); the renderer scales them by 1/256 when sampling a full page. */
struct GltfPrimitiveData
{
	std::vector<float> positions;			// 3 floats per vertex
	std::vector<float> normals;				// 3 floats per vertex (may be empty)
	std::vector<float> uvs;					// 2 floats per vertex (may be empty)
	std::vector<unsigned char> colors;		// RGBA8, 4 bytes per vertex (may be empty)
	std::vector<unsigned int> indices;		// triangle list (may be empty -> non-indexed)

	std::vector<unsigned char> imageRgba;	// RGBA8 row-major, width*height*4, may be empty
	int imageWidth;
	int imageHeight;

	float baseColorFactor[4];
	float metallicFactor;
	float roughnessFactor;

	int vertexCount;
	int triangleCount;

	GltfPrimitiveData()
		: imageWidth(0), imageHeight(0),
		  metallicFactor(1.0f), roughnessFactor(1.0f),
		  vertexCount(0), triangleCount(0)
	{
		baseColorFactor[0] = baseColorFactor[1] = baseColorFactor[2] = baseColorFactor[3] = 1.0f;
	}
};

/* A mesh with N primitives sharing one node transform, each primitive carrying
   its own base-colour material and an optional embedded base-colour image. This
   is the record the E03 multi-submesh building path consumes. */
struct GltfMultiMeshData
{
	std::vector<GltfPrimitiveData> primitives;

	// Node transform of the first node that references the mesh, column-major
	// (identity when the node has none).
	float transform[16];

	GltfMultiMeshData()
	{
		for (int i = 0; i < 16; i++)
			transform[i] = (i % 5 == 0) ? 1.0f : 0.0f;
	}
};

/* Loads the first mesh primitive of a .glb or .gltf. Returns 0 on success and
   fills `out`; returns -1 and writes a human-readable reason into `error`.
   Behaviour is preserved for existing callers. */
int Gltf_LoadFile(const char* path, GltfMeshData* out, char* error, int errorCapacity);

/* Loads every primitive of the first mesh of a .glb into `out`. Returns 0 on
   success and fills `out`; returns -1, writes a named reason into `error` and
   resets `out` to an empty record (no partial output) otherwise. */
int Gltf_LoadFileMulti(const char* path, GltfMultiMeshData* out, char* error, int errorCapacity);

/* Provenance/identity helper: a stable material id derived from the source
   file name (without directories) plus the base-colour factor, suitable as a
   stable identity independent of temporary PSX texture addresses. */
void Gltf_MakeMaterialId(const char* path, const GltfMeshData& mesh, char* id, int idCapacity);

#endif // GLTF_LOADER_H
