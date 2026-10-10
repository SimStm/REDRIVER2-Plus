#ifndef ASSET_CATALOG_GAME_H
#define ASSET_CATALOG_GAME_H

/*
 * Bridges the game's level loaders to the source-aware asset catalog
 * (roadmap item 04, milestone 3). The catalog core stays format-agnostic;
 * everything that knows about Driver 2 globals lives here.
 *
 * No-ops on the PSX build, where the catalog is compiled out.
 */

#include <stdbool.h>

/* Enter the level/variant context before any texture or model is registered. */
void AssetCatalogGame_BeginLevel(void);

/* Registers the loaded level models, car resident slots and their material
   relationships, and logs the resulting counts. */
void AssetCatalogGame_PopulateLevel(void);

/* Registers (or re-registers) one runtime model slot from the current
   `modelpointers` state, including its name, LOD link and material links. */
void AssetCatalogGame_RegisterModel(int modelIndex);

/* Invalidates one runtime model slot (a streamed slot was freed). */
void AssetCatalogGame_InvalidateModel(int modelIndex);

/* --- E02 level inventory -------------------------------------------------- */

/* Parses the developer `-assetinventory` flag from argv. No-op when absent. */
void AssetCatalogGame_ConfigureFromCommandLine(int argc, char** argv);

/* True when the loaded level's inventory should be reconciled once after the
   world regions are resident. */
bool AssetCatalogGame_InventoryRequested(void);

/* --- E03 block 2a model polygon-type probe -------------------------------- */

/* True when the developer `-modelptypes` flag requested the polygon-type
   report. The loader feeds the probe only while this is true. */
bool AssetCatalogGame_ModelPtypesRequested(void);

/* Records one model's byte span for the polygon-type probe. Reads the live
   MODEL header (poly_block, num_polys, shape_flags, flags2) before relocation,
   so it must be called at the loader's size-live site. No-op when the flag is
   absent. `modelBegin` is the MODEL base and `spanBytes` its byte size. */
void AssetCatalogGame_RecordModelSpan(int modelIndex, const unsigned char* modelBegin, int spanBytes);

/* Writes the accumulated polygon-type report to
   `<working dir>/model_ptypes_level_<GameLevel>.txt` with the atomic
   InspectorExport writer. Per-slot: model index, span bytes, shape_flags/flags2
   and the dispatch decision inputs, declared ptype counts, first zero-stride
   type+offset and occurrences outside the decoder allowlist {10,11,20,21,22,23};
   then aggregate totals. Never decides a draw-path class; it records the inputs.
   Returns true when the report was written; `status` always receives a short
   result. Windows-only (the atomic publisher is Win32). */
bool AssetCatalogGame_WriteModelPtypeReport(char* status, int statusCapacity);

/* Decodes the resident level regions' cell object lists into placements,
   reconciles them against the live asset catalog and publishes a readable
   report to `<working dir>/assetcatalog_level_<GameLevel>_inventory.txt` with
   the atomic InspectorExport writer.

   Each object is decoded once with the nearCell of the cell that references
   it; region-straddling objects are reported as shared placements. This is a
   resident union of default/event lists, not source duplication acceptance:
   geometric overlaps may belong to mutually exclusive variants. No catalog
   material link leaves source texture classification unknown. Traversal
   errors make the report NOT OK; skipped nonresident barrels are intentional
   partial residency, and no eligible barrel returns a no-data diagnostic. Returns
   true when the report was written; `status` always receives a short result
   ("inventory report: N placements, M findings -> <path>"). The body is
   Windows-only because the atomic publisher is Win32; other platforms return
   false with an explanatory status. */
bool AssetCatalogGame_ReconcileLevel(char* status, int statusCapacity);

#endif /* ASSET_CATALOG_GAME_H */
