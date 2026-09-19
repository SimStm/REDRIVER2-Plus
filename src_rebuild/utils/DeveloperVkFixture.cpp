#include "DeveloperVkFixture.h"

#if !defined(PSX) && !defined(__ANDROID__) && !defined(__EMSCRIPTEN__)

#include "driver2.h"

#include "GltfLoader.h"
#include "PsyX/PsyX_vk.h"

#include <SDL.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <vector>

// Screenshots are written with SDL's own BMP writer so this tool stays free of
// the game's PNG/WIC tooling; convert with any image tool if PNG is needed.
static void MakeBmpPath(const char* requested, char* out, size_t outSize)
{
	snprintf(out, outSize, "%s", requested ? requested : "vk_fixture.bmp");
	char* dot = strrchr(out, '.');
	if (dot && (strlen(dot) == 4))
	{
		dot[1] = 'b'; dot[2] = 'm'; dot[3] = 'p';
	}
	else if (!dot)
	{
		strncat(out, ".bmp", outSize - strlen(out) - 1);
	}
}

namespace
{
// The game's log is not open yet when this developer tool runs, so it keeps its
// own tiny log next to the executable.
void VkLog(const char* format, ...)
{
	FILE* file = fopen("vk_fixture.log", "a");
	if (!file)
		return;

	va_list args;
	va_start(args, format);
	vfprintf(file, format, args);
	va_end(args);
	fclose(file);
}

const char* const kFixtureDir = "../../../assets/modern_fixtures/";
const float kAssetScale = 400.0f;	// game units per glTF metre
const float kPi = 3.14159265359f;

// Same owned gallery as the OpenGL fixture (DeveloperModernMesh.cpp), in this
// window's own world space: rows along -Z, lateral offsets on X.
struct FixtureDef
{
	const char* file;
	float lateral;
	float forward;
	float yawDegrees;
};

const FixtureDef kFixtureDefs[] =
{
	{ "bollard.glb",        -1500.0f,  200.0f, 150.0f },
	{ "jersey_barrier.glb",  -500.0f,  200.0f, 170.0f },
	{ "wooden_crate.glb",     500.0f,  200.0f, 205.0f },
	{ "traffic_cone.glb",    1500.0f,  200.0f, 185.0f },
	{ "oil_barrel.glb",     -1500.0f, 1200.0f, 140.0f },
	{ "street_lamp.glb",     -500.0f, 1200.0f, 160.0f },
	{ "bollard_pbr.glb",      500.0f, 1200.0f, 195.0f },
	{ "oil_barrel_pbr.glb",  1500.0f, 1200.0f, 175.0f },
};
const int kFixtureDefCount = (int)(sizeof(kFixtureDefs) / sizeof(kFixtureDefs[0]));

struct MeshInstance
{
	int mesh;
	float pos[3];
	float yaw;
};

MeshInstance g_instances[16];
int g_instanceCount = 0;

float g_cameraPos[3] = { 0.0f, 900.0f, 2600.0f };
float g_cameraYaw = 0.0f;			// radians, 0 looks towards -Z
float g_cameraPitch = -0.28f;		// radians, negative looks down
float g_sunDir[3] = { -0.25f, 0.72f, -0.38f };
float g_exposure = 1.0f;
int g_shadows = 1;
int g_ao = 1;

void BuildLookAt(const float eye[3], const float at[3], float m[16])
{
	float f[3] = { at[0] - eye[0], at[1] - eye[1], at[2] - eye[2] };
	float fl = sqrtf(f[0] * f[0] + f[1] * f[1] + f[2] * f[2]);
	fl = fl > 1e-5f ? fl : 1.0f;
	f[0] /= fl; f[1] /= fl; f[2] /= fl;

	// cross(f, up) with up = (0,1,0).
	float s[3] = { -f[2], 0.0f, f[0] };

	float sl = sqrtf(s[0] * s[0] + s[1] * s[1] + s[2] * s[2]);
	sl = sl > 1e-5f ? sl : 1.0f;
	s[0] /= sl; s[1] /= sl; s[2] /= sl;

	const float u[3] = { s[1] * f[2] - s[2] * f[1], s[2] * f[0] - s[0] * f[2], s[0] * f[1] - s[1] * f[0] };

	m[0] = s[0]; m[4] = s[1]; m[8] = s[2];  m[12] = -(s[0] * eye[0] + s[1] * eye[1] + s[2] * eye[2]);
	m[1] = u[0]; m[5] = u[1]; m[9] = u[2];  m[13] = -(u[0] * eye[0] + u[1] * eye[1] + u[2] * eye[2]);
	m[2] = -f[0]; m[6] = -f[1]; m[10] = -f[2]; m[14] = (f[0] * eye[0] + f[1] * eye[1] + f[2] * eye[2]);
	m[3] = 0.0f; m[7] = 0.0f; m[11] = 0.0f; m[15] = 1.0f;
}

// Perspective with Vulkan conventions: Y-down NDC and depth in [0,1].
void BuildPerspective(float fovY, float aspect, float nearZ, float farZ, float m[16])
{
	memset(m, 0, sizeof(float) * 16);
	const float f = 1.0f / tanf(fovY * 0.5f);
	m[0] = f / aspect;
	m[5] = -f;
	m[10] = farZ / (nearZ - farZ);
	m[11] = -1.0f;
	m[14] = (nearZ * farZ) / (nearZ - farZ);
}

void BuildWorldMatrix(const float pos[3], float yaw, float m[16])
{
	const float cy = cosf(yaw);
	const float sy = sinf(yaw);
	m[0] = cy;   m[4] = 0.0f; m[8] = sy;    m[12] = pos[0];
	m[1] = 0.0f; m[5] = 1.0f; m[9] = 0.0f;  m[13] = pos[1];
	m[2] = -sy;  m[6] = 0.0f; m[10] = cy;   m[14] = pos[2];
	m[3] = 0.0f; m[7] = 0.0f; m[11] = 0.0f; m[15] = 1.0f;
}

void AppendSphere(std::vector<float>& positions, std::vector<float>& normals,
	std::vector<float>& uvs, std::vector<unsigned short>& indices, float radius, int segments, int rings)
{
	const unsigned short base = (unsigned short)(positions.size() / 3);
	for (int r = 0; r <= rings; r++)
	{
		const float v = (float)r / (float)rings;
		const float theta = v * kPi;
		for (int s = 0; s <= segments; s++)
		{
			const float u = (float)s / (float)segments;
			const float phi = u * 2.0f * kPi;
			const float x = sinf(theta) * cosf(phi);
			const float y = cosf(theta);
			const float z = sinf(theta) * sinf(phi);
			positions.push_back(x * radius);
			positions.push_back(y * radius);
			positions.push_back(z * radius);
			normals.push_back(x);
			normals.push_back(y);
			normals.push_back(z);
			uvs.push_back(u);
			uvs.push_back(v);
		}
	}
	for (int r = 0; r < rings; r++)
	{
		for (int s = 0; s < segments; s++)
		{
			const unsigned short a = (unsigned short)(base + r * (segments + 1) + s);
			const unsigned short b = (unsigned short)(a + segments + 1);
			indices.push_back(a); indices.push_back(b); indices.push_back((unsigned short)(a + 1));
			indices.push_back(b); indices.push_back((unsigned short)(b + 1)); indices.push_back((unsigned short)(a + 1));
		}
	}
}

void AddInstance(int mesh, const float pos[3], float yaw)
{
	if (mesh < 0 || g_instanceCount >= 16)
		return;

	MeshInstance* instance = &g_instances[g_instanceCount++];
	instance->mesh = mesh;
	instance->pos[0] = pos[0];
	instance->pos[1] = pos[1];
	instance->pos[2] = pos[2];
	instance->yaw = yaw;
}

int AddGltfFixture(const char* path, float lateral, float forward, float yawDegrees)
{
	VkLog("VkFixture: loading %s\n", path);

	GltfMeshData data;
	char error[256] = "";
	if (Gltf_LoadFile(path, &data, error, sizeof(error)) != 0)
	{
		VkLog("VkFixture: cannot load %s: %s\n", path, error);
		return -1;
	}
	VkLog("VkFixture: loaded %s (%d verts)\n", path, data.vertexCount);

	std::vector<float> positions(data.positions.size());
	float minY = 1e30f;
	for (size_t i = 0; i + 2 < positions.size(); i += 3)
	{
		const float x = data.positions[i + 0] * kAssetScale;
		const float y = data.positions[i + 1] * kAssetScale;
		const float z = data.positions[i + 2] * kAssetScale;
		positions[i + 0] = x;
		positions[i + 1] = y;
		positions[i + 2] = z;
		if (y < minY)
			minY = y;
	}

	std::vector<unsigned short> indices(data.indices.size());
	for (size_t i = 0; i < data.indices.size(); i++)
	{
		if (data.indices[i] >= 65535)
		{
			VkLog("VkFixture: %s has an index above 65534\n", path);
			return -1;
		}
		indices[i] = (unsigned short)data.indices[i];
	}

	PsyXModernMeshDesc desc;
	memset(&desc, 0, sizeof(desc));
	desc.positions = positions.data();
	desc.normals = data.normals.empty() ? NULL : data.normals.data();
	desc.uvs = data.uvs.empty() ? NULL : data.uvs.data();
	desc.vertexCount = data.vertexCount;
	desc.indices = indices.empty() ? NULL : indices.data();
	desc.indexCount = (int)indices.size();
	desc.baseColorFactor = data.baseColorFactor;
	desc.emissiveFactor = data.emissiveFactor;
	desc.metallicFactor = data.metallicFactor;
	desc.roughnessFactor = data.roughnessFactor;

	const int mesh = PsyX_Vk_CreateMesh(&desc);
	if (mesh < 0)
	{
		VkLog("VkFixture: mesh creation failed for %s\n", path);
		return -1;
	}
	VkLog("VkFixture: mesh %d created for %s\n", mesh, path);

	if (!data.baseColorRGBA.empty())
	{
		const int texture = PsyX_Vk_CreateTexture(data.baseColorRGBA.data(), data.baseColorWidth, data.baseColorHeight);
		if (texture >= 0)
			PsyX_Vk_SetMeshTexture(mesh, 0, texture);
	}
	if (!data.normalRGBA.empty())
	{
		const int texture = PsyX_Vk_CreateTexture(data.normalRGBA.data(), data.normalWidth, data.normalHeight);
		if (texture >= 0)
			PsyX_Vk_SetMeshTexture(mesh, 1, texture);
	}
	if (!data.metallicRoughnessRGBA.empty())
	{
		const int texture = PsyX_Vk_CreateTexture(data.metallicRoughnessRGBA.data(), data.metallicRoughnessWidth, data.metallicRoughnessHeight);
		if (texture >= 0)
			PsyX_Vk_SetMeshTexture(mesh, 2, texture);
	}
	if (!data.emissiveRGBA.empty())
	{
		const int texture = PsyX_Vk_CreateTexture(data.emissiveRGBA.data(), data.emissiveWidth, data.emissiveHeight);
		if (texture >= 0)
			PsyX_Vk_SetMeshTexture(mesh, 3, texture);
	}

	float pos[3];
	pos[0] = lateral;
	pos[1] = -minY;			// base rests on the plane y = 0
	pos[2] = -forward;
	AddInstance(mesh, pos, yawDegrees * kPi / 180.0f);

	VkLog("VkFixture: imported %s (%d verts, %d tris)\n", path, data.vertexCount, data.triangleCount);
	return mesh;
}

void AddAnalyticSpheres(void)
{
	const float metallic[4] = { 0.0f, 1.0f, 0.0f, 1.0f };
	const float roughness[4] = { 0.15f, 0.15f, 0.85f, 0.85f };
	const float colours[4][3] =
	{
		{ 0.9f, 0.9f, 0.9f }, { 0.95f, 0.75f, 0.35f },
		{ 0.8f, 0.4f, 0.35f }, { 0.6f, 0.7f, 0.85f }
	};

	for (int i = 0; i < 4; i++)
	{
		std::vector<float> positions, normals, uvs;
		std::vector<unsigned short> indices;
		AppendSphere(positions, normals, uvs, indices, 300.0f, 32, 20);

		PsyXModernMeshDesc desc;
		memset(&desc, 0, sizeof(desc));
		desc.positions = positions.data();
		desc.normals = normals.data();
		desc.uvs = uvs.data();
		desc.vertexCount = (int)(positions.size() / 3);
		desc.indices = indices.data();
		desc.indexCount = (int)indices.size();

		float factor[4] = { colours[i][0], colours[i][1], colours[i][2], 1.0f };
		desc.baseColorFactor = factor;
		desc.metallicFactor = metallic[i];
		desc.roughnessFactor = roughness[i];

		const int mesh = PsyX_Vk_CreateMesh(&desc);
		if (mesh < 0)
			continue;

		float pos[3];
		pos[0] = (i - 1.5f) * 900.0f;
		pos[1] = 300.0f;
		pos[2] = -2400.0f;
		AddInstance(mesh, pos, 0.0f);
	}
}

void BuildScene(void)
{
	VkLog("VkFixture: building scene\n");
	char path[640];
	for (int i = 0; i < kFixtureDefCount; i++)
	{
		const FixtureDef& def = kFixtureDefs[i];
		snprintf(path, sizeof(path), "%s%s", kFixtureDir, def.file);
		AddGltfFixture(path, def.lateral, def.forward, def.yawDegrees);
	}

	AddAnalyticSpheres();
	VkLog("VkFixture: %d instances ready\n", g_instanceCount);
}

void UpdateCamera(float deltaSeconds, const Uint8* keys, int rightMouseDown, int mouseDx, int mouseDy)
{
	const float lookSpeed = 0.0028f;
	if (rightMouseDown)
	{
		g_cameraYaw -= (float)mouseDx * lookSpeed;
		g_cameraPitch -= (float)mouseDy * lookSpeed;
		if (g_cameraPitch < -1.45f) g_cameraPitch = -1.45f;
		if (g_cameraPitch > 1.45f) g_cameraPitch = 1.45f;
	}

	float speed = 2600.0f;
	if (keys && keys[SDL_SCANCODE_LSHIFT])
		speed *= 3.0f;

	const float cy = cosf(g_cameraYaw);
	const float sy = sinf(g_cameraYaw);
	const float cp = cosf(g_cameraPitch);
	const float sp = sinf(g_cameraPitch);
	const float forward[3] = { -sy * cp, sp, -cy * cp };
	const float right[3] = { cy, 0.0f, -sy };

	const float step = speed * deltaSeconds;
	if (keys)
	{
		if (keys[SDL_SCANCODE_W]) { g_cameraPos[0] += forward[0] * step; g_cameraPos[1] += forward[1] * step; g_cameraPos[2] += forward[2] * step; }
		if (keys[SDL_SCANCODE_S]) { g_cameraPos[0] -= forward[0] * step; g_cameraPos[1] -= forward[1] * step; g_cameraPos[2] -= forward[2] * step; }
		if (keys[SDL_SCANCODE_D]) { g_cameraPos[0] += right[0] * step; g_cameraPos[2] += right[2] * step; }
		if (keys[SDL_SCANCODE_A]) { g_cameraPos[0] -= right[0] * step; g_cameraPos[2] -= right[2] * step; }
		if (keys[SDL_SCANCODE_R]) g_cameraPos[1] += step;
		if (keys[SDL_SCANCODE_F]) g_cameraPos[1] -= step;
	}
}

void RotateSun(float angle)
{
	const float c = cosf(angle);
	const float s = sinf(angle);
	const float x = g_sunDir[0];
	const float z = g_sunDir[2];
	g_sunDir[0] = x * c - z * s;
	g_sunDir[2] = x * s + z * c;
}

void HandleToggles(const Uint8* keys)
{
	static int previous[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
	const int current[8] =
	{
		keys ? keys[SDL_SCANCODE_LEFTBRACKET] : 0,
		keys ? keys[SDL_SCANCODE_RIGHTBRACKET] : 0,
		keys ? keys[SDL_SCANCODE_SEMICOLON] : 0,
		keys ? keys[SDL_SCANCODE_APOSTROPHE] : 0,
		keys ? keys[SDL_SCANCODE_MINUS] : 0,
		keys ? keys[SDL_SCANCODE_EQUALS] : 0,
		keys ? keys[SDL_SCANCODE_0] : 0,
		keys ? keys[SDL_SCANCODE_9] : 0,
	};

	if (current[0] && !previous[0]) RotateSun(-0.12f);
	if (current[1] && !previous[1]) RotateSun(0.12f);
	if (current[2] && !previous[2]) { g_sunDir[1] -= 0.06f; if (g_sunDir[1] < 0.05f) g_sunDir[1] = 0.05f; }
	if (current[3] && !previous[3]) { g_sunDir[1] += 0.06f; if (g_sunDir[1] > 0.99f) g_sunDir[1] = 0.99f; }
	if (current[4] && !previous[4]) { g_exposure -= 0.1f; if (g_exposure < 0.2f) g_exposure = 0.2f; }
	if (current[5] && !previous[5]) { g_exposure += 0.1f; if (g_exposure > 4.0f) g_exposure = 4.0f; }
	if (current[6] && !previous[6]) g_shadows = !g_shadows;
	if (current[7] && !previous[7]) g_ao = !g_ao;

	for (int i = 0; i < 8; i++)
		previous[i] = current[i];
}

int CaptureToFile(const char* path)
{
	PsyXVkInfo info;
	PsyX_Vk_GetInfo(&info);
	const int width = info.width;
	const int height = info.height;
	if (width <= 0 || height <= 0)
		return 0;

	std::vector<unsigned char> rgba((size_t)width * height * 4);
	if (!PsyX_Vk_ReadbackRgba(rgba.data(), NULL, NULL))
		return 0;

	char bmpPath[512];
	MakeBmpPath(path, bmpPath, sizeof(bmpPath));

	SDL_Surface* surface = SDL_CreateRGBSurfaceFrom(rgba.data(), width, height, 32, width * 4,
		0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000);
	if (!surface)
		return 0;

	const int saved = SDL_SaveBMP(surface, bmpPath);
	SDL_FreeSurface(surface);

	VkLog("VkFixture: screenshot %s (%dx%d, %s)\n", bmpPath, width, height, saved == 0 ? "saved" : "write failed");
	return saved == 0 ? 1 : 0;
}

// Phase-1 acceptance run for the emulated PSX path (roadmap R7b): no scene, no
// ImGui, just the VRAM/LUT/shader pipeline checked against known pixels.
int RunPsxSelfTest(void)
{
	VkLog("VkFixture: psx self-test start\n");

	PsyXVkConfig config;
	memset(&config, 0, sizeof(config));
	config.width = 1280;
	config.height = 720;
	config.title = "REDRIVER2 - Vulkan PSX self-test";
	config.enableImGui = 0;

	if (!PsyX_Vk_Initialise(&config))
	{
		VkLog("VkFixture: Vulkan backend unavailable\n");
		return 1;
	}

	char report[1024];
	const int ok = PsyX_Vk_GameSelfTest(report, sizeof(report));
	VkLog("%s", report);
	VkLog("VkFixture: psx self-test %s\n", ok ? "PASS" : "FAIL");

	PsyX_Vk_Shutdown();
	return ok ? 0 : 1;
}

int RunFixture(int captureFrames, const char* capturePath, int enableImGui)
{
	VkLog("VkFixture: start (captureFrames=%d gui=%d)\n", captureFrames, enableImGui);

	PsyXVkConfig config;
	memset(&config, 0, sizeof(config));
	config.width = 1280;
	config.height = 720;
	config.title = "REDRIVER2 - Vulkan fixture (press ESC to quit)";
	config.enableImGui = enableImGui;

	VkLog("VkFixture: supported=%d\n", PsyX_Vk_IsSupported());

	if (!PsyX_Vk_Initialise(&config))
	{
		VkLog("VkFixture: Vulkan backend unavailable\n");
		VkLog("VkFixture: Vulkan backend unavailable\n");
		return 1;
	}

	BuildScene();

	PsyXVkInfo info;
	PsyX_Vk_GetInfo(&info);
	VkLog("VkFixture: device '%s' api=%d meshes=%d shadow=%d imgui=%d size=%dx%d\n",
		info.deviceName, info.vulkanApiVersion, info.meshCount, info.shadowMapSize,
		info.imguiActive, info.width, info.height);
	VkLog("VkFixture: %s, %d meshes, shadow map %d, ImGui %s\n",
		info.deviceName, info.meshCount, info.shadowMapSize, info.imguiActive ? "on" : "off");

	Uint32 previousTicks = SDL_GetTicks();
	int frames = 0;
	int captured = 0;

	for (;;)
	{
		const Uint32 now = SDL_GetTicks();
		float deltaSeconds = (float)(now - previousTicks) * 0.001f;
		if (deltaSeconds > 0.25f)
			deltaSeconds = 0.25f;
		previousTicks = now;

		int mouseDx = 0, mouseDy = 0;
		const Uint32 mouseButtons = SDL_GetRelativeMouseState(&mouseDx, &mouseDy);
		const int rightMouseDown = (mouseButtons & SDL_BUTTON_RMASK) != 0;

		const Uint8* keys = SDL_GetKeyboardState(NULL);

		UpdateCamera(deltaSeconds, keys, rightMouseDown, mouseDx, mouseDy);
		HandleToggles(keys);

		// Camera matrices for this frame.
		const float cy = cosf(g_cameraYaw);
		const float sy = sinf(g_cameraYaw);
		const float cp = cosf(g_cameraPitch);
		const float sp = sinf(g_cameraPitch);
		const float at[3] =
		{
			g_cameraPos[0] - sy * cp * 1000.0f,
			g_cameraPos[1] + sp * 1000.0f,
			g_cameraPos[2] - cy * cp * 1000.0f,
		};

		PsyX_Vk_GetInfo(&info);
		const float aspect = (info.height > 0) ? (float)info.width / (float)info.height : (1280.0f / 720.0f);

		float view[16];
		float proj[16];
		BuildLookAt(g_cameraPos, at, view);
		BuildPerspective(60.0f * kPi / 180.0f, aspect, 10.0f, 60000.0f, proj);
		PsyX_Vk_SetCamera(view, proj, g_cameraPos);

		// Light set: one directional sun plus ambient/exposure.
		PsyXModernLightSet lights;
		memset(&lights, 0, sizeof(lights));
		lights.count = 1;
		lights.lights[0].type = 0;
		lights.lights[0].direction[0] = g_sunDir[0];
		lights.lights[0].direction[1] = g_sunDir[1];
		lights.lights[0].direction[2] = g_sunDir[2];
		lights.lights[0].color[0] = lights.lights[0].color[1] = lights.lights[0].color[2] = 1.0f;
		lights.lights[0].intensity = 1.0f;
		lights.ambient[0] = lights.ambient[1] = lights.ambient[2] = 0.18f;
		lights.exposure = g_exposure;
		lights.shadowsEnabled = g_shadows;
		lights.aoEnabled = g_ao;
		lights.shadowCenter[0] = 0.0f;
		lights.shadowCenter[1] = 0.0f;
		lights.shadowCenter[2] = -800.0f;
		lights.shadowExtent = 4200.0f;
		PsyX_Vk_SetLights(&lights);

		const float white[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
		for (int i = 0; i < g_instanceCount; i++)
		{
			float world[16];
			BuildWorldMatrix(g_instances[i].pos, g_instances[i].yaw, world);
			PsyX_Vk_SetInstance(g_instances[i].mesh, world, white, 1);
		}

		char overlay[512];
		snprintf(overlay, sizeof(overlay),
			"Vulkan modern-fixture slice (R7)\n"
			"%d meshes, %d instances\n"
			"WASD move, R/F up/down, Shift faster, RMB look\n"
			"[ ] sun azimuth, ; ' elevation, - = exposure\n"
			"0 shadows %s, 9 AO %s, F1 screenshot, ESC quit",
			info.meshCount, g_instanceCount,
			g_shadows ? "on" : "off", g_ao ? "on" : "off");
		PsyX_Vk_SetOverlayText(overlay);

		if (!PsyX_Vk_RenderFrame())
			break;

		frames++;

		if (frames == 2 && captureFrames > 0)
		{
			// A couple of frames guarantee the swapchain has presented once.
			PsyX_Vk_GetInfo(&info);
			VkLog("VkFixture: capture frame: draws=%d size=%dx%d\n", info.drawCalls, info.width, info.height);

			captured = CaptureToFile(capturePath);
		}

		if (captureFrames > 0 && frames >= captureFrames)
			break;
	}

	VkLog("VkFixture: loop ended after %d frames, captured=%d\n", frames, captured);
	PsyX_Vk_Shutdown();
	return 0;
}
} // namespace

int DeveloperVkFixture_HandleCommandLine(int argc, char** argv)
{
	int requested = 0;
	int psxSelfTest = 0;
	int captureFrames = 0;
	const char* capturePath = "vk_fixture.png";
	int enableImGui = 1;

	for (int i = 1; i < argc; i++)
	{
		if (!strcmp(argv[i], "-vkfixture"))
		{
			requested = 1;
		}
		else if (!strcmp(argv[i], "-vkpsxtest"))
		{
			requested = 1;
			psxSelfTest = 1;
		}
		else if (!strcmp(argv[i], "-vkcapture") && i + 1 < argc)
		{
			requested = 1;
			captureFrames = atoi(argv[++i]);
			if (captureFrames < 2)
				captureFrames = 2;
		}
		else if (!strcmp(argv[i], "-vkshot") && i + 1 < argc)
		{
			capturePath = argv[++i];
		}
		else if (!strcmp(argv[i], "-vknogui"))
		{
			enableImGui = 0;
		}
	}

	if (!requested)
		return 0;

	if (psxSelfTest)
	{
		RunPsxSelfTest();
		return 1;
	}

	RunFixture(captureFrames, capturePath, enableImGui);
	return 1;
}

#else // unsupported platform

int DeveloperVkFixture_HandleCommandLine(int argc, char** argv)
{
	(void)argc;
	(void)argv;
	return 0;
}

#endif
