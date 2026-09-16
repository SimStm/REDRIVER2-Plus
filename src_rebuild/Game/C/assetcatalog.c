#include "assetcatalog.h"

#ifndef PSX

#include <stdio.h>
#include <string.h>

/*
 * The desktop inspector diagnostic consumes the catalog. Keep it out of the
 * PSX build's static footprint; no PSX path calls these functions yet.
 */

namespace
{
// Cover every addressable model/texture slot a streaming level can touch, not
// just the resident set: a model index is registered once and reused.
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

void CopyName(char* destination, const char* source)
{
	if (source == NULL)
		source = "";
	snprintf(destination, ASSET_CATALOG_NAME_CAPACITY, "%s", source);
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
	if (!s_valid || modelIndex < 0)
		return -1;

	// Match any record for this slot, live or previously freed, so a streamed
	// slot reuses its record and both registration and invalidation bump the
	// revision that retained handles depend on.
	for (int i = 0; i < s_modelCount; i++)
	{
		if (s_models[i].modelIndex == modelIndex)
		{
			s_models[i].used = true;
			s_models[i].revision++;
			s_models[i].lodParentModelIndex = lodParentModelIndex;
			s_models[i].highDetailModelIndex = highDetailModelIndex;
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
	record->source = source;
	CopyName(record->name, name);

	return s_modelCount++;
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
		sizeof(s_materialRefs) + sizeof(s_carPalettes));
}

#endif /* !PSX */
