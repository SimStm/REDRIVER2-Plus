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
bool HdTextureOverrides_ExportTexture(unsigned short tpage, unsigned short clut,
	unsigned short u, unsigned short v, unsigned short width, unsigned short height,
	const char* textureName, int texturePage, int textureIndex, const char* modId,
	char* status, int statusCapacity);

#endif
