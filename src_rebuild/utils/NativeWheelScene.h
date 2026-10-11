#ifndef NATIVE_WHEEL_SCENE_H
#define NATIVE_WHEEL_SCENE_H
#include "../native/NativeRenderer.h"
#ifdef _WIN32
struct NativeWheelSceneCounts { int ready, submitted, failed; };
// Main-thread scene ownership; one immutable conversion worker, no frame waits.
void NativeWheelScene_Begin(unsigned int generation);
bool NativeWheelScene_Append(int carId, bool hidden, const NativeInstance& car, NativeInstance* combined, int* count);
void NativeWheelScene_Commit();
void NativeWheelScene_ClearFrameCounts();
void NativeWheelScene_RejectFrame();
NativeWheelSceneCounts NativeWheelScene_ReadCounts();
void NativeWheelScene_Shutdown();
// Includes old/new private wheel resources during a publication transition.
int NativeWheelScene_ResourceReserve();
#endif
#endif
