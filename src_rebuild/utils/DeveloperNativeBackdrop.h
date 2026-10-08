#ifndef DEVELOPER_NATIVE_BACKDROP_H
#define DEVELOPER_NATIVE_BACKDROP_H
#ifdef __cplusplus
extern "C" {
#endif
typedef struct DeveloperNativeBackdropSource
{
	const void* model;
	const unsigned char* textures;
	unsigned int textureCount;
	const unsigned char* uv;
	unsigned int uvCount;
	const unsigned short* pages;
	const unsigned short* cluts;
	int color[3];
	int verticalOffset; // original view-space Y-down sky offset
} DeveloperNativeBackdropSource;
// Consumes model/art-table spans immediately; no borrowed pointer is retained.
void DeveloperNativeBackdrop_Observe(const DeveloperNativeBackdropSource* source);
#ifdef __cplusplus
}
#if !defined(PSX) && !defined(__ANDROID__) && !defined(__EMSCRIPTEN__)
#include "PsyX/PsyX_native.h"
#include <vector>
void DeveloperNativeBackdrop_SetEnabled(int enabled);
void DeveloperNativeBackdrop_BeginFrame();
void DeveloperNativeBackdrop_Clear();
PsyXNativeResult DeveloperNativeBackdrop_Append(const PsyXNativeView& view, std::vector<PsyXNativeInstance>& instances);
#endif
#endif
#endif
