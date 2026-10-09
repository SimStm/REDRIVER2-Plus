// Standalone regression tests for the E02 conversion -> package pipeline.
//
// A source mesh is converted to a cached native GLB and its published content
// hash becomes a package manifest entry; a second entry depends on the first.
// The assembled package must round-trip through canonical serialization and the
// parser, and a package whose dependency is missing must fail to load. It needs
// no game assets or GPU context. Build and run instructions are in tests/README.md.

#include "../utils/AssetConverter.h"
#include "../utils/GltfLoader.h"
#include "../Game/C/assetpackage.h"
#include "../Game/C/conversioncache.h"

#include <direct.h>
#include <stdio.h>
#include <string.h>

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

static GltfMeshData MakeTriangle()
{
	GltfMeshData mesh;
	mesh.positions = { 0, 0, 0, 1, 0, 0, 0, 1, 0 };
	mesh.uvs = { 0, 0, 1, 0, 0, 1 };
	mesh.indices = { 0, 1, 2 };
	mesh.vertexCount = 3;
	mesh.triangleCount = 1;
	return mesh;
}

static ConversionCacheKey MakeKey(const char* converter, unsigned long long source)
{
	ConversionCacheKey key;
	ConversionCache_MakeKey(&key, converter, source, 0, 0, CONVERSION_CACHE_SCHEMA_VERSION);
	return key;
}

static void TestConvertAndAssemble()
{
	_mkdir("cache");
	char error[256];

	ConversionCacheKey bodyKey = MakeKey("body.glb", 0x1ULL);
	ConversionCacheKey wheelKey = MakeKey("wheel.glb", 0x2ULL);
	AssetConverterResult body, wheel;
	CHECK(AssetConverter_ConvertMesh("cache", &bodyKey, MakeTriangle(), &body, error, sizeof(error)) == ASSET_CONVERTER_OK);
	CHECK(AssetConverter_ConvertMesh("cache", &wheelKey, MakeTriangle(), &wheel, error, sizeof(error)) == ASSET_CONVERTER_OK);

	static AssetPackage package;
	AssetPackage_Reset(&package);
	CHECK(AssetPackage_SetId(&package, "car.slice") == 1);
	CHECK(AssetPackage_AddEntry(&package, "model:0:0:1", "model", body.glbPath, body.contentHash, NULL, 0) == 0);

	const char* dependencies[] = { "model:0:0:1" };
	CHECK(AssetPackage_AddEntry(&package, "model:0:0:2", "model", wheel.glbPath, wheel.contentHash, dependencies, 1) == 1);
	CHECK(package.entryCount == 2);

	// Duplicate and over-long ids are rejected.
	CHECK(AssetPackage_AddEntry(&package, "model:0:0:1", "model", "x.glb", "", NULL, 0) == -1);

	// Canonical serialization is deterministic and reloads to the same package.
	char first[4096], second[4096];
	const int length = AssetPackage_SerializeCanonical(&package, first, sizeof(first));
	CHECK(length > 0);
	CHECK(AssetPackage_SerializeCanonical(&package, second, sizeof(second)) == length);
	CHECK(strcmp(first, second) == 0);

	static AssetPackage reloaded;
	CHECK(AssetPackage_LoadFromText(&reloaded, first, length, error, sizeof(error)) == ASSET_PACKAGE_OK);
	CHECK(reloaded.entryCount == 2);
	const int wheelIndex = AssetPackage_FindEntry(&reloaded, "model:0:0:2");
	CHECK(wheelIndex == 1);
	CHECK(strcmp(reloaded.entries[wheelIndex].contentHash, wheel.contentHash) == 0);
	CHECK(reloaded.entries[wheelIndex].dependencyCount == 1);
	CHECK(strcmp(reloaded.entries[wheelIndex].dependencies[0], "model:0:0:1") == 0);
}

static void TestReproduciblePackage()
{
	ConversionCacheKey key = MakeKey("prop.glb", 0x3ULL);
	AssetConverterResult result;
	char error[256];
	CHECK(AssetConverter_ConvertMesh("cache", &key, MakeTriangle(), &result, error, sizeof(error)) == ASSET_CONVERTER_OK);

	static AssetPackage a, b;
	AssetPackage_Reset(&a);
	AssetPackage_Reset(&b);
	CHECK(AssetPackage_SetId(&a, "prop.pkg") && AssetPackage_SetId(&b, "prop.pkg"));
	CHECK(AssetPackage_AddEntry(&a, "model:0:0:9", "model", result.glbPath, result.contentHash, NULL, 0) == 0);
	CHECK(AssetPackage_AddEntry(&b, "model:0:0:9", "model", result.glbPath, result.contentHash, NULL, 0) == 0);

	char outA[1024], outB[1024];
	const int lengthA = AssetPackage_SerializeCanonical(&a, outA, sizeof(outA));
	const int lengthB = AssetPackage_SerializeCanonical(&b, outB, sizeof(outB));
	CHECK(lengthA > 0 && lengthA == lengthB);
	CHECK(strcmp(outA, outB) == 0);
}

static void TestMissingDependencyFails()
{
	static AssetPackage package;
	AssetPackage_Reset(&package);
	AssetPackage_SetId(&package, "broken");
	const char* danglingDependencies[] = { "model:0:0:1" };
	AssetPackage_AddEntry(&package, "model:0:0:2", "model", "wheel.glb", "abc", danglingDependencies, 1);

	char text[1024];
	const int length = AssetPackage_SerializeCanonical(&package, text, sizeof(text));
	CHECK(length > 0);

	// The assembled package references a dependency that was never added, so a
	// load must reject it rather than accept a dangling reference.
	char error[256];
	CHECK(AssetPackage_LoadFromText(&package, text, length, error, sizeof(error)) == ASSET_PACKAGE_ERR_UNKNOWN_DEPENDENCY);
	CHECK(error[0] != '\0');
}

int main()
{
	TestConvertAndAssemble();
	TestReproduciblePackage();
	TestMissingDependencyFails();

	printf("%d checks, %d failures\n", g_checks, g_failures);
	return g_failures == 0 ? 0 : 1;
}
