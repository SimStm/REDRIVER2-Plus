#include "PsyX/PsyX_native.h"
#include <string.h>

/* Compile as C, then return the complete descriptor across the C/C++ ABI. */
PsyXNativeMeshDesc NativeSceneCProbe(const PsyXNativeVertex* vertices, const uint32_t* indices)
{
	PsyXNativeMeshDesc desc;
	memset(&desc, 0, sizeof(desc));
	desc.size = sizeof(desc);
	desc.version = PSYX_NATIVE_VERSION;
	desc.vertices = vertices;
	desc.indices = indices;
	desc.vertexCount = 3;
	desc.indexCount = 3;
	desc.boundsMax[0] = desc.boundsMax[1] = 1;
	return desc;
}

PsyXNativeSampling NativeSamplingCProbe(void)
{
	return PSYX_NATIVE_SAMPLE_TRILINEAR;
}

PsyXNativePickResult NativePickCProbe(void)
{
	PsyXNativePickResult result;
	memset(&result,0,sizeof(result));
	result.size=sizeof(result); result.version=PSYX_NATIVE_VERSION;
	result.identity=UINT64_C(0x100000065); result.reason=PSYX_NATIVE_PENDING;
	return result;
}
