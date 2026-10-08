#ifndef DEVELOPER_NATIVE_REGION_H
#define DEVELOPER_NATIVE_REGION_H

#if !defined(PSX) && !defined(__ANDROID__) && !defined(__EMSCRIPTEN__)
#include "NativeAssetCache.h"
#include "NativeRegionCoverage.h"

enum class DeveloperNativeRegionMode { Off, Inventory, Render };
// Camera-independent, read-only source region. Rendering is explicitly opt-in
// and incomplete until R03's backdrop, picking and visual gates are accepted.
void DeveloperNativeRegion_Initialise(DeveloperNativeRegionMode mode);
void DeveloperNativeRegion_Shutdown();
void DeveloperNativeRegion_PrepareFrame();
void DeveloperNativeRegion_Update(const NativeAsset::GeometryCache& geometry);
void DeveloperNativeRegion_ObserveView(const void* packedObject);
// Producer publication counts are distinct from backend submission/pixel proof.
bool DeveloperNativeRegion_GetCoverage(NativeAsset::RegionCoverageCounts& counts,
	uint64_t& tick, bool& published);

struct DeveloperNativePickSource
{
	AssetCatalogHandle logicalModel, renderedModel, vertexModel;
	PsyXNativeMeshHandle mesh;
	PsyXNativeMaterialHandle material;
	uint64_t identity;
	int modelIndex, position[3];
	unsigned int yaw, part, sourceVertices, sourcePolygons;
	NativeAsset::MaterialReference artwork;
};
// Resolve only copied metadata from the submitted frame. Retained selections
// revalidate catalog generations/revisions and GPU handles on each query.
PsyXNativeResult DeveloperNativeRegion_GetPickSource(const PsyXNativePickResult& pick,
	DeveloperNativePickSource& source);

// Shared source owner, consumed immediately. Capture must precede legacy MODEL
// conversion. The returned entry is borrowed until the owner's next mutation.
const NativeAsset::CachedGeometry* DeveloperNativeAssets_CaptureModel(int sourceSlot);
#endif
#endif
