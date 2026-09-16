// Standalone regression tests for the source-aware asset catalog.
//
// No game data, PsyCross, or OpenGL context is required: the catalog is a pure
// data module, so the acceptance rules for roadmap item 04 can be checked in
// isolation. Build and run instructions are in tests/README.md.

#include "../Game/C/assetcatalog.h"

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

static bool TextIs(const char* value, const char* expected)
{
	return value != NULL && strcmp(value, expected) == 0;
}

static void TestLifetimeAndContext()
{
	AssetCatalog_Reset();
	CHECK(!AssetCatalog_IsValid());

	const unsigned int generation = AssetCatalog_GetGeneration();

	AssetCatalog_BeginContext(0, 0, 0);
	CHECK(AssetCatalog_IsValid());
	CHECK(AssetCatalog_GetGeneration() > generation);

	AssetCatalogContext context;
	CHECK(AssetCatalog_GetContext(&context));
	CHECK(context.level == 0 && context.variant == 0 && context.multiplayer == 0);

	// Same context keeps records; a different context resets them.
	AssetCatalog_RegisterModel(7, "ROAD", ASSET_CATALOG_SOURCE_DECLARED, -1, -1);
	AssetCatalog_BeginContext(0, 0, 0);
	CHECK(AssetCatalog_FindModel(7) >= 0);

	AssetCatalog_BeginContext(1, 1, 0);
	CHECK(AssetCatalog_FindModel(7) < 0);
	CHECK(AssetCatalog_IsValid());
}

static void TestModelIdentityAndInstances()
{
	AssetCatalog_Reset();
	AssetCatalog_BeginContext(2, 1, 0);

	const int road = AssetCatalog_RegisterModel(10, "ROADTILE", ASSET_CATALOG_SOURCE_DECLARED, -1, 11);
	const int carLod = AssetCatalog_RegisterModel(11, "CARTILE", ASSET_CATALOG_SOURCE_UNKNOWN, 10, -1);
	CHECK(road >= 0 && carLod >= 0);

	int modelIndex = -1;
	int lodParent = -2;
	int highDetail = -2;
	char name[ASSET_CATALOG_NAME_CAPACITY];
	AssetCatalogSource source = ASSET_CATALOG_SOURCE_UNKNOWN;
	CHECK(AssetCatalog_GetModel(road, &modelIndex, name, sizeof(name), &source, &lodParent, &highDetail));
	CHECK(modelIndex == 10);
	CHECK(TextIs(name, "ROADTILE"));
	CHECK(source == ASSET_CATALOG_SOURCE_DECLARED);
	CHECK(lodParent == -1 && highDetail == 11);

	char modelId[ASSET_CATALOG_ID_CAPACITY];
	CHECK(AssetCatalog_MakeModelId(10, modelId, sizeof(modelId)));
	CHECK(TextIs(modelId, "model:2:1:10"));

	// Two handles to one shared model both resolve; the resource is shared.
	AssetCatalogHandle first = AssetCatalog_GetModelHandle(road);
	AssetCatalogHandle second = AssetCatalog_GetModelHandle(road);
	CHECK(AssetCatalog_HandleValid(first));
	CHECK(AssetCatalog_HandleValid(second));
	CHECK(first.index == second.index);

	// Reusing the runtime slot must invalidate the retained handle.
	CHECK(AssetCatalog_RegisterModel(10, "OTHER", ASSET_CATALOG_SOURCE_DECLARED, -1, -1) == road);
	CHECK(!AssetCatalog_HandleValid(first));
	CHECK(!AssetCatalog_HandleValid(second));
	CHECK(AssetCatalog_HandleValid(AssetCatalog_GetModelHandle(road)));
	CHECK(AssetCatalog_GetModel(road, &modelIndex, name, sizeof(name), NULL, NULL, NULL));
	CHECK(TextIs(name, "OTHER"));
}

static void TestTextureDedupAndMaterials()
{
	AssetCatalog_Reset();
	AssetCatalog_BeginContext(0, 0, 0);

	const int roadA = AssetCatalog_RegisterModel(1, "ROADA", ASSET_CATALOG_SOURCE_DECLARED, -1, -1);
	const int roadB = AssetCatalog_RegisterModel(2, "ROADB", ASSET_CATALOG_SOURCE_DECLARED, -1, -1);

	const int asphalt = AssetCatalog_RegisterTexture("asphalt", 3, 5, 3, ASSET_CATALOG_SOURCE_DECLARED);
	const int shared = AssetCatalog_RegisterTexture("shared", 4, 1, 4, ASSET_CATALOG_SOURCE_UNKNOWN);
	CHECK(asphalt >= 0 && shared >= 0);

	// The same manifest triple is one record, not several.
	CHECK(AssetCatalog_RegisterTexture("asphalt", 3, 5, 3, ASSET_CATALOG_SOURCE_DECLARED) == asphalt);
	CHECK(AssetCatalog_FindTexture("asphalt", 3, 5) == asphalt);
	CHECK(AssetCatalog_FindTexture("asphalt", 3, 6) < 0);

	// Both models may reference the shared texture without duplicating it.
	CHECK(AssetCatalog_AddModelTexture(roadA, shared));
	CHECK(AssetCatalog_AddModelTexture(roadB, shared));
	CHECK(!AssetCatalog_AddModelTexture(roadA, shared)); // duplicate pair
	CHECK(AssetCatalog_AddModelTexture(roadA, asphalt));

	CHECK(AssetCatalog_CountTextureModels(shared) == 2);

	int textures[8];
	CHECK(AssetCatalog_EnumerateModelTextures(roadA, textures, 8) == 2);
	CHECK(AssetCatalog_EnumerateModelTextures(roadB, textures, 8) == 1);
	CHECK(textures[0] == shared);

	// Provenance is explicit.
	char textureName[ASSET_CATALOG_NAME_CAPACITY];
	int page = -1, index = -1, set = -1;
	AssetCatalogSource source = ASSET_CATALOG_SOURCE_DECLARED;
	CHECK(AssetCatalog_GetTexture(shared, textureName, sizeof(textureName), &page, &index, &set, &source));
	CHECK(TextIs(textureName, "shared"));
	CHECK(page == 4 && index == 1 && set == 4);
	CHECK(source == ASSET_CATALOG_SOURCE_UNKNOWN);

	char textureId[ASSET_CATALOG_ID_CAPACITY];
	CHECK(AssetCatalog_MakeTextureId("asphalt", 3, 5, textureId, sizeof(textureId)));
	CHECK(TextIs(textureId, "tex:asphalt:3:5"));

	// Enumerating into a small buffer reports the true count.
	CHECK(AssetCatalog_EnumerateModelTextures(roadA, NULL, 0) == 2);
}

static void TestSlotInvalidation()
{
	AssetCatalog_Reset();
	AssetCatalog_BeginContext(0, 0, 0);

	const int record = AssetCatalog_RegisterModel(5, "ROAD", ASSET_CATALOG_SOURCE_DECLARED, -1, -1);
	CHECK(record >= 0);

	AssetCatalogHandle handle = AssetCatalog_GetModelHandle(record);
	CHECK(AssetCatalog_HandleValid(handle));
	CHECK(AssetCatalog_FindModel(5) == record);

	// Streaming frees the slot: the retained handle goes stale.
	CHECK(AssetCatalog_InvalidateModel(5));
	CHECK(!AssetCatalog_HandleValid(handle));
	CHECK(AssetCatalog_FindModel(5) < 0);
	CHECK(!AssetCatalog_InvalidateModel(5));

	// Re-registering the slot reuses the record but bumps the revision, so the
	// old handle stays invalid even before any generation change.
	const int reused = AssetCatalog_RegisterModel(5, "OTHER", ASSET_CATALOG_SOURCE_DECLARED, -1, -1);
	CHECK(reused == record);
	CHECK(!AssetCatalog_HandleValid(handle));

	AssetCatalogHandle fresh = AssetCatalog_GetModelHandle(reused);
	CHECK(AssetCatalog_HandleValid(fresh));
	CHECK(fresh.revision != handle.revision);

	// A level/variant change invalidates every handle through the generation.
	const unsigned int generation = AssetCatalog_GetGeneration();
	AssetCatalog_BeginContext(1, 1, 0);
	CHECK(AssetCatalog_GetGeneration() > generation);
	CHECK(!AssetCatalog_HandleValid(fresh));
	CHECK(AssetCatalog_FindModel(5) < 0);
}

static void TestCarsAndPalettes()
{
	AssetCatalog_Reset();
	AssetCatalog_BeginContext(0, 0, 0);

	const int model = AssetCatalog_RegisterModel(3, "SEDAN", ASSET_CATALOG_SOURCE_DECLARED, -1, -1);
	const int car = AssetCatalog_RegisterCar(2, 3, "sedan", ASSET_CATALOG_SOURCE_DECLARED);
	CHECK(car >= 0);

	CHECK(AssetCatalog_LinkCarToModel(car, model));
	CHECK(AssetCatalog_AddCarPalette(car, 0));
	CHECK(AssetCatalog_AddCarPalette(car, 1));
	CHECK(!AssetCatalog_AddCarPalette(car, 1));

	int palettes[4];
	CHECK(AssetCatalog_EnumerateCarPalettes(car, palettes, 4) == 2);

	// Reusing the car slot changes the record but not the slot identity.
	CHECK(AssetCatalog_RegisterCar(2, 9, "limo", ASSET_CATALOG_SOURCE_UNKNOWN) == car);

	int slot = -1, modelNumber = -1;
	char name[ASSET_CATALOG_NAME_CAPACITY];
	CHECK(AssetCatalog_GetCar(car, &slot, &modelNumber, name, sizeof(name), NULL));
	CHECK(slot == 2 && modelNumber == 9);
	CHECK(TextIs(name, "limo"));
}

static void TestDiagnostics()
{
	int modelCap = 0, textureCap = 0, carCap = 0, refCap = 0;
	AssetCatalog_GetCapacities(&modelCap, &textureCap, &carCap, &refCap);
	CHECK(modelCap > 0 && textureCap > 0 && carCap > 0 && refCap > 0);
	CHECK(AssetCatalog_GetCapacityBytes() > 0);

	AssetCatalog_Reset();
	AssetCatalog_BeginContext(0, 0, 0);
	AssetCatalog_RegisterModel(1, "A", ASSET_CATALOG_SOURCE_DECLARED, -1, -1);
	AssetCatalog_RegisterTexture("tex", 0, 0, 0, ASSET_CATALOG_SOURCE_DECLARED);

	int models = 0, textures = 0, cars = 0, refs = 0;
	AssetCatalog_GetStats(&models, &textures, &cars, &refs);
	CHECK(models == 1 && textures == 1 && cars == 0 && refs == 0);

	// Reported completeness must never exceed the stated capacity.
	CHECK(modelCap >= models && textureCap >= textures && carCap >= cars && refCap >= refs);
}

static void TestBoundsAreGraceful()
{
	AssetCatalog_Reset();
	AssetCatalog_BeginContext(0, 0, 0);

	// Invalid inputs fail without touching storage.
	CHECK(AssetCatalog_RegisterModel(-1, "X", ASSET_CATALOG_SOURCE_UNKNOWN, -1, -1) < 0);
	CHECK(AssetCatalog_RegisterTexture("x", -1, 0, 0, ASSET_CATALOG_SOURCE_UNKNOWN) < 0);
	CHECK(!AssetCatalog_AddModelTexture(-1, -1));
	CHECK(!AssetCatalog_MakeTextureId("x", 0, 0, NULL, 0));

	int models = -1, textures = -1, cars = -1, refs = -1;
	AssetCatalog_GetStats(&models, &textures, &cars, &refs);
	CHECK(models == 0 && textures == 0 && cars == 0 && refs == 0);
}

int main()
{
	TestLifetimeAndContext();
	TestModelIdentityAndInstances();
	TestSlotInvalidation();
	TestTextureDedupAndMaterials();
	TestCarsAndPalettes();
	TestDiagnostics();
	TestBoundsAreGraceful();

	printf("%d checks, %d failures\n", g_checks, g_failures);
	return g_failures == 0 ? 0 : 1;
}
