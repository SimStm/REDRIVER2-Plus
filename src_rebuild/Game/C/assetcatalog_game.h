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

#include "conversioncache.h"

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

/* --- E03 block 2 item 3 one-model GLB conversion -------------------------- */

/* True when the developer `-convertmodel <slot>` flag requested a conversion. */
bool AssetCatalogGame_ConvertModelRequested(void);

/* The model slot given to `-convertmodel`, or -1 when absent/unparsed. */
int AssetCatalogGame_ConvertModelSlot(void);

/* Converts ONE resident world model slot to a cached native GLB: resolves the
   slot's catalog record and captured byte span (AssetCatalog_GetModelSpan),
   decodes the span with the building policy (SkipUnsupported), builds a single
   project-owned GltfMeshData (WorldMeshBuilder) and publishes it through
   AssetConverter under the "world-mesh" converter key.

   `status` always receives a short result. On success it reports the slot, span
   bytes, submesh count, vertex/triangle counts, skipped record count, source
   hash, published content hash and output path. On any failure it reports a
   named diagnostic (invalid/not-resident slot, missing span, decode/build/
   publish error) and never interrupts the game. Windows-only like the other
   developer reports; other platforms return false with an explanatory status. */
bool AssetCatalogGame_ConvertModel(char* status, int statusCapacity);

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

/* --- E03 block 2 item 4 native per-instance draw source ------------------- */

/* Bounded slot->resource registry and bounded placement collector. The
   registry is populated by a successful one-model conversion and read by the
   native-draw bridge; the collector reuses the level inventory walker without
   the report path and never re-decodes a model. */
#define ASSET_CATALOG_GAME_MAX_MODEL_RESOURCES 64
#define ASSET_CATALOG_GAME_MAX_DRAW_PLACEMENTS 64

/* One published model resource. `resourceKey` is the canonical
   ConversionCacheKey text used for the publish, `sourceHash` the source digest
   the key was built from, and `glbPath`/`contentHash` the published artifact. */
typedef struct AssetCatalogGameModelResource
{
	int modelSlot;
	char resourceKey[CONVERSION_CACHE_KEY_CAPACITY];
	char glbPath[CONVERSION_CACHE_PATH_CAPACITY];
	char contentHash[CONVERSION_CACHE_HASH_CAPACITY];
	unsigned long long sourceHash;
} AssetCatalogGameModelResource;

/* One bounded resident placement of the native-draw target model slot. The
   model handle {modelSlot, modelRevision} is carried for staleness diagnostics;
   the renderer only ever sees the resource key. */
typedef struct AssetCatalogGameDrawPlacement
{
	int x;
	int y;
	int z;
	int rotation;			/* packed yang */
	int sourceObjectIndex;	/* 14-bit cell object index, provenance only */
	int modelSlot;
	unsigned int modelRevision;
} AssetCatalogGameDrawPlacement;

/* True when the developer `-drawmodel <slot> [count]` flag requested a bounded
   native per-instance draw. */
bool AssetCatalogGame_NativeDrawRequested(void);

/* The model slot given to `-drawmodel`, or -1 when absent/unparsed. */
int AssetCatalogGame_NativeDrawSlot(void);

/* The requested instance bound given to `-drawmodel`, or the default when the
   optional count was omitted. */
int AssetCatalogGame_NativeDrawCount(void);

/* Converts the `-drawmodel` slot through the existing one-model conversion path
   (which registers its published resource), then collects up to the requested
   bound of that slot's resident placements. Warn-only: a failure leaves a named
   diagnostic in `status` and never interrupts the game. */
bool AssetCatalogGame_PrepareNativeDraw(char* status, int statusCapacity);

/* Collects up to `maxPlacements` resident placements of `modelSlot` into the
   bounded store read by AssetCatalogGame_GetDrawPlacements. It reuses the level
   inventory walker without the report path and never re-decodes a model, and is
   a no-op (0) unless the native-draw flag is set. A named diagnostic is written
   to `status`. */
int AssetCatalogGame_CollectDrawPlacements(int modelSlot, int maxPlacements,
	char* status, int statusCapacity);

/* Looks up the published resource of `modelSlot`. Returns true and fills the
   requested outputs when the slot has a published resource; any output pointer
   may be NULL. `sourceHash` may be NULL. */
bool AssetCatalogGame_GetModelResource(int modelSlot,
	char* resourceKey, int resourceKeyCapacity,
	char* glbPath, int glbPathCapacity,
	char* contentHash, int contentHashCapacity,
	unsigned long long* sourceHash);

/* Copies up to `capacity` of the collected placements. Returns the number
   copied (0 when none were collected or the native-draw flag is off). */
int AssetCatalogGame_GetDrawPlacements(AssetCatalogGameDrawPlacement* out, int capacity);

/* --- E03 region draw (full converted resident region) --------------------- */

/* Bounded full-region placement store. It is sized to the native renderer's
   instance bound (NATIVE_MAX_INSTANCES); the bridge asserts the relation so a
   change to either limit fails loudly instead of truncating silently. Reaching
   the bound is a named truncation diagnostic, never an overflow. */
#define ASSET_CATALOG_GAME_MAX_REGION_PLACEMENTS 4096

/* True when the developer `-drawregion` flag requested the full resident
   region's native draw. The flag takes no value. */
bool AssetCatalogGame_RegionDrawRequested(void);

/* Converts every distinct model slot referenced by the current level's resident
   placements (bounded by ASSET_CATALOG_GAME_MAX_MODEL_RESOURCES) through the
   existing one-model conversion path, which registers each published resource,
   then collects the full placement list (bounded by
   ASSET_CATALOG_GAME_MAX_REGION_PLACEMENTS). A slot whose conversion fails is a
   named warning and is skipped while the rest continue; a placement or resource
   capacity overflow is a named truncation diagnostic. Reuses the level
   inventory walker without the report path and never re-decodes a source model
   at draw time. Warn-only: a failure leaves a named diagnostic in `status` and
   never interrupts the game. Windows-only like the other developer reports. */
bool AssetCatalogGame_PrepareRegionDraw(char* status, int statusCapacity);

/* Number of placements collected by AssetCatalogGame_PrepareRegionDraw (0 when
   none were collected or the flag is off). */
int AssetCatalogGame_GetRegionDrawPlacementCount(void);

/* Reads one collected region placement by index. Returns true when `index` is
   in range and `out` is non-NULL. */
bool AssetCatalogGame_GetRegionDrawPlacement(int index, AssetCatalogGameDrawPlacement* out);

/* Number of published resources currently registered. The `-drawmodel` and
   `-drawregion` conversions share this registry; it is cleared on level start. */
int AssetCatalogGame_GetModelResourceCount(void);

/* Reads the published resource at `index` in registration order. Returns true
   and fills the requested outputs when `index` is in range; `resourceKey` and
   `glbPath` may be NULL, and `modelSlot` may be NULL. */
bool AssetCatalogGame_GetModelResourceAt(int index,
	char* resourceKey, int resourceKeyCapacity,
	char* glbPath, int glbPathCapacity,
	int* modelSlot);

/* True when the region placement store reached its capacity during preparation
   (including the inventory walker's own object bound). */
bool AssetCatalogGame_RegionDrawPlacementsTruncated(void);

/* True when more distinct model slots were referenced than the resource
   registry can hold, so the extra slots were not converted. */
bool AssetCatalogGame_RegionDrawResourcesTruncated(void);

/* Monotonic per-level generation, bumped in AssetCatalogGame_BeginLevel (the
   level boundary that also clears the resource registry and the collected
   stores). The native bridge compares it each frame and resets its per-mode
   registration/instance/report state when it changes, so a level change is
   detected even when the registry is already repopulated by the same frame's
   preparation (LoadGameLevel -> SpoolSYNC -> Prepare*). Starts at 0 before the
   first level. */
unsigned int AssetCatalogGame_LevelGeneration(void);

#endif /* ASSET_CATALOG_GAME_H */
