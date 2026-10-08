#include "DeveloperNativeScene.h"

#if !defined(PSX) && !defined(__ANDROID__) && !defined(__EMSCRIPTEN__)
#include "DeveloperNativeCamera.h"
#include "driver2.h"
#include "C/camera.h"
#include "C/draw.h"
#include "C/main.h"
#include "C/glaunch.h"
#include "C/mission.h"
#include "C/system.h"
#include "PsyX/PsyX_globals.h"
#include "PsyX/PsyX_public.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace
{
bool s_requested = false;
bool s_rebuild = true;
uint64_t s_generation = 0;
int s_level = -1;
int s_previousTick = -1;
PsyXNativeMeshHandle s_mesh = {};
PsyXNativeResult s_lastFailure = PSYX_NATIVE_OK;

struct AnalyticTriangle
{
	float points[3][3]; // initial game-camera coordinates; original Y-down units
	float color[3];
};

// Ordered deliberately: near/intersecting first, far last. Correct pixels
// require depth testing; painter order produces the wrong blue overlap.
const AnalyticTriangle kTriangles[] = {
	{ { {-900,-700,2300}, {900,-700,8000}, {-900,700,2300} }, {1.0f,0.24f,0.025f} },
	{ { {-1300,-900,6000}, {1300,-900,6000}, {-1300,900,6000} }, {0.03f,0.15f,0.95f} },
	{ { {900,-700,4500}, {2100,-100,4500}, {900,500,4500} }, {0.15f,0.85f,0.12f} },
	// Opposite winding: this magenta marker must be culled.
	{ { {-2100,-700,4500}, {-2100,500,4500}, {-900,-100,4500} }, {0.9f,0.015f,0.8f} },
	// Horizontal surface makes camera translation and pitch visible.
	{ { {-12000,800,1000}, {-12000,800,16000}, {12000,800,1000} }, {0.12f,0.14f,0.18f} },
	{ { {12000,800,1000}, {-12000,800,16000}, {12000,800,16000} }, {0.12f,0.14f,0.18f} }
};

void FinalRotation(float out[9])
{
	for (unsigned int row = 0; row < 3; ++row)
		for (unsigned int column = 0; column < 3; ++column)
			out[row * 3 + column] = inv_camera_matrix.m[row][column] / 4096.0f;
}

PsyXNativeResult CreateAnalyticMesh(const float rotation[9], const float camera[3])
{
	const unsigned int count = sizeof(kTriangles) / sizeof(kTriangles[0]);
	PsyXNativeVertex vertices[count * 3] = {};
	uint32_t indices[count * 3];
	PsyXNativeMeshDesc desc = {};
	desc.size = sizeof(desc);
	desc.version = PSYX_NATIVE_VERSION;
	desc.vertices = vertices;
	desc.indices = indices;
	desc.vertexCount = desc.indexCount = count * 3;
	for (unsigned int t = 0; t < count; ++t)
	{
		const AnalyticTriangle& triangle = kTriangles[t];
		float a[3], b[3], sourceNormal[3];
		for (unsigned int axis = 0; axis < 3; ++axis)
		{
			a[axis] = triangle.points[1][axis] - triangle.points[0][axis];
			b[axis] = triangle.points[2][axis] - triangle.points[0][axis];
		}
		sourceNormal[0] = a[1]*b[2] - a[2]*b[1];
		sourceNormal[1] = a[2]*b[0] - a[0]*b[2];
		sourceNormal[2] = a[0]*b[1] - a[1]*b[0];
		const float length = sqrtf(sourceNormal[0]*sourceNormal[0] + sourceNormal[1]*sourceNormal[1] + sourceNormal[2]*sourceNormal[2]);
		for (unsigned int axis = 0; axis < 3; ++axis) sourceNormal[axis] /= length;
		for (unsigned int v = 0; v < 3; ++v)
		{
			PsyXNativeVertex& vertex = vertices[t * 3 + v];
			for (unsigned int axis = 0; axis < 3; ++axis)
			{
				const float sign = axis == 1 ? -1.0f : 1.0f;
				// Inverse of the final rigid camera basis; anchor once, then keep
				// world positions unchanged while the real camera moves.
				vertex.position[axis] = sign * (camera[axis] + rotation[axis] * triangle.points[v][0] +
					rotation[3 + axis] * triangle.points[v][1] + rotation[6 + axis] * triangle.points[v][2]);
				vertex.normal[axis] = sign * (rotation[axis]*sourceNormal[0] + rotation[3+axis]*sourceNormal[1] + rotation[6+axis]*sourceNormal[2]);
				if (t == 0 && v == 0) desc.boundsMin[axis] = desc.boundsMax[axis] = vertex.position[axis];
				else
				{
					desc.boundsMin[axis] = std::min(desc.boundsMin[axis], vertex.position[axis]);
					desc.boundsMax[axis] = std::max(desc.boundsMax[axis], vertex.position[axis]);
				}
				vertex.color[axis] = triangle.color[axis];
			}
			vertex.color[3] = 1;
			vertex.uv[0] = v == 1 ? 1.0f : 0.0f;
			vertex.uv[1] = v == 2 ? 1.0f : 0.0f;
		}
		// Y reflection changes handedness; reverse once at the adapter boundary.
		indices[t * 3] = t * 3;
		indices[t * 3 + 1] = t * 3 + 2;
		indices[t * 3 + 2] = t * 3 + 1;
	}
	return PsyX_Native_CreateMesh(&desc, &s_mesh);
}

void ReportFailure(PsyXNativeResult result)
{
	PsyX_Native_SetFrameStatus(result);
	if (result != s_lastFailure)
		PsyX_Log_Warning("Native analytic scene: %s\n", PsyX_Native_ResultName(result));
	s_lastFailure = result;
}
}

void DeveloperNativeScene_Initialise(int requested)
{
	s_requested = requested != 0;
	s_generation = PsyX_Native_GetSceneGeneration();
	s_rebuild = true;
	PsyX_Native_SetRequested(requested);
}

void DeveloperNativeScene_Shutdown()
{
	PsyX_Native_ResetScene();
	PsyX_Native_SetRequested(0);
	s_mesh = PsyXNativeMeshHandle();
}

void DeveloperNativeScene_SetRequested(int requested)
{
	s_requested = requested != 0;
	PsyX_Native_SetRequested(requested);
	if (s_requested) s_rebuild = true;
}
int DeveloperNativeScene_GetRequested() { return s_requested; }
void DeveloperNativeScene_Rebuild() { s_rebuild = true; }

void DeveloperNativeScene_Update()
{
	if (!s_requested) return;
	// Split-screen has no tested native view/boundary; report it explicitly.
	if (NumPlayers != 1 && !NoPlayerControl) { ReportFailure(PSYX_NATIVE_UNSUPPORTED); return; }
	if (PsyX_GetRenderBackend() != PSYX_BACKEND_VULKAN) { ReportFailure(PSYX_NATIVE_UNSUPPORTED); return; }
	// Startup briefly exposes an empty basis and then a transitional camera.
	// Anchor this fixture after 30 ticks; report expected native loading until
	// then, rather than anchoring in the transitional view.
	if (FrameCnt < 30 && (s_rebuild || FrameCnt < s_previousTick)) { ReportFailure(PSYX_NATIVE_PENDING); return; }
	float rotation[9]; FinalRotation(rotation);
	const float camera[] = {float(camera_position.vx), float(camera_position.vy), float(camera_position.vz)};
	PsyXNativeSnapshot snapshot = {};
	snapshot.size = sizeof(snapshot);
	snapshot.version = PSYX_NATIVE_VERSION;
	snapshot.simulationTick = uint64_t(FrameCnt);
	if (!DeveloperNativeCamera_Build(&snapshot.view, rotation, camera, float(scr_z),
		float(current->disp.disp.w), float(current->disp.disp.h), float(g_windowWidth), float(g_windowHeight)))
	{
		ReportFailure(PSYX_NATIVE_INVALID);
		return;
	}
	const uint64_t generation = PsyX_Native_GetSceneGeneration();
	if (s_rebuild || generation != s_generation || s_level != GameLevel || FrameCnt < s_previousTick)
	{
		s_generation = PsyX_Native_ResetScene();
		s_mesh = PsyXNativeMeshHandle();
		const PsyXNativeResult result = CreateAnalyticMesh(rotation, camera);
		if (result != PSYX_NATIVE_PENDING && result != PSYX_NATIVE_OK) { ReportFailure(result); return; }
		s_rebuild = false;
		s_level = GameLevel;
	}
	s_previousTick = FrameCnt;
	PsyXNativeInstance instance = {};
	instance.mesh = s_mesh;
	instance.identity = 1;
	for (unsigned int i = 0; i < 16; i += 5) instance.world[i] = 1;
	for (unsigned int i = 0; i < 4; ++i) instance.tint[i] = 1;
	snapshot.sceneGeneration = s_generation;
	snapshot.instances = &instance;
	snapshot.instanceCount = 1;
	const PsyXNativeResult result = PsyX_Native_Publish(&snapshot);
	if (result != PSYX_NATIVE_OK) ReportFailure(result);
	else s_lastFailure = PSYX_NATIVE_OK;
}

#else
void DeveloperNativeScene_Initialise(int requested) { (void)requested; }
void DeveloperNativeScene_Shutdown(void) {}
void DeveloperNativeScene_Update(void) {}
void DeveloperNativeScene_SetRequested(int requested) { (void)requested; }
int DeveloperNativeScene_GetRequested(void) { return 0; }
void DeveloperNativeScene_Rebuild(void) {}
#endif
