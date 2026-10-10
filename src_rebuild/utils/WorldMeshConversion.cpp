#ifdef _WIN32
#include "WorldMeshConversion.h"
#include "GltfWriter.h"
#include <limits.h>
#include <mutex>
#include <stdio.h>
#include <string.h>

namespace
{
static unsigned long long HashSourceMesh(const SourceMesh& mesh)
{
	std::vector<unsigned char> bytes;
	bytes.reserve(mesh.positions.size() * sizeof(float) + mesh.uvs.size() * sizeof(float) + mesh.colors.size() +
				  mesh.shadeIndices.size() + mesh.indices.size() * sizeof(unsigned int) + mesh.submeshes.size() * 12);

	const unsigned char* positions = (const unsigned char*)mesh.positions.data();
	bytes.insert(bytes.end(), positions, positions + mesh.positions.size() * sizeof(float));
	const unsigned char* uvs = (const unsigned char*)mesh.uvs.data();
	bytes.insert(bytes.end(), uvs, uvs + mesh.uvs.size() * sizeof(float));
	bytes.insert(bytes.end(), mesh.colors.begin(), mesh.colors.end());
	bytes.insert(bytes.end(), mesh.shadeIndices.begin(), mesh.shadeIndices.end());
	const unsigned char* indices = (const unsigned char*)mesh.indices.data();
	bytes.insert(bytes.end(), indices, indices + mesh.indices.size() * sizeof(unsigned int));

	for (size_t i = 0; i < mesh.submeshes.size(); ++i)
	{
		const SourceSubmesh& submesh = mesh.submeshes[i];
		bytes.push_back((unsigned char)submesh.primitiveClass);
		bytes.push_back(submesh.textureSet);
		bytes.push_back(submesh.textureId);
		bytes.push_back(submesh.hasColors ? 1 : 0);
		bytes.push_back(submesh.hasNormals ? 1 : 0);
		for (int byte = 0; byte < 4; ++byte)
			bytes.push_back((unsigned char)(submesh.firstIndex >> (byte * 8)));
		for (int byte = 0; byte < 4; ++byte)
			bytes.push_back((unsigned char)(submesh.indexCount >> (byte * 8)));
	}

	if (bytes.size() > (size_t)INT_MAX)
		return 0;

	return ConversionCache_HashBytes(bytes.data(), (int)bytes.size());
}

static bool ReadWholeFileBytes(const char* path, std::vector<unsigned char>& out)
{
	FILE* file = fopen(path, "rb");
	if (file == NULL)
		return false;
	fseek(file, 0, SEEK_END);
	const long size = ftell(file);
	fseek(file, 0, SEEK_SET);
	if (size <= 0 || size > 128 * 1024 * 1024)
	{
		fclose(file);
		return false;
	}
	out.resize((size_t)size);
	const bool read = fread(out.data(), 1, (size_t)size, file) == (size_t)size;
	fclose(file);
	return read;
}

// Publishes a multi-primitive converted mesh transactionally, mirroring
// AssetConverter_ConvertMesh for the single-primitive record: a fresh cached GLB
// at the key's path is reused, otherwise the mesh is written to a temporary
// file, verified by reading it back, then renamed into place. AssetConverter is
// deliberately left untouched for this block, so the return codes reuse its
// vocabulary and the existing diagnostic wording stays unchanged.
static int PublishMultiMesh(const char* cacheDirectory, const ConversionCacheKey* key, const GltfMultiMeshData& mesh,
							AssetConverterResult* out, char* error, int errorCapacity)
{
	if (error && errorCapacity > 0)
		error[0] = '\0';
	if (cacheDirectory == NULL || key == NULL || out == NULL)
	{
		if (error && errorCapacity > 0)
			snprintf(error, errorCapacity, "no cache directory, key or output");
		return ASSET_CONVERTER_ERR_ARGUMENT;
	}

	memset(out, 0, sizeof(*out));
	if (mesh.primitives.empty())
	{
		if (error && errorCapacity > 0)
			snprintf(error, errorCapacity, "the converted mesh has no primitives");
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
	if (ReadWholeFileBytes(cachePath, bytes))
	{
		snprintf(out->contentHash, sizeof(out->contentHash), "%016llx",
				 ConversionCache_HashBytes(bytes.data(), (int)bytes.size()));
		out->converted = 0;
		out->reused = 1;
		return ASSET_CONVERTER_OK;
	}

	// Convert to a temporary file, verify it reads back, then publish it.
	char temporary[CONVERSION_CACHE_PATH_CAPACITY + 8];
	snprintf(temporary, sizeof(temporary), "%s.tmp", cachePath);
	if (Gltf_WriteFileMulti(temporary, mesh, error, errorCapacity) != 0)
	{
		remove(temporary);
		return ASSET_CONVERTER_ERR_WRITE;
	}

	GltfMultiMeshData verified;
	if (Gltf_LoadFileMulti(temporary, &verified, error, errorCapacity) != 0 ||
		verified.primitives.size() != mesh.primitives.size())
	{
		remove(temporary);
		if (error && errorCapacity > 0 && error[0] == '\0')
			snprintf(error, errorCapacity, "the converted GLB failed to read back");
		return ASSET_CONVERTER_ERR_VERIFY;
	}

	if (!ReadWholeFileBytes(temporary, bytes))
	{
		remove(temporary);
		if (error && errorCapacity > 0)
			snprintf(error, errorCapacity, "the converted GLB could not be read");
		return ASSET_CONVERTER_ERR_VERIFY;
	}
	snprintf(out->contentHash, sizeof(out->contentHash), "%016llx",
			 ConversionCache_HashBytes(bytes.data(), (int)bytes.size()));

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

// Incremental FNV-1a preserves the existing cache identity without allocating
// a second, potentially huge concatenation of repeated submesh page pixels.
struct HashStream
{
	// Preserve the existing cache's historical seed, including its nonstandard
	// offset basis. Changing this would invalidate every original world key.
	unsigned long long value = ConversionCache_HashBytes(NULL, 0);
	void byte(unsigned char b)
	{
		value = (value ^ b) * 1099511628211ULL;
	}
	void bytes(const unsigned char* data, size_t size)
	{
		for (size_t i = 0; i < size; ++i)
			byte(data[i]);
	}
	void number(unsigned long long n, int size)
	{
		for (int i = 0; i < size; ++i)
			byte((unsigned char)(n >> (8 * i)));
	}
};

unsigned long long OptionsHash(const WorldMeshConversionInput& input)
{
	unsigned long long imageHash = 0;
	if (!input.pixels.empty())
	{
		HashStream stream;
		for (size_t i = 0; i < input.images.size(); ++i)
		{
			const WorldConversionImage& image = input.images[i];
			const bool textured = image.pixelIndex >= 0;
			stream.byte(textured ? 1 : 0);
			stream.number(image.width, 4);
			stream.number(image.height, 4);
			const size_t bytes = textured ? input.pixels[image.pixelIndex].size() : 0;
			stream.number(bytes, 4);
			if (textured)
				stream.bytes(input.pixels[image.pixelIndex].data(), bytes);
		}
		imageHash = stream.value;
	}
	if (input.legacyColors.empty())
		return imageHash;
	HashStream stream;
	stream.byte(0xA5);
	stream.number(imageHash, 8);
	stream.bytes(input.legacyColors.data(), input.legacyColors.size());
	return stream.value;
}

std::mutex s_publishMutex;
} // namespace

int WorldMeshConversion_Publish(const char* cacheDirectory, const WorldMeshConversionInput& input,
								WorldMeshConversionResult* out, char* error, int errorCapacity)
{
	if (error && errorCapacity > 0)
		error[0] = '\0';
	if (!out || !cacheDirectory || !cacheDirectory[0])
	{
		if (error && errorCapacity > 0)
			snprintf(error, errorCapacity, "no world conversion output or cache directory");
		return ASSET_CONVERTER_ERR_ARGUMENT;
	}
	memset(out, 0, sizeof(*out));
	size_t bytes = 0;
	if (input.pixels.size() > 64 || input.images.size() != input.source.submeshes.size())
	{
		if (error && errorCapacity > 0)
			snprintf(error, errorCapacity, "invalid world snapshot image inventory");
		return ASSET_CONVERTER_ERR_ARGUMENT;
	}
	for (size_t i = 0; i < input.pixels.size(); ++i)
	{
		if (input.pixels[i].size() > 16u * 1024u * 1024u - bytes)
		{
			if (error && errorCapacity > 0)
				snprintf(error, errorCapacity, "world snapshot exceeds 16 MiB of pixels");
			return ASSET_CONVERTER_ERR_ARGUMENT;
		}
		bytes += input.pixels[i].size();
	}
	std::vector<WorldSubmeshImage> images(input.images.size());
	size_t expandedImageBytes = 0;
	for (size_t i = 0; i < images.size(); ++i)
	{
		const WorldConversionImage& image = input.images[i];
		if (image.pixelIndex < -1 || image.pixelIndex >= (int)input.pixels.size())
		{
			if (error && errorCapacity > 0)
				snprintf(error, errorCapacity, "world snapshot image references absent pixels");
			return ASSET_CONVERTER_ERR_ARGUMENT;
		}
		images[i].rgba = image.pixelIndex < 0 ? NULL : input.pixels[image.pixelIndex].data();
		images[i].width = image.width;
		images[i].height = image.height;
		images[i].byteCount = image.pixelIndex < 0 ? 0 : input.pixels[image.pixelIndex].size();
		if (image.pixelIndex >= 0)
		{
			const unsigned long long expectedBytes =
				image.width > 0 && image.height > 0
					? (unsigned long long)image.width * (unsigned long long)image.height * 4ULL
					: 0;
			if (!expectedBytes || expectedBytes != images[i].byteCount ||
				images[i].byteCount > 64u * 1024u * 1024u - expandedImageBytes)
			{
				if (error && errorCapacity > 0)
					snprintf(error, errorCapacity, "invalid world snapshot image size or expanded image capacity");
				return ASSET_CONVERTER_ERR_ARGUMENT;
			}
			expandedImageBytes += images[i].byteCount;
		}
	}
	GltfMultiMeshData mesh;
	if (WorldMeshBuilder_BuildMulti(input.source, &mesh, &out->built, error, errorCapacity,
									images.empty() ? NULL : images.data(), (int)images.size(),
									input.legacyColors.empty() ? NULL : input.legacyColors.data(),
									input.legacyColors.size()) != WORLD_MESH_ERROR_NONE)
		return ASSET_CONVERTER_ERR_ARGUMENT;
	out->sourceHash = HashSourceMesh(input.source);
	ConversionCacheKey key;
	ConversionCache_MakeKey(&key, "world-mesh", out->sourceHash, OptionsHash(input), 0ULL,
							CONVERSION_CACHE_SCHEMA_VERSION);
	if (ConversionCache_FormatKey(&key, out->resourceKey, sizeof(out->resourceKey)) <= 0)
	{
		if (error && errorCapacity > 0)
			snprintf(error, errorCapacity, "world snapshot cache key formatting failed");
		return ASSET_CONVERTER_ERR_ARGUMENT;
	}
	std::lock_guard<std::mutex> lock(s_publishMutex);
	return PublishMultiMesh(cacheDirectory, &key, mesh, &out->asset, error, errorCapacity);
}
#endif // _WIN32: matches the existing game-side conversion target
