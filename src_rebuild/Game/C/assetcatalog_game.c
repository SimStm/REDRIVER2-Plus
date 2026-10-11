#include "driver2.h"
#include "assetcatalog_game.h"

#ifndef PSX

#include "assetcatalog.h"
#include "modelptypeprobe.h"
#include "models.h"
#include "texture.h"
#include "mission.h"
#include "system.h"
#include "map.h"
#include "cell.h"
#include "spool.h"
#include "draw.h"
#include "texturebake.h"

#include "../../utils/WorldMeshBuilder.h"
#include "../../utils/AssetConverter.h"
#include "../../utils/GltfWriter.h"
#include "../../utils/WorldMeshConversion.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

#ifdef _WIN32

#include "../../utils/WorldShadePalette.h"
#include <direct.h>
#include <stdarg.h>
#include <future>
#include <memory>
#include "../../utils/InspectorExport.h"
#endif

// Developer flag: reconcile the loaded level's inventory once the world
// regions are resident. Parsed from the command line before the game starts.
static bool s_inventoryRequested = false;

// Developer flag: accumulate the model polygon-type probe at load and write a
// report once after SpoolSYNC. Never interrupts the game on failure.
static bool s_modelPtypesRequested = false;

// Developer flag: convert one resident model slot to a native GLB once after
// SpoolSYNC. Never interrupts the game on failure.
static bool s_convertModelRequested = false;
static int s_convertModelSlot = -1;

// Developer flag: bounded native per-instance draw of one converted model slot
// once after SpoolSYNC. Never interrupts the game on failure.
static bool s_nativeDrawRequested = false;
static int s_nativeDrawSlot = -1;
static int s_nativeDrawCount = 0; // 0 -> the default bound

// Developer flag: full-region native draw of every resident placement once
// after SpoolSYNC. Never interrupts the game on failure.
static bool s_regionDrawRequested = false;

// Monotonic level generation, bumped in AssetCatalogGame_BeginLevel. The native
// bridge reads it to detect a level change even when the resource registry is
// already repopulated by the same frame's preparation.
static unsigned int s_levelGeneration = 0;
static unsigned int s_regionDrawRevision = 0;

unsigned int AssetCatalogGame_RegionDrawRevision(void)
{
	return s_regionDrawRevision;
}

#define ASSET_CATALOG_GAME_DEFAULT_DRAW_COUNT 32

void AssetCatalogGame_ConfigureFromCommandLine(int argc, char** argv)
{
	for (int i = 1; i < argc; i++)
	{
		if (strcmp(argv[i], "-assetinventory") == 0)
			s_inventoryRequested = true;
		else if (strcmp(argv[i], "-modelptypes") == 0)
			s_modelPtypesRequested = true;
		else if (strcmp(argv[i], "-convertmodel") == 0)
		{
			s_convertModelRequested = true;
			if (i + 1 < argc)
			{
				s_convertModelSlot = atoi(argv[i + 1]);
				i++;
			}
		}
		else if (strcmp(argv[i], "-drawmodel") == 0)
		{
			s_nativeDrawRequested = true;
			if (i + 1 < argc)
			{
				s_nativeDrawSlot = atoi(argv[i + 1]);
				i++;

				// The optional count follows only when it is not another flag.
				if (i + 1 < argc && argv[i + 1][0] != '-')
				{
					s_nativeDrawCount = atoi(argv[i + 1]);
					i++;
				}
			}
		}
		else if (strcmp(argv[i], "-drawregion") == 0)
		{
			s_regionDrawRequested = true;
		}
	}
}

bool AssetCatalogGame_InventoryRequested(void)
{
	return s_inventoryRequested;
}

bool AssetCatalogGame_ModelPtypesRequested(void)
{
	return s_modelPtypesRequested;
}

bool AssetCatalogGame_ConvertModelRequested(void)
{
	return s_convertModelRequested;
}

int AssetCatalogGame_ConvertModelSlot(void)
{
	return s_convertModelSlot;
}

bool AssetCatalogGame_NativeDrawRequested(void)
{
	return s_nativeDrawRequested;
}

int AssetCatalogGame_NativeDrawSlot(void)
{
	return s_nativeDrawSlot;
}

int AssetCatalogGame_NativeDrawCount(void)
{
	return s_nativeDrawCount > 0 ? s_nativeDrawCount : ASSET_CATALOG_GAME_DEFAULT_DRAW_COUNT;
}

bool AssetCatalogGame_RegionDrawRequested(void)
{
	return s_regionDrawRequested;
}

unsigned int AssetCatalogGame_LevelGeneration(void)
{
	return s_levelGeneration;
}

namespace
{
// Bounded slot->published-resource registry and the last collected placements.
// Both are cleared when a new level starts so a level change never serves a
// stale resource or placement set.
AssetCatalogGameModelResource s_modelResources[ASSET_CATALOG_GAME_MAX_MODEL_RESOURCES];
int s_modelResourceCount = 0;

AssetCatalogGameDrawPlacement s_drawPlacements[ASSET_CATALOG_GAME_MAX_DRAW_PLACEMENTS];
int s_drawPlacementCount = 0;

// Full-region placement store and its capacity diagnostics. Republished by
// AssetCatalogGame_PrepareRegionDraw and read by the native-draw bridge; the
// store is sized to the renderer's instance bound so it never overflows.
AssetCatalogGameDrawPlacement s_regionPlacements[ASSET_CATALOG_GAME_MAX_REGION_PLACEMENTS];
int s_regionPlacementCount = 0;
std::vector<NativeWorldCellReference> s_regionCellReferences;
bool s_regionPlacementsTruncated = false;
bool s_regionResourcesTruncated = false;
int s_regionConvertedSlotCount = 0;
int s_regionFailedSlotCount = 0;
int s_regionLodSlotCount = 0;
int s_regionHighDetailSlotCount = 0;

// Local filesystem spooling runs on the game thread. This adapter samples only
// its completed state; volatile CD-thread flags are not synchronization.
struct ResidentRegionState
{
	int regions[4];
	int area;
	unsigned int modelRevision;
	unsigned char pageSlots[19];
	unsigned char loadedPages[128];
};
ResidentRegionState s_preparedRegionState = {};
bool s_preparedRegionStateValid = false;
bool s_regionPrepareSucceeded = false;
char s_regionPrepareError[512] = {};
unsigned int s_regionModelRevision = 0;
std::vector<AssetCatalogGameModelResource> s_reusableRegionResources;
int s_regionReusedSlotCount = 0;

#ifdef _WIN32
struct RegionConversionResult
{
	int code;
	WorldMeshConversionResult converted;
	char error[256];
};
std::future<RegionConversionResult> s_regionConversionTask;
ResidentRegionState s_regionTaskState = {};
int s_regionTaskSlot = -1;
enum class RegionPreparationMode { Immediate, Asynchronous };
RegionPreparationMode s_regionPreparationMode = RegionPreparationMode::Immediate;
int s_regionPendingSlots = 0;
bool s_conversionPending = false;
unsigned char s_regionFailedSlots[MAX_MODEL_SLOTS] = {};

void FinishRegionConversionTask()
{
	if (!s_regionConversionTask.valid())
		return;
	try { s_regionConversionTask.get(); }
	catch (const std::exception& error) { printWarning("AssetCatalog: conversion shutdown: %s\n", error.what()); }
}
#endif

// Per-slot probe record. The loader reports each model once at its size-live
// site; a reused slot replaces its record in place.
struct ModelPtypeSlotRecord
{
	bool used;
	int modelIndex;
	int spanBytes;
	int shapeFlags;
	int flags2;
	ModelPtypeCounts counts;
	ModelPtypeProbeIssue issue;
};

ModelPtypeSlotRecord s_ptypeSlots[MAX_MODEL_SLOTS];
int s_ptypeSlotCount = 0;
}

void AssetCatalogGame_RecordModelSpan(int modelIndex, const unsigned char* modelBegin, int spanBytes)
{
	if (!s_modelPtypesRequested)
		return;

	if (modelIndex < 0 || modelIndex >= MAX_MODEL_SLOTS || modelBegin == 0 || spanBytes <= 0)
		return;

	ModelPtypeSlotRecord* slot = 0;

	for (int i = 0; i < s_ptypeSlotCount; i++)
	{
		if (s_ptypeSlots[i].modelIndex == modelIndex)
		{
			slot = &s_ptypeSlots[i];
			break;
		}
	}

	if (slot == 0)
	{
		if (s_ptypeSlotCount >= MAX_MODEL_SLOTS)
			return;

		slot = &s_ptypeSlots[s_ptypeSlotCount++];
	}

	const MODEL* model = (const MODEL*)modelBegin;

	memset(slot, 0, sizeof(*slot));
	slot->used = true;
	slot->modelIndex = modelIndex;
	slot->spanBytes = spanBytes;
	slot->shapeFlags = (int)model->shape_flags;
	slot->flags2 = (int)model->flags2;
	slot->issue.kind = MODEL_PTYPE_PROBE_ISSUE_NONE;
	slot->issue.ptype = -1;
	slot->issue.recordIndex = -1;
	slot->issue.offset = -1;

	ModelPtypeView view;
	view.polyBlock = model->poly_block;
	view.numPolys = model->num_polys;

	ModelPtypeProbe_AddModel(modelBegin, modelBegin + spanBytes, &view, PolySizes, 56,
		&slot->counts, &slot->issue);
}

void AssetCatalogGame_BeginLevel(void)
{
#ifdef _WIN32
	// Level loading is a synchronization boundary. No prior worker may race
	// this level's synchronous cache writer; normal frames never wait here.
	FinishRegionConversionTask();
	memset(s_regionFailedSlots, 0, sizeof(s_regionFailedSlots));
	s_regionPendingSlots = 0;
#endif
	AssetCatalog_BeginContext(GameLevel, (int)GetCityType(), gMultiplayerLevels);

	// A new level starts a fresh generation. The bridge compares this to detect
	// a level change independently of the registry contents, because BeginLevel
	// and the post-SpoolSYNC preparation can run in the same frame, so the
	// registry may already be repopulated when the bridge frame handler runs.
	s_levelGeneration++;
	s_preparedRegionStateValid = false;
	s_regionPrepareSucceeded = false;
	s_regionDrawRevision++;

	// A new level starts a fresh probe: discard the previous level's per-slot
	// records so a multi-level session reports only the current level. This is
	// always safe and never interrupts the game; the probe itself stays
	// flag-gated in AssetCatalogGame_RecordModelSpan.
	s_ptypeSlotCount = 0;

	// The published-resource registry and collected placements belong to the
	// previous level; a level change must not serve them to the native draw.
	s_modelResourceCount = 0;
	s_drawPlacementCount = 0;
	s_regionPlacementCount = 0;
	s_regionCellReferences.clear();
	s_regionPlacementsTruncated = false;
	s_regionResourcesTruncated = false;
	s_regionConvertedSlotCount = 0;
	s_regionFailedSlotCount = 0;
	s_regionLodSlotCount = 0;
	s_regionHighDetailSlotCount = 0;
}

void AssetCatalogGame_ShutdownNativeConversion(void)
{
#ifdef _WIN32
	FinishRegionConversionTask();
#endif
}

void AssetCatalogGame_RegisterModel(int modelIndex)
{
	if (modelIndex < 0 || modelIndex >= MAX_MODEL_SLOTS)
		return;
	s_regionModelRevision++;

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

	int record = AssetCatalog_RegisterModelWithSpan(modelIndex, name ? name : "",
		name ? ASSET_CATALOG_SOURCE_DECLARED : ASSET_CATALOG_SOURCE_UNKNOWN, -1, highDetailModelIndex,
		modelIndex < MAX_MODEL_SLOTS ? model_byte_sizes[modelIndex] : 0);

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
	std::vector<NativeWorldCellReference>* cellReferences;
	bool referenceCapacityExceeded;
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
	placement->sourceObjectIndex = objectIndex;
}

// Walks every list (default and event variants) of every referenced cell of
// one barrel's resident region.
void VisitLevelObject(void* user, int object, int cellOffset, int selector)
{
	LevelPlacementWalk& walk = *(LevelPlacementWalk*)user;
	const int cellX = (walk.regionIndex % regions_across) * MAP_REGION_SIZE + cellOffset % MAP_REGION_SIZE;
	const int cellZ = (walk.regionIndex / regions_across) * MAP_REGION_SIZE + cellOffset / MAP_REGION_SIZE;
	XZPAIR nearCell;
	nearCell.x = (cellX - cells_across / 2) * MAP_CELL_SIZE;
	nearCell.z = (cellZ - cells_down / 2) * MAP_CELL_SIZE;
	AddLevelPlacement(walk, object, nearCell);
	// Keep every cell/list alias even when its placement is already decoded.
	if (walk.cellReferences && object >= 0 && object < walk.totalSlots &&
		object < ASSET_CATALOG_MAX_PLACEMENTS && walk.visited[object])
	{
		if (walk.cellReferences->size() >= NATIVE_WORLD_MAX_CELL_REFERENCES)
			walk.referenceCapacityExceeded = true;
		else
		{
			const NativeWorldCellReference reference = { object, cellX, cellZ, selector };
			walk.cellReferences->push_back(reference);
		}
	}
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

static const char* PtypeProbeIssueName(ModelPtypeProbeIssueKind kind)
{
	switch (kind)
	{
	case MODEL_PTYPE_PROBE_ISSUE_NONE: return "none";
	case MODEL_PTYPE_PROBE_ISSUE_ARGUMENT: return "argument";
	case MODEL_PTYPE_PROBE_ISSUE_RANGE: return "range";
	case MODEL_PTYPE_PROBE_ISSUE_POLY_BLOCK_RANGE: return "poly-block-range";
	case MODEL_PTYPE_PROBE_ISSUE_STRIDE_ZERO: return "stride-zero";
	case MODEL_PTYPE_PROBE_ISSUE_RECORD_RANGE: return "record-range";
	default: return "unknown";
	}
}

// The exact dispatch decision inputs the world draw uses (draw.c). They are
// recorded, never turned into a decided class, because the same model can be
// referenced from more than one context.
static void ClassifyDispatchInputs(int shapeFlags, int flags2, int* sprite, int* tile, int* building)
{
	*sprite = (shapeFlags & SHAPE_FLAG_SPRITE) != 0;
	*tile = ((shapeFlags & (SHAPE_FLAG_WATER | SHAPE_FLAG_TILE)) != 0) ||
		((flags2 & (MODEL_FLAG_PATH | MODEL_FLAG_GRASS)) != 0);
	*building = !*sprite && !*tile;
}

bool AssetCatalogGame_WriteModelPtypeReport(char* status, int statusCapacity)
{
	if (status == NULL || statusCapacity <= 0)
		return false;

	status[0] = '\0';

	if (!s_modelPtypesRequested)
	{
		snprintf(status, statusCapacity, "model ptype report not requested");
		return false;
	}

	const size_t capacity = 1048576;
	char* report = (char*)malloc(capacity);

	if (report == NULL)
	{
		snprintf(status, statusCapacity, "model ptype report allocation failed");
		return false;
	}

	size_t reportSize = 0;
	report[0] = '\0';

	AppendReport(report, capacity, &reportSize,
		"REDRIVER2-Plus model polygon-type probe\n"
		"city: %s (level %d)\n"
		"variant: %s (%d)\n"
		"multiplayer: %d\n"
		"recorded model slots: %d\n"
		"decoder allowlist: {10,11,20,21,22,23}\n"
		"scope: declared ptypes of the models seen at load; no draw-path class is decided.\n"
		"building path draws only {11,21,23} after its even->odd conversion; tile and sprite paths\n"
		"iterate every record as FT4, so their input count is the walked record count.\n",
		GameLevel >= 0 && GameLevel < 4 ? LevelNames[GameLevel] : "unknown", GameLevel,
		CityVariantName((int)GetCityType()), (int)GetCityType(), gMultiplayerLevels,
		s_ptypeSlotCount);

	ModelPtypeCounts totals;
	memset(&totals, 0, sizeof(totals));
	totals.firstStrideZeroPtype = -1;
	totals.firstStrideZeroOffset = -1;

	int slots = 0;
	int outsideSlots = 0;
	int strideZeroSlots = 0;
	int invalidSlots = 0;

	for (int i = 0; i < s_ptypeSlotCount; i++)
	{
		const ModelPtypeSlotRecord& slot = s_ptypeSlots[i];

		if (!slot.used)
			continue;

		slots++;

		int sprite = 0, tile = 0, building = 0;
		ClassifyDispatchInputs(slot.shapeFlags, slot.flags2, &sprite, &tile, &building);

		AppendReport(report, capacity, &reportSize,
			"slot %d: span %d bytes; shape_flags 0x%04x flags2 0x%04x\n"
			"  dispatch inputs: sprite %d, tile %d, building %d\n"
			"  path inputs: building-drawn %d, catalog-material %d, tile/sprite-as-FT4 %d\n"
			"  records %d, outside-allowlist %d, stride-zero %d, invalid %d, issue %s (ptype %d @ %lld)\n",
			slot.modelIndex, slot.spanBytes, (unsigned)slot.shapeFlags, (unsigned)slot.flags2,
			sprite, tile, building,
			slot.counts.buildingPathRecords, slot.counts.catalogMaterialRecords, slot.counts.records,
			slot.counts.records, slot.counts.outsideAllowlistRecords, slot.counts.strideZeroRecords,
			slot.counts.invalidRecords, PtypeProbeIssueName(slot.issue.kind), slot.issue.ptype,
			slot.issue.offset);

		AppendReport(report, capacity, &reportSize, "  declared ptypes:");

		for (int p = 0; p < MODEL_PTYPE_PROBE_PTYPE_COUNT; p++)
			if (slot.counts.declaredPtypeCounts[p] > 0)
				AppendReport(report, capacity, &reportSize, " %d=%d", p, slot.counts.declaredPtypeCounts[p]);

		AppendReport(report, capacity, &reportSize, "\n");

		if (slot.counts.outsideAllowlistRecords > 0)
			outsideSlots++;
		if (slot.counts.strideZeroRecords > 0)
			strideZeroSlots++;
		if (slot.counts.invalidRecords > 0)
			invalidSlots++;

		totals.models += slot.counts.models;
		totals.records += slot.counts.records;
		totals.outsideAllowlistRecords += slot.counts.outsideAllowlistRecords;
		totals.strideZeroRecords += slot.counts.strideZeroRecords;
		totals.invalidRecords += slot.counts.invalidRecords;
		totals.buildingPathRecords += slot.counts.buildingPathRecords;
		totals.catalogMaterialRecords += slot.counts.catalogMaterialRecords;

		for (int p = 0; p < MODEL_PTYPE_PROBE_PTYPE_COUNT; p++)
		{
			totals.declaredPtypeCounts[p] += slot.counts.declaredPtypeCounts[p];
			totals.outsideAllowlistPtypeCounts[p] += slot.counts.outsideAllowlistPtypeCounts[p];
		}

		if (totals.firstStrideZeroPtype < 0 && slot.counts.firstStrideZeroPtype >= 0)
		{
			totals.firstStrideZeroPtype = slot.counts.firstStrideZeroPtype;
			totals.firstStrideZeroOffset = slot.counts.firstStrideZeroOffset;
		}
	}

	AppendReport(report, capacity, &reportSize,
		"aggregate: slots %d, models %d, records %d, building-drawn %d, catalog-material %d\n"
		"aggregate: outside-allowlist %d (in %d slots), stride-zero %d (in %d slots, first ptype %d @ %lld), invalid %d (in %d slots)\n",
		slots, totals.models, totals.records, totals.buildingPathRecords, totals.catalogMaterialRecords,
		totals.outsideAllowlistRecords, outsideSlots, totals.strideZeroRecords, strideZeroSlots,
		totals.firstStrideZeroPtype, totals.firstStrideZeroOffset, totals.invalidRecords, invalidSlots);

	AppendReport(report, capacity, &reportSize, "aggregate declared ptypes:");

	for (int p = 0; p < MODEL_PTYPE_PROBE_PTYPE_COUNT; p++)
		if (totals.declaredPtypeCounts[p] > 0)
			AppendReport(report, capacity, &reportSize, " %d=%d", p, totals.declaredPtypeCounts[p]);

	AppendReport(report, capacity, &reportSize, "\naggregate outside-allowlist ptypes:");

	for (int p = 0; p < MODEL_PTYPE_PROBE_PTYPE_COUNT; p++)
		if (totals.outsideAllowlistPtypeCounts[p] > 0)
			AppendReport(report, capacity, &reportSize, " %d=%d", p, totals.outsideAllowlistPtypeCounts[p]);

	AppendReport(report, capacity, &reportSize, "\n");

	char path[96];
	snprintf(path, sizeof(path), "model_ptypes_level_%d.txt", GameLevel);

	char exportStatus[512];

	if (!InspectorExport_WriteText(path, report, exportStatus, sizeof(exportStatus)))
	{
		free(report);
		snprintf(status, statusCapacity, "model ptype report failed: %s", exportStatus);
		return false;
	}

	snprintf(status, statusCapacity,
		"model ptype report: %d slots, %d records, %d outside-allowlist, %d stride-zero -> %s",
		slots, totals.records, totals.outsideAllowlistRecords, totals.strideZeroRecords, path);

	free(report);
	return true;
}

// Stores (or replaces) the published resource of a slot. The registry is
// capacity-limited; a full table is a named warning and drops the new entry
// rather than evicting a resident one.
static unsigned int SharedModelRevision(int slot)
{
	const MODEL* model = modelpointers[slot];
	if (!model || model == &dummyModel || model->instance_number < 0 || model->instance_number >= MAX_MODEL_SLOTS)
		return 0;
	return AssetCatalog_GetModelHandle(AssetCatalog_FindModel(model->instance_number)).revision;
}

static void StoreModelResource(int slot, const char* resourceKey, const char* glbPath,
	const char* contentHash, unsigned long long sourceHash)
{
	for (int i = 0; i < s_modelResourceCount; i++)
	{
		if (s_modelResources[i].modelSlot != slot)
			continue;

		AssetCatalogGameModelResource& entry = s_modelResources[i];
		snprintf(entry.resourceKey, sizeof(entry.resourceKey), "%s", resourceKey);
		snprintf(entry.glbPath, sizeof(entry.glbPath), "%s", glbPath);
		snprintf(entry.contentHash, sizeof(entry.contentHash), "%s", contentHash);
		entry.sourceHash = sourceHash;
		entry.modelRevision = AssetCatalog_GetModelHandle(AssetCatalog_FindModel(slot)).revision;
		entry.baseModelRevision = SharedModelRevision(slot);
		return;
	}

	if (s_modelResourceCount >= ASSET_CATALOG_GAME_MAX_MODEL_RESOURCES)
	{
		printWarning("AssetCatalog: model resource registry full (%d); slot %d resource not tracked\n",
			ASSET_CATALOG_GAME_MAX_MODEL_RESOURCES, slot);
		return;
	}

	AssetCatalogGameModelResource& entry = s_modelResources[s_modelResourceCount++];
	entry.modelSlot = slot;
	snprintf(entry.resourceKey, sizeof(entry.resourceKey), "%s", resourceKey);
	snprintf(entry.glbPath, sizeof(entry.glbPath), "%s", glbPath);
	snprintf(entry.contentHash, sizeof(entry.contentHash), "%s", contentHash);
	entry.sourceHash = sourceHash;
	entry.modelRevision = AssetCatalog_GetModelHandle(AssetCatalog_FindModel(slot)).revision;
	entry.baseModelRevision = SharedModelRevision(slot);
}

// ---------------------------------------------------------------------------
// E03 visual parity P2: legacy world-shading bake.
//
// The legacy world draw does NOT use the stored RGB for buildings or tiles:
//   * buildings, ptype 21 (and 20 after the even->odd conversion) use the global
//     `combointensity` (draw.c:789,885,1169);
//   * buildings, ptype 11/23 (and 10/22 after conversion) use
//     `f4colourTable[(r - th) & 31]` (draw.c:793,1173), where `th` is the
//     polygon's normal index and `r` is the instance rotation bucket;
//   * tiles use the flat `plotContext.colour` computed in DrawTILES
//     (tile.c:227-253).
// This computes the same bytes at conversion time and hands them to
// WorldMeshBuilder as per-vertex colours (COLOR_0). `combointensity` and the
// time-of-day/weather are per-frame globals (main.c:1482), so the
// conversion-time value is baked; the dynamic coupling is documented here and is
// a follow-up to record in the knowledge handoff (a per-frame ambient factor
// would need the plane table and the instance rotation at draw time).
// The instance rotation `r` is not known per model slot, so the normal-shaded
// branch is baked at r = 0 (the plane table is symmetric in its shading pairs,
// so this matches an unrotated instance).
// ---------------------------------------------------------------------------

// The ambient intensity the world draw uses, in the same 0xRRGGBB form as
// `combointensity`. The live global is the primary source; the conversion runs
// at level load, before the first StepSim has set it, so a zero global falls
// back to StepSim's steady-state ambience (main.c:1444-1490) computed from the
// live time-of-day/weather. This keeps the bake from ever being all-black.
static u_int LegacyAmbient(void)
{
	if (combointensity != 0)
		return (u_int)combointensity;

	int ambient;
	if (gTimeOfDay == TIME_DAY)
		ambient = (gWeather - 1U > 1) ? 128 : 78;
	else if (gTimeOfDay == TIME_NIGHT)
		ambient = 128;
	else if (gTimeOfDay == TIME_DUSK)
		ambient = (gWeather - 1U < 2) ? 78 : 96;
	else
		ambient = (gTimeOfDay == TIME_DAWN) ? 26 : 128;

	return (u_int)(ambient | ambient << 8 | ambient << 16);
}

// Mirror of tile.c `fst_div_3`.
static int LegacyFstDiv3(int x)
{
	return x * 171 >> 9;
}

// Mirror of SetupPlaneColours (draw.c:370-412), computed locally so the bake
// never depends on plotContext having been populated by a prior frame.
static void LegacyComputePlaneColours(u_int ambient, u_int planeColours[8])
{
	u_int r, g, b;

	if (gWeather == 0 && gTimeOfDay >= 0 &&
		((1 << gTimeOfDay) & ((1 << TIME_DAWN) | (1 << TIME_DUSK))) == 0)
	{
		if (gTimeOfDay == TIME_DAY)
		{
			b = ambient & 255;
			g = ambient >> 8 & 255;
			r = ambient >> 16 & 255;

			planeColours[1] = (r * 120 >> 7) << 16 | (g * 120 >> 7) << 8 | b * 120 >> 7;
			planeColours[2] = (r * 103 >> 7) << 16 | (g * 103 >> 7) << 8 | b * 103 >> 7;
			planeColours[3] = (r * 13 >> 5) << 16 | (g * 13 >> 5) << 8 | b * 13 >> 5;
			planeColours[0] = r << 16 | g << 8 | b;
			planeColours[4] = (r * 3 >> 3) << 16 | (g * 3 >> 3) << 8 | b * 3 >> 3;
			planeColours[5] = planeColours[3];
			planeColours[6] = planeColours[2];
			planeColours[7] = planeColours[1];
			return;
		}

		for (int i = 0; i < 8; i++)
			planeColours[i] = ambient;
		return;
	}

	planeColours[0] = ambient;
	planeColours[1] = ambient + 0x10101;
	planeColours[2] = ambient + 0x30303;
	planeColours[3] = ambient + 0x80808;
	planeColours[4] = ambient + 0xa0a0a;
	planeColours[5] = ambient + 0x80808;
	planeColours[6] = ambient + 0x30303;
	planeColours[7] = ambient + 0x10101;
}

// Mirror of the DrawTILES colour block (tile.c:227-253), including its weather
// quartering. The dynamic-lighting variant (Tile1x1Lit / drawMeshLit) is not
// baked; the default gEnableDlights == 0 uses this flat colour.
static u_int LegacyComputeTileColour(void)
{
	u_int colour = 0;

	if (gTimeOfDay > -1)
	{
		const int combo = (int)LegacyAmbient();
		if (gTimeOfDay < TIME_NIGHT)
			colour = combo & 0xffffffU | 0x2C000000;
		else if (gTimeOfDay == TIME_NIGHT)
			colour = LegacyFstDiv3(combo >> 16 & 255) << 16 |
				LegacyFstDiv3(combo >> 8 & 255) << 8 |
				LegacyFstDiv3(combo & 255) | 0x2C000000U;
	}

	if (gWeather - 1U < 2)
	{
		const u_int col = colour >> 2 & 63;
		colour = col * 0x30000 | col * 0x300 | col * 3 | 0x2C000000;
	}

	return colour;
}

// True for the primitive classes the runtime shades with combointensity: the
// flat textured types 20/21 (after conversion 20->21). The PL (10/11) and
// gouraud (22/23) classes use the normal-indexed f4colourTable instead.
static bool LegacyClassUsesCombo(SourcePrimitiveClass primitiveClass)
{
	return primitiveClass == SourcePrimitiveClass::TexturedFlatTriangle ||
		primitiveClass == SourcePrimitiveClass::TexturedFlatQuad;
}

static void LegacyWriteRgb(std::vector<unsigned char>& out, unsigned int vertex, u_int colour)
{
	out[(size_t)vertex * 3 + 0] = (unsigned char)((colour >> 16) & 0xFF);
	out[(size_t)vertex * 3 + 1] = (unsigned char)((colour >> 8) & 0xFF);
	out[(size_t)vertex * 3 + 2] = (unsigned char)(colour & 0xFF);
}

// Copies the source's live legacy palette to RGB bytes for the native renderer.
void AssetCatalogGame_GetWorldShadePalette(unsigned char rgb[32 * 3])
{
	if (!rgb)
		return;
	u_int planes[8];
	LegacyComputePlaneColours(LegacyAmbient(), planes);
	WorldShadePalette_WriteRgb(planes, rgb);
}

// Builds one legacy-shaded RGB triple per source vertex. Returns false when the
// model is a sprite (it keeps the historical base-colour-factor behaviour) or
// when the decode carried no shading metadata, so the caller passes no override.
static bool LegacyComputeWorldColors(const SourceMesh& source, const MODEL* model,
	std::vector<unsigned char>& out)
{
	const size_t vertexCount = source.positions.size() / 3;
	if (vertexCount == 0 || source.shadeIndices.size() != vertexCount)
		return false;

	if ((model->shape_flags & SHAPE_FLAG_SPRITE) != 0)
		return false;

	const bool tile = ((model->shape_flags & (SHAPE_FLAG_WATER | SHAPE_FLAG_TILE)) != 0) ||
		((model->flags2 & (MODEL_FLAG_PATH | MODEL_FLAG_GRASS)) != 0);

	out.assign(vertexCount * 3, 0);

	if (tile)
	{
		const u_int colour = LegacyComputeTileColour();
		for (size_t v = 0; v < vertexCount; v++)
			LegacyWriteRgb(out, (unsigned int)v, colour);
		return true;
	}

	// Building: combo for ptype 20/21, the normal-indexed f4colourTable for
	// ptype 10/11/22/23. Every index belongs to exactly one submesh, so the
	// submesh's class selects the branch for each vertex it references.
	u_int planeColours[8];
	LegacyComputePlaneColours(LegacyAmbient(), planeColours);

	int f4colourTable[32];
	for (int i = 0; i < 8; i++)
	{
		f4colourTable[i * 4 + 0] = (int)(planeColours[i] | 0x2C000000);
		f4colourTable[i * 4 + 1] = (int)(planeColours[0] | 0x2C000000);
		f4colourTable[i * 4 + 2] = (int)(planeColours[5] | 0x2C000000);
		f4colourTable[i * 4 + 3] = (int)(planeColours[0] | 0x2C000000);
	}

	const u_int combo = LegacyAmbient() & 0x2ffffffU;

	for (size_t s = 0; s < source.submeshes.size(); s++)
	{
		const SourceSubmesh& submesh = source.submeshes[s];
		const bool usesCombo = LegacyClassUsesCombo(submesh.primitiveClass);

		for (unsigned int k = submesh.firstIndex; k < submesh.firstIndex + submesh.indexCount; k++)
		{
			const unsigned int vertex = source.indices[k];
			if (vertex >= vertexCount)
				return false;

			u_int colour;
			if (usesCombo)
				colour = combo;
			else
			{
				// Legacy: f4colourTable[(r - th) & 31]; baked at r = 0.
				const unsigned int index =
					(0u - (unsigned int)source.shadeIndices[vertex]) & 31u;
				colour = (u_int)f4colourTable[index];
			}
			LegacyWriteRgb(out, vertex, colour);
		}
	}

	return true;
}

// Presentation metadata only; the original normal bucket remains immutable.
static void CaptureWorldShadeIndices(const SourceMesh& source, const MODEL* model,
	std::vector<float>& out)
{
	const bool normalShadedBuilding = !(model->shape_flags &
		(SHAPE_FLAG_SPRITE | SHAPE_FLAG_WATER | SHAPE_FLAG_TILE)) &&
		!(model->flags2 & (MODEL_FLAG_PATH | MODEL_FLAG_GRASS));
	if (normalShadedBuilding && source.shadeIndices.size() == source.positions.size() / 3)
	{
		out.assign(source.shadeIndices.size(), -1.0f);
		bool hasNormalShade = false;
		for (size_t s = 0; s < source.submeshes.size(); ++s)
		{
			const SourceSubmesh& submesh = source.submeshes[s];
			if (LegacyClassUsesCombo(submesh.primitiveClass))
				continue;
			for (unsigned int k = submesh.firstIndex; k < submesh.firstIndex + submesh.indexCount; ++k)
			{
				const unsigned int vertex = source.indices[k];
				out[vertex] = (float)source.shadeIndices[vertex];
				hasNormalShade = true;
			}
		}
		if (!hasNormalShade)
			out.clear();
	}
}

// Converts one resident model slot and registers its resource. The caller owns
// request gating; this is shared by `-convertmodel` and `-drawmodel` preparation.
static bool ConvertModelSlot(int slot, char* status, int statusCapacity)
{
	s_conversionPending = false;
	if (slot < 0 || slot >= MAX_MODEL_SLOTS)
	{
		snprintf(status, statusCapacity, "convert-model: slot %d is out of range", slot);
		return false;
	}

	MODEL* model = modelpointers[slot];
	if (model == NULL || model == &dummyModel)
	{
		snprintf(status, statusCapacity, "convert-model: slot %d has no resident model", slot);
		return false;
	}

	const int record = AssetCatalog_FindModel(slot);
	if (record < 0)
	{
		snprintf(status, statusCapacity, "convert-model: slot %d is not resident in the asset catalog", slot);
		return false;
	}

	int span = 0;
	if (!AssetCatalog_GetModelSpan(record, &span) || span <= 0)
	{
		snprintf(status, statusCapacity, "convert-model: slot %d has no captured byte span", slot);
		return false;
	}

	const unsigned char* begin = (const unsigned char*)model;
	const unsigned int revision = AssetCatalog_GetModelHandle(record).revision;
	for (size_t i = 0; i < s_reusableRegionResources.size(); ++i)
	{
		const AssetCatalogGameModelResource& resource = s_reusableRegionResources[i];
		if (resource.modelSlot != slot || resource.modelRevision != revision ||
			resource.baseModelRevision != SharedModelRevision(slot))
			continue;
		StoreModelResource(slot, resource.resourceKey, resource.glbPath, resource.contentHash, resource.sourceHash);
		++s_regionReusedSlotCount;
		snprintf(status, statusCapacity, "convert-model: slot %d reuses its unchanged resident resource", slot);
		return true;
	}
	if (s_regionPreparationMode == RegionPreparationMode::Asynchronous)
	{
		if (s_regionFailedSlots[slot])
		{
			snprintf(status, statusCapacity, "convert-model: slot %d worker failed (previous diagnostic)", slot);
			return false;
		}
		if (s_regionConversionTask.valid())
		{
			s_conversionPending = true;
			return false;
		}
	}
	const unsigned char* end = begin + span;

	SourceModelView view = {};
	view.instanceNumber = model->instance_number;
	view.numVertices = model->num_vertices;
	view.numPolys = model->num_polys;
	view.numPointNormals = model->num_point_normals;
	view.vertices = model->vertices;
	view.polyBlock = model->poly_block;
	view.pointNormals = model->point_normals;

	// An instanced slot shares the base model's vertex and point-normal arrays;
	// its polygon indices are absolute into those arrays (models.c
	// `_MDL_GETTER_vertices`). The base may not be one of the region's distinct
	// slots, so resolve its span straight from the catalog and require the base
	// resident (never the dummy placeholder). A missing base is a named
	// diagnostic; the rest of the region still converts.
	if (model->instance_number != -1)
	{
		const int baseSlot = model->instance_number;
		if (baseSlot < 0 || baseSlot >= MAX_MODEL_SLOTS)
		{
			snprintf(status, statusCapacity,
				"convert-model: slot %d instance base %d is out of range", slot, baseSlot);
			return false;
		}

		MODEL* baseModel = modelpointers[baseSlot];
		if (baseModel == NULL || baseModel == &dummyModel)
		{
			snprintf(status, statusCapacity,
				"convert-model: slot %d instance base %d is not resident", slot, baseSlot);
			return false;
		}

		const int baseRecord = AssetCatalog_FindModel(baseSlot);
		if (baseRecord < 0)
		{
			snprintf(status, statusCapacity,
				"convert-model: slot %d instance base %d is not resident in the asset catalog",
				slot, baseSlot);
			return false;
		}

		int baseSpan = 0;
		if (!AssetCatalog_GetModelSpan(baseRecord, &baseSpan) || baseSpan <= 0)
		{
			snprintf(status, statusCapacity,
				"convert-model: slot %d instance base %d has no captured byte span", slot, baseSlot);
			return false;
		}

		view.baseBegin = (const unsigned char*)baseModel;
		view.baseEnd = view.baseBegin + baseSpan;
		view.baseVertices = baseModel->vertices;
		view.basePointNormals = baseModel->point_normals;
		view.baseNumVertices = baseModel->num_vertices;
		view.baseNumPointNormals = baseModel->num_point_normals;
	}

	SourceMesh source;
	char error[256];
	const SourceModelDecoderResult decoded = DecodeSourceModel(begin, end, view, &source,
		error, sizeof(error), SourceModelDecoderPolicy::SkipUnsupported);

	if (decoded.error != SOURCE_MODEL_ERROR_NONE)
	{
		snprintf(status, statusCapacity, "convert-model: slot %d decode failed: %s (%s)",
			slot, SourceModelDecoder_ErrorName(decoded.error), error);
		return false;
	}

	// Capture all game-owned inputs before handing work to another thread.
	TextureBakeResult bake;
	TextureBake_BakeModel(source, &bake);
	std::shared_ptr<WorldMeshConversionInput> input(new WorldMeshConversionInput);
	LegacyComputeWorldColors(source, model, input->legacyColors);
	CaptureWorldShadeIndices(source, model, input->worldShadeIndices);
	input->source = std::move(source);
	for (size_t i = 0; i < bake.images.size(); ++i)
	{
		const WorldSubmeshImage& image = bake.images[i];
		WorldConversionImage snapshot = { -1, image.width, image.height };
		if (image.rgba)
		{
			for (size_t pixel = 0; pixel < bake.pixels.size(); ++pixel)
				if (image.rgba == bake.pixels[pixel].data())
					{
						snapshot.pixelIndex = (int)pixel;
						break;
					}
			if (snapshot.pixelIndex < 0)
			{
				snprintf(status, statusCapacity, "convert-model: slot %d has an unowned image", slot);
				return false;
			}
		}
		input->images.push_back(snapshot);
	}
	input->pixels = std::move(bake.pixels);
	if (bake.untexturedCount || bake.truncatedCount)
		printWarning("AssetCatalog: texture-bake: slot %d: %d untextured, %d truncated: %s (%s)\n",
			slot, bake.untexturedCount, bake.truncatedCount,
			TextureBake_ReasonName(bake.firstReason), bake.firstDetail);

	_mkdir("native_cache");
	if (s_regionPreparationMode == RegionPreparationMode::Asynchronous)
	{
		try
		{
			const std::shared_ptr<const WorldMeshConversionInput> taskInput = input;
			s_regionConversionTask = std::async(std::launch::async, [taskInput]() {
				RegionConversionResult result = {};
				try
				{
					result.code = WorldMeshConversion_Publish("native_cache", *taskInput,
						&result.converted, result.error, sizeof(result.error));
				}
				catch (const std::exception& error)
				{
					result.code = ASSET_CONVERTER_ERR_PUBLISH;
					snprintf(result.error, sizeof(result.error), "%s", error.what());
				}
				catch (...)
				{
					result.code = ASSET_CONVERTER_ERR_PUBLISH;
					snprintf(result.error, sizeof(result.error), "unknown conversion exception");
				}
				return result;
			});
			s_regionTaskState = s_preparedRegionState;
			s_regionTaskSlot = slot;
			s_conversionPending = true;
			return false;
		}
		catch (const std::exception& error)
		{
			snprintf(status, statusCapacity, "convert-model: slot %d task launch failed: %s", slot, error.what());
			return false;
		}
	}
	WorldMeshConversionResult converted = {};
	const int code = WorldMeshConversion_Publish("native_cache", *input, &converted, error, sizeof(error));
	if (code != ASSET_CONVERTER_OK)
	{
		snprintf(status, statusCapacity, "convert-model: slot %d failed: %s (%s)",
			slot, AssetConverter_ErrorName(code), error);
		return false;
	}
	StoreModelResource(slot, converted.resourceKey, converted.asset.glbPath,
		converted.asset.contentHash, converted.sourceHash);
	snprintf(status, statusCapacity, "convert-model: slot %d, span %d bytes, vertices %d, triangles %d, skipped %u -> %s",
		slot, span, converted.built.vertexCount, converted.built.triangleCount, decoded.skippedCount, converted.asset.glbPath);
	return true;
}

bool AssetCatalogGame_ConvertModel(char* status, int statusCapacity)
{
	if (status == NULL || statusCapacity <= 0)
		return false;

	status[0] = '\0';

	if (!s_convertModelRequested)
	{
		snprintf(status, statusCapacity, "convert-model not requested");
		return false;
	}

	return ConvertModelSlot(s_convertModelSlot, status, statusCapacity);
}

// Bounded resident-placement collector for the native draw. It reuses the level
// inventory walker (VisitLevelObject/AddLevelPlacement via
// CollectBarrelPlacements) without the report path, decodes each referenced
// object once through QuickUnpackCellObject (never a source-model re-decode),
// and keeps only the target model slot's placements up to the bound.
int AssetCatalogGame_CollectDrawPlacements(int modelSlot, int maxPlacements,
	char* status, int statusCapacity)
{
	if (status && statusCapacity > 0)
		status[0] = '\0';

	s_drawPlacementCount = 0;

	if (!s_nativeDrawRequested)
	{
		if (status && statusCapacity > 0)
			snprintf(status, statusCapacity, "native draw not requested");
		return 0;
	}

	if (modelSlot < 0 || modelSlot >= MAX_MODEL_SLOTS)
	{
		if (status && statusCapacity > 0)
			snprintf(status, statusCapacity, "native draw: slot %d is out of range", modelSlot);
		return 0;
	}

	if (maxPlacements <= 0)
	{
		if (status && statusCapacity > 0)
			snprintf(status, statusCapacity, "native draw: no placement bound");
		return 0;
	}

	if (maxPlacements > ASSET_CATALOG_GAME_MAX_DRAW_PLACEMENTS)
		maxPlacements = ASSET_CATALOG_GAME_MAX_DRAW_PLACEMENTS;

	if (!AssetCatalog_IsValid())
	{
		if (status && statusCapacity > 0)
			snprintf(status, statusCapacity, "native draw: no level catalog context; load a level first");
		return 0;
	}

	if (cell_objects == NULL || cell_ptrs == NULL || cells == NULL || cell_objects_add[4] <= 0)
	{
		if (status && statusCapacity > 0)
			snprintf(status, statusCapacity, "native draw: no map region data is resident");
		return 0;
	}

	if (MAP_REGION_SIZE != 32 || MAP_CELL_SIZE <= 0 || regions_across <= 0 ||
		num_straddlers < 0 || num_straddlers > ASSET_CATALOG_MAX_PLACEMENTS ||
		cell_objects_add[4] > ASSET_CATALOG_MAX_PLACEMENTS - num_straddlers)
	{
		if (status && statusCapacity > 0)
			snprintf(status, statusCapacity, "native draw: invalid map dimensions or object capacity");
		return 0;
	}

	const int totalSlots = num_straddlers + cell_objects_add[4];
	const int placementCapacity = totalSlots < ASSET_CATALOG_MAX_PLACEMENTS ? totalSlots : ASSET_CATALOG_MAX_PLACEMENTS;

	AssetCatalogPlacement* placements = (AssetCatalogPlacement*)malloc((size_t)placementCapacity * sizeof(AssetCatalogPlacement));
	unsigned char* visited = (unsigned char*)calloc(ASSET_CATALOG_MAX_PLACEMENTS, 1);

	if (placements == NULL || visited == NULL)
	{
		free(placements);
		free(visited);
		if (status && statusCapacity > 0)
			snprintf(status, statusCapacity, "native draw: placement allocation failed");
		return 0;
	}

	LevelPlacementWalk walk = {};
	walk.placements = placements;
	walk.capacity = placementCapacity;
	walk.totalSlots = totalSlots;
	walk.visited = visited;

	for (int barrel = 0; barrel < 4; barrel++)
		CollectBarrelPlacements(walk, barrel);

	// The model handle carries the slot revision so the bridge can detect a
	// stale handle; the renderer itself only ever sees the resource key.
	const int record = AssetCatalog_FindModel(modelSlot);
	AssetCatalogHandle handle = {};
	if (record >= 0)
		handle = AssetCatalog_GetModelHandle(record);

	int copied = 0;
	for (int i = 0; i < walk.count && copied < maxPlacements; i++)
	{
		if (walk.placements[i].modelNumber != modelSlot)
			continue;

		AssetCatalogGameDrawPlacement& placement = s_drawPlacements[copied++];
		placement.x = walk.placements[i].x;
		placement.y = walk.placements[i].y;
		placement.z = walk.placements[i].z;
		placement.rotation = walk.placements[i].rotation;
		placement.sourceObjectIndex = walk.placements[i].sourceObjectIndex;
		placement.modelSlot = modelSlot;
		placement.modelRevision = handle.revision;
		placement.lodModelSlot = -1;	// the bounded dev draw keeps one model slot
		placement.lodModelRevision = 0;
		placement.highDetailModelSlot = -1;
		placement.highDetailModelRevision = 0;
	}

	s_drawPlacementCount = copied;

	if (status && statusCapacity > 0)
	{
		if (copied == 0)
			snprintf(status, statusCapacity, "native draw: slot %d has no resident placements", modelSlot);
		else
			snprintf(status, statusCapacity, "native draw: slot %d, %d placements (bound %d)", modelSlot, copied, maxPlacements);
	}

	free(placements);
	free(visited);
	return copied;
}

bool AssetCatalogGame_PrepareNativeDraw(char* status, int statusCapacity)
{
	if (status == NULL || statusCapacity <= 0)
		return false;

	status[0] = '\0';

	if (!s_nativeDrawRequested)
	{
		snprintf(status, statusCapacity, "native draw not requested");
		return false;
	}

	// Convert through the existing one-model path so the published resource is
	// registered; a failed conversion leaves the bridge without a key and the
	// scene stays on its existing content/placeholder path.
	char convertStatus[512];
	if (!ConvertModelSlot(s_nativeDrawSlot, convertStatus, sizeof(convertStatus)))
	{
		snprintf(status, statusCapacity, "native draw: %s", convertStatus);
		return false;
	}

	char collectStatus[256];
	const int count = AssetCatalogGame_CollectDrawPlacements(s_nativeDrawSlot,
		AssetCatalogGame_NativeDrawCount(), collectStatus, sizeof(collectStatus));

	if (count <= 0)
	{
		snprintf(status, statusCapacity, "native draw: slot %d converted; %s", s_nativeDrawSlot, collectStatus);
		return false;
	}

	char resourceKey[CONVERSION_CACHE_KEY_CAPACITY];
	char contentHash[CONVERSION_CACHE_HASH_CAPACITY];
	unsigned long long sourceHash = 0;

	if (!AssetCatalogGame_GetModelResource(s_nativeDrawSlot, resourceKey, sizeof(resourceKey),
		NULL, 0, contentHash, sizeof(contentHash), &sourceHash))
	{
		snprintf(status, statusCapacity, "native draw: slot %d placements collected but resource missing", s_nativeDrawSlot);
		return false;
	}

	snprintf(status, statusCapacity,
		"native draw: slot %d, %d placements, source-hash %016llx, content-hash %s, key %s",
		s_nativeDrawSlot, count, sourceHash, contentHash, resourceKey);
	return true;
}

// --- E03 visual-parity P8 dev-only content filter --------------------------
// RED2_NATIVE_REGION_FILTER selects one content class to drop (or keep) from
// the region prepare, so a capture can localize a class whose converted
// geometry/placement is suspect without a rebuild. It is read once from the
// environment on the first region prepare; unset/empty/"none" means no filter
// and the prepare output stays byte-for-byte unchanged. Developer/diagnostic
// only: the filter never mutates gameplay or the asset catalog.
enum RegionContentFilter
{
	REGION_FILTER_NONE = 0,
	REGION_FILTER_NO_INSTANCED,		// drop models with instance_number != -1
	REGION_FILTER_INSTANCED_ONLY,	// keep only those
	REGION_FILTER_NO_TILES,			// drop tile-class models
	REGION_FILTER_TILES_ONLY,		// keep only those
};

static RegionContentFilter s_regionFilter = REGION_FILTER_NONE;
static bool s_regionFilterRead = false;

static RegionContentFilter RegionContentFilterFromEnvironment(void)
{
	const char* value = getenv("RED2_NATIVE_REGION_FILTER");

	if (value == NULL || value[0] == '\0' || strcmp(value, "none") == 0)
		return REGION_FILTER_NONE;
	if (strcmp(value, "noinstanced") == 0)
		return REGION_FILTER_NO_INSTANCED;
	if (strcmp(value, "instancedonly") == 0)
		return REGION_FILTER_INSTANCED_ONLY;
	if (strcmp(value, "notiles") == 0)
		return REGION_FILTER_NO_TILES;
	if (strcmp(value, "tilesonly") == 0)
		return REGION_FILTER_TILES_ONLY;

	printWarning("AssetCatalog: region draw: unknown RED2_NATIVE_REGION_FILTER \"%s\"; no filter\n", value);
	return REGION_FILTER_NONE;
}

static RegionContentFilter GetRegionContentFilter(void)
{
	if (!s_regionFilterRead)
	{
		s_regionFilter = RegionContentFilterFromEnvironment();
		s_regionFilterRead = true;
	}

	return s_regionFilter;
}

static const char* RegionContentFilterName(RegionContentFilter filter)
{
	switch (filter)
	{
	case REGION_FILTER_NO_INSTANCED: return "noinstanced";
	case REGION_FILTER_INSTANCED_ONLY: return "instancedonly";
	case REGION_FILTER_NO_TILES: return "notiles";
	case REGION_FILTER_TILES_ONLY: return "tilesonly";
	default: return "none";
	}
}

// True when `modelSlot`'s live MODEL is an instanced sub-model
// (instance_number != -1). A non-resident or unclassifiable slot is not
// instanced, so the "no" filters retain it and the "only" filters drop it.
static bool RegionFilterModelIsInstanced(int modelSlot)
{
	if (modelSlot < 0 || modelSlot >= MAX_MODEL_SLOTS)
		return false;

	const MODEL* model = modelpointers[modelSlot];
	if (model == NULL || model == &dummyModel)
		return false;

	return model->instance_number != -1;
}

// True when `modelSlot`'s live MODEL is a tile-class model, using the exact
// predicate of the world draw dispatch (ClassifyDispatchInputs). A
// non-resident or unclassifiable slot is not a tile.
static bool RegionFilterModelIsTile(int modelSlot)
{
	if (modelSlot < 0 || modelSlot >= MAX_MODEL_SLOTS)
		return false;

	const MODEL* model = modelpointers[modelSlot];
	if (model == NULL || model == &dummyModel)
		return false;

	return ((model->shape_flags & (SHAPE_FLAG_WATER | SHAPE_FLAG_TILE)) != 0) ||
		((model->flags2 & (MODEL_FLAG_PATH | MODEL_FLAG_GRASS)) != 0);
}

// True when the active filter keeps `modelSlot`. The "only" filters keep a
// model solely when its predicate holds; the "no" filters drop a model solely
// when its predicate holds, so an unclassifiable slot is always retained by a
// "no" filter and never by an "only" filter.
static bool RegionFilterKeepsModel(int modelSlot)
{
	switch (GetRegionContentFilter())
	{
	case REGION_FILTER_NO_INSTANCED: return !RegionFilterModelIsInstanced(modelSlot);
	case REGION_FILTER_INSTANCED_ONLY: return RegionFilterModelIsInstanced(modelSlot);
	case REGION_FILTER_NO_TILES: return !RegionFilterModelIsTile(modelSlot);
	case REGION_FILTER_TILES_ONLY: return RegionFilterModelIsTile(modelSlot);
	default: return true;
	}
}

// --- E03 visual-parity P9 LOD tier -----------------------------------------
// The legacy world draw swaps to the low-detail model for distant placements:
//   draw.c:1041 (buildings) and tile.c:304 both use
//   `Z > DRAW_LOD_DIST_LOW ? pLodModels[slot] : modelpointers[slot]`.
// `pLodModels[slot]` is a DIFFERENT model slot when Low2LowerDetailTable maps
// this slot to another resident one (spool.c:667-677); otherwise it is the
// detail model itself. These helpers resolve that distinct variant so the
// region prepare can convert it as an additional resource.

// True when the legacy LOD tables declare a distinct lower-detail target for
// `modelSlot`, whether or not it is resident. Used only to name a fallback once.
static bool RegionHasDeclaredLod(int modelSlot)
{
	if (modelSlot < 0 || modelSlot >= MAX_MODEL_SLOTS || Low2LowerDetailTable == NULL)
		return false;

	// Low2LowerDetailTable has exactly num_models_in_pack entries (tile.c:
	// ProcessLowDetailTable); a slot outside that range has no declared LOD.
	if (modelSlot >= num_models_in_pack)
		return false;

	const unsigned int lod = Low2LowerDetailTable[modelSlot];
	return lod != 0xFFFFU && lod != (unsigned int)modelSlot && lod < (unsigned int)num_models_in_pack;
}

// The distinct, resident LOD variant slot for `modelSlot`, or -1 when the LOD
// model is the detail model (no link) or its target is not resident. The pointer
// comparison is the reliable check: pLodModels[slot] is a different model only
// when the mapping applies and the target was loaded.
static int RegionLodSlotFor(int modelSlot)
{
	if (modelSlot < 0 || modelSlot >= MAX_MODEL_SLOTS)
		return -1;

	if (pLodModels[modelSlot] == NULL || pLodModels[modelSlot] == modelpointers[modelSlot])
		return -1;

	if (!RegionHasDeclaredLod(modelSlot))
		return -1;

	const int lod = (int)Low2LowerDetailTable[modelSlot];

	if (pLodModels[modelSlot] != modelpointers[lod])
		return -1;

	if (modelpointers[lod] == NULL || modelpointers[lod] == &dummyModel)
		return -1;

	return lod;
}

// --- E03 visual-parity P10a near-tile high-detail tier ----------------------
// The legacy tile path swaps a close tile's model number up through
// Low2HighDetailTable before drawing it:
//   tile.c:290-295 `if (Z <= DRAW_LOD_DIST_HIGH) { if
//   (Low2HighDetailTable[model_number] != 0xffff) model_number =
//   Low2HighDetailTable[model_number]; ... TileNxN(pModel, ...); }`
// The cell data references the LOW-detail tile model, so the native draw must
// resolve and convert the HIGH-detail sibling to draw the pavement/curb
// structure the far LOD pass (P9) cannot recover. Only the tile dispatch path
// has this swap; buildings never do.

// True when Low2HighDetailTable declares a distinct high-detail sibling for
// `modelSlot`, whether or not it is resident. Used only to name a fallback once.
static bool RegionHasDeclaredHighDetail(int modelSlot)
{
	if (modelSlot < 0 || modelSlot >= MAX_MODEL_SLOTS || Low2HighDetailTable == NULL)
		return false;

	// Low2HighDetailTable has exactly num_models_in_pack entries; the lower-
	// detail table immediately follows it (tile.c:731), so a slot outside that
	// range must not be indexed here.
	if (modelSlot >= num_models_in_pack)
		return false;

	const unsigned int hd = Low2HighDetailTable[modelSlot];
	return hd != 0xFFFFU && hd != (unsigned int)modelSlot && hd < (unsigned int)MAX_MODEL_SLOTS;
}

// The distinct, resident high-detail sibling slot for a TILE-classified
// `modelSlot`, or -1 when the model is not a tile, has no declared sibling, or
// the sibling is not resident. The legacy applies this swap only in the tile
// path, so a non-tile model always returns -1.
static int RegionHighDetailSlotFor(int modelSlot)
{
	if (!RegionFilterModelIsTile(modelSlot))
		return -1;

	if (!RegionHasDeclaredHighDetail(modelSlot))
		return -1;

	const int hd = (int)Low2HighDetailTable[modelSlot];

	if (modelpointers[hd] == NULL || modelpointers[hd] == &dummyModel)
		return -1;

	return hd;
}

// Full-region native draw preparation. It reuses the level inventory walker
// without the report path (the same walker the bounded `-drawmodel` collector
// uses), derives the distinct referenced model slots, converts each once through
// the shared ConvertModelSlot path (which registers its published resource), and
// copies every resident placement into the bounded region store. A slot whose
// conversion fails is a named warning and is skipped while the rest continue;
// capacity overflow of placements or resources is a named truncation. No source
// model is decoded here beyond the conversion itself, so the draw never
// re-decodes.
static bool PrepareResidentRegionDraw(char* status, int statusCapacity)
{
	if (status == NULL || statusCapacity <= 0)
		return false;

	status[0] = '\0';

	if (!s_regionDrawRequested)
	{
		snprintf(status, statusCapacity, "region draw not requested");
		return false;
	}

	s_regionPlacementCount = 0;
	s_regionCellReferences.clear();
	// Only this snapshot's resources belong in the published registry. A failed
	// conversion of a reused slot must never retain its previous model/texture.
	s_modelResourceCount = 0;
	s_regionPlacementsTruncated = false;
	s_regionResourcesTruncated = false;
	s_regionConvertedSlotCount = 0;
	s_regionFailedSlotCount = 0;
	s_regionPendingSlots = 0;

	// Dev-only content filter (RED2_NATIVE_REGION_FILTER). Read once; NONE when
	// unset, in which case every branch below is the unchanged original path.
	const RegionContentFilter filter = GetRegionContentFilter();

	if (!AssetCatalog_IsValid())
	{
		snprintf(status, statusCapacity, "region draw: no level catalog context; load a level first");
		return false;
	}

	if (cell_objects == NULL || cell_ptrs == NULL || cells == NULL || cell_objects_add[4] <= 0)
	{
		snprintf(status, statusCapacity, "region draw: no map region data is resident");
		return false;
	}

	if (MAP_REGION_SIZE != 32 || MAP_CELL_SIZE <= 0 || regions_across <= 0 ||
		num_straddlers < 0 || num_straddlers > ASSET_CATALOG_MAX_PLACEMENTS ||
		cell_objects_add[4] > ASSET_CATALOG_MAX_PLACEMENTS - num_straddlers)
	{
		snprintf(status, statusCapacity, "region draw: invalid map dimensions or object capacity");
		return false;
	}

	const int totalSlots = num_straddlers + cell_objects_add[4];
	const int placementCapacity = totalSlots < ASSET_CATALOG_MAX_PLACEMENTS ? totalSlots : ASSET_CATALOG_MAX_PLACEMENTS;

	AssetCatalogPlacement* placements = (AssetCatalogPlacement*)malloc((size_t)placementCapacity * sizeof(AssetCatalogPlacement));
	unsigned char* visited = (unsigned char*)calloc(ASSET_CATALOG_MAX_PLACEMENTS, 1);

	if (placements == NULL || visited == NULL)
	{
		free(placements);
		free(visited);
		snprintf(status, statusCapacity, "region draw: placement allocation failed");
		return false;
	}

	LevelPlacementWalk walk = {};
	walk.placements = placements;
	walk.capacity = placementCapacity;
	walk.totalSlots = totalSlots;
	walk.visited = visited;
	walk.cellReferences = &s_regionCellReferences;

	for (int barrel = 0; barrel < 4; barrel++)
		CollectBarrelPlacements(walk, barrel);

	if (walk.referenceCapacityExceeded)
	{
		free(placements);
		free(visited);
		s_regionCellReferences.clear();
		snprintf(status, statusCapacity, "region draw: cell membership capacity exceeded");
		return false;
	}

	if (walk.eligible == 0)
	{
		free(placements);
		free(visited);
		snprintf(status, statusCapacity, "region draw: no eligible resident barrel data");
		return false;
	}

	// Distinct referenced slots, bounded by the resource registry capacity. A
	// full list is a named truncation: the extra slots are not converted.
	int distinctSlots[ASSET_CATALOG_GAME_MAX_MODEL_RESOURCES];
	int distinctCount = 0;

	for (int i = 0; i < walk.count; i++)
	{
		const int modelSlot = walk.placements[i].modelNumber;
		if (modelSlot < 0 || modelSlot >= MAX_MODEL_SLOTS)
			continue;

		if (!RegionFilterKeepsModel(modelSlot))
			continue;

		bool seen = false;
		for (int s = 0; s < distinctCount; s++)
			if (distinctSlots[s] == modelSlot)
			{
				seen = true;
				break;
			}
		if (seen)
			continue;

		if (distinctCount >= ASSET_CATALOG_GAME_MAX_MODEL_RESOURCES)
		{
			s_regionResourcesTruncated = true;
			continue;
		}

		distinctSlots[distinctCount++] = modelSlot;
	}

	// Distinct declared LOD variants for those detail slots (E03 visual-parity
	// P9). A variant that duplicates a detail slot or another variant is
	// converted once. The combined count is bounded by the resource registry
	// capacity; exceeding it is the same named resource truncation. A declared
	// LOD target that is not resident (or has no distinct model) is used as the
	// detail key and is named once.
	int lodSlots[ASSET_CATALOG_GAME_MAX_MODEL_RESOURCES];
	int lodCount = 0;
	bool lodFallbackLogged = false;

	for (int s = 0; s < distinctCount; s++)
	{
		const int detailSlot = distinctSlots[s];
		const int lod = RegionLodSlotFor(detailSlot);

		if (lod < 0)
		{
			if (RegionHasDeclaredLod(detailSlot) && !lodFallbackLogged)
			{
				printWarning("AssetCatalog: region draw: slot %d LOD variant is not resident; detail key used\n",
					detailSlot);
				lodFallbackLogged = true;
			}
			continue;
		}

		bool seen = false;
		for (int l = 0; l < lodCount && !seen; l++)
			seen = lodSlots[l] == lod;
		for (int d = 0; d < distinctCount && !seen; d++)
			seen = distinctSlots[d] == lod;
		if (seen)
			continue;

		if (distinctCount + lodCount >= ASSET_CATALOG_GAME_MAX_MODEL_RESOURCES)
		{
			s_regionResourcesTruncated = true;
			continue;
		}

		lodSlots[lodCount++] = lod;
	}

	// Distinct declared high-detail siblings of the TILE-classified detail
	// slots (E03 visual-parity P10a). The cell data references the low-detail
	// tile, so the legacy swaps the model number up through Low2HighDetailTable
	// when the tile is close (tile.c:290-295); that sibling must be converted as
	// an additional resource. Only the tile dispatch path has this swap, so the
	// resolver already returns -1 for non-tile models. A sibling that duplicates
	// a detail/LOD slot or another sibling is converted once. The combined count
	// is bounded by the resource registry capacity; exceeding it is the same
	// named resource truncation. A declared sibling that is not resident, or has
	// no distinct model, is used as the detail key and is named once.
	int highDetailSlots[ASSET_CATALOG_GAME_MAX_MODEL_RESOURCES];
	int highDetailCount = 0;
	bool highDetailFallbackLogged = false;

	for (int s = 0; s < distinctCount; s++)
	{
		const int detailSlot = distinctSlots[s];
		const int hd = RegionHighDetailSlotFor(detailSlot);

		if (hd < 0)
		{
			if (RegionFilterModelIsTile(detailSlot) && RegionHasDeclaredHighDetail(detailSlot) &&
				!highDetailFallbackLogged)
			{
				printWarning("AssetCatalog: region draw: slot %d high-detail sibling is not resident; detail key used\n",
					detailSlot);
				highDetailFallbackLogged = true;
			}
			continue;
		}

		bool seen = false;
		for (int h = 0; h < highDetailCount && !seen; h++)
			seen = highDetailSlots[h] == hd;
		for (int l = 0; l < lodCount && !seen; l++)
			seen = lodSlots[l] == hd;
		for (int d = 0; d < distinctCount && !seen; d++)
			seen = distinctSlots[d] == hd;
		if (seen)
			continue;

		if (distinctCount + lodCount + highDetailCount >= ASSET_CATALOG_GAME_MAX_MODEL_RESOURCES)
		{
			s_regionResourcesTruncated = true;
			continue;
		}

		highDetailSlots[highDetailCount++] = hd;
	}

	// Convert each distinct detail slot once; a failed slot is a named warning
	// and is skipped while the rest continue. A failed slot keeps no resource,
	// so the bridge submits its instances with an empty key and the renderer
	// counts them as skippedUnresolved rather than falling back to a placeholder.
	for (int s = 0; s < distinctCount; s++)
	{
		char convertStatus[512];

		if (ConvertModelSlot(distinctSlots[s], convertStatus, sizeof(convertStatus)))
			s_regionConvertedSlotCount++;
		else if (s_conversionPending)
			s_regionPendingSlots++;
		else
		{
			s_regionFailedSlotCount++;
			printWarning("AssetCatalog: region draw: %s\n", convertStatus);
		}
	}

	// Convert each distinct LOD variant once. A failed LOD conversion is a named
	// warning and leaves the placement on its detail key; the rest continue.
	for (int l = 0; l < lodCount; l++)
	{
		char convertStatus[512];

		if (ConvertModelSlot(lodSlots[l], convertStatus, sizeof(convertStatus)))
			s_regionConvertedSlotCount++;
		else if (s_conversionPending)
			s_regionPendingSlots++;
		else
		{
			s_regionFailedSlotCount++;
			printWarning("AssetCatalog: region draw (LOD): %s\n", convertStatus);
		}
	}

	// Convert each distinct high-detail sibling once. A failed high-detail
	// conversion is a named warning and leaves the placement on its detail key;
	// the rest continue.
	for (int h = 0; h < highDetailCount; h++)
	{
		char convertStatus[512];

		if (ConvertModelSlot(highDetailSlots[h], convertStatus, sizeof(convertStatus)))
			s_regionConvertedSlotCount++;
		else if (s_conversionPending)
			s_regionPendingSlots++;
		else
		{
			s_regionFailedSlotCount++;
			printWarning("AssetCatalog: region draw (high-detail): %s\n", convertStatus);
		}
	}

	if (s_regionPendingSlots > 0)
	{
		free(placements);
		free(visited);
		snprintf(status, statusCapacity, "region draw: preparing %d resource slots", s_regionPendingSlots);
		return false;
	}

	s_regionLodSlotCount = lodCount;
	s_regionHighDetailSlotCount = highDetailCount;

	// Resolve each distinct slot's revision once for the placement store; the
	// renderer only ever sees the resource key, so this is staleness provenance.
	unsigned int revisions[ASSET_CATALOG_GAME_MAX_MODEL_RESOURCES];

	for (int s = 0; s < distinctCount; s++)
	{
		const int record = AssetCatalog_FindModel(distinctSlots[s]);
		revisions[s] = record >= 0 ? AssetCatalog_GetModelHandle(record).revision : 0;
	}

	unsigned int lodRevisions[ASSET_CATALOG_GAME_MAX_MODEL_RESOURCES];

	for (int l = 0; l < lodCount; l++)
	{
		const int record = AssetCatalog_FindModel(lodSlots[l]);
		lodRevisions[l] = record >= 0 ? AssetCatalog_GetModelHandle(record).revision : 0;
	}

	unsigned int highDetailRevisions[ASSET_CATALOG_GAME_MAX_MODEL_RESOURCES];

	for (int h = 0; h < highDetailCount; h++)
	{
		const int record = AssetCatalog_FindModel(highDetailSlots[h]);
		highDetailRevisions[h] = record >= 0 ? AssetCatalog_GetModelHandle(record).revision : 0;
	}

	// Copy every resident placement into the bounded store. The store bound is
	// the native instance bound; a full store is a named truncation.
	int copied = 0;
	int filteredOut = 0;

	for (int i = 0; i < walk.count; i++)
	{
		const AssetCatalogPlacement& source = walk.placements[i];
		if (source.modelNumber < 0 || source.modelNumber >= MAX_MODEL_SLOTS)
			continue;

		if (!RegionFilterKeepsModel(source.modelNumber))
		{
			filteredOut++;
			continue;
		}

		if (copied >= ASSET_CATALOG_GAME_MAX_REGION_PLACEMENTS)
		{
			s_regionPlacementsTruncated = true;
			break;
		}

		unsigned int revision = 0;
		for (int s = 0; s < distinctCount; s++)
			if (distinctSlots[s] == source.modelNumber)
			{
				revision = revisions[s];
				break;
			}

		// E03 visual-parity P9: the LOD variant is usable only once its own
		// resource is published; a failed LOD conversion falls back to the detail
		// key (named once) instead of leaving the placement unresolved.
		int lodSlot = RegionLodSlotFor(source.modelNumber);
		unsigned int lodRevision = 0;

		if (lodSlot >= 0)
		{
			if (!AssetCatalogGame_GetModelResource(lodSlot, NULL, 0, NULL, 0, NULL, 0, NULL))
			{
				if (!lodFallbackLogged)
				{
					printWarning("AssetCatalog: region draw: LOD slot %d has no resource; detail key used\n",
						lodSlot);
					lodFallbackLogged = true;
				}
				lodSlot = -1;
			}
			else
			{
				for (int l = 0; l < lodCount; l++)
					if (lodSlots[l] == lodSlot)
					{
						lodRevision = lodRevisions[l];
						break;
					}
			}
		}

		// E03 visual-parity P10a: the near-tile high-detail sibling is usable
		// only once its own resource is published; a failed conversion falls back
		// to the detail key (named once). The resolver returns -1 for non-tile
		// models, so a building never carries a high-detail tier.
		int highDetailSlot = RegionHighDetailSlotFor(source.modelNumber);
		unsigned int highDetailRevision = 0;

		if (highDetailSlot >= 0)
		{
			if (!AssetCatalogGame_GetModelResource(highDetailSlot, NULL, 0, NULL, 0, NULL, 0, NULL))
			{
				if (!highDetailFallbackLogged)
				{
					printWarning("AssetCatalog: region draw: high-detail slot %d has no resource; detail key used\n",
						highDetailSlot);
					highDetailFallbackLogged = true;
				}
				highDetailSlot = -1;
			}
			else
			{
				for (int h = 0; h < highDetailCount; h++)
					if (highDetailSlots[h] == highDetailSlot)
					{
						highDetailRevision = highDetailRevisions[h];
						break;
					}
			}
		}

		AssetCatalogGameDrawPlacement& placement = s_regionPlacements[copied++];
		placement.x = source.x;
		placement.y = source.y;
		placement.z = source.z;
		placement.rotation = source.rotation;
		placement.sourceObjectIndex = source.sourceObjectIndex;
		placement.modelSlot = source.modelNumber;
		placement.modelRevision = revision;
		placement.lodModelSlot = lodSlot;
		placement.lodModelRevision = lodRevision;
		placement.highDetailModelSlot = highDetailSlot;
		placement.highDetailModelRevision = highDetailRevision;
	}

	// The walker's own object bound is a placement truncation too: placements
	// beyond it were never seen, so the drawn region is incomplete either way.
	if (walk.capacityExceeded)
		s_regionPlacementsTruncated = true;

	s_regionPlacementCount = copied;

	free(placements);
	free(visited);

	if (copied == 0)
	{
		if (filter != REGION_FILTER_NONE)
			snprintf(status, statusCapacity,
				"region draw: filter %s: no resident placements kept (%d filtered out)",
				RegionContentFilterName(filter), filteredOut);
		else
			snprintf(status, statusCapacity, "region draw: no resident placements");
		return false;
	}

	if (filter != REGION_FILTER_NONE)
	{
		printInfo("AssetCatalog: region draw: filter %s: %d placements kept, %d filtered out; "
			"%d detail slots, %d LOD slots, %d high-detail slots\n",
			RegionContentFilterName(filter), copied, filteredOut, distinctCount, lodCount, highDetailCount);

		snprintf(status, statusCapacity,
			"region draw: filter %s: %d placements%s, %d detail slots, %d LOD slots, %d high-detail slots%s, %d converted, %d failed; %s",
			RegionContentFilterName(filter),
			copied, s_regionPlacementsTruncated ? " (placement-truncated)" : "",
			distinctCount, lodCount, highDetailCount, s_regionResourcesTruncated ? " (resource-truncated)" : "",
			s_regionConvertedSlotCount, s_regionFailedSlotCount,
			walk.capacityExceeded ? "walker object bound reached" : "walker complete");
	}
	else
	{
		snprintf(status, statusCapacity,
			"region draw: %d placements%s, %d detail slots, %d LOD slots, %d high-detail slots%s, %d converted, %d failed; %s",
			copied, s_regionPlacementsTruncated ? " (placement-truncated)" : "",
			distinctCount, lodCount, highDetailCount, s_regionResourcesTruncated ? " (resource-truncated)" : "",
			s_regionConvertedSlotCount, s_regionFailedSlotCount,
			walk.capacityExceeded ? "walker object bound reached" : "walker complete");
	}
	return true;
}

static ResidentRegionState ReadResidentRegionState()
{
	ResidentRegionState state = {};
	for (int i = 0; i < 4; ++i)
		state.regions[i] = regions_unpacked[i];
	state.area = LoadedArea;
	state.modelRevision = s_regionModelRevision;
	memcpy(state.pageSlots, tpageslots, sizeof(state.pageSlots));
	memcpy(state.loadedPages, tpageloaded, sizeof(state.loadedPages));
	return state;
}

static bool SameResidentRegionState(const ResidentRegionState& a, const ResidentRegionState& b)
{
	return a.area == b.area && a.modelRevision == b.modelRevision &&
		memcmp(a.regions, b.regions, sizeof(a.regions)) == 0 &&
		memcmp(a.pageSlots, b.pageSlots, sizeof(a.pageSlots)) == 0 &&
		memcmp(a.loadedPages, b.loadedPages, sizeof(a.loadedPages)) == 0;
}

bool AssetCatalogGame_PrepareRegionDraw(char* status, int statusCapacity)
{
	if (!status || statusCapacity <= 0 || !s_regionDrawRequested)
		return false;
	const ResidentRegionState current = ReadResidentRegionState();
	s_reusableRegionResources.clear();
	s_regionReusedSlotCount = 0;
	if (s_preparedRegionStateValid &&
		current.area == s_preparedRegionState.area &&
		memcmp(current.pageSlots, s_preparedRegionState.pageSlots, sizeof(current.pageSlots)) == 0 &&
		memcmp(current.loadedPages, s_preparedRegionState.loadedPages, sizeof(current.loadedPages)) == 0)
	{
		s_reusableRegionResources.assign(s_modelResources, s_modelResources + s_modelResourceCount);
	}
	s_preparedRegionState = current;
	s_preparedRegionStateValid = true;
	s_regionPrepareSucceeded = PrepareResidentRegionDraw(status, statusCapacity);
	snprintf(s_regionPrepareError, sizeof(s_regionPrepareError), "%s", s_regionPrepareSucceeded ? "" : status);
	s_reusableRegionResources.clear();
	if (s_regionPendingSlots == 0)
	{
		s_regionDrawRevision++;
		printInfo("AssetCatalog: native publication %u reuses %d converted slots\n",
			s_regionDrawRevision, s_regionReusedSlotCount);
	}
	return s_regionPrepareSucceeded;
}

int AssetCatalogGame_UpdateRegionDraw(char* status, int statusCapacity)
{
	if (!status || statusCapacity <= 0)
		return -1;
	status[0] = '\0';
	if (!s_regionDrawRequested)
		return 0;
	if (!AssetCatalog_IsValid() || !cell_objects || !cell_ptrs || !cells)
		return 1;
	// UpdateSpoolPC is synchronous only with a local level file. Never inspect
	// the asynchronously rewritten CD buffers through this compatibility bridge.
#if USE_PC_FILESYSTEM
	extern char g_CurrentLevelFileName[64];
	if (g_CurrentLevelFileName[0] == '\0')
#endif
	{
		snprintf(status, statusCapacity, "native region streaming requires a local level file");
		return -1;
	}
	if (spoolactive || LoadingArea)
		return 1;
	for (int i = 0; i < 4; ++i)
		if (loading_region[i] != -1)
			return 1;
	const ResidentRegionState current = ReadResidentRegionState();
	if (!s_preparedRegionStateValid || !SameResidentRegionState(current, s_preparedRegionState))
		memset(s_regionFailedSlots, 0, sizeof(s_regionFailedSlots));
	if (s_regionConversionTask.valid() &&
		s_regionConversionTask.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
	{
		const RegionConversionResult result = s_regionConversionTask.get();
		if (SameResidentRegionState(current, s_regionTaskState))
		{
			if (result.code == ASSET_CONVERTER_OK)
				StoreModelResource(s_regionTaskSlot, result.converted.resourceKey, result.converted.asset.glbPath,
					result.converted.asset.contentHash, result.converted.sourceHash);
			else
			{
				s_regionFailedSlots[s_regionTaskSlot] = 1;
				printWarning("AssetCatalog: slot %d asynchronous conversion failed: %s\n", s_regionTaskSlot, result.error);
			}
		}
	}
	if (s_preparedRegionStateValid && SameResidentRegionState(current, s_preparedRegionState) && s_regionPendingSlots == 0)
	{
		if (!s_regionPrepareSucceeded)
			snprintf(status, statusCapacity, "%s", s_regionPrepareError);
		return s_regionPrepareSucceeded ? 0 : -1;
	}
	s_regionPreparationMode = RegionPreparationMode::Asynchronous;
	const bool success = AssetCatalogGame_PrepareRegionDraw(status, statusCapacity);
	s_regionPreparationMode = RegionPreparationMode::Immediate;
	if (s_regionPendingSlots > 0)
		return 1;
	printInfo("AssetCatalog: native resident revision %u: regions [%d,%d,%d,%d], area %d: %s\n",
		s_regionDrawRevision, current.regions[0], current.regions[1], current.regions[2],
		current.regions[3], current.area, status);
	return success ? 0 : -1;
}

#else // !_WIN32

void AssetCatalogGame_GetWorldShadePalette(unsigned char rgb[32 * 3])
{
	if (rgb)
		memset(rgb, 0, 32 * 3);
}

bool AssetCatalogGame_ReconcileLevel(char* status, int statusCapacity)
{
	if (status != NULL && statusCapacity > 0)
		snprintf(status, statusCapacity, "Level inventory reconciliation is currently available on Windows only");

	return false;
}

bool AssetCatalogGame_WriteModelPtypeReport(char* status, int statusCapacity)
{
	if (status != NULL && statusCapacity > 0)
		snprintf(status, statusCapacity, "Model polygon-type report is currently available on Windows only");

	return false;
}

bool AssetCatalogGame_ConvertModel(char* status, int statusCapacity)
{
	if (status != NULL && statusCapacity > 0)
		snprintf(status, statusCapacity, "Model conversion is currently available on Windows only");

	return false;
}

int AssetCatalogGame_CollectDrawPlacements(int modelSlot, int maxPlacements,
	char* status, int statusCapacity)
{
	(void)modelSlot;
	(void)maxPlacements;

	if (status != NULL && statusCapacity > 0)
		snprintf(status, statusCapacity, "Native draw placement collection is currently available on Windows only");

	return 0;
}

bool AssetCatalogGame_PrepareNativeDraw(char* status, int statusCapacity)
{
	if (status != NULL && statusCapacity > 0)
		snprintf(status, statusCapacity, "Native draw preparation is currently available on Windows only");

	return false;
}

bool AssetCatalogGame_PrepareRegionDraw(char* status, int statusCapacity)
{
	if (status != NULL && statusCapacity > 0)
		snprintf(status, statusCapacity, "Region draw preparation is currently available on Windows only");

	return false;
}

int AssetCatalogGame_UpdateRegionDraw(char* status, int statusCapacity)
{
	if (status && statusCapacity > 0)
		status[0] = '\0';
	return 0;
}

#endif // _WIN32

// Reads the bounded placement store on every non-PSX platform; it is only
// populated by the Windows-only collector.
int AssetCatalogGame_GetDrawPlacements(AssetCatalogGameDrawPlacement* out, int capacity)
{
	if (out == NULL || capacity <= 0)
		return 0;

	const int count = s_drawPlacementCount < capacity ? s_drawPlacementCount : capacity;
	for (int i = 0; i < count; i++)
		out[i] = s_drawPlacements[i];

	return count;
}

// Reads the published-resource registry on every non-PSX platform; it is only
// populated by the Windows-only conversion path.
bool AssetCatalogGame_GetModelResource(int modelSlot,
	char* resourceKey, int resourceKeyCapacity,
	char* glbPath, int glbPathCapacity,
	char* contentHash, int contentHashCapacity,
	unsigned long long* sourceHash)
{
	for (int i = 0; i < s_modelResourceCount; i++)
	{
		if (s_modelResources[i].modelSlot != modelSlot)
			continue;

		const AssetCatalogGameModelResource& entry = s_modelResources[i];
		if (resourceKey && resourceKeyCapacity > 0)
			snprintf(resourceKey, resourceKeyCapacity, "%s", entry.resourceKey);
		if (glbPath && glbPathCapacity > 0)
			snprintf(glbPath, glbPathCapacity, "%s", entry.glbPath);
		if (contentHash && contentHashCapacity > 0)
			snprintf(contentHash, contentHashCapacity, "%s", entry.contentHash);
		if (sourceHash)
			*sourceHash = entry.sourceHash;
		return true;
	}

	return false;
}

// Region-draw accessors read on every non-PSX platform; the region store is
// only populated by the Windows-only preparation path.
int AssetCatalogGame_GetRegionCellReferences(NativeWorldCellReference* out, int capacity)
{
	const int count = s_regionPlacementCount > 0 ? (int)s_regionCellReferences.size() : 0;
	if (!out && capacity == 0)
		return count;
	if (!out || capacity < count)
		return -1;
	if (count)
		memcpy(out, s_regionCellReferences.data(), (size_t)count * sizeof(*out));
	return count;
}

int AssetCatalogGame_GetRegionDrawPlacementCount(void)
{
	return s_regionPlacementCount;
}

bool AssetCatalogGame_GetRegionDrawPlacement(int index, AssetCatalogGameDrawPlacement* out)
{
	if (out == NULL || index < 0 || index >= s_regionPlacementCount)
		return false;

	*out = s_regionPlacements[index];
	return true;
}

int AssetCatalogGame_GetModelResourceCount(void)
{
	return s_modelResourceCount;
}

bool AssetCatalogGame_GetModelResourceAt(int index,
	char* resourceKey, int resourceKeyCapacity,
	char* glbPath, int glbPathCapacity,
	int* modelSlot)
{
	if (index < 0 || index >= s_modelResourceCount)
		return false;

	const AssetCatalogGameModelResource& entry = s_modelResources[index];
	if (resourceKey && resourceKeyCapacity > 0)
		snprintf(resourceKey, resourceKeyCapacity, "%s", entry.resourceKey);
	if (glbPath && glbPathCapacity > 0)
		snprintf(glbPath, glbPathCapacity, "%s", entry.glbPath);
	if (modelSlot)
		*modelSlot = entry.modelSlot;
	return true;
}

bool AssetCatalogGame_RegionDrawPlacementsTruncated(void)
{
	return s_regionPlacementsTruncated;
}

bool AssetCatalogGame_RegionDrawResourcesTruncated(void)
{
	return s_regionResourcesTruncated;
}

int AssetCatalogGame_RegionDrawLodSlotCount(void)
{
	return s_regionLodSlotCount;
}

int AssetCatalogGame_RegionDrawHighDetailSlotCount(void)
{
	return s_regionHighDetailSlotCount;
}

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

bool AssetCatalogGame_ModelPtypesRequested(void)
{
	return false;
}

bool AssetCatalogGame_ConvertModelRequested(void)
{
	return false;
}

int AssetCatalogGame_ConvertModelSlot(void)
{
	return -1;
}

bool AssetCatalogGame_NativeDrawRequested(void)
{
	return false;
}

int AssetCatalogGame_NativeDrawSlot(void)
{
	return -1;
}

int AssetCatalogGame_NativeDrawCount(void)
{
	return 0;
}

bool AssetCatalogGame_ConvertModel(char* status, int statusCapacity)
{
	if (status != NULL && statusCapacity > 0)
		status[0] = '\0';

	return false;
}

bool AssetCatalogGame_PrepareNativeDraw(char* status, int statusCapacity)
{
	if (status != NULL && statusCapacity > 0)
		status[0] = '\0';

	return false;
}

bool AssetCatalogGame_GetModelResource(int modelSlot,
	char* resourceKey, int resourceKeyCapacity,
	char* glbPath, int glbPathCapacity,
	char* contentHash, int contentHashCapacity,
	unsigned long long* sourceHash)
{
	(void)modelSlot;
	(void)resourceKey;
	(void)resourceKeyCapacity;
	(void)glbPath;
	(void)glbPathCapacity;
	(void)contentHash;
	(void)contentHashCapacity;
	(void)sourceHash;

	return false;
}

int AssetCatalogGame_CollectDrawPlacements(int modelSlot, int maxPlacements,
	char* status, int statusCapacity)
{
	(void)modelSlot;
	(void)maxPlacements;

	if (status != NULL && statusCapacity > 0)
		status[0] = '\0';

	return 0;
}

int AssetCatalogGame_GetDrawPlacements(AssetCatalogGameDrawPlacement* out, int capacity)
{
	(void)out;
	(void)capacity;

	return 0;
}

bool AssetCatalogGame_RegionDrawRequested(void)
{
	return false;
}

unsigned int AssetCatalogGame_LevelGeneration(void)
{
	return 0;
}

unsigned int AssetCatalogGame_RegionDrawRevision(void)
{
	return 0;
}

void AssetCatalogGame_ShutdownNativeConversion(void)
{
}

int AssetCatalogGame_UpdateRegionDraw(char* status, int statusCapacity)
{
	if (status && statusCapacity > 0)
		status[0] = '\0';
	return 0;
}

bool AssetCatalogGame_PrepareRegionDraw(char* status, int statusCapacity)
{
	if (status != NULL && statusCapacity > 0)
		status[0] = '\0';

	return false;
}

int AssetCatalogGame_GetRegionDrawPlacementCount(void)
{
	return 0;
}

int AssetCatalogGame_GetRegionCellReferences(NativeWorldCellReference* out, int capacity)
{
	(void)out;
	(void)capacity;
	return 0;
}

void AssetCatalogGame_GetWorldShadePalette(unsigned char rgb[32 * 3])
{
	if (rgb)
		memset(rgb, 0, 32 * 3);
}

bool AssetCatalogGame_GetRegionDrawPlacement(int index, AssetCatalogGameDrawPlacement* out)
{
	(void)index;
	(void)out;

	return false;
}

int AssetCatalogGame_GetModelResourceCount(void)
{
	return 0;
}

bool AssetCatalogGame_GetModelResourceAt(int index,
	char* resourceKey, int resourceKeyCapacity,
	char* glbPath, int glbPathCapacity,
	int* modelSlot)
{
	(void)index;
	(void)resourceKey;
	(void)resourceKeyCapacity;
	(void)glbPath;
	(void)glbPathCapacity;
	(void)modelSlot;

	return false;
}

bool AssetCatalogGame_RegionDrawPlacementsTruncated(void)
{
	return false;
}

bool AssetCatalogGame_RegionDrawResourcesTruncated(void)
{
	return false;
}

int AssetCatalogGame_RegionDrawLodSlotCount(void)
{
	return 0;
}

int AssetCatalogGame_RegionDrawHighDetailSlotCount(void)
{
	return 0;
}

void AssetCatalogGame_RecordModelSpan(int modelIndex, const unsigned char* modelBegin, int spanBytes)
{
	(void)modelIndex;
	(void)modelBegin;
	(void)spanBytes;
}

bool AssetCatalogGame_ReconcileLevel(char* status, int statusCapacity)
{
	if (status != NULL && statusCapacity > 0)
		status[0] = '\0';

	return false;
}

bool AssetCatalogGame_WriteModelPtypeReport(char* status, int statusCapacity)
{
	if (status != NULL && statusCapacity > 0)
		status[0] = '\0';

	return false;
}

#endif /* !PSX */
