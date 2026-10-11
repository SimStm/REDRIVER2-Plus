#ifndef NATIVE_PED_CAPTURE_H
#define NATIVE_PED_CAPTURE_H
#include "NativePedMesh.h"
struct PEDESTRIAN;
struct MODEL;
struct NativePedSprite
{
	float xy[8];
	unsigned char uv[8];
	unsigned short page, clut;
	int depth[2], canonicalUvOffset;
};
// Main-thread CPU presentation collector; no retained game/model/packet pointers.
void NativePedCapture_BeginFrame();
bool NativePedCapture_Enabled();
void NativePedCapture_SelectView(int view);
void NativePedCapture_Select(PEDESTRIAN* ped);
void NativePedCapture_Created(PEDESTRIAN* ped);
void NativePedCapture_RecordModel(PEDESTRIAN* ped, int component, MODEL* model,
	const short rotation[3][3], const int offset[3], int customClut = -1);
void NativePedCapture_RecordSprite(PEDESTRIAN* ped, int component, const NativePedSprite& sprite);
bool NativePedCapture_Consume(int view, const NativePedFrame** frames);
void NativePedCapture_Shutdown();
#endif
