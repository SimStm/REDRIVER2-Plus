#ifndef DEVELOPER_NATIVE_ASSET_FIXTURE_H
#define DEVELOPER_NATIVE_ASSET_FIXTURE_H
#include "NativeGpuAssetCache.h"
void DeveloperNativeAssetFixture_Initialise(bool requested);
void DeveloperNativeAssetFixture_Shutdown();
void DeveloperNativeAssetFixture_PrepareFrame();
void DeveloperNativeAssetFixture_Observe(const NativeAsset::CachedGeometry& geometry, const NativeAsset::Placement& placement);
void DeveloperNativeAssetFixture_Update(const NativeAsset::GeometryCache& geometry);
#endif
