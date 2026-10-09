// Standalone regression tests for the E02-B car mesh serializer.
//
// The serializer adapts raw car geometry (positions, 32-bit triangle indices
// and the active mods list) to the E02 conversion pipeline: identical inputs
// reuse one cached GLB, changed geometry rebuilds it, and the conversion-cache
// key is sensitive to the geometry bytes, the source identity and the mods
// list. It needs no game assets and no GPU context. Build and run instructions
// are in tests/README.md.

#include "../utils/CarMeshSerializer.h"
#include "../utils/GltfLoader.h"
#include "../Game/C/conversioncache.h"

#include <direct.h>
#include <stdio.h>
#include <string.h>
#include <string>
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

static const float kQuadPositions[12] = {
	-1.0f, -1.0f, 0.0f,
	 1.0f, -1.0f, 0.0f,
	 1.0f,  1.0f, 0.0f,
	-1.0f,  1.0f, 0.0f,
};

static const unsigned int kQuadIndices[6] = { 0, 1, 2, 0, 2, 3 };

static bool FileExists(const char* path)
{
	FILE* file = fopen(path, "rb");
	if (!file)
		return false;
	fclose(file);
	return true;
}

// Mirrors the serializer's documented canonical key inputs, so the tests pin
// the exact derivation: geometry bytes then identity, no options, and mods
// joined in caller order each followed by '\n'.
static ConversionCacheKey ExpectedCarKey(const char* sourceIdentity,
	const float* positions, int vertexCount, const unsigned int* triangleIndices, int triangleCount,
	const char* const* activeMods, int activeModCount)
{
	std::vector<unsigned char> sourceBytes;
	const unsigned char* positionBegin = (const unsigned char*)positions;
	sourceBytes.insert(sourceBytes.end(), positionBegin, positionBegin + (size_t)vertexCount * 3 * sizeof(float));
	const unsigned char* indexBegin = (const unsigned char*)triangleIndices;
	sourceBytes.insert(sourceBytes.end(), indexBegin, indexBegin + (size_t)triangleCount * 3 * sizeof(unsigned int));
	if (sourceIdentity)
	{
		const unsigned char* identityBegin = (const unsigned char*)sourceIdentity;
		sourceBytes.insert(sourceBytes.end(), identityBegin, identityBegin + strlen(sourceIdentity));
	}
	const unsigned long long sourceHash = ConversionCache_HashBytes(sourceBytes.data(), (int)sourceBytes.size());

	std::string modText;
	if (activeMods && activeModCount > 0)
	{
		for (int i = 0; i < activeModCount; ++i)
		{
			if (activeMods[i])
				modText += activeMods[i];
			modText += '\n';
		}
	}
	const unsigned long long modsHash = modText.empty() ? 0ULL
		: ConversionCache_HashBytes(modText.data(), (int)modText.size());

	ConversionCacheKey key;
	ConversionCache_MakeKey(&key, "car-mesh", sourceHash, 0ULL, modsHash, CONVERSION_CACHE_SCHEMA_VERSION);
	return key;
}

static bool ExpectedPathMatches(const char* cacheDirectory, const ConversionCacheKey& key,
	const char* actualPath)
{
	char expected[CONVERSION_CACHE_PATH_CAPACITY];
	if (!ConversionCache_MakeCachePath(cacheDirectory, &key, expected, sizeof(expected)))
		return false;
	return strcmp(expected, actualPath) == 0;
}

static void TestDeterministicReuse()
{
	_mkdir("cache");
	char error[256];

	AssetConverterResult first;
	CHECK(CarMeshSerializer_Export("cache", "car_7_id_2", kQuadPositions, 4, kQuadIndices, 2,
		NULL, 0, &first, error, sizeof(error)) == ASSET_CONVERTER_OK);
	CHECK(first.converted == 1 && first.reused == 0);
	CHECK(first.glbPath[0] != '\0' && first.contentHash[0] != '\0');
	CHECK(FileExists(first.glbPath));

	// The published path must come from the documented canonical key.
	ConversionCacheKey expectedKey = ExpectedCarKey("car_7_id_2", kQuadPositions, 4, kQuadIndices, 2, NULL, 0);
	CHECK(ExpectedPathMatches("cache", expectedKey, first.glbPath));

	// The GLB reads back with the geometry that was fed in.
	GltfMeshData loaded;
	CHECK(Gltf_LoadFile(first.glbPath, &loaded, error, sizeof(error)) == 0);
	CHECK(loaded.vertexCount == 4 && loaded.triangleCount == 2);

	// Identical inputs reuse the cached artifact without rewriting it.
	AssetConverterResult second;
	CHECK(CarMeshSerializer_Export("cache", "car_7_id_2", kQuadPositions, 4, kQuadIndices, 2,
		NULL, 0, &second, error, sizeof(error)) == ASSET_CONVERTER_OK);
	CHECK(second.converted == 0 && second.reused == 1);
	CHECK(strcmp(second.glbPath, first.glbPath) == 0);
	CHECK(strcmp(second.contentHash, first.contentHash) == 0);

	// A changed position is a different source hash and rebuilds a new artifact.
	float movedPositions[12];
	memcpy(movedPositions, kQuadPositions, sizeof(movedPositions));
	movedPositions[0] = -2.0f;
	AssetConverterResult changed;
	CHECK(CarMeshSerializer_Export("cache", "car_7_id_2", movedPositions, 4, kQuadIndices, 2,
		NULL, 0, &changed, error, sizeof(error)) == ASSET_CONVERTER_OK);
	CHECK(changed.converted == 1 && changed.reused == 0);
	CHECK(strcmp(changed.contentHash, first.contentHash) != 0);
	CHECK(strcmp(changed.glbPath, first.glbPath) != 0);
}

static void TestModsKeySensitivity()
{
	_mkdir("cache");
	char error[256];
	const char* alpha[] = { "alpha" };
	const char* beta[] = { "beta" };
	const char* alphaBeta[] = { "alpha", "beta" };

	AssetConverterResult a, b, ab, none;
	CHECK(CarMeshSerializer_Export("cache", "car_9_id_4", kQuadPositions, 4, kQuadIndices, 2,
		alpha, 1, &a, error, sizeof(error)) == ASSET_CONVERTER_OK);
	CHECK(CarMeshSerializer_Export("cache", "car_9_id_4", kQuadPositions, 4, kQuadIndices, 2,
		beta, 1, &b, error, sizeof(error)) == ASSET_CONVERTER_OK);
	CHECK(CarMeshSerializer_Export("cache", "car_9_id_4", kQuadPositions, 4, kQuadIndices, 2,
		alphaBeta, 2, &ab, error, sizeof(error)) == ASSET_CONVERTER_OK);
	CHECK(CarMeshSerializer_Export("cache", "car_9_id_4", kQuadPositions, 4, kQuadIndices, 2,
		NULL, 0, &none, error, sizeof(error)) == ASSET_CONVERTER_OK);
	CHECK(a.converted == 1 && b.converted == 1 && ab.converted == 1 && none.converted == 1);

	// Every mods list is a distinct key, so every export publishes its own path.
	CHECK(strcmp(a.glbPath, b.glbPath) != 0);
	CHECK(strcmp(a.glbPath, ab.glbPath) != 0);
	CHECK(strcmp(a.glbPath, none.glbPath) != 0);

	ConversionCacheKey keyAlpha = ExpectedCarKey("car_9_id_4", kQuadPositions, 4, kQuadIndices, 2, alpha, 1);
	ConversionCacheKey keyBeta = ExpectedCarKey("car_9_id_4", kQuadPositions, 4, kQuadIndices, 2, beta, 1);
	ConversionCacheKey keyAlphaBeta = ExpectedCarKey("car_9_id_4", kQuadPositions, 4, kQuadIndices, 2, alphaBeta, 2);
	ConversionCacheKey keyNone = ExpectedCarKey("car_9_id_4", kQuadPositions, 4, kQuadIndices, 2, NULL, 0);
	CHECK(ExpectedPathMatches("cache", keyAlpha, a.glbPath));
	CHECK(ExpectedPathMatches("cache", keyBeta, b.glbPath));
	CHECK(ExpectedPathMatches("cache", keyAlphaBeta, ab.glbPath));
	CHECK(ExpectedPathMatches("cache", keyNone, none.glbPath));

	// The canonical key text (the hash) differs only by the mods digest.
	char alphaText[CONVERSION_CACHE_KEY_CAPACITY];
	char betaText[CONVERSION_CACHE_KEY_CAPACITY];
	CHECK(ConversionCache_FormatKey(&keyAlpha, alphaText, sizeof(alphaText)) > 0);
	CHECK(ConversionCache_FormatKey(&keyBeta, betaText, sizeof(betaText)) > 0);
	CHECK(strcmp(alphaText, betaText) != 0);
	CHECK(ConversionCache_Staleness(&keyAlpha, &keyBeta) == CONVERSION_CACHE_STALE_MODS);
	CHECK(ConversionCache_Staleness(&keyAlpha, &keyAlphaBeta) == CONVERSION_CACHE_STALE_MODS);
	CHECK(ConversionCache_Staleness(&keyAlpha, &keyNone) == CONVERSION_CACHE_STALE_MODS);

	// Repeating a mods list reuses its own cached entry.
	AssetConverterResult aAgain;
	CHECK(CarMeshSerializer_Export("cache", "car_9_id_4", kQuadPositions, 4, kQuadIndices, 2,
		alpha, 1, &aAgain, error, sizeof(error)) == ASSET_CONVERTER_OK);
	CHECK(aAgain.reused == 1 && strcmp(aAgain.glbPath, a.glbPath) == 0);

	// The artifact bytes describe the geometry only, so identical geometry has
	// an equal content hash while the cache keys and paths stay distinct.
	CHECK(strcmp(a.contentHash, b.contentHash) == 0);
}

static void TestInvalidArguments()
{
	char error[256];
	AssetConverterResult result;

	CHECK(CarMeshSerializer_Export(NULL, "car_1_id_0", kQuadPositions, 4, kQuadIndices, 2,
		NULL, 0, &result, error, sizeof(error)) == ASSET_CONVERTER_ERR_ARGUMENT);
	CHECK(error[0] != '\0');

	CHECK(CarMeshSerializer_Export("", "car_1_id_0", kQuadPositions, 4, kQuadIndices, 2,
		NULL, 0, &result, error, sizeof(error)) == ASSET_CONVERTER_ERR_ARGUMENT);
	CHECK(error[0] != '\0');

	CHECK(CarMeshSerializer_Export("cache", "car_1_id_0", NULL, 4, kQuadIndices, 2,
		NULL, 0, &result, error, sizeof(error)) == ASSET_CONVERTER_ERR_ARGUMENT);
	CHECK(error[0] != '\0');

	CHECK(CarMeshSerializer_Export("cache", "car_1_id_0", kQuadPositions, 0, kQuadIndices, 2,
		NULL, 0, &result, error, sizeof(error)) == ASSET_CONVERTER_ERR_EMPTY_MESH);
	CHECK(error[0] != '\0');

	CHECK(CarMeshSerializer_Export("cache", "car_1_id_0", kQuadPositions, 4, NULL, 2,
		NULL, 0, &result, error, sizeof(error)) == ASSET_CONVERTER_ERR_ARGUMENT);
	CHECK(error[0] != '\0');

	CHECK(CarMeshSerializer_Export("cache", "car_1_id_0", kQuadPositions, 4, kQuadIndices, 0,
		NULL, 0, &result, error, sizeof(error)) == ASSET_CONVERTER_ERR_EMPTY_MESH);
	CHECK(error[0] != '\0');

	CHECK(CarMeshSerializer_Export("cache", "car_1_id_0", kQuadPositions, 4, kQuadIndices, 2,
		NULL, 0, NULL, error, sizeof(error)) == ASSET_CONVERTER_ERR_ARGUMENT);

	// Counts whose hash input would exceed the int length are rejected before
	// any geometry is read (the pointers below are far too small for them).
	CHECK(CarMeshSerializer_Export("cache", "car_1_id_0", kQuadPositions, 200000000, kQuadIndices, 2,
		NULL, 0, &result, error, sizeof(error)) == ASSET_CONVERTER_ERR_ARGUMENT);
	CHECK(error[0] != '\0');
	CHECK(CarMeshSerializer_Export("cache", "car_1_id_0", kQuadPositions, 4, kQuadIndices, 200000000,
		NULL, 0, &result, error, sizeof(error)) == ASSET_CONVERTER_ERR_ARGUMENT);
	CHECK(error[0] != '\0');

	// A NULL diagnostic buffer must not crash.
	CHECK(CarMeshSerializer_Export(NULL, NULL, NULL, 0, NULL, 0, NULL, 0, &result, NULL, 0) == ASSET_CONVERTER_ERR_ARGUMENT);
}

static void TestMissingCacheDirectory()
{
	char error[256] = "";
	AssetConverterResult result;
	CHECK(CarMeshSerializer_Export("missing-cache-root/nested", "car_7_id_2",
		kQuadPositions, 4, kQuadIndices, 2, NULL, 0, &result, error, sizeof(error)) == ASSET_CONVERTER_ERR_WRITE);
	CHECK(error[0] != '\0');
	CHECK(!FileExists(result.glbPath));
	CHECK(result.glbPath[0] == '\0'); // failure contract: the result is zeroed
}

static void TestNullDiagnosticBuffer()
{
	_mkdir("cache");
	AssetConverterResult result;

	// A valid conversion must succeed and publish even when the caller supplies
	// no diagnostic buffer (the serializer uses a local scratch buffer).
	CHECK(CarMeshSerializer_Export("cache", "car_3_id_1", kQuadPositions, 4, kQuadIndices, 2,
		NULL, 0, &result, NULL, 0) == ASSET_CONVERTER_OK);
	CHECK(result.converted == 1 && result.reused == 0);
	CHECK(result.glbPath[0] != '\0' && result.contentHash[0] != '\0');
	CHECK(FileExists(result.glbPath));

	// A failure with no diagnostic buffer still returns a named code and a
	// zeroed result.
	CHECK(CarMeshSerializer_Export("cache", "car_3_id_1", NULL, 4, kQuadIndices, 2,
		NULL, 0, &result, NULL, 0) == ASSET_CONVERTER_ERR_ARGUMENT);
	CHECK(result.glbPath[0] == '\0');
}

int main()
{
	TestDeterministicReuse();
	TestModsKeySensitivity();
	TestInvalidArguments();
	TestMissingCacheDirectory();
	TestNullDiagnosticBuffer();

	printf("%d checks, %d failures\n", g_checks, g_failures);
	return g_failures == 0 ? 0 : 1;
}
