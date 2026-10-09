// Standalone regression tests for the E02 asset converter (mesh -> cached GLB).
//
// The converter keys output by source hash/converter/schema/options/mods,
// reuses a fresh cached GLB, rebuilds a stale one, verifies the written GLB by
// reading it back, and returns the content hash the package manifest records.
// It needs no game assets or GPU context. Build and run instructions are in
// tests/README.md.

#include "../utils/AssetConverter.h"
#include "../utils/GltfLoader.h"
#include "../Game/C/conversioncache.h"

#include <direct.h>
#include <stdio.h>
#include <string.h>
#include <vector>

static int g_checks = 0;
static int g_failures = 0;

#define CHECK(condition) \
	do { \
		g_checks++; \
		if (!(condition)) { \
			g_failures++; \
			printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
		} \
	} while (0)

static GltfMeshData MakeQuad()
{
	GltfMeshData mesh;
	mesh.positions = { 0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0 };
	mesh.uvs = { 0, 0, 1, 0, 1, 1, 0, 1 };
	mesh.indices = { 0, 1, 2, 0, 2, 3 };
	mesh.vertexCount = 4;
	mesh.triangleCount = 2;
	return mesh;
}

static int ReadFileBytes(const char* path, std::vector<unsigned char>* out)
{
	FILE* file = fopen(path, "rb");
	if (!file)
		return 0;
	fseek(file, 0, SEEK_END);
	const long size = ftell(file);
	fseek(file, 0, SEEK_SET);
	if (size <= 0) { fclose(file); return 0; }
	out->resize((size_t)size);
	const bool read = fread(out->data(), 1, (size_t)size, file) == (size_t)size;
	fclose(file);
	return read ? 1 : 0;
}

static ConversionCacheKey MakeMeshKey(unsigned long long source, int schema)
{
	ConversionCacheKey key;
	ConversionCache_MakeKey(&key, "mesh.glb", source, 0x11ULL, 0x22ULL, schema);
	return key;
}

static void TestConvertReuseAndRebuild()
{
	_mkdir("cache");
	ConversionCacheKey key = MakeMeshKey(0x100ULL, 1);
	AssetConverterResult result;
	char error[256];

	CHECK(AssetConverter_ConvertMesh("cache", &key, MakeQuad(), &result, error, sizeof(error)) == ASSET_CONVERTER_OK);
	CHECK(result.converted == 1 && result.reused == 0);
	CHECK(result.glbPath[0] != '\0' && result.contentHash[0] != '\0');

	// The content hash is the hash of the published GLB bytes.
	std::vector<unsigned char> bytes;
	CHECK(ReadFileBytes(result.glbPath, &bytes));
	char expected[CONVERSION_CACHE_HASH_CAPACITY];
	snprintf(expected, sizeof(expected), "%016llx", ConversionCache_HashBytes(bytes.data(), (int)bytes.size()));
	CHECK(strcmp(result.contentHash, expected) == 0);

	// A second conversion with the same key reuses the cached GLB.
	AssetConverterResult again;
	CHECK(AssetConverter_ConvertMesh("cache", &key, MakeQuad(), &again, error, sizeof(error)) == ASSET_CONVERTER_OK);
	CHECK(again.converted == 0 && again.reused == 1);
	CHECK(strcmp(again.glbPath, result.glbPath) == 0);
	CHECK(strcmp(again.contentHash, result.contentHash) == 0);

	// A changed source hash is a different key and rebuilds at a new path.
	AssetConverterResult changed;
	ConversionCacheKey other = MakeMeshKey(0x200ULL, 1);
	CHECK(AssetConverter_ConvertMesh("cache", &other, MakeQuad(), &changed, error, sizeof(error)) == ASSET_CONVERTER_OK);
	CHECK(changed.converted == 1 && changed.reused == 0);
	CHECK(strcmp(changed.glbPath, result.glbPath) != 0);

	// A changed schema is also stale and rebuilds at a new path.
	AssetConverterResult schemaChanged;
	ConversionCacheKey schemaKey = MakeMeshKey(0x100ULL, 2);
	CHECK(AssetConverter_ConvertMesh("cache", &schemaKey, MakeQuad(), &schemaChanged, error, sizeof(error)) == ASSET_CONVERTER_OK);
	CHECK(schemaChanged.converted == 1);
	CHECK(strcmp(schemaChanged.glbPath, result.glbPath) != 0);
}

static void TestReproducibleArtifacts()
{
	_mkdir("cacheA");
	_mkdir("cacheB");
	ConversionCacheKey key = MakeMeshKey(0x300ULL, 1);
	AssetConverterResult a, b;
	char error[256];
	CHECK(AssetConverter_ConvertMesh("cacheA", &key, MakeQuad(), &a, error, sizeof(error)) == ASSET_CONVERTER_OK);
	CHECK(AssetConverter_ConvertMesh("cacheB", &key, MakeQuad(), &b, error, sizeof(error)) == ASSET_CONVERTER_OK);
	CHECK(a.converted == 1 && b.converted == 1);

	// Two identical conversions produce byte-identical GLB and equal hashes.
	std::vector<unsigned char> bytesA, bytesB;
	CHECK(ReadFileBytes(a.glbPath, &bytesA));
	CHECK(ReadFileBytes(b.glbPath, &bytesB));
	CHECK(bytesA.size() == bytesB.size() && memcmp(bytesA.data(), bytesB.data(), bytesA.size()) == 0);
	CHECK(strcmp(a.contentHash, b.contentHash) == 0);
}

static void TestDiagnostics()
{
	ConversionCacheKey key = MakeMeshKey(0x400ULL, 1);
	AssetConverterResult result;
	char error[256];

	CHECK(AssetConverter_ConvertMesh(NULL, &key, MakeQuad(), &result, error, sizeof(error)) == ASSET_CONVERTER_ERR_ARGUMENT);
	CHECK(AssetConverter_ConvertMesh("cache", NULL, MakeQuad(), &result, error, sizeof(error)) == ASSET_CONVERTER_ERR_ARGUMENT);

	GltfMeshData empty;
	CHECK(AssetConverter_ConvertMesh("cache", &key, empty, &result, error, sizeof(error)) == ASSET_CONVERTER_ERR_EMPTY_MESH);
	CHECK(error[0] != '\0');

	CHECK(strcmp(AssetConverter_ErrorName(ASSET_CONVERTER_OK), "OK") == 0);
	CHECK(strcmp(AssetConverter_ErrorName(ASSET_CONVERTER_ERR_EMPTY_MESH), "empty-mesh") == 0);
}

int main()
{
	TestConvertReuseAndRebuild();
	TestReproducibleArtifacts();
	TestDiagnostics();

	printf("%d checks, %d failures\n", g_checks, g_failures);
	return g_failures == 0 ? 0 : 1;
}
