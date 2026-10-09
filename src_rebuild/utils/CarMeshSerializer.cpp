#include "CarMeshSerializer.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>

namespace
{
void SetError(char* error, int capacity, const char* message)
{
	if (error && capacity > 0)
		snprintf(error, capacity, "%s", message);
}

// Canonical mods digest: every active mod string in caller order followed by a
// '\n' byte; 0 when there are no active mods. Callers pass validated non-empty
// ids; a NULL entry is hashed as a bare separator like an empty one.
unsigned long long HashActiveMods(const char* const* activeMods, int activeModCount)
{
	if (!activeMods || activeModCount <= 0)
		return 0;

	std::string joined;
	for (int i = 0; i < activeModCount; ++i)
	{
		if (activeMods[i])
			joined += activeMods[i];
		joined += '\n';
	}
	return ConversionCache_HashBytes(joined.data(), (int)joined.size());
}
} // namespace

int CarMeshSerializer_Export(const char* cacheDirectory, const char* sourceIdentity,
	const float* positions, int vertexCount, const unsigned int* triangleIndices, int triangleCount,
	const char* const* activeMods, int activeModCount,
	AssetConverterResult* out, char* error, int errorCapacity)
{
	if (error && errorCapacity > 0)
		error[0] = '\0';
	if (!out)
		return ASSET_CONVERTER_ERR_ARGUMENT;
	memset(out, 0, sizeof(*out));

	if (!cacheDirectory || cacheDirectory[0] == '\0')
	{
		SetError(error, errorCapacity, "a cache directory is required");
		return ASSET_CONVERTER_ERR_ARGUMENT;
	}
	if (!positions)
	{
		SetError(error, errorCapacity, "the car mesh has no positions");
		return ASSET_CONVERTER_ERR_ARGUMENT;
	}
	if (vertexCount <= 0)
	{
		SetError(error, errorCapacity, "the car mesh has no vertices");
		return ASSET_CONVERTER_ERR_EMPTY_MESH;
	}
	if (!triangleIndices)
	{
		SetError(error, errorCapacity, "the car mesh has no indices");
		return ASSET_CONVERTER_ERR_ARGUMENT;
	}
	if (triangleCount <= 0)
	{
		SetError(error, errorCapacity, "the car mesh has no triangles");
		return ASSET_CONVERTER_ERR_EMPTY_MESH;
	}

	// Every hash input must fit the int length ConversionCache_HashBytes takes.
	const long long positionBytes = (long long)vertexCount * 3 * (long long)sizeof(float);
	const long long indexBytes = (long long)triangleCount * 3 * (long long)sizeof(unsigned int);
	const long long identityBytes = sourceIdentity ? (long long)strlen(sourceIdentity) : 0;
	if (positionBytes > (long long)INT_MAX || indexBytes > (long long)INT_MAX ||
		positionBytes + indexBytes + identityBytes > (long long)INT_MAX)
	{
		SetError(error, errorCapacity, "the car mesh is too large to serialize");
		return ASSET_CONVERTER_ERR_ARGUMENT;
	}

	// No UVs or normals: the car serializer produces flat-shaded geometry only.
	GltfMeshData mesh;
	mesh.positions.assign(positions, positions + (size_t)vertexCount * 3);
	mesh.indices.assign(triangleIndices, triangleIndices + (size_t)triangleCount * 3);
	mesh.vertexCount = vertexCount;
	mesh.triangleCount = triangleCount;

	std::vector<unsigned char> sourceBytes;
	sourceBytes.reserve((size_t)(positionBytes + indexBytes + identityBytes));
	const unsigned char* positionBegin = (const unsigned char*)positions;
	sourceBytes.insert(sourceBytes.end(), positionBegin, positionBegin + (size_t)positionBytes);
	const unsigned char* indexBegin = (const unsigned char*)triangleIndices;
	sourceBytes.insert(sourceBytes.end(), indexBegin, indexBegin + (size_t)indexBytes);
	if (identityBytes > 0)
	{
		const unsigned char* identityBegin = (const unsigned char*)sourceIdentity;
		sourceBytes.insert(sourceBytes.end(), identityBegin, identityBegin + (size_t)identityBytes);
	}
	const unsigned long long sourceHash = ConversionCache_HashBytes(sourceBytes.data(), (int)sourceBytes.size());

	ConversionCacheKey key;
	ConversionCache_MakeKey(&key, "car-mesh", sourceHash, 0ULL,
		HashActiveMods(activeMods, activeModCount), CONVERSION_CACHE_SCHEMA_VERSION);

	// The E02 writer and verifier require a diagnostic buffer, so a caller that
	// supplies none gets a local scratch one and a valid conversion can still
	// publish. The caller's buffer is never touched when it is NULL or empty.
	char scratch[CONVERSION_CACHE_ERROR_CAPACITY] = { 0 };
	char* diagnostic = error && errorCapacity > 0 ? error : scratch;
	const int diagnosticCapacity = error && errorCapacity > 0 ? errorCapacity : (int)sizeof(scratch);

	const int code = AssetConverter_ConvertMesh(cacheDirectory, &key, mesh, out, diagnostic, diagnosticCapacity);
	if (code != ASSET_CONVERTER_OK)
		memset(out, 0, sizeof(*out)); // the documented failure contract: no partial path/hash
	return code;
}
