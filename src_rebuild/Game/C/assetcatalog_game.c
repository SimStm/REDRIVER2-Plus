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

// Developer flag: accumulate the model polygon-type probe at load and write a
// report once after SpoolSYNC. Never interrupts the game on failure.
static bool s_modelPtypesRequested = false;

void AssetCatalogGame_ConfigureFromCommandLine(int argc, char** argv)
{
	for (int i = 1; i < argc; i++)
	{
		if (strcmp(argv[i], "-assetinventory") == 0)
			s_inventoryRequested = true;
		else if (strcmp(argv[i], "-modelptypes") == 0)
			s_modelPtypesRequested = true;
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

namespace
{
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

	// A new level starts a fresh probe: discard the previous level's per-slot
	// records so a multi-level session reports only the current level. This is
	// always safe and never interrupts the game; the probe itself stays
	// flag-gated in AssetCatalogGame_RecordModelSpan.
	s_ptypeSlotCount = 0;
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

bool AssetCatalogGame_ModelPtypesRequested(void)
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
