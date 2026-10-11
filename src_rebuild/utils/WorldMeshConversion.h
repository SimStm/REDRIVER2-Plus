#ifndef WORLD_MESH_CONVERSION_H
#define WORLD_MESH_CONVERSION_H

#include "AssetConverter.h"
#include "WorldMeshBuilder.h"

// Snapshot references use indices into owned pixels, never VRAM/game pointers.
struct WorldConversionImage
{
	int pixelIndex; // -1: untextured
	int width;
	int height;
};

struct WorldMeshConversionInput
{
	SourceMesh source;
	std::vector<std::vector<unsigned char>> pixels;
	std::vector<WorldConversionImage> images;
	std::vector<unsigned char> legacyColors;
	std::vector<float> worldShadeIndices; // explicit normal-shaded building metadata; -1 for other corners
};

struct WorldMeshConversionResult
{
	AssetConverterResult asset;
	WorldMeshBuilderResult built;
	unsigned long long sourceHash;
	char resourceKey[CONVERSION_CACHE_KEY_CAPACITY];
};

// Pure owned snapshot -> cache artifact. Safe on a worker; no game/SDL/GPU calls.
// Windows implementation. Images: 64 buffers / 16 MiB, 64 MiB after expansion;
// cached file reads: 128 MiB. Writers are serialized so a
// synchronous developer conversion cannot race an asynchronous .tmp publication.
int WorldMeshConversion_Publish(const char* cacheDirectory, const WorldMeshConversionInput& input,
								WorldMeshConversionResult* out, char* error, int errorCapacity);

#endif
