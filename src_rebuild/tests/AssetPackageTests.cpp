// Standalone regression tests for the E02 native asset package manifest.
//
// The package module is pure data (no PsyCross, GPU or game state), so package
// loading, canonical serialization and diagnostics can be checked in isolation.
// Build and run instructions are in tests/README.md.

#include "../Game/C/assetpackage.h"

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

static const char* kValid =
	"{\n"
	"  \"schemaVersion\": 1,\n"
	"  \"id\": \"chicago.slice\",\n"
	"  \"entries\": [\n"
	"    { \"id\": \"model:0:0:37\", \"type\": \"model\", \"file\": \"models/37.glb\", \"contentHash\": \"0000000000000037\", \"dependencies\": [\"tex:asphalt:3:5\"] },\n"
	"    { \"id\": \"tex:asphalt:3:5\", \"type\": \"texture\", \"file\": \"textures/asphalt_p3_i5.png\", \"contentHash\": \"00000000000000ab\", \"dependencies\": [] }\n"
	"  ]\n"
	"}\n";

static int LoadValid(AssetPackage* package, char* error, int errorCapacity)
{
	return AssetPackage_LoadFromText(package, kValid, -1, error, errorCapacity);
}

static void TestValidPackage()
{
	static AssetPackage package;
	char error[ASSET_PACKAGE_ERROR_CAPACITY];
	CHECK(LoadValid(&package, error, sizeof(error)) == ASSET_PACKAGE_OK);
	CHECK(package.schemaVersion == ASSET_PACKAGE_SCHEMA_VERSION);
	CHECK(strcmp(package.id, "chicago.slice") == 0);
	CHECK(package.entryCount == 2);
	CHECK(AssetPackage_FindEntry(&package, "model:0:0:37") == 0);
	CHECK(AssetPackage_FindEntry(&package, "tex:asphalt:3:5") == 1);
	CHECK(AssetPackage_FindEntry(&package, "missing") < 0);
	CHECK(strcmp(package.entries[0].type, "model") == 0);
	CHECK(strcmp(package.entries[0].file, "models/37.glb") == 0);
	CHECK(strcmp(package.entries[0].contentHash, "0000000000000037") == 0);
	CHECK(package.entries[0].dependencyCount == 1);
	CHECK(strcmp(package.entries[0].dependencies[0], "tex:asphalt:3:5") == 0);
	CHECK(package.entries[1].dependencyCount == 0);
}

static void TestCanonicalSerializationIsReproducible()
{
	static AssetPackage a, b;
	char error[ASSET_PACKAGE_ERROR_CAPACITY];
	CHECK(LoadValid(&a, error, sizeof(error)) == ASSET_PACKAGE_OK);
	CHECK(LoadValid(&b, error, sizeof(error)) == ASSET_PACKAGE_OK);

	char outA[2048], outB[2048];
	const int lengthA = AssetPackage_SerializeCanonical(&a, outA, sizeof(outA));
	const int lengthB = AssetPackage_SerializeCanonical(&b, outB, sizeof(outB));
	CHECK(lengthA > 0 && lengthA == lengthB);
	CHECK(strcmp(outA, outB) == 0);

	// Serializing twice from one package is stable too.
	char outA2[2048];
	CHECK(AssetPackage_SerializeCanonical(&a, outA2, sizeof(outA2)) == lengthA);
	CHECK(strcmp(outA, outA2) == 0);

	// A reloaded canonical manifest round-trips to the same bytes.
	static AssetPackage reloaded;
	CHECK(AssetPackage_LoadFromText(&reloaded, outA, lengthA, error, sizeof(error)) == ASSET_PACKAGE_OK);
	char outReloaded[2048];
	CHECK(AssetPackage_SerializeCanonical(&reloaded, outReloaded, sizeof(outReloaded)) == lengthA);
	CHECK(strcmp(outA, outReloaded) == 0);

	// Too small a buffer is an explicit failure, not a truncation.
	CHECK(AssetPackage_SerializeCanonical(&a, outA, 8) < 0);
}

static void TestDiagnostics()
{
	static AssetPackage package;
	char error[ASSET_PACKAGE_ERROR_CAPACITY];

	CHECK(AssetPackage_LoadFromText(&package, NULL, -1, error, sizeof(error)) == ASSET_PACKAGE_ERR_ARGUMENT);
	CHECK(AssetPackage_LoadFromText(&package, "{ \"schemaVersion\": 1", -1, error, sizeof(error)) == ASSET_PACKAGE_ERR_SYNTAX);
	CHECK(AssetPackage_LoadFromText(&package, "not json", -1, error, sizeof(error)) == ASSET_PACKAGE_ERR_SYNTAX);
	CHECK(AssetPackage_LoadFromText(&package, "{ \"schemaVersion\": 1, \"id\": \"x\", \"entries\": [", -1, error, sizeof(error)) == ASSET_PACKAGE_ERR_SYNTAX);

	CHECK(AssetPackage_LoadFromText(&package,
		"{ \"schemaVersion\": 99, \"id\": \"x\", \"entries\": [] }", -1, error, sizeof(error)) == ASSET_PACKAGE_ERR_SCHEMA_VERSION);
	CHECK(AssetPackage_LoadFromText(&package,
		"{ \"id\": \"x\", \"entries\": [] }", -1, error, sizeof(error)) == ASSET_PACKAGE_ERR_MISSING_FIELD);
	CHECK(AssetPackage_LoadFromText(&package,
		"{ \"schemaVersion\": 1, \"entries\": [] }", -1, error, sizeof(error)) == ASSET_PACKAGE_ERR_MISSING_FIELD);
	CHECK(AssetPackage_LoadFromText(&package,
		"{ \"schemaVersion\": 1, \"id\": \"x\", \"entries\": [ { \"id\": \"a\", \"type\": \"model\" } ] }", -1, error, sizeof(error)) == ASSET_PACKAGE_ERR_MISSING_FIELD);
	CHECK(AssetPackage_LoadFromText(&package,
		"{ \"schemaVersion\": 1, \"id\": \"x\", \"entries\": [ { \"id\": \"a\", \"type\": \"model\", \"file\": \"a.glb\" }, { \"id\": \"a\", \"type\": \"model\", \"file\": \"b.glb\" } ] }", -1, error, sizeof(error)) == ASSET_PACKAGE_ERR_DUPLICATE_ID);
	CHECK(AssetPackage_LoadFromText(&package,
		"{ \"schemaVersion\": 1, \"id\": \"x\", \"entries\": [ { \"id\": \"a\", \"type\": \"model\", \"file\": \"a.glb\", \"dependencies\": [\"ghost\"] } ] }", -1, error, sizeof(error)) == ASSET_PACKAGE_ERR_UNKNOWN_DEPENDENCY);

	// A failed load reports a named diagnostic and leaves the package empty.
	CHECK(error[0] != '\0');
	CHECK(strcmp(AssetPackage_ErrorName(ASSET_PACKAGE_ERR_DUPLICATE_ID), "duplicate-id") == 0);
	CHECK(package.entryCount == 0 && package.id[0] == '\0');

	// Unknown top-level and entry keys are skipped, not rejected.
	CHECK(AssetPackage_LoadFromText(&package,
		"{ \"schemaVersion\": 1, \"id\": \"x\", \"future\": { \"a\": [1, 2] }, \"entries\": [ { \"id\": \"a\", \"type\": \"model\", \"file\": \"a.glb\", \"extra\": 7 } ] }",
		-1, error, sizeof(error)) == ASSET_PACKAGE_OK);
	CHECK(package.entryCount == 1);

	// A trailing document after the object is rejected.
	CHECK(AssetPackage_LoadFromText(&package,
		"{ \"schemaVersion\": 1, \"id\": \"x\", \"entries\": [] } trailing", -1, error, sizeof(error)) == ASSET_PACKAGE_ERR_SYNTAX);
}

int main()
{
	TestValidPackage();
	TestCanonicalSerializationIsReproducible();
	TestDiagnostics();

	printf("%d checks, %d failures\n", g_checks, g_failures);
	return g_failures == 0 ? 0 : 1;
}
