#include "assetcatalog.h"

#ifndef PSX

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * The desktop inspector diagnostic consumes the catalog. Keep it out of the
 * PSX build's static footprint; no PSX path calls these functions yet.
 */

namespace
{
// Cover every addressable model/texture slot a streaming level can touch, not
// just the resident set: a model index is registered once and reused.
// Marks a component key and separates it from its parent instance key. Neither
// the parent nor the component token may contain it, so keys round-trip.
const char kComponentMarker[] = "/component:";

const int kMaxModels = 1536;
const int kMaxTextures = 2048;
const int kMaxCars = 32;
const int kMaxMaterialRefs = 8192;
const int kMaxCarPalettes = 64;

struct ModelRecord
{
	bool used;
	unsigned int revision;
	int modelIndex;
	int lodParentModelIndex;
	int highDetailModelIndex;
	int spanBytes;
	AssetCatalogSource source;
	char name[ASSET_CATALOG_NAME_CAPACITY];
};

struct TextureRecord
{
	bool used;
	unsigned int revision;
	int texturePage;
	int textureIndex;
	int textureSet;
	AssetCatalogSource source;
	char name[ASSET_CATALOG_NAME_CAPACITY];
};

struct CarRecord
{
	bool used;
	unsigned int revision;
	int slot;
	int modelNumber;
	int modelRecord;
	AssetCatalogSource source;
	char name[ASSET_CATALOG_NAME_CAPACITY];
};

struct MaterialRef
{
	int modelRecord;
	int textureRecord;
};

struct CarPalette
{
	int carRecord;
	int paletteIndex;
};

ModelRecord s_models[kMaxModels];
TextureRecord s_textures[kMaxTextures];
CarRecord s_cars[kMaxCars];
MaterialRef s_materialRefs[kMaxMaterialRefs];
CarPalette s_carPalettes[kMaxCarPalettes];

int s_modelCount = 0;
int s_textureCount = 0;
int s_carCount = 0;
int s_materialRefCount = 0;
int s_carPaletteCount = 0;

unsigned int s_generation = 0;
bool s_valid = false;
AssetCatalogContext s_context = { 0, 0, 0 };

// Scratch for the placement identity sort. The catalog is a singleton and the
// reconcile runs on one thread, so the order buffer and the placement pointer
// the comparator needs are plain file state.
int s_placementOrder[ASSET_CATALOG_MAX_PLACEMENTS];
const AssetCatalogPlacement* s_sortPlacements = NULL;

void CopyName(char* destination, const char* source)
{
	if (source == NULL)
		source = "";
	snprintf(destination, ASSET_CATALOG_NAME_CAPACITY, "%s", source);
}

void RemoveModelMaterials(int record);

// Shared registration path. `spanBytes` <= 0 means the loader did not capture a
// size; the span is provenance and never participates in identity or revision.
int RegisterModelRecord(int modelIndex, const char* name, AssetCatalogSource source,
	int lodParentModelIndex, int highDetailModelIndex, int spanBytes)
{
	if (!s_valid || modelIndex < 0)
		return -1;

	// Match any record for this slot, live or previously freed, so a streamed
	// slot reuses its record and both registration and invalidation bump the
	// revision that retained handles depend on.
	for (int i = 0; i < s_modelCount; i++)
	{
		if (s_models[i].modelIndex == modelIndex)
		{
			RemoveModelMaterials(i);
			s_models[i].used = true;
			s_models[i].revision++;
			s_models[i].lodParentModelIndex = lodParentModelIndex;
			s_models[i].highDetailModelIndex = highDetailModelIndex;
			s_models[i].spanBytes = spanBytes;
			s_models[i].source = source;
			CopyName(s_models[i].name, name);
			return i;
		}
	}

	if (s_modelCount >= kMaxModels)
		return -1;

	ModelRecord* record = &s_models[s_modelCount];
	record->used = true;
	record->revision = 1;
	record->modelIndex = modelIndex;
	record->lodParentModelIndex = lodParentModelIndex;
	record->highDetailModelIndex = highDetailModelIndex;
	record->spanBytes = spanBytes;
	record->source = source;
	CopyName(record->name, name);

	return s_modelCount++;
}

bool ValidModelRecord(int record)
{
	return record >= 0 && record < s_modelCount && s_models[record].used;
}

bool ValidTextureRecord(int record)
{
	return record >= 0 && record < s_textureCount && s_textures[record].used;
}

bool ValidCarRecord(int record)
{
	return record >= 0 && record < s_carCount && s_cars[record].used;
}

void RemoveModelMaterials(int record)
{
	int kept = 0;
	for (int i = 0; i < s_materialRefCount; ++i)
		if (s_materialRefs[i].modelRecord != record)
			s_materialRefs[kept++] = s_materialRefs[i];
	s_materialRefCount = kept;
}

bool SamePlacement(const AssetCatalogPlacement& a, const AssetCatalogPlacement& b)
{
	return a.region == b.region && a.modelNumber == b.modelNumber && a.x == b.x &&
		a.y == b.y && a.z == b.z && a.rotation == b.rotation;
}

int ComparePlacementIndices(const void* left, const void* right)
{
	const AssetCatalogPlacement& a = s_sortPlacements[*(const int*)left];
	const AssetCatalogPlacement& b = s_sortPlacements[*(const int*)right];

	if (a.region != b.region) return a.region < b.region ? -1 : 1;
	if (a.modelNumber != b.modelNumber) return a.modelNumber < b.modelNumber ? -1 : 1;
	if (a.x != b.x) return a.x < b.x ? -1 : 1;
	if (a.y != b.y) return a.y < b.y ? -1 : 1;
	if (a.z != b.z) return a.z < b.z ? -1 : 1;
	if (a.rotation != b.rotation) return a.rotation < b.rotation ? -1 : 1;
	return 0;
}

// Stores one bounded finding. Error findings are added before warning findings
// by the caller, so truncation can only drop warnings after real problems.
void AddInventoryFinding(AssetCatalogInventoryResult* result, bool warning, const char* reason,
	int region, int modelNumber, int x, int y, int z, int rotation)
{
	if (warning) result->warningCount++; else result->errorCount++;
	if (result->findingCount >= ASSET_CATALOG_INVENTORY_MAX_FINDINGS)
	{
		result->findingsTruncated = true;
		if (warning) return;
		int slot = result->findingCount - 1;
		while (slot >= 0 && !result->findings[slot].warning) --slot;
		if (slot < 0) return;
		// Displace a warning; preserve true warning/error totals.
		result->findings[slot] = result->findings[--result->findingCount];
	}

	AssetCatalogInventoryFinding* finding = &result->findings[result->findingCount++];
	memset(finding, 0, sizeof(*finding));
	finding->warning = warning;
	snprintf(finding->reason, sizeof(finding->reason), "%s", reason ? reason : "");
	finding->region = region;
	finding->modelNumber = modelNumber;
	finding->x = x;
	finding->y = y;
	finding->z = z;
	finding->rotation = rotation;
}
}

void AssetCatalog_Reset(void)
{
	memset(s_models, 0, sizeof(s_models));
	memset(s_textures, 0, sizeof(s_textures));
	memset(s_cars, 0, sizeof(s_cars));
	memset(s_materialRefs, 0, sizeof(s_materialRefs));
	memset(s_carPalettes, 0, sizeof(s_carPalettes));

	s_modelCount = 0;
	s_textureCount = 0;
	s_carCount = 0;
	s_materialRefCount = 0;
	s_carPaletteCount = 0;

	s_generation++;
	s_valid = false;
}

void AssetCatalog_BeginContext(int level, int variant, int multiplayer)
{
	const bool sameContext = s_valid &&
		s_context.level == level && s_context.variant == variant &&
		s_context.multiplayer == multiplayer;

	if (!sameContext)
		AssetCatalog_Reset();

	s_context.level = level;
	s_context.variant = variant;
	s_context.multiplayer = multiplayer;
	s_valid = true;
}

bool AssetCatalog_IsValid(void)
{
	return s_valid;
}

unsigned int AssetCatalog_GetGeneration(void)
{
	return s_generation;
}

bool AssetCatalog_GetContext(AssetCatalogContext* context)
{
	if (!s_valid || context == NULL)
		return false;

	*context = s_context;
	return true;
}

int AssetCatalog_RegisterModel(int modelIndex, const char* name,
	AssetCatalogSource source, int lodParentModelIndex, int highDetailModelIndex)
{
	return RegisterModelRecord(modelIndex, name, source, lodParentModelIndex, highDetailModelIndex, 0);
}

int AssetCatalog_RegisterModelWithSpan(int modelIndex, const char* name,
	AssetCatalogSource source, int lodParentModelIndex, int highDetailModelIndex, int spanBytes)
{
	return RegisterModelRecord(modelIndex, name, source, lodParentModelIndex, highDetailModelIndex, spanBytes);
}

bool AssetCatalog_GetModelSpan(int record, int* byteSize)
{
	if (!ValidModelRecord(record))
		return false;

	if (byteSize)
		*byteSize = s_models[record].spanBytes;

	return true;
}

int AssetCatalog_FindModel(int modelIndex)
{
	for (int i = 0; i < s_modelCount; i++)
	{
		if (s_models[i].used && s_models[i].modelIndex == modelIndex)
			return i;
	}
	return -1;
}

bool AssetCatalog_InvalidateModel(int modelIndex)
{
	if (modelIndex < 0)
		return false;

	for (int i = 0; i < s_modelCount; i++)
	{
		if (s_models[i].modelIndex == modelIndex && s_models[i].used)
		{
			RemoveModelMaterials(i);
			s_models[i].used = false;
			s_models[i].revision++;
			return true;
		}
	}

	return false;
}

bool AssetCatalog_GetModel(int record, int* modelIndex, char* name, int nameCapacity,
	AssetCatalogSource* source, int* lodParentModelIndex, int* highDetailModelIndex)
{
	if (!ValidModelRecord(record))
		return false;

	if (modelIndex) *modelIndex = s_models[record].modelIndex;
	if (name) snprintf(name, nameCapacity, "%s", s_models[record].name);
	if (source) *source = s_models[record].source;
	if (lodParentModelIndex) *lodParentModelIndex = s_models[record].lodParentModelIndex;
	if (highDetailModelIndex) *highDetailModelIndex = s_models[record].highDetailModelIndex;

	return true;
}

AssetCatalogHandle AssetCatalog_GetModelHandle(int record)
{
	AssetCatalogHandle handle = { 0, 0, -1 };

	if (ValidModelRecord(record))
	{
		handle.generation = s_generation;
		handle.revision = s_models[record].revision;
		handle.index = record;
	}

	return handle;
}

bool AssetCatalog_HandleValid(AssetCatalogHandle handle)
{
	if (!s_valid || handle.index < 0 || handle.index >= s_modelCount)
		return false;

	if (!s_models[handle.index].used)
		return false;

	return handle.generation == s_generation && handle.revision == s_models[handle.index].revision;
}

bool AssetCatalog_MakeModelId(int modelIndex, char* out, int capacity)
{
	if (!s_valid || out == NULL || capacity <= 0 || modelIndex < 0)
		return false;

	return snprintf(out, capacity, "model:%d:%d:%d", s_context.level, s_context.variant, modelIndex) > 0;
}

int AssetCatalog_RegisterTexture(const char* name, int texturePage, int textureIndex,
	int textureSet, AssetCatalogSource source)
{
	if (!s_valid || texturePage < 0 || textureIndex < 0)
		return -1;

	const char* textureName = name ? name : "";

	for (int i = 0; i < s_textureCount; i++)
	{
		if (s_textures[i].used &&
			s_textures[i].texturePage == texturePage &&
			s_textures[i].textureIndex == textureIndex &&
			strcmp(s_textures[i].name, textureName) == 0)
		{
			s_textures[i].textureSet = textureSet;
			return i;
		}
	}

	if (s_textureCount >= kMaxTextures)
		return -1;

	TextureRecord* record = &s_textures[s_textureCount];
	record->used = true;
	record->revision = 1;
	record->texturePage = texturePage;
	record->textureIndex = textureIndex;
	record->textureSet = textureSet;
	record->source = source;
	CopyName(record->name, textureName);

	return s_textureCount++;
}

int AssetCatalog_FindTexture(const char* name, int texturePage, int textureIndex)
{
	const char* textureName = name ? name : "";

	for (int i = 0; i < s_textureCount; i++)
	{
		if (s_textures[i].used &&
			s_textures[i].texturePage == texturePage &&
			s_textures[i].textureIndex == textureIndex &&
			strcmp(s_textures[i].name, textureName) == 0)
		{
			return i;
		}
	}

	return -1;
}

bool AssetCatalog_GetTexture(int record, char* name, int nameCapacity,
	int* texturePage, int* textureIndex, int* textureSet, AssetCatalogSource* source)
{
	if (!ValidTextureRecord(record))
		return false;

	if (name) snprintf(name, nameCapacity, "%s", s_textures[record].name);
	if (texturePage) *texturePage = s_textures[record].texturePage;
	if (textureIndex) *textureIndex = s_textures[record].textureIndex;
	if (textureSet) *textureSet = s_textures[record].textureSet;
	if (source) *source = s_textures[record].source;

	return true;
}

bool AssetCatalog_MakeTextureId(const char* name, int texturePage, int textureIndex,
	char* out, int capacity)
{
	if (out == NULL || capacity <= 0)
		return false;

	return snprintf(out, capacity, "tex:%s:%d:%d", name ? name : "", texturePage, textureIndex) > 0;
}

bool AssetCatalog_MakeComponentKey(const char* parentKey, const char* kind, int index,
	char* out, int capacity)
{
	if (out == NULL || capacity <= 0 || parentKey == NULL || parentKey[0] == '\0' ||
		kind == NULL || kind[0] == '\0' || index < 0)
		return false;

	// A parent must be an instance (or resource) key, not another component, and
	// the kind may not contain the separators, so ParseComponentKey is exact.
	if (strstr(parentKey, kComponentMarker) != NULL)
		return false;

	for (const char* character = kind; *character; ++character)
	{
		if (*character == '/' || *character == ':')
			return false;
	}

	return snprintf(out, capacity, "%s%s%s:%d", parentKey, kComponentMarker, kind, index) > 0;
}

bool AssetCatalog_IsComponentKey(const char* key)
{
	return key != NULL && strstr(key, kComponentMarker) != NULL;
}

bool AssetCatalog_ParseComponentKey(const char* key, char* parentOut, int parentCapacity,
	char* kindOut, int kindCapacity, int* index)
{
	if (key == NULL)
		return false;

	const char* marker = strstr(key, kComponentMarker);
	if (marker == NULL || marker == key)
		return false;

	const char* kind = marker + sizeof(kComponentMarker) - 1;
	const char* separator = strrchr(kind, ':');
	if (separator == NULL || separator == kind)
		return false;

	const size_t parentLength = (size_t)(marker - key);
	const size_t kindLength = (size_t)(separator - kind);

	char* end = NULL;
	const long value = strtol(separator + 1, &end, 10);
	if (end == separator + 1 || *end != '\0' || value < 0 || value > 0x7fffffff)
		return false;

	if (parentOut != NULL && parentCapacity > 0)
	{
		if (parentLength + 1 > (size_t)parentCapacity)
			return false;
		memcpy(parentOut, key, parentLength);
		parentOut[parentLength] = '\0';
	}

	if (kindOut != NULL && kindCapacity > 0)
	{
		if (kindLength + 1 > (size_t)kindCapacity)
			return false;
		memcpy(kindOut, kind, kindLength);
		kindOut[kindLength] = '\0';
	}

	if (index != NULL)
		*index = (int)value;

	return true;
}

bool AssetCatalog_CaptureAnchor(const char* key, AssetCatalogAnchor* anchor)
{
	if (anchor == NULL)
		return false;

	memset(anchor, 0, sizeof(*anchor));
	if (key == NULL || key[0] == '\0')
		return false;

	snprintf(anchor->key, sizeof(anchor->key), "%s", key);
	anchor->generation = s_generation;
	anchor->valid = true;

	// A model id embedded in a building/tile/anim or model-resource key is
	// "model:<level>:<variant>:<index>". A component key anchors through the
	// parent that carries it, so the same search covers both.
	const char* marker = strstr(key, "model:");
	if (marker != NULL && s_valid)
	{
		int level = -1, variant = -1, index = -1;
		if (sscanf(marker, "model:%d:%d:%d", &level, &variant, &index) == 3 && index >= 0 &&
			level == s_context.level && variant == s_context.variant)
		{
			const int record = AssetCatalog_FindModel(index);
			if (record >= 0)
			{
				anchor->hasModel = true;
				anchor->model = AssetCatalog_GetModelHandle(record);
			}
		}
	}

	return true;
}

AssetCatalogSelectionState AssetCatalog_CheckAnchor(const AssetCatalogAnchor* anchor)
{
	if (anchor == NULL || !anchor->valid)
		return ASSET_CATALOG_SELECTION_NONE;

	if (!s_valid || anchor->generation != s_generation)
		return ASSET_CATALOG_SELECTION_STALE;

	if (anchor->hasModel)
		return AssetCatalog_HandleValid(anchor->model) ? ASSET_CATALOG_SELECTION_VALID : ASSET_CATALOG_SELECTION_STALE;

	return ASSET_CATALOG_SELECTION_UNKNOWN;
}

bool AssetCatalog_AddModelTexture(int modelRecord, int textureRecord)
{
	if (!ValidModelRecord(modelRecord) || !ValidTextureRecord(textureRecord))
		return false;

	for (int i = 0; i < s_materialRefCount; i++)
	{
		if (s_materialRefs[i].modelRecord == modelRecord &&
			s_materialRefs[i].textureRecord == textureRecord)
		{
			return false;
		}
	}

	if (s_materialRefCount >= kMaxMaterialRefs)
		return false;

	s_materialRefs[s_materialRefCount].modelRecord = modelRecord;
	s_materialRefs[s_materialRefCount].textureRecord = textureRecord;
	s_materialRefCount++;

	return true;
}

int AssetCatalog_EnumerateModelTextures(int modelRecord, int* outTextureRecords, int capacity)
{
	if (!ValidModelRecord(modelRecord))
		return 0;

	int count = 0;

	for (int i = 0; i < s_materialRefCount; i++)
	{
		if (s_materialRefs[i].modelRecord != modelRecord)
			continue;

		if (outTextureRecords && count < capacity)
			outTextureRecords[count] = s_materialRefs[i].textureRecord;

		count++;
	}

	return count;
}

int AssetCatalog_CollectExportModels(int modelRecord, int* outModelRecords, int capacity)
{
	if (!ValidModelRecord(modelRecord))
		return 0;

	int count = 0;

	if (outModelRecords && count < capacity)
		outModelRecords[count] = modelRecord;
	count++;

	// The loader only links a low-detail model to its high-detail sibling, so
	// the reverse (lower-detail) relationship is not available to include.
	const int highDetailModelIndex = s_models[modelRecord].highDetailModelIndex;
	if (highDetailModelIndex >= 0)
	{
		const int sibling = AssetCatalog_FindModel(highDetailModelIndex);
		if (sibling >= 0 && sibling != modelRecord)
		{
			if (outModelRecords && count < capacity)
				outModelRecords[count] = sibling;
			count++;
		}
	}

	return count;
}

int AssetCatalog_CountTextureModels(int textureRecord)
{
	if (!ValidTextureRecord(textureRecord))
		return 0;

	int count = 0;

	for (int i = 0; i < s_materialRefCount; i++)
	{
		if (s_materialRefs[i].textureRecord == textureRecord)
			count++;
	}

	return count;
}

int AssetCatalog_EnumerateTextureModels(int textureRecord, int* outModelRecords, int capacity)
{
	if (!ValidTextureRecord(textureRecord))
		return 0;

	int count = 0;

	for (int i = 0; i < s_materialRefCount; i++)
	{
		if (s_materialRefs[i].textureRecord != textureRecord)
			continue;

		if (outModelRecords && count < capacity)
			outModelRecords[count] = s_materialRefs[i].modelRecord;

		count++;
	}

	return count;
}

int AssetCatalog_RegisterCar(int slot, int modelNumber, const char* name,
	AssetCatalogSource source)
{
	if (!s_valid || slot < 0)
		return -1;

	for (int i = 0; i < s_carCount; i++)
	{
		if (s_cars[i].used && s_cars[i].slot == slot)
		{
			s_cars[i].revision++;
			s_cars[i].modelNumber = modelNumber;
			s_cars[i].source = source;
			CopyName(s_cars[i].name, name);
			return i;
		}
	}

	if (s_carCount >= kMaxCars)
		return -1;

	CarRecord* record = &s_cars[s_carCount];
	record->used = true;
	record->revision = 1;
	record->slot = slot;
	record->modelNumber = modelNumber;
	record->modelRecord = -1;
	record->source = source;
	CopyName(record->name, name);

	return s_carCount++;
}

int AssetCatalog_FindCar(int slot)
{
	for (int i = 0; i < s_carCount; i++)
	{
		if (s_cars[i].used && s_cars[i].slot == slot)
			return i;
	}
	return -1;
}

bool AssetCatalog_GetCar(int record, int* slot, int* modelNumber, char* name, int nameCapacity,
	AssetCatalogSource* source)
{
	if (!ValidCarRecord(record))
		return false;

	if (slot) *slot = s_cars[record].slot;
	if (modelNumber) *modelNumber = s_cars[record].modelNumber;
	if (name) snprintf(name, nameCapacity, "%s", s_cars[record].name);
	if (source) *source = s_cars[record].source;

	return true;
}

bool AssetCatalog_LinkCarToModel(int carRecord, int modelRecord)
{
	if (!ValidCarRecord(carRecord) || !ValidModelRecord(modelRecord))
		return false;

	s_cars[carRecord].modelRecord = modelRecord;
	return true;
}

bool AssetCatalog_AddCarPalette(int carRecord, int paletteIndex)
{
	if (!ValidCarRecord(carRecord) || paletteIndex < 0)
		return false;

	for (int i = 0; i < s_carPaletteCount; i++)
	{
		if (s_carPalettes[i].carRecord == carRecord && s_carPalettes[i].paletteIndex == paletteIndex)
			return false;
	}

	if (s_carPaletteCount >= kMaxCarPalettes)
		return false;

	s_carPalettes[s_carPaletteCount].carRecord = carRecord;
	s_carPalettes[s_carPaletteCount].paletteIndex = paletteIndex;
	s_carPaletteCount++;

	return true;
}

int AssetCatalog_EnumerateCarPalettes(int carRecord, int* out, int capacity)
{
	if (!ValidCarRecord(carRecord))
		return 0;

	int count = 0;

	for (int i = 0; i < s_carPaletteCount; i++)
	{
		if (s_carPalettes[i].carRecord != carRecord)
			continue;

		if (out && count < capacity)
			out[count] = s_carPalettes[i].paletteIndex;

		count++;
	}

	return count;
}

void AssetCatalog_InventoryDiagnostic(AssetCatalogInventoryResult* out, bool warning,
	const char* reason, int region, int modelNumber)
{
	if (!out) return;
	if (!warning) out->ok = false;
	AddInventoryFinding(out, warning, reason, region, modelNumber, 0, 0, 0, 0);
}

bool AssetCatalog_WalkInventoryLists(const unsigned short* pointers, int pointerCount,
	const unsigned short* entries, int entryCount, int cellBegin, int cellEnd,
	int straddlers, int objectBegin, int objectEnd, int barrel,
	AssetCatalogInventoryVisit visit, void* user, AssetCatalogInventoryResult* diagnostics)
{
	if (!diagnostics) return false;
	if (!pointers || !entries || !visit || pointerCount < 0 || cellBegin < 0 ||
		cellEnd < cellBegin || cellEnd > entryCount || straddlers < 0 ||
		objectBegin < straddlers || objectEnd < objectBegin || objectEnd > ASSET_CATALOG_MAX_PLACEMENTS)
	{
		AssetCatalog_InventoryDiagnostic(diagnostics, false, "invalid traversal spans", barrel, -1);
		return false;
	}
	const int before = diagnostics->errorCount;
	for (int cell = 0; cell < pointerCount; ++cell)
	{
		int entry = pointers[cell];
		if (entry == 0xffff) continue;
		if (entry < cellBegin || entry >= cellEnd)
		{
			AssetCatalog_InventoryDiagnostic(diagnostics, false, "cell pointer outside barrel span", barrel, cell);
			continue;
		}
		bool terminated = false;
		int selector = -1;
		while (entry < cellEnd)
		{
			const unsigned short num = entries[entry++];
			// The end bit belongs to the final entry, not a separate sentinel:
			// GetFirst/GetNextPackedCop visit that object before stopping.
			terminated = (num & 0x8000) != 0;
			if (num & 0x4000)
			{
				selector = num & 0x3fff;
				if (terminated) break;
				continue;
			}
			const int object = num & 0x3fff;
			if (object >= straddlers && (object < objectBegin || object >= objectEnd))
			{
				AssetCatalog_InventoryDiagnostic(diagnostics, false, "object reference outside owning barrel", barrel, object);
				if (terminated) break;
				continue;
			}
			visit(user, object, cell, selector);
			if (terminated) break;
		}
		if (!terminated)
			AssetCatalog_InventoryDiagnostic(diagnostics, false, "cell list missing terminator", barrel, cell);
	}
	return diagnostics->errorCount == before;
}

int AssetCatalog_ReconcileInventory(const AssetCatalogPlacement* placements, int placementCount,
	const int* expectedPerRegionPlacements, AssetCatalogInventoryResult* out)
{
	if (out == NULL)
		return ASSET_CATALOG_INVENTORY_ERROR_ARGUMENT;

	memset(out, 0, sizeof(*out));

	if (!s_valid)
		return ASSET_CATALOG_INVENTORY_ERROR_NO_CATALOG;

	if (placements == NULL || placementCount < 0)
		return ASSET_CATALOG_INVENTORY_ERROR_ARGUMENT;

	if (placementCount > ASSET_CATALOG_MAX_PLACEMENTS)
		return ASSET_CATALOG_INVENTORY_ERROR_CAPACITY;

	if (expectedPerRegionPlacements != NULL)
	{
		for (int region = 0; region < 4; region++)
		{
			if (expectedPerRegionPlacements[region] < 0)
				return ASSET_CATALOG_INVENTORY_ERROR_ARGUMENT;
		}
	}

	out->placements = placementCount;

	for (int i = 0; i < s_modelCount; i++)
	{
		if (s_models[i].used)
			out->registeredModels++;
	}

	for (int i = 0; i < s_textureCount; i++)
	{
		if (s_textures[i].used)
			out->registeredTextures++;
	}

	// Live records touched by the placements, and which of them carry a
	// material link. Sized by the fixed catalog capacities in this file.
	bool referencedModels[kMaxModels] = {};
	bool modelHasLink[kMaxModels] = {};
	bool referencedTextures[kMaxTextures] = {};
	int referencedModelCount = 0;
	int invalidRegionCount = 0;

	for (int i = 0; i < placementCount; i++)
	{
		const AssetCatalogPlacement& placement = placements[i];

		if (placement.region >= 0 && placement.region < 4)
		{
			out->perRegionPlacements[placement.region]++;
		}
		else if (placement.region == -1)
		{
			out->sharedPlacements++;
		}
		else
		{
			invalidRegionCount++;
			AddInventoryFinding(out, false, "invalid region", placement.region, placement.modelNumber,
				placement.x, placement.y, placement.z, placement.rotation);
		}

		const int record = AssetCatalog_FindModel(placement.modelNumber);

		if (record < 0)
		{
			out->missingModelRefs++;
			AddInventoryFinding(out, false, "missing model", placement.region, placement.modelNumber,
				placement.x, placement.y, placement.z, placement.rotation);
		}
		else if (!referencedModels[record])
		{
			referencedModels[record] = true;
			referencedModelCount++;
		}
	}

	out->referencedModels = referencedModelCount;
	out->unreferencedModels = out->registeredModels - referencedModelCount;

	// A material reference only counts toward the report when its model is
	// referenced by a placement; the rest are library records.
	for (int i = 0; i < s_materialRefCount; i++)
	{
		const MaterialRef& ref = s_materialRefs[i];

		if (ref.modelRecord < 0 || ref.modelRecord >= s_modelCount || !referencedModels[ref.modelRecord])
			continue;

		modelHasLink[ref.modelRecord] = true;
		if (ref.textureRecord >= 0 && ref.textureRecord < s_textureCount && s_textures[ref.textureRecord].used)
			referencedTextures[ref.textureRecord] = true;
	}

	for (int i = 0; i < s_textureCount; i++)
	{
		if (referencedTextures[i])
			out->referencedTextures++;
	}

	for (int i = 0; i < s_modelCount; i++)
	{
		if (referencedModels[i] && !modelHasLink[i])
			out->missingTextureLinks++;
	}

	// Duplicate placement identities. Sorting the index array groups equal
	// identities, so each extra occurrence is one duplicate count and finding.
	if (placementCount > 1)
	{
		for (int i = 0; i < placementCount; i++)
			s_placementOrder[i] = i;

		s_sortPlacements = placements;
		qsort(s_placementOrder, (size_t)placementCount, sizeof(int), ComparePlacementIndices);
		s_sortPlacements = NULL;

		for (int i = 1; i < placementCount; i++)
		{
			const AssetCatalogPlacement& duplicate = placements[s_placementOrder[i]];
			const AssetCatalogPlacement& previous = placements[s_placementOrder[i - 1]];

			if (!SamePlacement(duplicate, previous))
				continue;

			out->duplicatePlacements++;
			AddInventoryFinding(out, false, "duplicate placement", duplicate.region, duplicate.modelNumber,
				duplicate.x, duplicate.y, duplicate.z, duplicate.rotation);
		}
	}

	out->uniquePlacements = placementCount - out->duplicatePlacements;

	bool expectedMismatch = false;

	if (expectedPerRegionPlacements != NULL)
	{
		for (int region = 0; region < 4; region++)
		{
			if (out->perRegionPlacements[region] == expectedPerRegionPlacements[region])
				continue;

			expectedMismatch = true;
			char reason[ASSET_CATALOG_INVENTORY_REASON_CAPACITY];
			snprintf(reason, sizeof(reason), "region %d expected %d got %d", region,
				expectedPerRegionPlacements[region], out->perRegionPlacements[region]);
			AddInventoryFinding(out, false, reason, region, -1, 0, 0, 0, 0);
		}
	}

	// Warnings last: they explain legitimate states and never flip ok. Both
	// models of unknown texture classification and registered models no
	// placement uses are informational.
	for (int i = 0; i < s_modelCount; i++)
	{
		if (referencedModels[i] && !modelHasLink[i])
			AddInventoryFinding(out, true, "no catalog material link; texture classification unknown", -1, s_models[i].modelIndex, 0, 0, 0, 0);
	}

	for (int i = 0; i < s_modelCount; i++)
	{
		if (s_models[i].used && !referencedModels[i])
			AddInventoryFinding(out, true, "unreferenced model", -1, s_models[i].modelIndex, 0, 0, 0, 0);
	}

	out->ok = out->missingModelRefs == 0 && out->duplicatePlacements == 0 &&
		invalidRegionCount == 0 && !expectedMismatch;

	return ASSET_CATALOG_INVENTORY_OK;
}

void AssetCatalog_GetStats(int* models, int* textures, int* cars, int* materialRefs)
{
	if (models) *models = s_modelCount;
	if (textures) *textures = s_textureCount;
	if (cars) *cars = s_carCount;
	if (materialRefs) *materialRefs = s_materialRefCount;
}

void AssetCatalog_GetCapacities(int* models, int* textures, int* cars, int* materialRefs)
{
	if (models) *models = kMaxModels;
	if (textures) *textures = kMaxTextures;
	if (cars) *cars = kMaxCars;
	if (materialRefs) *materialRefs = kMaxMaterialRefs;
}

int AssetCatalog_GetCapacityBytes(void)
{
	return (int)(sizeof(s_models) + sizeof(s_textures) + sizeof(s_cars) +
		sizeof(s_materialRefs) + sizeof(s_carPalettes) + sizeof(s_placementOrder));
}

#endif /* !PSX */
