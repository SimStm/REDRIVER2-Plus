#ifndef HD_TEXTURE_OVERRIDES_H
#define HD_TEXTURE_OVERRIDES_H

struct HdTextureOverrideDiagnostics
{
	int supported;
	int enabled;
	int manifestFound;
	int manifestEntries;
	int discoveredMods;
	int activeMods;
	int loadedImages;
	int registeredOverrides;
	char status[160];
};

struct HdTextureOverrideModInfo
{
	char id[48];
	char name[96];
	char description[160];
	int enabled;
	int textureEntries;
};

struct HdTextureOverrideEntryInfo
{
	char modId[48];
	char textureName[48];
	char assetPath[192];
	int texturePage;
	int textureIndex;
	int active;
};

struct HdTextureInspectorInfo
{
	char textureName[48];
	char modId[48];
	char overridePath[192];
	int texturePage;
	int textureIndex;
	int hasOverride;
	unsigned short u, v, width, height;
	unsigned int previewTextureId;
};

#define HD_TEXTURE_MODEL_REFERENCE_CAPACITY 128
#define HD_TEXTURE_MODEL_REFERENCES_MAX 8
#define HD_TEXTURE_CONTEXT_CAPACITY 32

// Descriptive metadata for one export: which kind of object the texture belongs
// to and which level uses it. It never changes the identity triple or the
// override matching key; it is written to the manifest and, when organized
// export is enabled, becomes part of the output directory.
struct HdTextureExportContext
{
	const char* objectType;	// "buildings", "cars", "pedestrians", ...
	const char* levelName;	// "CHICAGO", "HAVANA", ...
};

// One resource requested by a batch export. Model references are informational
// metadata: they are written to the manifest but never change the identity
// triple, the override matching key, or the exported image count.
struct HdTextureOverrideBatchItem
{
	unsigned short tpage, clut, u, v, width, height;
	char textureName[48];
	int texturePage;
	int textureIndex;
	char filenameSuffix[48];
	char modelReferences[HD_TEXTURE_MODEL_REFERENCES_MAX][HD_TEXTURE_MODEL_REFERENCE_CAPACITY];
	int modelReferenceCount;
	char objectType[HD_TEXTURE_CONTEXT_CAPACITY];
	char levelName[HD_TEXTURE_CONTEXT_CAPACITY];
};

struct HdTextureOverrideBatchResult
{
	int requested;
	int exported;
	int duplicates;
	int failed;
	char status[256];
};

#define HD_TEXTURE_BATCH_MAX_ITEMS 128
#define HD_TEXTURE_BATCH_MESSAGE_CAPACITY 128

enum HdTextureOverrideBatchOutcome
{
	HD_TEXTURE_BATCH_OUTCOME_PENDING = 0,
	HD_TEXTURE_BATCH_OUTCOME_DUPLICATE = 1,
	HD_TEXTURE_BATCH_OUTCOME_EXPORTED = 2,
	HD_TEXTURE_BATCH_OUTCOME_FAILED = 3
};

// Cooperative batch state. The caller owns the item array and keeps it alive
// while the job runs. The job stores plan indices and per-item outcomes only,
// so a large batch can be stepped across frames with progress and cancellation.
// Each resource is published through the same atomic per-file path as a single
// export; there is no all-or-nothing transaction.
struct HdTextureOverrideBatchJob
{
	const HdTextureOverrideBatchItem* items;
	int itemCount;
	char modId[48];
	int uniqueIndices[HD_TEXTURE_BATCH_MAX_ITEMS];
	int uniqueCount;
	int nextIndex;
	int exported;
	int duplicates;
	int failed;
	int cancelled;
	signed char outcomes[HD_TEXTURE_BATCH_MAX_ITEMS];
	char messages[HD_TEXTURE_BATCH_MAX_ITEMS][HD_TEXTURE_BATCH_MESSAGE_CAPACITY];
};

void HdTextureOverrides_GetModsDirectory(char* path, int capacity);
bool HdTextureOverrides_ExportInspectorReport(const char* modId, const char* report, char* status, int capacity);

// Clears associations for a newly parsed texture set. Loaded RGBA images stay
// cached until process exit, while the original TIM/VRAM data is never changed.
void HdTextureOverrides_Reset();
void HdTextureOverrides_BeginPage(int texturePage);
void HdTextureOverrides_RegisterTexture(int texturePage, int textureIndex, const char* textureName,
	unsigned short tpage, unsigned short clut, unsigned short u, unsigned short v,
	unsigned short width, unsigned short height);

void HdTextureOverrides_SetEnabled(int enabled);
bool HdTextureOverrides_Reload();
void HdTextureOverrides_GetDiagnostics(HdTextureOverrideDiagnostics* diagnostics);
int HdTextureOverrides_GetModCount();
bool HdTextureOverrides_GetModInfo(int index, HdTextureOverrideModInfo* info);
int HdTextureOverrides_GetEntryCount();
bool HdTextureOverrides_GetEntryInfo(int index, HdTextureOverrideEntryInfo* info);
bool HdTextureOverrides_FindTextureInfo(unsigned short tpage, unsigned short clut,
	unsigned short u, unsigned short v, unsigned short width, unsigned short height,
	HdTextureInspectorInfo* info);
/* Diagnostic: explains a texture-name lookup. Fills g_hdDebugPageClutMatches
   (known textures sharing the page/CLUT), g_hdDebugRegionMatches (those that
   also contain the region) and g_hdDebugBestIndex (the containing entry). */
extern int g_hdDebugPageClutMatches;
extern int g_hdDebugRegionMatches;
extern int g_hdDebugBestIndex;
extern char g_hdDebugBestName[48];
extern int g_hdDebugPageMatches;
extern int g_hdDebugPageFirstClut;
extern char g_hdDebugPageFirstName[48];
extern int g_hdDebugXYMatches;
extern int g_hdDebugXYFirstClut;
extern char g_hdDebugXYFirstName[48];
/* Named textures on the same page that contain the region but whose CLUT
   differs: the palette fallback used to resolve car/pedestrian palettes. */
extern int g_hdDebugPaletteFallbackMatches;
void HdTextureOverrides_DebugRegion(unsigned short tpage, unsigned short clut,
	unsigned short u, unsigned short v, unsigned short width, unsigned short height);
bool HdTextureOverrides_ExportTexture(unsigned short tpage, unsigned short clut,
	unsigned short u, unsigned short v, unsigned short width, unsigned short height,
	const char* textureName, int texturePage, int textureIndex, const char* modId,
	char* status, int statusCapacity);

// Same export with object/level metadata. The plain overload above keeps the
// historical behaviour of exporting without metadata.
bool HdTextureOverrides_ExportTextureWithContext(unsigned short tpage, unsigned short clut,
	unsigned short u, unsigned short v, unsigned short width, unsigned short height,
	const char* textureName, int texturePage, int textureIndex, const char* modId,
	const HdTextureExportContext* context, char* status, int statusCapacity);

// When enabled, exports are written under assets/inspector/<type>/<level>/ and
// the manifest records that relative path. Disabled by default; the manifest
// always stores the real location either way.
void HdTextureOverrides_SetOrganizedExport(int enabled);
int HdTextureOverrides_IsOrganizedExportEnabled();

// When enabled (the default), an export resolves the palette the level
// registered for the exported region instead of the palette the clicked
// primitive is drawn with. Cars and pedestrians use runtime palettes, so this
// chooses between "original colours, best for CLUT recolouring mods" and "the
// colours this instance currently shows".
void HdTextureOverrides_SetBaseColourExport(int enabled);
int HdTextureOverrides_IsBaseColourExportEnabled();

// Maps an inspector object key ("building:...", "car:...") to a stable type
// token used for export metadata and the organized directory. Never NULL.
const char* HdTextureOverrides_ObjectTypeFromKey(const char* key);

// Sanitizes an arbitrary model label into a short, filesystem-safe token.
// Returns false and clears out when no usable character remains.
bool HdTextureOverrides_MakeSafeNameToken(const char* text, char* out, int capacity);

// Exports a set of identified textures in one call, deduplicating by identity
// triple so a shared texture is published once. Failed and omitted resources
// are counted and named in result->status. Returns false only when the request
// itself is invalid; per-resource failures leave the batch running.
bool HdTextureOverrides_ExportTextureBatch(const char* modId,
	const HdTextureOverrideBatchItem* items, int itemCount,
	HdTextureOverrideBatchResult* result);

// Cooperative batch driver. Begin builds the identity plan (marking invalid
// and duplicate items), Step exports up to maxSteps pending resources, and
// Cancel stops further work while keeping already-published files. RetryFailed
// re-queues only the failed resources. Returns whether work remains.
bool HdTextureOverrides_BeginBatchJob(HdTextureOverrideBatchJob* job, const char* modId,
	const HdTextureOverrideBatchItem* items, int itemCount);
bool HdTextureOverrides_BatchJobActive(const HdTextureOverrideBatchJob* job);
int HdTextureOverrides_StepBatchJob(HdTextureOverrideBatchJob* job, int maxSteps);
void HdTextureOverrides_CancelBatchJob(HdTextureOverrideBatchJob* job);
void HdTextureOverrides_RetryFailedBatchJob(HdTextureOverrideBatchJob* job);
void HdTextureOverrides_GetBatchJobStatus(const HdTextureOverrideBatchJob* job, char* status, int capacity);

// Resolves the VRAM region registered for a level texture identity, so a
// catalog material that has no submitted triangle can still be exported.
// Returns false when the identity is not in the currently registered set.
bool HdTextureOverrides_GetKnownTextureRegion(int texturePage, int textureIndex,
	unsigned short* tpage, unsigned short* clut, unsigned short* u, unsigned short* v,
	unsigned short* width, unsigned short* height, char* textureName, int nameCapacity);

#endif
