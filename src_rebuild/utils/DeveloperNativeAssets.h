#ifndef DEVELOPER_NATIVE_ASSETS_H
#define DEVELOPER_NATIVE_ASSETS_H
#ifdef __cplusplus
extern "C" {
#endif
// Game-owned read-only source collection, before legacy polygon/GTE processing.
// Opaque inputs are consumed during the call; no game pointers are retained.
void DeveloperNativeAssets_Initialise(int requested);
void DeveloperNativeAssets_Shutdown(void);
void DeveloperNativeAssets_PrepareFrame(void);
void DeveloperNativeAssets_ObserveBuilding(const void* object, const void* model);
void DeveloperNativeAssets_ObserveTile(const void* packedObject, const void* model);
void DeveloperNativeAssets_ObserveRegionView(const void* packedObject);
void DeveloperNativeAssets_Update(void);
#ifdef __cplusplus
}
#endif
#endif
