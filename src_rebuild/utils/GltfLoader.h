#ifndef GLTF_LOADER_H
#define GLTF_LOADER_H

#include <vector>

/*
 * Bounded glTF 2.0 / GLB static-mesh importer (renderer roadmap R3).
 *
 * Scope: one mesh, one primitive. Attributes POSITION/NORMAL/TEXCOORD_0 as
 * float accessors, unsigned 16/32-bit indices, one pbrMetallicRoughness
 * material with a base-colour factor and (optionally) one embedded image.
 * Images are decoded on Windows through WIC (PNG/JPEG), the same decoder the
 * override path uses. Anything outside that subset fails with a clear message
 * instead of importing silently wrong geometry.
 *
 * The importer never mutates game state; the caller owns the returned data.
 */

struct GltfMeshData
{
	std::vector<float> positions;			// 3 floats per vertex
	std::vector<float> normals;				// 3 floats per vertex (may be empty)
	std::vector<float> uvs;					// 2 floats per vertex (may be empty)
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
	}
};

/* Loads the first mesh primitive of a .glb or .gltf. Returns 0 on success and
   fills `out`; returns -1 and writes a human-readable reason into `error`. */
int Gltf_LoadFile(const char* path, GltfMeshData* out, char* error, int errorCapacity);

/* Provenance/identity helper: a stable material id derived from the source
   file name (without directories) plus the base-colour factor, suitable as a
   stable identity independent of temporary PSX texture addresses. */
void Gltf_MakeMaterialId(const char* path, const GltfMeshData& mesh, char* id, int idCapacity);

#endif // GLTF_LOADER_H
