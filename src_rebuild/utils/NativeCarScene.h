#ifndef NATIVE_CAR_SCENE_H
#define NATIVE_CAR_SCENE_H

#include "../native/NativeRenderer.h"
#include <string.h>

#ifdef _WIN32
inline bool NativeCarScene_HideBody(int cameraView, int cameraCarId, int carId)
{
	return cameraView == 2 && cameraCarId == carId;
}

// The bridge uses this same bounded seam for the single world/actor submission.
// An empty visible list can still be a valid published world; loading cannot.
inline bool NativeCarScene_PrepareWorld(const NativeInstance* world, int worldCount,
	bool publicationValid, int actorReserve, NativeInstance* combined, int capacity, int* count)
{
	if (!count) return false;
	*count = 0;
	if (!publicationValid) return false;
	if (!combined || worldCount < 0 || actorReserve < 0 || actorReserve > capacity ||
		worldCount > capacity - actorReserve || (worldCount && !world)) return false;
	if (worldCount) memcpy(combined, world, (size_t)worldCount * sizeof(NativeInstance));
	*count = worldCount;
	return true;
}

inline bool NativeCarScene_Append(const NativeInstance& actor, NativeInstance* combined,
	int capacity, int* count)
{
	if (!combined || !count || *count < 0 || *count >= capacity) return false;
	combined[(*count)++] = actor;
	return true;
}
#endif
#endif
