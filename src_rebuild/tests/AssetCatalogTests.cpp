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

	// A source export covers the selected model and its high-detail LOD
	// sibling; the reverse (lower-detail) link is not stored.
	int exportModels[4];
	CHECK(AssetCatalog_CollectExportModels(road, exportModels, 4) == 2);
	CHECK(exportModels[0] == road && exportModels[1] == carLod);
	CHECK(AssetCatalog_CollectExportModels(carLod, exportModels, 4) == 1 && exportModels[0] == carLod);
	CHECK(AssetCatalog_CollectExportModels(road, exportModels, 1) == 2 && exportModels[0] == road);
	CHECK(AssetCatalog_CollectExportModels(-1, exportModels, 4) == 0);

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

	// Enumerating the models that share a texture keeps every reference, so a
	// batch export can list them without duplicating the texture record.
	int sharedModels[4];
	CHECK(AssetCatalog_EnumerateTextureModels(shared, sharedModels, 4) == 2);
	CHECK(AssetCatalog_EnumerateTextureModels(shared, NULL, 0) == 2);
	AssetCatalog_EnumerateTextureModels(shared, sharedModels, 1);
	CHECK(sharedModels[0] == roadA);
	CHECK(AssetCatalog_EnumerateTextureModels(asphalt, sharedModels, 4) == 1 && sharedModels[0] == roadA);
	CHECK(AssetCatalog_EnumerateTextureModels(-1, sharedModels, 4) == 0);

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

static void TestComponentIdentity()
{
	char key[ASSET_CATALOG_ID_CAPACITY] = {};
	const char* carKey = "car:0:0:3:2:12";

	// A component key is the parent instance key plus a stable component suffix.
	CHECK(AssetCatalog_MakeComponentKey(carKey, "wheel", 0, key, sizeof(key)));
	CHECK(TextIs(key, "car:0:0:3:2:12/component:wheel:0"));
	CHECK(AssetCatalog_IsComponentKey(key));
	CHECK(!AssetCatalog_IsComponentKey(carKey));

	// It round-trips into the same parent, kind and index.
	char parent[ASSET_CATALOG_ID_CAPACITY] = {};
	char kind[ASSET_CATALOG_NAME_CAPACITY] = {};
	int index = -1;
	CHECK(AssetCatalog_ParseComponentKey(key, parent, sizeof(parent), kind, sizeof(kind), &index));
	CHECK(TextIs(parent, carKey));
	CHECK(TextIs(kind, "wheel"));
	CHECK(index == 0);
	CHECK(AssetCatalog_ParseComponentKey(key, NULL, 0, NULL, 0, NULL));

	// Pedestrian parts use the same encoding with a different kind.
	CHECK(AssetCatalog_MakeComponentKey("ped:0:0:7:2", "bone", 11, key, sizeof(key)));
	CHECK(TextIs(key, "ped:0:0:7:2/component:bone:11"));
	CHECK(AssetCatalog_ParseComponentKey(key, parent, sizeof(parent), kind, sizeof(kind), &index));
	CHECK(TextIs(parent, "ped:0:0:7:2") && TextIs(kind, "bone") && index == 11);

	// A parent may not itself be a component, and the kind may not contain the
	// separators, so a key can never be ambiguous.
	CHECK(!AssetCatalog_MakeComponentKey("car:0:0:3:2:12/component:wheel:0", "wheel", 1, key, sizeof(key)));
	CHECK(!AssetCatalog_MakeComponentKey(carKey, "bad:kind", 0, key, sizeof(key)));
	CHECK(!AssetCatalog_MakeComponentKey(carKey, "bad/kind", 0, key, sizeof(key)));
	CHECK(!AssetCatalog_MakeComponentKey(carKey, "", 0, key, sizeof(key)));
	CHECK(!AssetCatalog_MakeComponentKey("", "wheel", 0, key, sizeof(key)));
	CHECK(!AssetCatalog_MakeComponentKey(carKey, "wheel", -1, key, sizeof(key)));
	CHECK(!AssetCatalog_MakeComponentKey(NULL, "wheel", 0, key, sizeof(key)));
	CHECK(!AssetCatalog_MakeComponentKey(carKey, NULL, 0, key, sizeof(key)));
	CHECK(!AssetCatalog_MakeComponentKey(carKey, "wheel", 0, NULL, 0));

	// Parsing rejects plain, malformed and unsupported keys.
	CHECK(!AssetCatalog_ParseComponentKey(carKey, parent, sizeof(parent), kind, sizeof(kind), &index));
	CHECK(!AssetCatalog_ParseComponentKey(NULL, parent, sizeof(parent), kind, sizeof(kind), &index));
	CHECK(!AssetCatalog_ParseComponentKey("/component:wheel:0", parent, sizeof(parent), kind, sizeof(kind), &index));
	CHECK(!AssetCatalog_ParseComponentKey("car:1/component::0", parent, sizeof(parent), kind, sizeof(kind), &index));
	CHECK(!AssetCatalog_ParseComponentKey("car:1/component:wheel:0x", parent, sizeof(parent), kind, sizeof(kind), &index));
	CHECK(!AssetCatalog_ParseComponentKey("car:1/component:wheel:", parent, sizeof(parent), kind, sizeof(kind), &index));
	CHECK(!AssetCatalog_ParseComponentKey("car:1/component:wheel:-2", parent, sizeof(parent), kind, sizeof(kind), &index));
	CHECK(!AssetCatalog_ParseComponentKey("car:1/component:wheel:2junk", parent, sizeof(parent), kind, sizeof(kind), &index));

	// A buffer too small for the parent or kind fails instead of truncating.
	CHECK(AssetCatalog_MakeComponentKey(carKey, "wheel", 0, key, sizeof(key)));
	char tinyParent[4] = {};
	char tinyKind[4] = {};
	CHECK(!AssetCatalog_ParseComponentKey(key, tinyParent, sizeof(tinyParent), tinyKind, sizeof(tinyKind), &index));

	// A resource key (not an instance slot) is a valid parent too.
	CHECK(AssetCatalog_MakeComponentKey("model:0:0:37", "material", 3, key, sizeof(key)));
	CHECK(TextIs(key, "model:0:0:37/component:material:3"));
}

static void TestSelectionAnchors()
{
	AssetCatalog_Reset();
	AssetCatalog_BeginContext(0, 0, 0);

	// A building/tile key embeds the model id, so the anchor resolves a model
	// handle and stays valid while that slot is live.
	CHECK(AssetCatalog_RegisterModel(37, "ROAD", ASSET_CATALOG_SOURCE_DECLARED, -1, -1) >= 0);

	AssetCatalogAnchor anchor = {};
	CHECK(AssetCatalog_CaptureAnchor("building:model:0:0:37:12000:0:8000:0", &anchor));
	CHECK(anchor.valid && anchor.hasModel);
	CHECK(TextIs(anchor.key, "building:model:0:0:37:12000:0:8000:0"));
	CHECK(AssetCatalog_CheckAnchor(&anchor) == ASSET_CATALOG_SELECTION_VALID);

	// Streaming frees the slot: the retained selection must go stale, and stay
	// stale even after the slot is reused by a different model.
	CHECK(AssetCatalog_InvalidateModel(37));
	CHECK(AssetCatalog_CheckAnchor(&anchor) == ASSET_CATALOG_SELECTION_STALE);
	CHECK(AssetCatalog_RegisterModel(37, "OTHER", ASSET_CATALOG_SOURCE_DECLARED, -1, -1) >= 0);
	CHECK(AssetCatalog_CheckAnchor(&anchor) == ASSET_CATALOG_SELECTION_STALE);

	// A fresh pick re-anchors to the live slot.
	AssetCatalogAnchor fresh = {};
	CHECK(AssetCatalog_CaptureAnchor("tile:model:0:0:37:1:2:3:4", &fresh));
	CHECK(AssetCatalog_CheckAnchor(&fresh) == ASSET_CATALOG_SELECTION_VALID);

	// A level/variant change invalidates every retained anchor.
	AssetCatalog_BeginContext(1, 0, 0);
	CHECK(AssetCatalog_CheckAnchor(&fresh) == ASSET_CATALOG_SELECTION_STALE);

	// A component of an instance (a wheel of a car) has no model part; the
	// caller validates the instance separately.
	AssetCatalog_BeginContext(0, 0, 0);
	AssetCatalogAnchor wheel = {};
	CHECK(AssetCatalog_CaptureAnchor("car:0:0:3:2:12/component:wheel:0", &wheel));
	CHECK(!wheel.hasModel && AssetCatalog_CheckAnchor(&wheel) == ASSET_CATALOG_SELECTION_UNKNOWN);

	// A component of a model resource still anchors the model.
	CHECK(AssetCatalog_RegisterModel(11, "SEDAN", ASSET_CATALOG_SOURCE_DECLARED, -1, -1) >= 0);
	AssetCatalogAnchor material = {};
	CHECK(AssetCatalog_CaptureAnchor("model:0:0:11/component:material:3", &material));
	CHECK(material.hasModel && AssetCatalog_CheckAnchor(&material) == ASSET_CATALOG_SELECTION_VALID);

	// A key naming a different level never resolves a handle here.
	AssetCatalogAnchor foreign = {};
	CHECK(AssetCatalog_CaptureAnchor("building:model:2:0:11:0:0:0:0", &foreign));
	CHECK(!foreign.hasModel && AssetCatalog_CheckAnchor(&foreign) == ASSET_CATALOG_SELECTION_UNKNOWN);

	// Bounds: no anchor for an empty key; a cleared anchor reports NONE.
	CHECK(!AssetCatalog_CaptureAnchor("", &anchor));
	CHECK(!AssetCatalog_CaptureAnchor(NULL, &anchor));
	CHECK(!AssetCatalog_CaptureAnchor("building:x", NULL));
	CHECK(AssetCatalog_CheckAnchor(NULL) == ASSET_CATALOG_SELECTION_NONE);
	AssetCatalogAnchor none = {};
	CHECK(AssetCatalog_CheckAnchor(&none) == ASSET_CATALOG_SELECTION_NONE);
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

static void TestInventoryReconciliation()
{
	AssetCatalog_Reset();
	AssetCatalog_BeginContext(0, 0, 0);

	const int roadA = AssetCatalog_RegisterModel(10, "ROADA", ASSET_CATALOG_SOURCE_DECLARED, -1, -1);
	const int roadB = AssetCatalog_RegisterModel(11, "ROADB", ASSET_CATALOG_SOURCE_DECLARED, -1, -1);
	const int asphalt = AssetCatalog_RegisterTexture("asphalt", 1, 2, 3, ASSET_CATALOG_SOURCE_DECLARED);
	const int kerb = AssetCatalog_RegisterTexture("kerb", 1, 3, 3, ASSET_CATALOG_SOURCE_DECLARED);
	CHECK(roadA >= 0 && roadB >= 0 && asphalt >= 0 && kerb >= 0);
	CHECK(AssetCatalog_AddModelTexture(roadA, asphalt));
	CHECK(AssetCatalog_AddModelTexture(roadB, kerb));

	// A clean set: every model resolves, every referenced model has a material
	// link and the expected per-region counts match.
	AssetCatalogPlacement clean[3];
	memset(clean, 0, sizeof(clean));
	clean[0].region = 0; clean[0].modelNumber = 10; clean[0].x = 100; clean[0].z = 200; clean[0].rotation = 5;
	clean[1].region = 0; clean[1].modelNumber = 10; clean[1].x = 300; clean[1].z = 400; clean[1].rotation = 6;
	clean[2].region = 1; clean[2].modelNumber = 11; clean[2].x = 500; clean[2].y = 1; clean[2].z = 600; clean[2].rotation = 7;

	int expected[4] = { 2, 1, 0, 0 };
	AssetCatalogInventoryResult result;
	CHECK(AssetCatalog_ReconcileInventory(clean, 3, expected, &result) == ASSET_CATALOG_INVENTORY_OK);
	CHECK(result.ok);
	CHECK(result.placements == 3 && result.uniquePlacements == 3 && result.duplicatePlacements == 0);
	CHECK(result.sharedPlacements == 0);
	CHECK(result.missingModelRefs == 0);
	CHECK(result.registeredModels == 2 && result.referencedModels == 2 && result.unreferencedModels == 0);
	CHECK(result.registeredTextures == 2 && result.referencedTextures == 2);
	CHECK(result.missingTextureLinks == 0);
	CHECK(result.perRegionPlacements[0] == 2 && result.perRegionPlacements[1] == 1);
	CHECK(result.perRegionPlacements[2] == 0 && result.perRegionPlacements[3] == 0);
	CHECK(result.findingCount == 0 && !result.findingsTruncated);

	// Without expectations the region check is skipped, not assumed.
	CHECK(AssetCatalog_ReconcileInventory(clean, 3, NULL, &result) == ASSET_CATALOG_INVENTORY_OK && result.ok);

	// A dangling model reference is counted per occurrence and reported.
	AssetCatalogPlacement dangling[2];
	memset(dangling, 0, sizeof(dangling));
	dangling[0] = clean[0];
	dangling[1].region = 0; dangling[1].modelNumber = 999; dangling[1].x = 1; dangling[1].y = 2; dangling[1].z = 3; dangling[1].rotation = 4;
	CHECK(AssetCatalog_ReconcileInventory(dangling, 2, NULL, &result) == ASSET_CATALOG_INVENTORY_OK);
	CHECK(!result.ok && result.missingModelRefs == 1);
	CHECK(result.referencedModels == 1 && result.unreferencedModels == 1);
	CHECK(result.findingCount >= 1 && !result.findings[0].warning);
	CHECK(TextIs(result.findings[0].reason, "missing model"));
	CHECK(result.findings[0].modelNumber == 999 && result.findings[0].region == 0);
	CHECK(result.findings[0].x == 1 && result.findings[0].z == 3 && result.findings[0].rotation == 4);

	// A duplicate placement identity is one extra occurrence, one error.
	AssetCatalogPlacement duplicate[3];
	duplicate[0] = clean[0];
	duplicate[1] = clean[0];
	duplicate[2] = clean[2];
	CHECK(AssetCatalog_ReconcileInventory(duplicate, 3, NULL, &result) == ASSET_CATALOG_INVENTORY_OK);
	CHECK(!result.ok && result.duplicatePlacements == 1 && result.uniquePlacements == 2);
	CHECK(result.placements == 3);
	CHECK(result.findingCount >= 1 && !result.findings[0].warning);
	CHECK(TextIs(result.findings[0].reason, "duplicate placement"));

	// Rotation and region are part of the identity, so those are not duplicates.
	AssetCatalogPlacement distinct[2];
	distinct[0] = clean[0];
	distinct[1] = clean[0];
	distinct[1].rotation = 6;
	CHECK(AssetCatalog_ReconcileInventory(distinct, 2, NULL, &result) == ASSET_CATALOG_INVENTORY_OK);
	CHECK(result.duplicatePlacements == 0 && result.ok);
	distinct[1].region = 1;
	CHECK(AssetCatalog_ReconcileInventory(distinct, 2, NULL, &result) == ASSET_CATALOG_INVENTORY_OK);
	CHECK(result.duplicatePlacements == 0);
	for (int axis = 0; axis < 3; ++axis)
	{
		distinct[1] = clean[0];
		if (axis == 0) distinct[1].x++;
		if (axis == 1) distinct[1].y++;
		if (axis == 2) distinct[1].z++;
		CHECK(AssetCatalog_ReconcileInventory(distinct, 2, NULL, &result) == ASSET_CATALOG_INVENTORY_OK);
		CHECK(result.duplicatePlacements == 0 && result.ok);
	}

	// An unreferenced registered model is a warning that keeps ok true.
	AssetCatalogPlacement onlyA[1];
	onlyA[0] = clean[0];
	CHECK(AssetCatalog_ReconcileInventory(onlyA, 1, NULL, &result) == ASSET_CATALOG_INVENTORY_OK);
	CHECK(result.ok && result.unreferencedModels == 1);
	CHECK(result.referencedModels == 1 && result.referencedTextures == 1);
	CHECK(result.findingCount == 1 && result.findings[0].warning);
	CHECK(TextIs(result.findings[0].reason, "unreferenced model"));
	CHECK(result.findings[0].modelNumber == 11);

	// A per-region expectation mismatch is a finding and flips ok.
	int wrongExpected[4] = { 2, 0, 0, 0 };
	CHECK(AssetCatalog_ReconcileInventory(clean, 3, wrongExpected, &result) == ASSET_CATALOG_INVENTORY_OK);
	CHECK(!result.ok && result.perRegionPlacements[1] == 1);
	CHECK(result.findingCount >= 1 && !result.findings[0].warning);
	CHECK(strncmp(result.findings[0].reason, "region 1 expected 0 got 1", 25) == 0);
	CHECK(result.findings[0].region == 1);

	// Negative expected counts are malformed input, not a finding.
	int negativeExpected[4] = { -1, 0, 0, 0 };
	CHECK(AssetCatalog_ReconcileInventory(clean, 3, negativeExpected, &result) == ASSET_CATALOG_INVENTORY_ERROR_ARGUMENT);

	// No catalog link leaves source texture classification unknown. This
	// bounded diagnostic warns; ok does not prove source-material coverage.
	const int roadC = AssetCatalog_RegisterModel(12, "ROADC", ASSET_CATALOG_SOURCE_DECLARED, -1, -1);
	CHECK(roadC >= 0);
	AssetCatalogPlacement noLink[1];
	memset(noLink, 0, sizeof(noLink));
	noLink[0].region = 2; noLink[0].modelNumber = 12; noLink[0].x = 7; noLink[0].z = 8;
	CHECK(AssetCatalog_ReconcileInventory(noLink, 1, NULL, &result) == ASSET_CATALOG_INVENTORY_OK);
	CHECK(result.ok && result.missingTextureLinks == 1);
	CHECK(result.referencedTextures == 0);
	CHECK(result.findingCount == 3 && !result.findingsTruncated);
	CHECK(result.findings[0].warning && result.findings[1].warning && result.findings[2].warning);
	CHECK(TextIs(result.findings[0].reason, "no catalog material link; texture classification unknown"));
	CHECK(result.findings[0].modelNumber == 12);

	// A genuine error on top of the warning still flips ok, and is stored
	// before the warnings so it can never be hidden by them.
	AssetCatalogPlacement noLinkDuplicate[2];
	noLinkDuplicate[0] = noLink[0];
	noLinkDuplicate[1] = noLink[0];
	CHECK(AssetCatalog_ReconcileInventory(noLinkDuplicate, 2, NULL, &result) == ASSET_CATALOG_INVENTORY_OK);
	CHECK(!result.ok && result.duplicatePlacements == 1 && result.missingTextureLinks == 1);
	CHECK(result.findingCount == 4);
	CHECK(!result.findings[0].warning && TextIs(result.findings[0].reason, "duplicate placement"));
	CHECK(result.findings[1].warning);

	// A shared (region -1) placement counts toward the totals but not toward
	// any per-region count.
	AssetCatalogPlacement shared[2];
	shared[0] = clean[0];
	memset(&shared[1], 0, sizeof(shared[1]));
	shared[1].region = -1; shared[1].modelNumber = 11; shared[1].x = 9; shared[1].z = 9; shared[1].rotation = 1;
	int sharedExpected[4] = { 1, 0, 0, 0 };
	CHECK(AssetCatalog_ReconcileInventory(shared, 2, sharedExpected, &result) == ASSET_CATALOG_INVENTORY_OK);
	CHECK(result.ok && result.sharedPlacements == 1);
	CHECK(result.placements == 2 && result.uniquePlacements == 2);
	CHECK(result.perRegionPlacements[0] == 1 && result.perRegionPlacements[1] == 0);

	// An invalid region is an error finding and never pollutes a region count.
	AssetCatalogPlacement invalid[1];
	memset(invalid, 0, sizeof(invalid));
	invalid[0].region = 7; invalid[0].modelNumber = 10; invalid[0].x = 1; invalid[0].z = 1;
	CHECK(AssetCatalog_ReconcileInventory(invalid, 1, NULL, &result) == ASSET_CATALOG_INVENTORY_OK);
	CHECK(!result.ok);
	CHECK(result.perRegionPlacements[0] == 0 && result.sharedPlacements == 0);
	CHECK(result.findingCount >= 1 && !result.findings[0].warning);
	CHECK(TextIs(result.findings[0].reason, "invalid region"));

	// Malformed arguments and the fixed capacity bound.
	CHECK(AssetCatalog_ReconcileInventory(NULL, 0, NULL, &result) == ASSET_CATALOG_INVENTORY_ERROR_ARGUMENT);
	CHECK(AssetCatalog_ReconcileInventory(clean, -1, NULL, &result) == ASSET_CATALOG_INVENTORY_ERROR_ARGUMENT);
	CHECK(AssetCatalog_ReconcileInventory(NULL, 3, NULL, &result) == ASSET_CATALOG_INVENTORY_ERROR_ARGUMENT);
	CHECK(AssetCatalog_ReconcileInventory(clean, 3, NULL, NULL) == ASSET_CATALOG_INVENTORY_ERROR_ARGUMENT);
	CHECK(AssetCatalog_ReconcileInventory(clean, ASSET_CATALOG_MAX_PLACEMENTS + 1, NULL, &result) ==
		ASSET_CATALOG_INVENTORY_ERROR_CAPACITY);

	// No live context: a reset catalog refuses to reconcile.
	AssetCatalog_Reset();
	CHECK(AssetCatalog_ReconcileInventory(clean, 3, NULL, &result) == ASSET_CATALOG_INVENTORY_ERROR_NO_CATALOG);

	// Finding bounding: the stored list is capped and truncation is explicit;
	// error findings are stored before warnings.
	AssetCatalog_BeginContext(0, 0, 0);
	static AssetCatalogPlacement many[ASSET_CATALOG_INVENTORY_MAX_FINDINGS + 1];
	const int manyCount = ASSET_CATALOG_INVENTORY_MAX_FINDINGS + 1;
	for (int i = 0; i < manyCount; i++)
	{
		many[i].region = 0;
		many[i].modelNumber = 777;
		many[i].x = i;
		many[i].y = 0;
		many[i].z = i;
		many[i].rotation = i & 63;
	}
	CHECK(AssetCatalog_ReconcileInventory(many, manyCount, NULL, &result) == ASSET_CATALOG_INVENTORY_OK);
	CHECK(result.missingModelRefs == manyCount);
	CHECK(!result.ok);
	CHECK(result.findingCount == ASSET_CATALOG_INVENTORY_MAX_FINDINGS);
	CHECK(result.findingsTruncated);
	CHECK(!result.findings[0].warning && !result.findings[result.findingCount - 1].warning);
}

struct WalkFixture { int count; int lastObject; int lastSelector; };
static void VisitFixture(void* user, int object, int cell, int selector)
{
	(void)cell;
	WalkFixture* fixture = (WalkFixture*)user;
	fixture->count++;
	fixture->lastObject = object;
	fixture->lastSelector = selector;
}

static void TestTraversalAndMaterialLifetime()
{
	unsigned short pointers[] = { 1, 0xffff };
	unsigned short entries[] = { 0, 2, 0x4007, 0x8003, 0x8004 };
	AssetCatalogInventoryResult diagnostics = {};
	diagnostics.ok = true;
	WalkFixture fixture = {};
	CHECK(AssetCatalog_WalkInventoryLists(pointers, 2, entries, 5, 1, 4, 2, 2, 4, 0,
		VisitFixture, &fixture, &diagnostics));
	CHECK(fixture.count == 2 && fixture.lastObject == 3 && fixture.lastSelector == 7);
	CHECK(diagnostics.errorCount == 0 && diagnostics.ok);
	pointers[0] = 0; // inside allocation but outside barrel
	CHECK(!AssetCatalog_WalkInventoryLists(pointers, 2, entries, 5, 1, 4, 2, 2, 4, 0,
		VisitFixture, &fixture, &diagnostics));
	CHECK(diagnostics.errorCount == 1 && !diagnostics.ok);
	pointers[0] = 4;
	CHECK(!AssetCatalog_WalkInventoryLists(pointers, 2, entries, 5, 1, 4, 2, 2, 4, 0,
		VisitFixture, &fixture, &diagnostics));
	CHECK(diagnostics.errorCount == 2);
	pointers[0] = 1;
	entries[3] = 3; // cannot borrow termination from next barrel's entry 4
	CHECK(!AssetCatalog_WalkInventoryLists(pointers, 2, entries, 5, 1, 4, 2, 2, 4, 0,
		VisitFixture, &fixture, &diagnostics));
	CHECK(TextIs(diagnostics.findings[2].reason, "cell list missing terminator"));
	entries[3] = 0x8004;
	CHECK(!AssetCatalog_WalkInventoryLists(pointers, 2, entries, 5, 1, 4, 2, 2, 4, 0,
		VisitFixture, &fixture, &diagnostics));
	CHECK(TextIs(diagnostics.findings[3].reason, "object reference outside owning barrel"));
	entries[3] = 0x8001; // shared straddler exception
	fixture = {};
	CHECK(AssetCatalog_WalkInventoryLists(pointers, 2, entries, 5, 1, 4, 2, 2, 4, 0,
		VisitFixture, &fixture, &diagnostics));
	CHECK(fixture.count == 2 && fixture.lastObject == 1);
	CHECK(!AssetCatalog_WalkInventoryLists(pointers, 2, entries, 5, 1, 6, 2, 2, 4, 0,
		VisitFixture, &fixture, &diagnostics));

	diagnostics = {};
	diagnostics.ok = true;
	for (int i = 0; i < 70; ++i)
		AssetCatalog_InventoryDiagnostic(&diagnostics, true, "warning", 0, i);
	AssetCatalog_InventoryDiagnostic(&diagnostics, false, "adapter traversal failed", 0, 99);
	CHECK(diagnostics.warningCount == 70 && diagnostics.errorCount == 1);
	CHECK(diagnostics.findingCount == 64 && diagnostics.findingsTruncated && !diagnostics.ok);
	bool retained = false;
	for (int i = 0; i < diagnostics.findingCount; ++i)
		retained |= !diagnostics.findings[i].warning && TextIs(diagnostics.findings[i].reason, "adapter traversal failed");
	CHECK(retained);

	AssetCatalog_Reset();
	AssetCatalog_BeginContext(0, 0, 0);
	int a = AssetCatalog_RegisterModel(1, "old", ASSET_CATALOG_SOURCE_DECLARED, -1, -1);
	int b = AssetCatalog_RegisterModel(2, "other", ASSET_CATALOG_SOURCE_DECLARED, -1, -1);
	int texture = AssetCatalog_RegisterTexture("old", 0, 0, 0, ASSET_CATALOG_SOURCE_DECLARED);
	CHECK(AssetCatalog_AddModelTexture(a, texture));
	CHECK(AssetCatalog_AddModelTexture(b, texture));
	CHECK(AssetCatalog_InvalidateModel(1));
	CHECK(AssetCatalog_EnumerateModelTextures(b, NULL, 0) == 1);
	CHECK(AssetCatalog_RegisterModel(1, "new", ASSET_CATALOG_SOURCE_DECLARED, -1, -1) == a);
	CHECK(AssetCatalog_EnumerateModelTextures(a, NULL, 0) == 0);
	AssetCatalogPlacement placement = { 0, 1, 0, 0, 0, 0 };
	CHECK(AssetCatalog_ReconcileInventory(&placement, 1, NULL, &diagnostics) == 0);
	CHECK(diagnostics.ok && diagnostics.referencedTextures == 0 && diagnostics.missingTextureLinks == 1);
	CHECK(TextIs(diagnostics.findings[0].reason, "no catalog material link; texture classification unknown"));
	CHECK(AssetCatalog_AddModelTexture(a, texture));
	CHECK(AssetCatalog_RegisterModel(1, "replace-live", ASSET_CATALOG_SOURCE_DECLARED, -1, -1) == a);
	CHECK(AssetCatalog_EnumerateModelTextures(a, NULL, 0) == 0);
	CHECK(AssetCatalog_EnumerateModelTextures(b, NULL, 0) == 1);
}

static void TestModelSpanLifecycle()
{
	AssetCatalog_Reset();
	AssetCatalog_BeginContext(0, 0, 0);

	const int record = AssetCatalog_RegisterModelWithSpan(7, "ROAD", ASSET_CATALOG_SOURCE_DECLARED, -1, -1, 4096);
	CHECK(record >= 0);

	int span = -1;
	CHECK(AssetCatalog_GetModelSpan(record, &span) && span == 4096);

	// Registering without a span records "not captured" (0).
	CHECK(AssetCatalog_RegisterModel(7, "ROAD", ASSET_CATALOG_SOURCE_DECLARED, -1, -1) == record);
	CHECK(AssetCatalog_GetModelSpan(record, &span) && span == 0);

	// The span is provenance, not identity: a re-registration with a new span
	// keeps the record, bumps the revision and invalidates the retained handle.
	CHECK(AssetCatalog_RegisterModelWithSpan(7, "ROAD", ASSET_CATALOG_SOURCE_DECLARED, -1, -1, 4096) == record);
	AssetCatalogHandle handle = AssetCatalog_GetModelHandle(record);
	CHECK(AssetCatalog_HandleValid(handle));
	CHECK(AssetCatalog_RegisterModelWithSpan(7, "ROAD", ASSET_CATALOG_SOURCE_DECLARED, -1, -1, 512) == record);
	CHECK(!AssetCatalog_HandleValid(handle));
	CHECK(AssetCatalog_GetModelSpan(record, &span) && span == 512);
	CHECK(AssetCatalog_HandleValid(AssetCatalog_GetModelHandle(record)));

	// A freed slot serves no span; re-registering restores one.
	CHECK(AssetCatalog_InvalidateModel(7));
	CHECK(!AssetCatalog_GetModelSpan(record, &span));
	CHECK(AssetCatalog_RegisterModelWithSpan(7, "ROAD", ASSET_CATALOG_SOURCE_DECLARED, -1, -1, 2048) == record);
	CHECK(AssetCatalog_GetModelSpan(record, &span) && span == 2048);

	// Bounds: an invalid record fails, a valid one tolerates a NULL output.
	CHECK(!AssetCatalog_GetModelSpan(-1, &span));
	CHECK(!AssetCatalog_GetModelSpan(record + 999, &span));
	CHECK(AssetCatalog_GetModelSpan(record, NULL));
}

static void TestPlacementSourceObjectIndex()
{
	AssetCatalog_Reset();
	AssetCatalog_BeginContext(0, 0, 0);
	AssetCatalog_RegisterModel(10, "ROAD", ASSET_CATALOG_SOURCE_DECLARED, -1, -1);

	// sourceObjectIndex is provenance and is NOT part of the duplicate key:
	// two placements identical except for it are still one duplicate.
	AssetCatalogPlacement placements[2];
	memset(placements, 0, sizeof(placements));
	placements[0].region = 0;
	placements[0].modelNumber = 10;
	placements[0].sourceObjectIndex = 100;
	placements[1] = placements[0];
	placements[1].sourceObjectIndex = 101;

	AssetCatalogInventoryResult result;
	CHECK(AssetCatalog_ReconcileInventory(placements, 2, NULL, &result) == ASSET_CATALOG_INVENTORY_OK);
	CHECK(result.duplicatePlacements == 1 && result.uniquePlacements == 1);
	CHECK(placements[0].sourceObjectIndex == 100 && placements[1].sourceObjectIndex == 101);
}

int main()
{
	TestLifetimeAndContext();
	TestModelIdentityAndInstances();
	TestSlotInvalidation();
	TestTextureDedupAndMaterials();
	TestComponentIdentity();
	TestSelectionAnchors();
	TestCarsAndPalettes();
	TestDiagnostics();
	TestBoundsAreGraceful();
	TestInventoryReconciliation();
	TestTraversalAndMaterialLifetime();
	TestModelSpanLifecycle();
	TestPlacementSourceObjectIndex();

	printf("%d checks, %d failures\n", g_checks, g_failures);
	return g_failures == 0 ? 0 : 1;
}
