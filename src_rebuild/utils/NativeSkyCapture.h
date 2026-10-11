#ifndef NATIVE_SKY_CAPTURE_H
#define NATIVE_SKY_CAPTURE_H
#include "NativeSkyMesh.h"
void NativeSkyCapture_BeginFrame();
void NativeSkyCapture_SelectView(int view);
void NativeSkyCapture_Load(const unsigned char* raw, int bytes, int level, int segment,
	const unsigned char* uv, const short* pages, const short* cluts,
	const unsigned char* lookup, const unsigned char* textures);
void NativeSkyCapture_Record(const short rotation[3][3], int yOffset, const int rgb[3]);
std::shared_ptr<const NativeSkyAssets> NativeSkyCapture_Read(unsigned* revision, const char** error);
bool NativeSkyCapture_Consume(int view, unsigned generation, unsigned revision, NativeSkyFrame* out);
void NativeSkyCapture_Shutdown();
#endif
