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

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

#ifdef _WIN32
#include <direct.h>
#include <stdarg.h>
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

// Full-region placement store and its capacity diagnostics. Populated once by
// AssetCatalogGame_PrepareRegionDraw and read by the native-draw bridge; the
// store is sized to the renderer's instance bound so it never overflows.
AssetCatalogGameDrawPlacement s_regionPlacements[ASSET_CATALOG_GAME_MAX_REGION_PLACEMENTS];
int s_regionPlacementCount = 0;
bool s_regionPlacementsTruncated = false;
bool s_regionResourcesTruncated = false;
int s_regionConvertedSlotCount = 0;
int s_regionFailedSlotCount = 0;

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
	AssetCatalog_BeginContext(GameLevel, (int)GetCityType(), gMultiplayerLevels);

	// A new level starts a fresh generation. The bridge compares this to detect
	// a level change independently of the registry contents, because BeginLevel
	// and the post-SpoolSYNC preparation can run in the same frame, so the
	// registry may already be repopulated when the bridge frame handler runs.
	s_levelGeneration++;

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
	s_regionPlacementsTruncated = false;
	s_regionResourcesTruncated = false;
	s_regionConvertedSlotCount = 0;
	s_regionFailedSlotCount = 0;
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

// Canonical digest of the decoded SourceMesh inputs the world mesh builder
// consumes, so a changed source model produces a different conversion-cache key.
// Arrays are hashed in a fixed order, then each submesh descriptor, so colour
// presence and material grouping are part of the identity. Deterministic.
static unsigned long long HashSourceMesh(const SourceMesh& mesh)
{
	std::vector<unsigned char> bytes;
	bytes.reserve(mesh.positions.size() * sizeof(float) + mesh.uvs.size() * sizeof(float) +
		mesh.colors.size() + mesh.indices.size() * sizeof(unsigned int) +
		mesh.submeshes.size() * 12);

	const unsigned char* positions = (const unsigned char*)mesh.positions.data();
	bytes.insert(bytes.end(), positions, positions + mesh.positions.size() * sizeof(float));
	const unsigned char* uvs = (const unsigned char*)mesh.uvs.data();
	bytes.insert(bytes.end(), uvs, uvs + mesh.uvs.size() * sizeof(float));
	bytes.insert(bytes.end(), mesh.colors.begin(), mesh.colors.end());
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

// Stores (or replaces) the published resource of a slot. The registry is
// capacity-limited; a full table is a named warning and drops the new entry
// rather than evicting a resident one.
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
}

// Reads a whole file into `out`. Returns false for a missing/empty/unreadable
// file. Local to the multi-mesh publish path below.
static bool ReadWholeFileBytes(const char* path, std::vector<unsigned char>& out)
{
	FILE* file = fopen(path, "rb");
	if (file == NULL)
		return false;
	fseek(file, 0, SEEK_END);
	const long size = ftell(file);
	fseek(file, 0, SEEK_SET);
	if (size <= 0)
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
static int PublishMultiMesh(const char* cacheDirectory, const ConversionCacheKey* key,
	const GltfMultiMeshData& mesh, AssetConverterResult* out, char* error, int errorCapacity)
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

// Deterministic FNV-1a 64 over the baked submesh images, used as the conversion
// key's options hash. A textured conversion must not reuse a cached untextured
// GLB for the same source model (and a re-bake with different VRAM pixels must
// not reuse an older one), so the key reflects the baked pixels. Returns 0 when
// nothing was baked, which keeps the legacy untextured key exactly unchanged.
static unsigned long long HashBakedImages(const TextureBakeResult& bake)
{
	if (bake.pixels.empty())
		return 0;

	std::vector<unsigned char> stream;
	for (size_t i = 0; i < bake.images.size(); i++)
	{
		const WorldSubmeshImage& image = bake.images[i];
		stream.push_back(image.rgba ? 1 : 0);
		for (int shift = 0; shift < 32; shift += 8)
		{
			stream.push_back((unsigned char)((unsigned)image.width >> shift));
			stream.push_back((unsigned char)((unsigned)image.height >> shift));
			stream.push_back((unsigned char)((unsigned)image.byteCount >> shift));
		}
		if (image.rgba)
			stream.insert(stream.end(), image.rgba, image.rgba + image.byteCount);
	}
	return ConversionCache_HashBytes(stream.data(), (int)stream.size());
}

// Converts one resident model slot and registers its published resource. The
// caller owns the request gating; this is the shared core of the
// `-convertmodel` flag and the `-drawmodel` preparation.
static bool ConvertModelSlot(int slot, char* status, int statusCapacity)
{
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

	GltfMultiMeshData mesh;
	WorldMeshBuilderResult built;

	// Bake the model's per-submesh base-colour images from the resident VRAM
	// before building. The decoded mesh only carries the texture set/id, so this
	// reads VRAM once and turns each referenced (page, CLUT) pair into one RGBA
	// image. A submesh whose page is missing or fails to decode stays untextured;
	// the conversion still proceeds.
	TextureBakeResult bake;
	TextureBake_BakeModel(source, &bake);

	if (WorldMeshBuilder_BuildMulti(source, &mesh, &built, error, sizeof(error),
			bake.images.empty() ? NULL : bake.images.data(), (int)bake.images.size()) != WORLD_MESH_ERROR_NONE)
	{
		snprintf(status, statusCapacity, "convert-model: slot %d build failed: %s (%s)",
			slot, WorldMeshBuilder_ErrorName(built.error), error);
		return false;
	}

	// Additive texture-bake diagnostics; the conversion/publish wording below is
	// unchanged. A missing/evicted page, an unsupported format or a decode
	// failure is a named warning and leaves only that submesh untextured.
	if (bake.submeshCount > 0)
	{
		if (bake.untexturedCount == 0 && bake.truncatedCount == 0)
			printInfo("AssetCatalog: texture-bake: slot %d, %d/%d submeshes textured (%d deduped, %d images)\n",
				slot, bake.bakedCount, bake.submeshCount, bake.dedupedCount, (int)bake.pixels.size());
		else
			printWarning("AssetCatalog: texture-bake: slot %d, %d/%d submeshes textured (%d deduped), "
				"%d untextured (%d by image cap); first %s (%s)\n",
				slot, bake.bakedCount, bake.submeshCount, bake.dedupedCount,
				bake.untexturedCount, bake.truncatedCount,
				TextureBake_ReasonName(bake.firstReason), bake.firstDetail);
	}

	const unsigned long long sourceHash = HashSourceMesh(source);
	const unsigned long long optionsHash = HashBakedImages(bake);
	ConversionCacheKey key;
	ConversionCache_MakeKey(&key, "world-mesh", sourceHash, optionsHash, 0ULL, CONVERSION_CACHE_SCHEMA_VERSION);

	const char* cacheDirectory = "native_cache";
	_mkdir(cacheDirectory); // an existing directory is fine; a real failure surfaces in the publish step

	AssetConverterResult published;
	const int code = PublishMultiMesh(cacheDirectory, &key, mesh, &published, error, sizeof(error));
	if (code != ASSET_CONVERTER_OK)
	{
		snprintf(status, statusCapacity, "convert-model: slot %d publish failed: %s (%s)",
			slot, AssetConverter_ErrorName(code), error);
		return false;
	}

	// Record the published resource so the native-draw bridge can register it
	// by key without re-decoding. A key-format failure leaves the entry out and
	// is a named warning, not a conversion failure.
	char keyText[CONVERSION_CACHE_KEY_CAPACITY];
	if (ConversionCache_FormatKey(&key, keyText, sizeof(keyText)) > 0)
		StoreModelResource(slot, keyText, published.glbPath, published.contentHash, sourceHash);
	else
		printWarning("AssetCatalog: convert-model: slot %d key formatting failed; resource not tracked\n", slot);

	snprintf(status, statusCapacity,
		"convert-model: slot %d, span %d bytes, submeshes %d, vertices %d, triangles %d, skipped %u, "
		"source-hash %016llx, content-hash %s -> %s",
		slot, span, built.submeshCount, built.vertexCount, built.triangleCount, decoded.skippedCount,
		sourceHash, published.contentHash, published.glbPath);
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

// Full-region native draw preparation. It reuses the level inventory walker
// without the report path (the same walker the bounded `-drawmodel` collector
// uses), derives the distinct referenced model slots, converts each once through
// the shared ConvertModelSlot path (which registers its published resource), and
// copies every resident placement into the bounded region store. A slot whose
// conversion fails is a named warning and is skipped while the rest continue;
// capacity overflow of placements or resources is a named truncation. No source
// model is decoded here beyond the conversion itself, so the draw never
// re-decodes.
bool AssetCatalogGame_PrepareRegionDraw(char* status, int statusCapacity)
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
	s_regionPlacementsTruncated = false;
	s_regionResourcesTruncated = false;
	s_regionConvertedSlotCount = 0;
	s_regionFailedSlotCount = 0;

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

	for (int barrel = 0; barrel < 4; barrel++)
		CollectBarrelPlacements(walk, barrel);

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

	// Convert each distinct slot once; a failed slot is a named warning and is
	// skipped while the rest continue. A failed slot keeps no resource, so the
	// bridge submits its instances with an empty key and the renderer counts
	// them as skippedUnresolved rather than falling back to a placeholder.
	for (int s = 0; s < distinctCount; s++)
	{
		char convertStatus[512];

		if (ConvertModelSlot(distinctSlots[s], convertStatus, sizeof(convertStatus)))
			s_regionConvertedSlotCount++;
		else
		{
			s_regionFailedSlotCount++;
			printWarning("AssetCatalog: region draw: %s\n", convertStatus);
		}
	}

	// Resolve each distinct slot's revision once for the placement store; the
	// renderer only ever sees the resource key, so this is staleness provenance.
	unsigned int revisions[ASSET_CATALOG_GAME_MAX_MODEL_RESOURCES];

	for (int s = 0; s < distinctCount; s++)
	{
		const int record = AssetCatalog_FindModel(distinctSlots[s]);
		revisions[s] = record >= 0 ? AssetCatalog_GetModelHandle(record).revision : 0;
	}

	// Copy every resident placement into the bounded store. The store bound is
	// the native instance bound; a full store is a named truncation.
	int copied = 0;

	for (int i = 0; i < walk.count; i++)
	{
		const AssetCatalogPlacement& source = walk.placements[i];
		if (source.modelNumber < 0 || source.modelNumber >= MAX_MODEL_SLOTS)
			continue;

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

		AssetCatalogGameDrawPlacement& placement = s_regionPlacements[copied++];
		placement.x = source.x;
		placement.y = source.y;
		placement.z = source.z;
		placement.rotation = source.rotation;
		placement.sourceObjectIndex = source.sourceObjectIndex;
		placement.modelSlot = source.modelNumber;
		placement.modelRevision = revision;
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
		snprintf(status, statusCapacity, "region draw: no resident placements");
		return false;
	}

	snprintf(status, statusCapacity,
		"region draw: %d placements%s, %d distinct slots%s, %d converted, %d failed; %s",
		copied, s_regionPlacementsTruncated ? " (placement-truncated)" : "",
		distinctCount, s_regionResourcesTruncated ? " (resource-truncated)" : "",
		s_regionConvertedSlotCount, s_regionFailedSlotCount,
		walk.capacityExceeded ? "walker object bound reached" : "walker complete");
	return true;
}

#else // !_WIN32

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
