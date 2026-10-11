#ifndef NATIVE_PED_SCENE_H
#define NATIVE_PED_SCENE_H
#include "../native/NativeRenderer.h"
#ifdef _WIN32
struct NativePedSceneCounts { int active, ready, submitted, waiting, failed; };
void NativePedScene_ClearFrameCounts();
bool NativePedScene_Append(unsigned generation, int view, int level, int variant, NativeInstance* combined, int* count);
void NativePedScene_Commit();
void NativePedScene_RejectFrame();
void NativePedScene_Shutdown();
NativePedSceneCounts NativePedScene_ReadCounts();
int NativePedScene_ResourceReserve();
#endif
#endif
