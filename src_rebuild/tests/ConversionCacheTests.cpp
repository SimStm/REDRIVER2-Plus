// Standalone regression tests for the E02 conversion cache contract.
//
// The module is pure data (no PsyCross, GPU or game state), so keying, parsing,
// staleness reasons and deterministic cache paths can be checked in isolation.
// Build and run instructions are in tests/README.md.

#include "../Game/C/conversioncache.h"

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

static void TestHashing()
{
	const char a[] = "source bytes";
	const char b[] = "source bytes";
	const char c[] = "source bytez";
	CHECK(ConversionCache_HashBytes(a, (int)strlen(a)) == ConversionCache_HashBytes(b, (int)strlen(b)));
	CHECK(ConversionCache_HashBytes(a, (int)strlen(a)) != ConversionCache_HashBytes(c, (int)strlen(c)));
	CHECK(ConversionCache_HashBytes(NULL, 0) == ConversionCache_HashBytes(NULL, -1));
}

static void TestKeyRoundTrip()
{
	ConversionCacheKey key;
	ConversionCache_MakeKey(&key, "mesh.glb", 0x0123456789abcdefULL, 0xfedcba9876543210ULL, 0x0000000000000001ULL, 3);

	char text[CONVERSION_CACHE_KEY_CAPACITY];
	const int length = ConversionCache_FormatKey(&key, text, sizeof(text));
	CHECK(length > 0);
	CHECK(strcmp(text, "mesh.glb|0123456789abcdef|fedcba9876543210|0000000000000001|3") == 0);

	ConversionCacheKey parsed;
	char error[CONVERSION_CACHE_ERROR_CAPACITY];
	CHECK(ConversionCache_ParseKey(&parsed, text, error, sizeof(error)) == 1);
	CHECK(strcmp(parsed.converter, key.converter) == 0);
	CHECK(parsed.sourceHash == key.sourceHash);
	CHECK(parsed.optionsHash == key.optionsHash);
	CHECK(parsed.modsHash == key.modsHash);
	CHECK(parsed.schemaVersion == key.schemaVersion);

	// Two identical inputs produce the same canonical key.
	ConversionCacheKey same;
	ConversionCache_MakeKey(&same, "mesh.glb", 0x0123456789abcdefULL, 0xfedcba9876543210ULL, 0x0000000000000001ULL, 3);
	char text2[CONVERSION_CACHE_KEY_CAPACITY];
	CHECK(ConversionCache_FormatKey(&same, text2, sizeof(text2)) == length);
	CHECK(strcmp(text, text2) == 0);

	// A too-small buffer is an explicit failure.
	CHECK(ConversionCache_FormatKey(&key, text, 8) < 0);
}

static void TestParseDiagnostics()
{
	ConversionCacheKey key;
	char error[CONVERSION_CACHE_ERROR_CAPACITY];

	CHECK(ConversionCache_ParseKey(&key, NULL, error, sizeof(error)) == 0);
	CHECK(ConversionCache_ParseKey(&key, "", error, sizeof(error)) == 0);
	CHECK(ConversionCache_ParseKey(&key, "mesh.glb|short|0|0|1", error, sizeof(error)) == 0);
	CHECK(ConversionCache_ParseKey(&key, "mesh.glb|0123456789abcdef|fedcba9876543210|0000000000000001", error, sizeof(error)) == 0);
	CHECK(ConversionCache_ParseKey(&key, "mesh.glb|0123456789abcdef|fedcba9876543210|0000000000000001|0", error, sizeof(error)) == 0);
	CHECK(ConversionCache_ParseKey(&key, "mesh.glb|0123456789abcdeX|fedcba9876543210|0000000000000001|1", error, sizeof(error)) == 0);
	CHECK(error[0] != '\0');
}

static void TestStaleness()
{
	ConversionCacheKey cached, current;
	ConversionCache_MakeKey(&cached, "mesh.glb", 10, 20, 30, 1);
	ConversionCache_MakeKey(&current, "mesh.glb", 10, 20, 30, 1);
	CHECK(ConversionCache_Staleness(&cached, &current) == CONVERSION_CACHE_MATCH);

	ConversionCache_MakeKey(&current, "mesh.glb", 11, 20, 30, 1);
	CHECK(ConversionCache_Staleness(&cached, &current) == CONVERSION_CACHE_STALE_SOURCE);

	ConversionCache_MakeKey(&current, "mesh.glb", 10, 21, 30, 1);
	CHECK(ConversionCache_Staleness(&cached, &current) == CONVERSION_CACHE_STALE_OPTIONS);

	ConversionCache_MakeKey(&current, "mesh.glb", 10, 20, 31, 1);
	CHECK(ConversionCache_Staleness(&cached, &current) == CONVERSION_CACHE_STALE_MODS);

	ConversionCache_MakeKey(&current, "mesh.glb", 10, 20, 30, 2);
	CHECK(ConversionCache_Staleness(&cached, &current) == CONVERSION_CACHE_STALE_SCHEMA);

	ConversionCache_MakeKey(&current, "sound.wav", 10, 20, 30, 1);
	CHECK(ConversionCache_Staleness(&cached, &current) == CONVERSION_CACHE_STALE_CONVERTER);

	CHECK(ConversionCache_Staleness(NULL, &current) == CONVERSION_CACHE_INVALID);

	CHECK(strcmp(ConversionCache_StalenessName(CONVERSION_CACHE_MATCH), "match") == 0);
	CHECK(strcmp(ConversionCache_StalenessName(CONVERSION_CACHE_STALE_SCHEMA), "stale-schema") == 0);
	CHECK(strcmp(ConversionCache_StalenessName(CONVERSION_CACHE_STALE_MODS), "stale-mods") == 0);
}

static void TestCachePath()
{
	ConversionCacheKey key;
	ConversionCache_MakeKey(&key, "mesh.glb", 0xabcdefabcdefabcdULL, 1, 2, 1);

	char path[CONVERSION_CACHE_PATH_CAPACITY];
	CHECK(ConversionCache_MakeCachePath("cache", &key, path, sizeof(path)) == 1);
	CHECK(strcmp(path, "cache/mesh.glb_abcdefabcdefabcd_0000000000000001_0000000000000002_1.bin") == 0);

	// A trailing slash does not double up.
	char path2[CONVERSION_CACHE_PATH_CAPACITY];
	CHECK(ConversionCache_MakeCachePath("cache/", &key, path2, sizeof(path2)) == 1);
	CHECK(strcmp(path, path2) == 0);

	// A different key produces a different path.
	ConversionCacheKey other;
	ConversionCache_MakeKey(&other, "mesh.glb", 0xabcdefabcdefabcdULL, 1, 2, 2);
	char path3[CONVERSION_CACHE_PATH_CAPACITY];
	CHECK(ConversionCache_MakeCachePath("cache", &other, path3, sizeof(path3)) == 1);
	CHECK(strcmp(path, path3) != 0);

	// A too-small buffer fails rather than truncating.
	CHECK(ConversionCache_MakeCachePath("cache", &key, path, 8) == 0);
}

int main()
{
	TestHashing();
	TestKeyRoundTrip();
	TestParseDiagnostics();
	TestStaleness();
	TestCachePath();

	printf("%d checks, %d failures\n", g_checks, g_failures);
	return g_failures == 0 ? 0 : 1;
}
