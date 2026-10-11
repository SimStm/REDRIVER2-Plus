#include "NativeSceneDebug.h"

#include <stdlib.h>
#include <string.h>

namespace
{
NativeSceneDebugSnapshot g_snapshot = {};
}

void NativeSceneDebug_Publish(const NativeSceneDebugSnapshot& snapshot)
{
	g_snapshot = snapshot;
}

NativeSceneDebugSnapshot NativeSceneDebug_Read()
{
	return g_snapshot;
}

bool NativeSceneDebug_Enabled()
{
	const char* value = getenv("RED2_NATIVE_VISUAL_DEBUG");
	return value && strcmp(value, "1") == 0;
}
