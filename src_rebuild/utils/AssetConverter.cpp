#include "AssetConverter.h"
#include "GltfWriter.h"

#include <stdio.h>
#include <string.h>
#include <vector>

namespace
{
void FormatHash(unsigned long long hash, char* out, int capacity)
{
	snprintf(out, capacity, "%016llx", hash);
}

int ReadFileBytes(const char* path, std::vector<unsigned char>* out)
{
	FILE* file = fopen(path, "rb");
	if (!file)
		return 0;
	fseek(file, 0, SEEK_END);
	const long size = ftell(file);
	fseek(file, 0, SEEK_SET);
	if (size <= 0)
	{
		fclose(file);
		return 0;
	}
	out->resize((size_t)size);
	const bool read = fread(out->data(), 1, (size_t)size, file) == (size_t)size;
	fclose(file);
	return read ? 1 : 0;
}
} // namespace

const char* AssetConverter_ErrorName(int code)
{
	switch (code)
	{
	case ASSET_CONVERTER_OK: return "OK";
	case ASSET_CONVERTER_ERR_ARGUMENT: return "argument";
	case ASSET_CONVERTER_ERR_EMPTY_MESH: return "empty-mesh";
	case ASSET_CONVERTER_ERR_WRITE: return "write";
	case ASSET_CONVERTER_ERR_VERIFY: return "verify";
	case ASSET_CONVERTER_ERR_PUBLISH: return "publish";
	default: return "unknown";
	}
}

int AssetConverter_ConvertMesh(const char* cacheDirectory, const ConversionCacheKey* key,
	const GltfMeshData& mesh, AssetConverterResult* out, char* error, int errorCapacity)
{
	if (error && errorCapacity > 0)
		error[0] = '\0';
	if (!cacheDirectory || !key || !out)
	{
		if (error && errorCapacity > 0)
			snprintf(error, errorCapacity, "no cache directory, key or output");
		return ASSET_CONVERTER_ERR_ARGUMENT;
	}

	memset(out, 0, sizeof(*out));
	const int vertexCount = mesh.vertexCount > 0 ? mesh.vertexCount : (int)(mesh.positions.size() / 3);
	if (vertexCount <= 0)
	{
		if (error && errorCapacity > 0)
			snprintf(error, errorCapacity, "the source mesh has no vertices");
		return ASSET_CONVERTER_ERR_EMPTY_MESH;
	}

	char cachePath[CONVERSION_CACHE_PATH_CAPACITY];
	if (!ConversionCache_MakeCachePath(cacheDirectory, key, cachePath, sizeof(cachePath)))
	{
		if (error && errorCapacity > 0)
			snprintf(error, errorCapacity, "the cache path is too long");
		return ASSET_CONVERTER_ERR_ARGUMENT;
	}
	snprintf(out->glbPath, sizeof(out->glbPath), "%s", cachePath);

	// A file at the key's path is fresh by construction: the key encodes the
	// source hash, converter, schema, options and active mods.
	std::vector<unsigned char> bytes;
	if (ReadFileBytes(cachePath, &bytes))
	{
		FormatHash(ConversionCache_HashBytes(bytes.data(), (int)bytes.size()), out->contentHash, sizeof(out->contentHash));
		out->converted = 0;
		out->reused = 1;
		return ASSET_CONVERTER_OK;
	}

	// Convert to a temporary file, verify it reads back, then publish it.
	char temporary[CONVERSION_CACHE_PATH_CAPACITY + 8];
	snprintf(temporary, sizeof(temporary), "%s.tmp", cachePath);
	if (Gltf_WriteFile(temporary, mesh, error, errorCapacity) != 0)
	{
		remove(temporary);
		return ASSET_CONVERTER_ERR_WRITE;
	}

	GltfMeshData verified;
	if (Gltf_LoadFile(temporary, &verified, error, errorCapacity) != 0 ||
		verified.vertexCount != vertexCount)
	{
		remove(temporary);
		if (error && errorCapacity > 0 && error[0] == '\0')
			snprintf(error, errorCapacity, "the converted GLB failed to read back");
		return ASSET_CONVERTER_ERR_VERIFY;
	}

	if (!ReadFileBytes(temporary, &bytes))
	{
		remove(temporary);
		if (error && errorCapacity > 0)
			snprintf(error, errorCapacity, "the converted GLB could not be read");
		return ASSET_CONVERTER_ERR_VERIFY;
	}
	FormatHash(ConversionCache_HashBytes(bytes.data(), (int)bytes.size()), out->contentHash, sizeof(out->contentHash));

	remove(cachePath);
	if (rename(temporary, cachePath) != 0)
	{
		remove(temporary);
		if (error && errorCapacity > 0)
			snprintf(error, errorCapacity, "the converted GLB could not be published");
		return ASSET_CONVERTER_ERR_PUBLISH;
	}

	out->converted = 1;
	out->reused = 0;
	return ASSET_CONVERTER_OK;
}
