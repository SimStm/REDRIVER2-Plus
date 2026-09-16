#ifndef ASSET_CATALOG_GAME_H
#define ASSET_CATALOG_GAME_H

/*
 * Bridges the game's level loaders to the source-aware asset catalog
 * (roadmap item 04, milestone 3). The catalog core stays format-agnostic;
 * everything that knows about Driver 2 globals lives here.
 *
 * No-ops on the PSX build, where the catalog is compiled out.
 */

/* Enters the level/variant context before any texture or model is registered. */
void AssetCatalogGame_BeginLevel(void);

/* Registers the loaded level models, car resident slots and their material
   relationships, and logs the resulting counts. */
void AssetCatalogGame_PopulateLevel(void);

/* Registers (or re-registers) one runtime model slot from the current
   `modelpointers` state, including its name, LOD link and material links. */
void AssetCatalogGame_RegisterModel(int modelIndex);

/* Invalidates one runtime model slot (a streamed slot was freed). */
void AssetCatalogGame_InvalidateModel(int modelIndex);

#endif /* ASSET_CATALOG_GAME_H */
