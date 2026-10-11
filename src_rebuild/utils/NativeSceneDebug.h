#ifndef NATIVE_SCENE_DEBUG_H
#define NATIVE_SCENE_DEBUG_H

// Main-thread-only diagnostic snapshot shared by the native scene producer
// and its developer overlay consumer. Publish and Read copy this POD by value;
// neither API retains pointers or crosses threads.
struct NativeSceneDebugSnapshot
{
	unsigned generation;
	bool worldReady;
	bool worldFailed;
	int worldResources;
	int worldPlacements;
	int worldVisible;
	int carActive;
	int carReady;
	int carSubmitted;
	int carWaiting;
	int carFailed;
	int carMissing;
	int wheelReady;
	int wheelSubmitted;
	int wheelFailed;
};

void NativeSceneDebug_Publish(const NativeSceneDebugSnapshot& snapshot);
NativeSceneDebugSnapshot NativeSceneDebug_Read();
bool NativeSceneDebug_Enabled();

#endif
