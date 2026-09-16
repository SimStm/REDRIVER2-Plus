#ifndef ASSET_CATALOG_H
#define ASSET_CATALOG_H

/*
 * Source-aware asset catalog (roadmap item 04).
 *
 * Connects a source asset, model, LOD, material, texture, palette and runtime
 * instance without coupling PsyCross to Driver 2 formats. The catalog is
 * game-owned: it stores identity, provenance and relationships only, and never
 * changes how any loader, manifest or renderer works.
 *
 * It is deliberately dependency-free and allocation-free so the same code can
 * be unit-tested standalone and reused by the inspector diagnostics.
 */

#include <stdbool.h>

#define ASSET_CATALOG_SCHEMA_VERSION 1

#define ASSET_CATALOG_NAME_CAPACITY 64
#define ASSET_CATALOG_ID_CAPACITY 128

/* Source identifier provenance. A record never invents an archive name from a
   display label; unknown is an explicit state. */
typedef enum AssetCatalogSource
{
	ASSET_CATALOG_SOURCE_UNKNOWN = 0,
	ASSET_CATALOG_SOURCE_DECLARED,	/* name came from the level's name table */
	ASSET_CATALOG_SOURCE_VERIFIED	/* name matched a known definition */
} AssetCatalogSource;

/* Context a record was captured in. Two cities/variants can share a texture
   name, so context is part of a resource's identity but not of its id string. */
typedef struct AssetCatalogContext
{
	int level;			/* GameLevel / city index */
	int variant;		/* city type: day, night, multiplayer day/night */
	int multiplayer;	/* gMultiplayerLevels */
} AssetCatalogContext;

/* A retained reference. It is valid only while the generation and revision
   still match, so reusing a slot cannot silently redirect the holder. */
typedef struct AssetCatalogHandle
{
	unsigned int generation;
	unsigned int revision;
	int index;
} AssetCatalogHandle;

/* --- lifetime ------------------------------------------------------------- */

/* Clears every record and starts a new generation. Use on level unload. */
void AssetCatalog_Reset(void);

/* Enters a level/variant context. Calling it again with the same context keeps
   records; a different context resets the catalog. */
void AssetCatalog_BeginContext(int level, int variant, int multiplayer);

bool AssetCatalog_IsValid(void);
unsigned int AssetCatalog_GetGeneration(void);
bool AssetCatalog_GetContext(AssetCatalogContext* context);

/* --- models --------------------------------------------------------------- */

/* Registers or replaces the model occupying a runtime slot. Replacing a slot
   bumps the record revision so old handles become invalid. lodParent and
   highDetail are model indices (or -1). Returns a record index or -1. */
int AssetCatalog_RegisterModel(int modelIndex, const char* name,
	AssetCatalogSource source, int lodParentModelIndex, int highDetailModelIndex);

int AssetCatalog_FindModel(int modelIndex);

/* Marks a model slot's record as no longer live (a streamed slot was freed).
   Retained handles become invalid; a later re-registration of the same slot
   reuses the record and bumps its revision again. */
bool AssetCatalog_InvalidateModel(int modelIndex);

bool AssetCatalog_GetModel(int record, int* modelIndex, char* name, int nameCapacity,
	AssetCatalogSource* source, int* lodParentModelIndex, int* highDetailModelIndex);

AssetCatalogHandle AssetCatalog_GetModelHandle(int record);
bool AssetCatalog_HandleValid(AssetCatalogHandle handle);

/* Stable, level/variant-aware id: "model:<level>:<variant>:<index>". */
bool AssetCatalog_MakeModelId(int modelIndex, char* out, int capacity);

/* --- textures ------------------------------------------------------------- */

/* Registers or returns the existing texture for the manifest identity triple
   (name, texturePage, textureIndex). The triple is exactly what the mod
   manifest matches on, so shared textures are one record, not several. */
int AssetCatalog_RegisterTexture(const char* name, int texturePage, int textureIndex,
	int textureSet, AssetCatalogSource source);

int AssetCatalog_FindTexture(const char* name, int texturePage, int textureIndex);

bool AssetCatalog_GetTexture(int record, char* name, int nameCapacity,
	int* texturePage, int* textureIndex, int* textureSet, AssetCatalogSource* source);

/* Stable id: "tex:<name>:<page>:<index>". */
bool AssetCatalog_MakeTextureId(const char* name, int texturePage, int textureIndex,
	char* out, int capacity);

/* --- relationships -------------------------------------------------------- */

/* Many-to-many model <-> texture. Adds the pair once; returns false on a
   duplicate, an invalid record, or full storage. */
bool AssetCatalog_AddModelTexture(int modelRecord, int textureRecord);

/* Fills out[0..capacity) with the distinct texture records of a model and
   returns the count, so hidden/extra materials can be enumerated. */
int AssetCatalog_EnumerateModelTextures(int modelRecord, int* outTextureRecords, int capacity);

/* Fills out[0..capacity) with the model records a source export should cover:
   the model itself plus its high-detail LOD sibling when the loader linked one.
   Returns the true count, which may exceed capacity. Lower-detail siblings are
   not linked in the catalog and are therefore never included. */
int AssetCatalog_CollectExportModels(int modelRecord, int* outModelRecords, int capacity);

/* Number of models that reference a texture record. */
int AssetCatalog_CountTextureModels(int textureRecord);

/* Fills out[0..capacity) with the distinct model records that reference a
   texture record and returns the true count, which may exceed capacity. */
int AssetCatalog_EnumerateTextureModels(int textureRecord, int* outModelRecords, int capacity);

/* --- cars and palettes ---------------------------------------------------- */

int AssetCatalog_RegisterCar(int slot, int modelNumber, const char* name,
	AssetCatalogSource source);

int AssetCatalog_FindCar(int slot);

bool AssetCatalog_GetCar(int record, int* slot, int* modelNumber, char* name, int nameCapacity,
	AssetCatalogSource* source);

/* Car -> model relationship (the source model record). */
bool AssetCatalog_LinkCarToModel(int carRecord, int modelRecord);

bool AssetCatalog_AddCarPalette(int carRecord, int paletteIndex);
int AssetCatalog_EnumerateCarPalettes(int carRecord, int* out, int capacity);

/* --- diagnostics ---------------------------------------------------------- */

void AssetCatalog_GetStats(int* models, int* textures, int* cars, int* materialRefs);

/* Fixed capacities and the static footprint, for truthful completeness and
   overhead reporting. The catalog never grows at runtime. */
void AssetCatalog_GetCapacities(int* models, int* textures, int* cars, int* materialRefs);
int AssetCatalog_GetCapacityBytes(void);

#endif /* ASSET_CATALOG_H */
