#include "driver2.h"
#include "assetcatalog_game.h"

#ifndef PSX

#include "assetcatalog.h"
#include "models.h"
#include "texture.h"
#include "mission.h"
#include "system.h"
#include "map.h"
#include "cell.h"
#include "spool.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <stdarg.h>
#include "../../utils/InspectorExport.h"
#endif

// Developer flag: reconcile the loaded level's inventory once the world
// regions are resident. Parsed from the command line before the game starts.
static bool s_inventoryRequested = false;

void AssetCatalogGame_ConfigureFromCommandLine(int argc, char** argv)
{
	for (int i = 1; i < argc; i++)
	{
		if (strcmp(argv[i], "-assetinventory") == 0)
			s_inventoryRequested = true;
	}
}

bool AssetCatalogGame_InventoryRequested(void)
{
	return s_inventoryRequested;
}

void AssetCatalogGame_BeginLevel(void)
{
	AssetCatalog_BeginContext(GameLevel, (int)GetCityType(), gMultiplayerLevels);
}

void AssetCatalogGame_RegisterModel(int modelIndex)
{
	if (modelIndex < 0 || modelIndex >= MAX_MODEL_SLOTS)
		return;

	MODEL* model = modelpointers[modelIndex];

	if (model == NULL || model == &dummyModel)
	{
		AssetCatalog_InvalidateModel(modelIndex);
		return;
	}

	const char* name = GetModelNameByIndex(modelIndex);

	// Low2HighDetailTable maps a low-detail model to its high-detail sibling;
	// record the link so a selection can survive an LOD change.
	int highDetailModelIndex = -1;
	if (Low2HighDetailTable != NULL && Low2HighDetailTable[modelIndex] != 0xFFFF &&
		Low2HighDetailTable[modelIndex] != modelIndex && Low2HighDetailTable[modelIndex] < MAX_MODEL_SLOTS)
	{
		highDetailModelIndex = Low2HighDetailTable[modelIndex];
	}

	int record = AssetCatalog_RegisterModel(modelIndex, name ? name : "",
		name ? ASSET_CATALOG_SOURCE_DECLARED : ASSET_CATALOG_SOURCE_UNKNOWN, -1, highDetailModelIndex);

	if (record >= 0)
		RegisterCatalogModelTextures(model, record);
}

void AssetCatalogGame_InvalidateModel(int modelIndex)
{
	AssetCatalog_InvalidateModel(modelIndex);
}

void AssetCatalogGame_PopulateLevel(void)
{
	int registeredModels = 0;
	const int modelCount = num_models_in_pack < MAX_MODEL_SLOTS ? num_models_in_pack : MAX_MODEL_SLOTS;

	for (int i = 0; i < modelCount; i++)
	{
		if (modelpointers[i] == &dummyModel)
			continue;

		AssetCatalogGame_RegisterModel(i);
		registeredModels++;
	}

	// Resident car slots reference a source model number; the slot is the live
	// instance and must not be treated as the asset identity.
	for (int slot = 0; slot < MAX_CAR_RESIDENT_MODELS; slot++)
	{
		if (residentCarModels[slot] < 0)
			continue;

		AssetCatalog_RegisterCar(slot, residentCarModels[slot], "", ASSET_CATALOG_SOURCE_DECLARED);
	}

	int models = 0, textures = 0, cars = 0, materialRefs = 0;
	AssetCatalog_GetStats(&models, &textures, &cars, &materialRefs);

	printInfo("AssetCatalog: level %d variant %d has %d models (%d), %d textures, %d cars, %d material refs\n",
		GameLevel, (int)GetCityType(), registeredModels, models, textures, cars, materialRefs);
}

#ifdef _WIN32

/*
 * E02 level inventory: the resident world regions are decoded from their cell
 * lists (not scanned linearly) because a packed position is only exact with
 * the nearCell of the cell that references it. cell_objects_add deltas are
 * per-barrel capacities (the largest streamed region size of each quadrant
 * parity), and one region per barrel is resident, so the report prints
 * placements against that capacity instead of treating it as an expected
 * count.
 */

namespace
{
struct LevelPlacementWalk
{
	AssetCatalogPlacement* placements;
	int capacity;
	int count;
	int totalSlots;
	unsigned char* visited;
	bool capacityExceeded;
	int barrel;
	int regionIndex;
	int eligible;
	int scanned;
	int skipped;
	AssetCatalogInventoryResult diagnostics;
};

// Decodes one referenced object once. Empty slot markers are skipped exactly
// like GetFirstPackedCop/GetNextPackedCop skip them; region-straddling objects
// (global indices before the barrel arrays) are reported as shared placements.
void AddLevelPlacement(LevelPlacementWalk& walk, int objectIndex, const XZPAIR& nearCell)
{
	if (objectIndex < 0 || objectIndex >= walk.totalSlots || objectIndex >= ASSET_CATALOG_MAX_PLACEMENTS)
	{
		walk.capacityExceeded = true;
		return;
	}

	if (walk.visited[objectIndex])
		return;

	const PACKED_CELL_OBJECT* packed = &cell_objects[objectIndex];

	if (packed->value == 0xffff && (packed->pos.vy & 1))
		return; // explicit empty slot

	if (walk.count >= walk.capacity)
	{
		walk.capacityExceeded = true;
		return;
	}

	walk.visited[objectIndex] = 1;

	CELL_OBJECT decoded;
	QuickUnpackCellObject(const_cast<PACKED_CELL_OBJECT*>(packed), const_cast<XZPAIR*>(&nearCell), &decoded);

	// Attribution is by storage range, so a corrupt cross-barrel reference can
	// never silently move an object into the wrong region.
	int region = -1;

	for (int candidate = 0; candidate < 4; candidate++)
	{
		if (objectIndex >= num_straddlers + cell_objects_add[candidate] &&
			objectIndex < num_straddlers + cell_objects_add[candidate + 1])
		{
			region = candidate;
			break;
		}
	}

	AssetCatalogPlacement* placement = &walk.placements[walk.count++];
	placement->region = region;
	placement->modelNumber = decoded.type;
	placement->x = decoded.pos.vx;
	placement->y = decoded.pos.vy;
	placement->z = decoded.pos.vz;
	placement->rotation = decoded.yang;
}

// Walks every list (default and event variants) of every referenced cell of
// one barrel's resident region.
void VisitLevelObject(void* user, int object, int cellOffset, int selector)
{
	(void)selector; // Resident union: list activation is deliberately not inferred.
	LevelPlacementWalk& walk = *(LevelPlacementWalk*)user;
	const int cellX = (walk.regionIndex % regions_across) * MAP_REGION_SIZE + cellOffset % MAP_REGION_SIZE;
	const int cellZ = (walk.regionIndex / regions_across) * MAP_REGION_SIZE + cellOffset / MAP_REGION_SIZE;
	XZPAIR nearCell;
	nearCell.x = (cellX - cells_across / 2) * MAP_CELL_SIZE;
	nearCell.z = (cellZ - cells_down / 2) * MAP_CELL_SIZE;
	AddLevelPlacement(walk, object, nearCell);
}

void CollectBarrelPlacements(LevelPlacementWalk& walk, int barrel)
{
	if (regions_unpacked[barrel] < 0 || loading_region[barrel] != -1)
	{
		walk.skipped++;
		return; // Intentional partial residency, not a missing-city error.
	}
	walk.eligible++;
	walk.barrel = barrel;
	walk.regionIndex = regions_unpacked[barrel];
	if (walk.regionIndex >= num_regions)
	{
		AssetCatalog_InventoryDiagnostic(&walk.diagnostics, false, "invalid resident region index", barrel, walk.regionIndex);
		return;
	}
	if (AssetCatalog_WalkInventoryLists(cell_ptrs + barrel * 1024, 1024,
		(const unsigned short*)cells, cell_slots_add[4], cell_slots_add[barrel], cell_slots_add[barrel + 1],
		num_straddlers, num_straddlers + cell_objects_add[barrel], num_straddlers + cell_objects_add[barrel + 1],
		barrel, VisitLevelObject, &walk, &walk.diagnostics))
		walk.scanned++;
}

void AppendReport(char* buffer, size_t capacity, size_t* offset, const char* format, ...)
{
	if (*offset >= capacity)
		return;

	va_list args;
	va_start(args, format);
	const size_t remaining = capacity - *offset;
	const int written = vsnprintf(buffer + *offset, remaining, format, args);
	va_end(args);

	if (written <= 0)
		return;

	*offset += (size_t)written < remaining ? (size_t)written : remaining - 1;
}

const char* CityVariantName(int variant)
{
	switch (variant)
	{
	case 0: return "day";
	case 1: return "night";
	case 2: return "multiplayer day";
	case 3: return "multiplayer night";
	default: return "unknown";
	}
}

// The loader cannot know a resident region's expected instance count: the
// cell_objects_add deltas are slot capacities, and unreferenced array slots
// hold other streamed data. An explicit capacity finding keeps the bound
// visible instead of overflowing the placement buffer.
void AddCapacityFinding(AssetCatalogInventoryResult& result, int capacity)
{
	char reason[96];
	snprintf(reason, sizeof(reason), "placement capacity %d reached", capacity);
	AssetCatalog_InventoryDiagnostic(&result, false, reason, -1, -1);
}
}

bool AssetCatalogGame_ReconcileLevel(char* status, int statusCapacity)
{
	if (status == NULL || statusCapacity <= 0)
		return false;

	status[0] = '\0';

	if (!AssetCatalog_IsValid())
	{
		snprintf(status, statusCapacity, "No level catalog context is active; load a level first");
		return false;
	}

	if (cell_objects == NULL || cell_ptrs == NULL || cells == NULL || cell_objects_add[4] <= 0)
	{
		snprintf(status, statusCapacity, "No map region data is resident; enter a level world first");
		return false;
	}

	if (MAP_REGION_SIZE != 32 || MAP_CELL_SIZE <= 0 || regions_across <= 0 ||
		num_straddlers < 0 || num_straddlers > ASSET_CATALOG_MAX_PLACEMENTS ||
		cell_objects_add[4] > ASSET_CATALOG_MAX_PLACEMENTS - num_straddlers)
	{
		snprintf(status, statusCapacity, "Invalid map dimensions or object capacity; inventory NOT OK");
		return false;
	}
	const int totalSlots = num_straddlers + cell_objects_add[4];
	for (int barrel = 0; barrel < 4; ++barrel)
	{
		if (cell_objects_add[barrel] < 0 || cell_objects_add[barrel] > cell_objects_add[barrel + 1] ||
			cell_slots_add[barrel] < 0 || cell_slots_add[barrel] > cell_slots_add[barrel + 1])
		{
			snprintf(status, statusCapacity, "Invalid barrel allocation spans; inventory NOT OK");
			return false;
		}
	}
	const int placementCapacity = totalSlots < ASSET_CATALOG_MAX_PLACEMENTS ?
		totalSlots : ASSET_CATALOG_MAX_PLACEMENTS;

	AssetCatalogPlacement* placements = (AssetCatalogPlacement*)malloc((size_t)placementCapacity * sizeof(AssetCatalogPlacement));
	unsigned char* visited = (unsigned char*)calloc(ASSET_CATALOG_MAX_PLACEMENTS, 1);

	if (placements == NULL || visited == NULL)
	{
		free(placements);
		free(visited);
		snprintf(status, statusCapacity, "Inventory report allocation failed");
		return false;
	}

	LevelPlacementWalk walk = {};
	walk.placements = placements;
	walk.capacity = placementCapacity;
	walk.count = 0;
	walk.totalSlots = totalSlots;
	walk.visited = visited;
	walk.capacityExceeded = false;

	for (int barrel = 0; barrel < 4; barrel++)
		CollectBarrelPlacements(walk, barrel);
	if (walk.eligible == 0)
	{
		free(placements);
		free(visited);
		snprintf(status, statusCapacity, "No eligible resident barrel data; inventory NOT OK (4 skipped)");
		return false;
	}

	AssetCatalogInventoryResult result;

	if (AssetCatalog_ReconcileInventory(placements, walk.count, NULL, &result) != ASSET_CATALOG_INVENTORY_OK)
	{
		// The catalog was validated above; this only guards future changes.
		free(placements);
		free(visited);
		snprintf(status, statusCapacity, "Inventory reconciliation failed");
		return false;
	}

	// The union includes mutually exclusive event lists. Geometric equality
	// is an overlap diagnostic, not proof of duplicate source instances.
	result.errorCount -= result.duplicatePlacements;
	result.warningCount += result.duplicatePlacements;
	for (int i = 0; i < result.findingCount; ++i)
		if (strcmp(result.findings[i].reason, "duplicate placement") == 0)
		{
			result.findings[i].warning = true;
			snprintf(result.findings[i].reason, sizeof(result.findings[i].reason), "resident-union geometric overlap; activation unknown");
		}
	result.ok = result.errorCount == 0;
	for (int i = 0; i < walk.diagnostics.findingCount; ++i)
	{
		const AssetCatalogInventoryFinding& finding = walk.diagnostics.findings[i];
		AssetCatalog_InventoryDiagnostic(&result, false, finding.reason, finding.region, finding.modelNumber);
	}
	result.errorCount += walk.diagnostics.errorCount - walk.diagnostics.findingCount;
	if (walk.diagnostics.errorCount) result.ok = false;
	if (walk.diagnostics.findingsTruncated) result.findingsTruncated = true;
	if (walk.capacityExceeded)
		AddCapacityFinding(result, placementCapacity);

	char report[16384];
	size_t reportSize = 0;
	report[0] = '\0';
	AppendReport(report, sizeof(report), &reportSize,
		"Scope: resident union of default/event lists; deduplicated by source object index.\n"
		"Geometric overlaps are not source duplication acceptance; ok is not source-material coverage.\n"
		"Partial residency is intentional; no whole-city or stage acceptance is implied.\n"
		"Barrels: eligible %d, completely scanned %d, skipped %d\n",
		walk.eligible, walk.scanned, walk.skipped);

	const char* cityName = GameLevel >= 0 && GameLevel < 4 ? LevelNames[GameLevel] : "unknown";

	AppendReport(report, sizeof(report), &reportSize,
		"REDRIVER2-Plus asset catalog level inventory\n"
		"city: %s (level %d)\n"
		"variant: %s (%d)\n"
		"multiplayer: %d\n"
		"catalog generation: %u\n"
		"placements: %d\n"
		"unique placements: %d\n"
		"resident-union geometric overlaps: %d\n"
		"shared (region-straddling) placements: %d\n"
		"registered models: %d\n"
		"referenced models: %d\n"
		"missing model refs: %d\n"
		"unreferenced registered models: %d\n"
		"registered textures: %d\n"
		"referenced textures: %d\n"
		"models with no catalog material link (classification unknown): %d\n"
		"reconciliation: %s\n",
		cityName, GameLevel, CityVariantName((int)GetCityType()), (int)GetCityType(), gMultiplayerLevels,
		AssetCatalog_GetGeneration(), result.placements, result.uniquePlacements, result.duplicatePlacements,
		result.sharedPlacements, result.registeredModels, result.referencedModels, result.missingModelRefs,
		result.unreferencedModels, result.registeredTextures, result.referencedTextures,
		result.missingTextureLinks, result.ok ? "OK" : "NOT OK");

	// Per-barrel placements against the barrel slot capacity. The capacity is
	// the maximum streamed region size of that quadrant parity, not a resident
	// count, so a smaller placement count is expected, not missing data.
	for (int barrel = 0; barrel < 4; barrel++)
	{
		const int capacity = cell_objects_add[barrel + 1] - cell_objects_add[barrel];
		AppendReport(report, sizeof(report), &reportSize,
			"region %d: region index %d, placements %d / barrel capacity %d\n",
			barrel, regions_unpacked[barrel], result.perRegionPlacements[barrel], capacity);
	}

	AppendReport(report, sizeof(report), &reportSize, "findings: %d (%d errors, %d warnings)%s\n",
		result.findingCount, result.errorCount, result.warningCount,
		result.findingsTruncated ? " (more truncated)" : "");

	for (int i = 0; i < result.findingCount; i++)
	{
		const AssetCatalogInventoryFinding* finding = &result.findings[i];
		AppendReport(report, sizeof(report), &reportSize,
			"  [%s] %s: region %d model %d at (%d, %d, %d) rotation %d\n",
			finding->warning ? "warning" : "error", finding->reason, finding->region,
			finding->modelNumber, finding->x, finding->y, finding->z, finding->rotation);
	}

	char path[96];
	snprintf(path, sizeof(path), "assetcatalog_level_%d_inventory.txt", GameLevel);

	char exportStatus[512];

	if (!InspectorExport_WriteText(path, report, exportStatus, sizeof(exportStatus)))
	{
		free(placements);
		free(visited);
		snprintf(status, statusCapacity, "inventory report failed: %s", exportStatus);
		return false;
	}

	snprintf(status, statusCapacity, "inventory report: %d placements, %d findings -> %s",
		result.placements, result.findingCount, path);

	free(placements);
	free(visited);
	return true;
}

#else // !_WIN32

bool AssetCatalogGame_ReconcileLevel(char* status, int statusCapacity)
{
	if (status != NULL && statusCapacity > 0)
		snprintf(status, statusCapacity, "Level inventory reconciliation is currently available on Windows only");

	return false;
}

#endif // _WIN32

#else

void AssetCatalogGame_BeginLevel(void) {}
void AssetCatalogGame_PopulateLevel(void) {}

void AssetCatalogGame_ConfigureFromCommandLine(int argc, char** argv)
{
	(void)argc;
	(void)argv;
}

bool AssetCatalogGame_InventoryRequested(void)
{
	return false;
}

bool AssetCatalogGame_ReconcileLevel(char* status, int statusCapacity)
{
	if (status != NULL && statusCapacity > 0)
		status[0] = '\0';

	return false;
}

#endif /* !PSX */
