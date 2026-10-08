#include "DeveloperModernMesh.h"
#include "DeveloperWorldShadow.h"
#include "DeveloperStaticScene.h"

#include "driver2.h"

#include "C/camera.h"
#include "C/cars.h"
#include "C/draw.h"
#include "C/dr2roads.h"
#include "C/gamesnd.h"
#include "C/mission.h"
#include "C/players.h"

#include "GltfLoader.h"

#include "PsyX/PsyX_public.h"

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <vector>

#if !defined(PSX) && !defined(__ANDROID__) && !defined(__EMSCRIPTEN__)

#include <SDL.h>

namespace
{
const char* const kStateFilename = "developer_modern_mesh.ini";
const char* const kFixtureDir = "../../../assets/modern_fixtures/";
const float kHalfSize = 300.0f;		// world units; the analytic sphere radius
const float kAssetScale = 400.0f;	// game units per glTF unit (metre)
const float kAnchorDistance = 3000.0f;	// gallery centre ahead of the spawn

// Owned static fixtures (Meshy tasks; see assets/modern_fixtures/provenance.md).
// `lateral` and `forward` are offsets on the spawn right/forward axes, `yaw` is
// relative to the spawn heading. Each object is grounded on the terrain once.
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

const int kFixtureCapacity = 16;

struct FixtureTextures
{
	unsigned int base;
	unsigned int normal;
	unsigned int mr;
	unsigned int emissive;
};

struct Fixture
{
	int mesh;				// PsyX_ModernMesh handle, -1 when unused
	float pos[3];			// world-space model origin (grounded)
	float yaw;				// world yaw around Y
	FixtureTextures textures;
};

void AddFixture(int mesh, const float pos[3], float yaw, const FixtureTextures* textures);
float GroundHeightAt(float x, float z);

// Source scenery/cameras use Y down; glTF, the sun UI and vehicle physics
// use Y up. Convert at the Vulkan game boundary, leaving OpenGL unchanged.
float SceneUpSign()
{
	return PsyX_GetRenderBackend() == PSYX_BACKEND_VULKAN ? -1.0f : 1.0f;
}
void PlaceFromLayout(float lateral, float forward, const float anchor[3], float out[3]);

int s_enabled = 0;
int s_keyHeld = 0;
int s_positioned = 0;
Fixture s_fixtures[kFixtureCapacity];
int s_fixtureCount = 0;
int s_cubeMesh = -1;
float s_spawnForward[3] = { 0.0f, 0.0f, -1.0f };
float s_spawnRight[3] = { 1.0f, 0.0f, 0.0f };
float s_baseYaw = 0.0f;
float s_anchor[3] = { 0.0f, 0.0f, 0.0f };
int s_analytic = 0;
int s_shadowDebug = 0;
char s_assetPath[512] = "";
// Sun towards the light: the default keeps cast shadows falling towards the
// follow camera so the legacy receiver projection is visible by default.
float s_lightDir[3] = { -0.25f, 0.72f, -0.38f };
float s_lightIntensity = 1.0f;
float s_ambient = 0.18f;
float s_exposure = 1.0f;
int s_aoEnabled = 0;
int s_shadowsEnabled = 0;
int s_legacyShadowCasters = 0;
int s_worldShadowCasters = 0;
float s_shadowExtent = 2500.0f;
int s_farShadows = 0;
// Legacy-geometry light receptivity (roadmap legacy-lighting-receptivity):
// off by default so the shipped legacy shading is untouched. The scale is the
// diffuse-sun multiplier applied to the already-lit legacy colour.
int s_legacyLighting = 0;
float s_legacyLightReceptivity = 0.35f;
// Point-light sources (roadmap point-light-sources): published from live game
// state next to the sun. Off by default, like the receptivity term they feed.
// The cap counts published point lights only; the sun is always light 0 and is
// never counted, and the composite shades at most
// PSYX_MODERN_COMPOSITE_POINT_LIGHTS of them on both backends.
int s_pointLights = 0;
int s_pointLightLimit = 2;
// Range and intensity defaults are sized from the player car's own geometry:
// `Baseline: playerCar boxLength` logs the collision box (139x86x396 units for
// the sedan used in the captures), so ~4000 units is about a car length of
// useful range and the offsets below stay a fraction of the car without
// depending on a units-per-metre assumption.
float s_pointLightRange = 4000.0f;
float s_pointLightIntensity = 1.5f;
int s_headlights = 1;
int s_sirenLights = 1;
// Validation aid: one known light anchored on the player vehicle instead of the
// game-derived sources, so a capture shows exactly one published point light.
int s_pointLightProbe = 0;
// Point lights in the last published set, logged once for a capture's baseline.
int s_publishedPointLights = 0;

const int kMaxPointLightCandidates = 8;

// A distinct colour per corner keeps the cube's orientation readable.
const unsigned char kColors[8][4] =
{
	{ 230,  60,  60, 255 }, {  60, 200,  60, 255 },
	{  60,  60, 230, 255 }, { 230, 200,  60, 255 },
	{ 230,  60, 230, 255 }, {  60, 220, 220, 255 },
	{ 240, 240, 240, 255 }, {  40,  40,  40, 255 },
};

const float kPositions[8][3] =
{
	{ -kHalfSize, -kHalfSize, -kHalfSize }, { -kHalfSize, -kHalfSize,  kHalfSize },
	{ -kHalfSize,  kHalfSize, -kHalfSize }, { -kHalfSize,  kHalfSize,  kHalfSize },
	{  kHalfSize, -kHalfSize, -kHalfSize }, {  kHalfSize, -kHalfSize,  kHalfSize },
	{  kHalfSize,  kHalfSize, -kHalfSize }, {  kHalfSize,  kHalfSize,  kHalfSize },
};

const unsigned short kIndices[36] =
{
	0, 1, 3, 0, 3, 2,	// -X
	4, 6, 7, 4, 7, 5,	// +X
	0, 4, 5, 0, 5, 1,	// -Y
	2, 3, 7, 2, 7, 6,	// +Y
	0, 2, 6, 0, 6, 4,	// -Z
	1, 5, 7, 1, 7, 3,	// +Z
};

void ReadEnabledState()
{
	FILE* file = fopen(kStateFilename, "rb");
	if (!file)
		return;

	char line[600];
	while (fgets(line, sizeof(line), file))
	{
		int value = 0;
		float staticStrength = 0.0f;
		float staticRange = 0.0f;
		if (sscanf(line, " enabled=%d", &value) == 1 || sscanf(line, "enabled=%d", &value) == 1)
			s_enabled = value != 0;
		else if (sscanf(line, " asset=%511s", s_assetPath) == 1)
			;
		else if (sscanf(line, " lightdir=%f,%f,%f", &s_lightDir[0], &s_lightDir[1], &s_lightDir[2]) == 3)
			;
		else if (sscanf(line, " lightintensity=%f", &s_lightIntensity) == 1)
			;
		else if (sscanf(line, " ambient=%f", &s_ambient) == 1)
			;
		else if (sscanf(line, " exposure=%f", &s_exposure) == 1)
			;
		else if (sscanf(line, " analytic=%d", &value) == 1)
			s_analytic = value != 0;
		else if (sscanf(line, " ao=%d", &value) == 1 || sscanf(line, "ao=%d", &value) == 1)
			s_aoEnabled = value != 0;
		else if (sscanf(line, " shadows=%d", &value) == 1 || sscanf(line, "shadows=%d", &value) == 1)
			s_shadowsEnabled = value != 0;
		else if (sscanf(line, " staticunlit=%d", &value) == 1)
			DeveloperStaticScene_SetEnabled(value);
		else if (sscanf(line, " staticlighting=%d", &value) == 1)
			DeveloperStaticScene_SetLighting(value);
		else if (sscanf(line, " staticlightingstrength=%f", &staticStrength) == 1)
			DeveloperStaticScene_SetLightingStrength(staticStrength);
		else if (sscanf(line, " staticscenelimit=%d", &value) == 1)
			DeveloperStaticScene_SetObjectLimit(value);
		else if (sscanf(line, " staticrange=%f", &staticRange) == 1)
			DeveloperStaticScene_SetRange(staticRange);
		else if (sscanf(line, " worldshadowcasters=%d", &value) == 1)
			s_worldShadowCasters = value != 0;
		else if (sscanf(line, " legacyshadowcasters=%d", &value) == 1)
			s_legacyShadowCasters = value != 0;
		else if (sscanf(line, " farshadows=%d", &value) == 1)
			s_farShadows = value != 0;
		else if (sscanf(line, " shadowextent=%f", &s_shadowExtent) == 1)
			;
		else if (sscanf(line, " shadowdebug=%d", &value) == 1 || sscanf(line, "shadowdebug=%d", &value) == 1)
			s_shadowDebug = value;
		else if (sscanf(line, " legacylighting=%d", &value) == 1 || sscanf(line, "legacyLighting=%d", &value) == 1)
			s_legacyLighting = value != 0;
		else if (sscanf(line, " legacyLightReceptivity=%f", &s_legacyLightReceptivity) == 1)
		{
			if (s_legacyLightReceptivity < 0.0f) s_legacyLightReceptivity = 0.0f;
			if (s_legacyLightReceptivity > 2.0f) s_legacyLightReceptivity = 2.0f;
		}
		else if (sscanf(line, " pointlights=%d", &value) == 1 || sscanf(line, "pointlights=%d", &value) == 1)
			s_pointLights = value != 0;
		else if (sscanf(line, " pointlightlimit=%d", &value) == 1)
		{
			if (value < 1) value = 1;
			if (value > PSYX_MODERN_COMPOSITE_POINT_LIGHTS) value = PSYX_MODERN_COMPOSITE_POINT_LIGHTS;
			s_pointLightLimit = value;
		}
		else if (sscanf(line, " pointlightrange=%f", &s_pointLightRange) == 1)
		{
			if (s_pointLightRange < 100.0f) s_pointLightRange = 100.0f;
			if (s_pointLightRange > 8000.0f) s_pointLightRange = 8000.0f;
		}
		else if (sscanf(line, " pointlightintensity=%f", &s_pointLightIntensity) == 1)
		{
			if (s_pointLightIntensity < 0.0f) s_pointLightIntensity = 0.0f;
			if (s_pointLightIntensity > 4.0f) s_pointLightIntensity = 4.0f;
		}
		else if (sscanf(line, " headlights=%d", &value) == 1)
			s_headlights = value != 0;
		else if (sscanf(line, " sirenlights=%d", &value) == 1 || sscanf(line, "sirenlights=%d", &value) == 1)
			s_sirenLights = value != 0;
		else if (sscanf(line, " pointlightprobe=%d", &value) == 1)
			s_pointLightProbe = value != 0;
	}

	fclose(file);
}

void WriteEnabledState()
{
	FILE* file = fopen(kStateFilename, "wb");
	if (!file)
		return;

	fprintf(file, "# REDRIVER2-Plus experimental modern mesh (renderer roadmap R2/R3/R5).\n");
	fprintf(file, "# Toggle in game with F10; light keys: [ ] azimuth, ; ' tilt, - = exposure, \\ AO.\n");
	fprintf(file, "# F9 toggles legacy light receptivity; legacyLightReceptivity scales it.\n");
	fprintf(file, "# pointlights publishes headlight/siren point lights next to the sun;\n");
	fprintf(file, "# pointlightprobe replaces them with one known light on the player car.\n");
	fprintf(file, "[modern_mesh]\n");
	fprintf(file, "enabled=%d\n", s_enabled ? 1 : 0);
	fprintf(file, "asset=%s\n", s_assetPath);
	fprintf(file, "analytic=%d\n", s_analytic);
	fprintf(file, "lightdir=%.4f,%.4f,%.4f\n", s_lightDir[0], s_lightDir[1], s_lightDir[2]);
	fprintf(file, "lightintensity=%.3f\n", s_lightIntensity);
	fprintf(file, "ambient=%.3f\n", s_ambient);
	fprintf(file, "exposure=%.3f\n", s_exposure);
	fprintf(file, "ao=%d\n", s_aoEnabled);
	fprintf(file, "shadows=%d\n", s_shadowsEnabled);
	fprintf(file, "legacyshadowcasters=%d\n", s_legacyShadowCasters);
	fprintf(file, "worldshadowcasters=%d\n", s_worldShadowCasters);
	fprintf(file, "staticunlit=%d\n", DeveloperStaticScene_GetEnabled());
	fprintf(file, "staticlighting=%d\n", DeveloperStaticScene_GetLighting());
	fprintf(file, "staticlightingstrength=%.3f\n", DeveloperStaticScene_GetLightingStrength());
	fprintf(file, "staticscenelimit=%d\n", DeveloperStaticScene_GetObjectLimit());
	fprintf(file, "staticrange=%.1f\n", DeveloperStaticScene_GetRange());
	fprintf(file, "shadowextent=%.1f\n", s_shadowExtent);
	fprintf(file, "farshadows=%d\n", s_farShadows);
	fprintf(file, "shadowdebug=%d\n", s_shadowDebug);
	fprintf(file, "legacyLighting=%d\n", s_legacyLighting ? 1 : 0);
	fprintf(file, "legacyLightReceptivity=%.3f\n", s_legacyLightReceptivity);
	fprintf(file, "pointlights=%d\n", s_pointLights ? 1 : 0);
	fprintf(file, "pointlightlimit=%d\n", s_pointLightLimit);
	fprintf(file, "pointlightrange=%.1f\n", s_pointLightRange);
	fprintf(file, "pointlightintensity=%.3f\n", s_pointLightIntensity);
	fprintf(file, "headlights=%d\n", s_headlights ? 1 : 0);
	fprintf(file, "sirenlights=%d\n", s_sirenLights ? 1 : 0);
	fprintf(file, "pointlightprobe=%d\n", s_pointLightProbe ? 1 : 0);
	fclose(file);
}

// Analytic material fixtures: one UV sphere per metallic/roughness sample, so
// the PBR response can be checked against known values without any texture.
void AppendSphere(std::vector<float>& positions, std::vector<float>& normals,
	std::vector<float>& uvs, std::vector<unsigned short>& indices, float radius, int segments, int rings)
{
	const unsigned short base = (unsigned short)(positions.size() / 3);
	for (int r = 0; r <= rings; r++)
	{
		const float v = (float)r / (float)rings;
		const float theta = v * 3.14159265f;
		for (int s = 0; s <= segments; s++)
		{
			const float u = (float)s / (float)segments;
			const float phi = u * 2.0f * 3.14159265f;
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

void CreateAnalyticSpheres()
{
	// 2x2 grid: dielectric/metal across smooth/rough.
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
		AppendSphere(positions, normals, uvs, indices, kHalfSize, 32, 20);

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

		const int mesh = PsyX_ModernMesh_CreateEx(&desc);
		if (mesh < 0)
			continue;

		float pos[3];
		PlaceFromLayout((i - 1.5f) * kHalfSize * 3.0f, 3400.0f, s_anchor, pos);
		pos[1] = GroundHeightAt(pos[0], pos[2]) + SceneUpSign() * kHalfSize;	// sphere rests on the ground
		AddFixture(mesh, pos, s_baseYaw, NULL);
	}
}

void AddFixture(int mesh, const float pos[3], float yaw, const FixtureTextures* textures)
{
	if (mesh < 0 || s_fixtureCount >= kFixtureCapacity)
		return;

	Fixture* f = &s_fixtures[s_fixtureCount++];
	f->mesh = mesh;
	f->pos[0] = pos[0];
	f->pos[1] = pos[1];
	f->pos[2] = pos[2];
	f->yaw = yaw;
	if (textures)
		f->textures = *textures;
	else
		memset(&f->textures, 0, sizeof(f->textures));
}

float GroundHeightAt(float x, float z)
{
	VECTOR probe;
	probe.vx = (int)x;
	probe.vy = 0;
	probe.vz = (int)z;

	const int height = MapHeight(&probe);

	// The playground map query can miss; the camera height is a usable fallback
	// there, while a genuine surface height of zero is kept as-is.
	if (height == 0 && (camera_position.vy > 200 || camera_position.vy < -200) &&
		(PsyX_GetRenderBackend() != PSYX_BACKEND_VULKAN || !sdGetCell(&probe)))
		return (float)camera_position.vy;

	return (float)height;
}

void PlaceFromLayout(float lateral, float forward, const float anchor[3], float out[3])
{
	out[0] = anchor[0] + s_spawnRight[0] * lateral + s_spawnForward[0] * forward;
	out[1] = anchor[1];
	out[2] = anchor[2] + s_spawnRight[2] * lateral + s_spawnForward[2] * forward;
}

// Imports one owned glTF/GLB (roadmap R3), grounds its base on the terrain and
// registers it as a fixed world-space instance. Returns the mesh handle or -1.
int AddGltfFixture(const char* path, float lateral, float forward, float yawDegrees)
{
	GltfMeshData data;
	char error[256] = "";
	if (Gltf_LoadFile(path, &data, error, sizeof(error)) != 0)
	{
		printError("ModernMesh: cannot load %s: %s\n", path, error);
		return -1;
	}

	std::vector<float> positions(data.positions.size());
	float minY = 1e30f, maxY = -1e30f;
	float minX = 1e30f, maxX = -1e30f;
	float minZ = 1e30f, maxZ = -1e30f;
	for (size_t i = 0; i + 2 < positions.size(); i += 3)
	{
		const float x = data.positions[i + 0] * kAssetScale;
		const float y = data.positions[i + 1] * kAssetScale;
		const float z = data.positions[i + 2] * kAssetScale;
		positions[i + 0] = x;
		positions[i + 1] = y;
		positions[i + 2] = z;
		if (x < minX) minX = x;
		if (x > maxX) maxX = x;
		if (y < minY) minY = y;
		if (y > maxY) maxY = y;
		if (z < minZ) minZ = z;
		if (z > maxZ) maxZ = z;
	}

	std::vector<unsigned short> indices(data.indices.size());
	const int indexCount = (int)data.indices.size();
	for (size_t i = 0; i < data.indices.size(); i++)
	{
		if (data.indices[i] >= 65535)
		{
			printError("ModernMesh: %s has an index above 65534\n", path);
			return -1;
		}
		indices[i] = (unsigned short)data.indices[i];
	}

	FixtureTextures textures;
	memset(&textures, 0, sizeof(textures));
	if (!data.baseColorRGBA.empty())
		textures.base = PsyX_CreateRGBATexture(data.baseColorWidth, data.baseColorHeight, data.baseColorRGBA.data());
	if (!data.normalRGBA.empty())
		textures.normal = PsyX_CreateRGBATexture(data.normalWidth, data.normalHeight, data.normalRGBA.data());
	if (!data.metallicRoughnessRGBA.empty())
		textures.mr = PsyX_CreateRGBATexture(data.metallicRoughnessWidth, data.metallicRoughnessHeight, data.metallicRoughnessRGBA.data());
	if (!data.emissiveRGBA.empty())
		textures.emissive = PsyX_CreateRGBATexture(data.emissiveWidth, data.emissiveHeight, data.emissiveRGBA.data());

	PsyXModernMeshDesc desc;
	memset(&desc, 0, sizeof(desc));
	desc.positions = positions.data();
	desc.normals = data.normals.empty() ? NULL : data.normals.data();
	desc.uvs = data.uvs.empty() ? NULL : data.uvs.data();
	desc.vertexCount = data.vertexCount;
	desc.indices = indices.empty() ? NULL : indices.data();
	desc.indexCount = indexCount;
	desc.baseColorTexture = textures.base;
	desc.normalTexture = textures.normal;
	desc.metallicRoughnessTexture = textures.mr;
	desc.emissiveTexture = textures.emissive;
	desc.baseColorFactor = data.baseColorFactor;
	desc.emissiveFactor = data.emissiveFactor;
	desc.metallicFactor = data.metallicFactor;
	desc.roughnessFactor = data.roughnessFactor;

	const int mesh = PsyX_ModernMesh_CreateEx(&desc);
	if (mesh < 0)
	{
		if (textures.base) PsyX_DestroyRGBATexture(textures.base);
		if (textures.normal) PsyX_DestroyRGBATexture(textures.normal);
		if (textures.mr) PsyX_DestroyRGBATexture(textures.mr);
		if (textures.emissive) PsyX_DestroyRGBATexture(textures.emissive);
		return -1;
	}

	float pos[3];
	PlaceFromLayout(lateral, forward, s_anchor, pos);
	pos[1] = GroundHeightAt(pos[0], pos[2]) - SceneUpSign() * minY;	// base rests on the surface
	AddFixture(mesh, pos, s_baseYaw + yawDegrees * 3.14159265f / 180.0f, &textures);

	char materialId[192];
	Gltf_MakeMaterialId(path, data, materialId, sizeof(materialId));
	printInfo("ModernMesh: imported %s (%d verts, %d tris, %.0fx%.0fx%.0f units, base=%dx%d normal=%dx%d mr=%dx%d, material %s)\n",
		path, data.vertexCount, data.triangleCount,
		maxX - minX, maxY - minY, maxZ - minZ,
		data.baseColorWidth, data.baseColorHeight,
		data.normalWidth, data.normalHeight,
		data.metallicRoughnessWidth, data.metallicRoughnessHeight, materialId);

	return mesh;
}

void CreateCubeFallback()
{
	float positions[8 * 3];
	unsigned char colors[8 * 4];

	for (int i = 0; i < 8; i++)
	{
		positions[i * 3 + 0] = kPositions[i][0];
		positions[i * 3 + 1] = kPositions[i][1];
		positions[i * 3 + 2] = kPositions[i][2];
		memcpy(&colors[i * 4], kColors[i], 4);
	}

	s_cubeMesh = PsyX_ModernMesh_Create(positions, 8, kIndices, 36, colors);
	if (s_cubeMesh < 0)
		return;

	float pos[3];
	PlaceFromLayout(0.0f, 1600.0f, s_anchor, pos);
	pos[1] = GroundHeightAt(pos[0], pos[2]) + SceneUpSign() * kHalfSize;
	AddFixture(s_cubeMesh, pos, s_baseYaw, NULL);
}

void BuildGallery()
{
	memset(s_fixtures, 0, sizeof(s_fixtures));
	for (int i = 0; i < kFixtureCapacity; i++)
		s_fixtures[i].mesh = -1;
	s_fixtureCount = 0;

	char path[640];
	for (int i = 0; i < kFixtureDefCount; i++)
	{
		const FixtureDef& def = kFixtureDefs[i];
		snprintf(path, sizeof(path), "%s%s", kFixtureDir, def.file);
		AddGltfFixture(path, def.lateral, def.forward, def.yawDegrees);
	}

	// Optional extra import from the ini (asset=<path>), for testing new models
	// alongside the owned gallery.
	if (s_assetPath[0] != '\0')
		AddGltfFixture(s_assetPath, 0.0f, 2200.0f, 180.0f);

	if (s_analytic)
		CreateAnalyticSpheres();

	if (s_fixtureCount == 0)
		CreateCubeFallback();
}

void CameraRotation(float view[16])
{
	const float scale = 1.0f / 4096.0f;
	view[0] = inv_camera_matrix.m[0][0] * scale; view[4] = inv_camera_matrix.m[0][1] * scale; view[8] = inv_camera_matrix.m[0][2] * scale;  view[12] = 0.0f;
	view[1] = inv_camera_matrix.m[1][0] * scale; view[5] = inv_camera_matrix.m[1][1] * scale; view[9] = inv_camera_matrix.m[1][2] * scale;  view[13] = 0.0f;
	view[2] = inv_camera_matrix.m[2][0] * scale; view[6] = inv_camera_matrix.m[2][1] * scale; view[10] = inv_camera_matrix.m[2][2] * scale; view[14] = 0.0f;
	view[3] = 0.0f; view[7] = 0.0f; view[11] = 0.0f; view[15] = 1.0f;
}

// Mesh local space to world space: yaw around Y then translate. The shadow map
// pass needs this so casters are rendered where they stand.
void BuildInstanceWorld(const float pos[3], float yaw, float world[16])
{
	const float cy = cosf(yaw);
	const float sy = sinf(yaw);
	world[0] = cy;  world[4] = 0.0f; world[8] = sy;   world[12] = pos[0];
	world[1] = 0.0f; world[5] = SceneUpSign(); world[9] = 0.0f; world[13] = pos[1];
	world[2] = -sy; world[6] = 0.0f; world[10] = cy;  world[14] = pos[2];
	world[3] = 0.0f; world[7] = 0.0f; world[11] = 0.0f; world[15] = 1.0f;
}

void BuildInstanceView(const float pos[3], float yaw, float view[16])
{
	const float scale = 1.0f / 4096.0f;
	const float r[3][3] =
	{
		{ inv_camera_matrix.m[0][0] * scale, inv_camera_matrix.m[0][1] * scale, inv_camera_matrix.m[0][2] * scale },
		{ inv_camera_matrix.m[1][0] * scale, inv_camera_matrix.m[1][1] * scale, inv_camera_matrix.m[1][2] * scale },
		{ inv_camera_matrix.m[2][0] * scale, inv_camera_matrix.m[2][1] * scale, inv_camera_matrix.m[2][2] * scale },
	};

	// Object yaw around Y, composed with the camera rotation.
	const float cy = cosf(yaw);
	const float sy = sinf(yaw);
	const float o[3][3] =
	{
		{  cy, 0.0f, sy },
		{ 0.0f, 1.0f, 0.0f },
		{ -sy, 0.0f, cy },
	};

	float m[3][3];
	for (int i = 0; i < 3; i++)
		for (int j = 0; j < 3; j++)
			m[i][j] = r[i][0] * o[0][j] + r[i][1] * o[1][j] + r[i][2] * o[2][j];

	const float dx = pos[0] - (float)camera_position.vx;
	const float dy = pos[1] - (float)camera_position.vy;
	const float dz = pos[2] - (float)camera_position.vz;

	const float tx = r[0][0] * dx + r[0][1] * dy + r[0][2] * dz;
	const float ty = r[1][0] * dx + r[1][1] * dy + r[1][2] * dz;
	const float tz = r[2][0] * dx + r[2][1] * dy + r[2][2] * dz;

	view[0] = m[0][0]; view[4] = m[0][1]; view[8] = m[0][2];  view[12] = tx;
	view[1] = m[1][0]; view[5] = m[1][1]; view[9] = m[1][2];  view[13] = ty;
	view[2] = m[2][0]; view[6] = m[2][1]; view[10] = m[2][2]; view[14] = tz;
	view[3] = 0.0f;    view[7] = 0.0f;    view[11] = 0.0f;    view[15] = 1.0f;
}

// Anchors the gallery once, in world space: the spawn pose fixes the layout
// basis so nothing follows the camera afterwards.
void PositionGallery()
{
	if (MainPlayer.playerCarId >= 0 && car_data[MainPlayer.playerCarId].hd.where.t != NULL)
	{
		CAR_DATA* cp = &car_data[MainPlayer.playerCarId];
		const int dir = cp->hd.direction & 0xFFF;
		const float fx = (float)RSIN(dir) / 4096.0f;
		const float fz = (float)RCOS(dir) / 4096.0f;
		VECTOR* p = (VECTOR*)cp->hd.where.t;

		s_spawnForward[0] = fx;
		s_spawnForward[1] = 0.0f;
		s_spawnForward[2] = fz;
		s_spawnRight[0] = fz;
		s_spawnRight[1] = 0.0f;
		s_spawnRight[2] = -fx;
		s_baseYaw = atan2f(fx, fz) + 3.14159265f;	// face back towards the spawn

		s_anchor[0] = (float)p->vx + fx * kAnchorDistance;
		s_anchor[1] = (float)p->vy;
		s_anchor[2] = (float)p->vz + fz * kAnchorDistance;
	}
	else
	{
		const float scale = 1.0f / 4096.0f;
		const float fx = inv_camera_matrix.m[0][2] * scale;
		const float fz = inv_camera_matrix.m[2][2] * scale;

		s_spawnForward[0] = fx;
		s_spawnForward[1] = 0.0f;
		s_spawnForward[2] = fz;
		s_spawnRight[0] = fz;
		s_spawnRight[1] = 0.0f;
		s_spawnRight[2] = -fx;
		s_baseYaw = atan2f(fx, fz) + 3.14159265f;

		s_anchor[0] = camera_position.vx + fx * kAnchorDistance;
		s_anchor[1] = camera_position.vy;
		s_anchor[2] = camera_position.vz + fz * kAnchorDistance;
	}

	s_anchor[1] = GroundHeightAt(s_anchor[0], s_anchor[2]);
	s_positioned = 1;

	BuildGallery();
}
} // namespace

void RotateSun(float angle)
{
	const float c = cosf(angle);
	const float s = sinf(angle);
	const float x = s_lightDir[0];
	const float z = s_lightDir[2];
	s_lightDir[0] = x * c - z * s;
	s_lightDir[2] = x * s + z * c;
}

void TiltSun(float delta)
{
	s_lightDir[1] += delta;
	if (s_lightDir[1] < 0.05f) s_lightDir[1] = 0.05f;
	if (s_lightDir[1] > 0.99f) s_lightDir[1] = 0.99f;
}

/* A published point-light candidate. The list is kept in ascending distance
   from the shadow centre, so a small cap always keeps the nearest lights. */
struct PointLightCandidate
{
	float position[3];
	float color[3];
	float range;
	float distanceSq;
};

int PointLightLimit()
{
	if (s_pointLightLimit < 1)
		return 1;
	if (s_pointLightLimit > PSYX_MODERN_COMPOSITE_POINT_LIGHTS)
		return PSYX_MODERN_COMPOSITE_POINT_LIGHTS;
	return s_pointLightLimit;
}

void InsertPointLight(PointLightCandidate* candidates, int& count, const float position[3],
	const float color[3], float range, const float centre[3])
{
	const float dx = position[0] - centre[0];
	const float sceneY = SceneUpSign() * position[1];
	const float dy = sceneY - centre[1];
	const float dz = position[2] - centre[2];
	const float distanceSq = dx * dx + dy * dy + dz * dz;

	int at;
	if (count < kMaxPointLightCandidates)
	{
		at = count++;
	}
	else
	{
		at = kMaxPointLightCandidates - 1;
		if (distanceSq >= candidates[at].distanceSq)
			return;
	}

	while (at > 0 && candidates[at - 1].distanceSq > distanceSq)
	{
		candidates[at] = candidates[at - 1];
		at--;
	}

	candidates[at].position[0] = position[0];
	candidates[at].position[1] = sceneY;
	candidates[at].position[2] = position[2];
	candidates[at].color[0] = color[0];
	candidates[at].color[1] = color[1];
	candidates[at].color[2] = color[2];
	candidates[at].range = range;
	candidates[at].distanceSq = distanceSq;
}

/* The car's collision box, which is the only in-game length reference the
   modern path has. Its `length` fields are full extents in world units, so the
   light offsets below are expressed as fractions of the car instead of
   hard-coded distances that would sit inside or behind it. */
void CarBoxExtents(const CAR_DATA* cp, float out[3])
{
	const float fallback[3] = { 150.0f, 90.0f, 400.0f };
	for (int i = 0; i < 3; i++)
	{
		const float extent = (float)cp->hd.oBox.length[i];
		out[i] = extent > 1.0f ? extent : fallback[i];
	}
}

/* The two headlights of the player car, level with the road just outside the
   front corners, so the road and whatever the car faces catch the light. The
   car's forward/right basis comes from its heading exactly like
   PositionGallery. */
void AppendHeadlightCandidates(PointLightCandidate* candidates, int& count, const CAR_DATA* cp, const float centre[3])
{
	if (cp->hd.where.t == NULL)
		return;

	const VECTOR* p = (const VECTOR*)cp->hd.where.t;
	const float pos[3] = { (float)p->vx, (float)p->vy, (float)p->vz };
	const int dir = cp->hd.direction & 0xFFF;
	const float forwardX = (float)RSIN(dir) / 4096.0f;
	const float forwardZ = (float)RCOS(dir) / 4096.0f;
	const float rightX = forwardZ;
	const float rightZ = -forwardX;
	const float color[3] = { 1.0f, 0.92f, 0.72f };

	float box[3];
	CarBoxExtents(cp, box);
	const float forwardOffset = box[2];
	const float lateralOffset = box[0] * 0.8f;
	const float height = box[1] * 0.9f;

	for (int side = -1; side <= 1; side += 2)
	{
		const float light[3] =
		{
			pos[0] + forwardX * forwardOffset + rightX * lateralOffset * (float)side,
			pos[1] + height,
			pos[2] + forwardZ * forwardOffset + rightZ * lateralOffset * (float)side,
		};
		InsertPointLight(candidates, count, light, color, s_pointLightRange, centre);
	}
}

/* The light bar of every active police car with a siren, nearest first. The
   colour follows the phase the game already uses for the drawn flash, so the
   two agree instead of flashing out of step. */
void AppendSirenCandidates(PointLightCandidate* candidates, int& count, const float centre[3])
{
	for (int i = 0; i < MAX_CARS; i++)
	{
		const CAR_DATA* cp = &car_data[i];
		if (cp->controlType == CONTROL_TYPE_NONE || cp->hd.where.t == NULL)
			continue;
		if (cp->controlType != CONTROL_TYPE_PURSUER_AI && (cp->controlFlags & CONTROL_FLAG_COP) == 0)
			continue;
		if (CarHasSiren(cp->ap.model) == 0)
			continue;

		const VECTOR* p = (const VECTOR*)cp->hd.where.t;
		float box[3];
		CarBoxExtents(cp, box);
		const float position[3] = { (float)p->vx, (float)p->vy + box[1] * 1.3f, (float)p->vz };
		const int red = ((unsigned char)cp->ap.coplife >> 6) & 1;
		const float color[3] = { red ? 1.0f : 0.15f, red ? 0.16f : 0.25f, red ? 0.10f : 1.0f };
		InsertPointLight(candidates, count, position, color, s_pointLightRange * 0.75f, centre);
	}
}

/* The validation probe: one known light on the player vehicle, in front of and
   above it, so an acceptance capture has exactly one published point light and
   no dependence on where the traffic happens to be. */
void AppendProbeCandidate(PointLightCandidate* candidates, int& count, const float centre[3])
{
	const CAR_DATA* cp = (MainPlayer.playerCarId >= 0) ? &car_data[MainPlayer.playerCarId] : NULL;
	const float color[3] = { 1.0f, 0.85f, 0.55f };

	float position[3] = { centre[0], SceneUpSign() * centre[1] + 300.0f, centre[2] };
	if (cp && cp->hd.where.t != NULL)
	{
		const VECTOR* p = (const VECTOR*)cp->hd.where.t;
		const int dir = cp->hd.direction & 0xFFF;
		const float forwardX = (float)RSIN(dir) / 4096.0f;
		const float forwardZ = (float)RCOS(dir) / 4096.0f;
		float box[3];
		CarBoxExtents(cp, box);
		position[0] = (float)p->vx + forwardX * box[2] * 1.6f;
		position[1] = (float)p->vy + box[1] * 1.6f;
		position[2] = (float)p->vz + forwardZ * box[2] * 1.6f;
	}

	InsertPointLight(candidates, count, position, color, s_pointLightRange, centre);
}

void ApplyLightSet(float* shadowCentre = NULL, float farExtent = 0.0f)
{
	PsyXModernLightSet set;
	memset(&set, 0, sizeof(set));
	set.ambient[0] = set.ambient[1] = set.ambient[2] = s_ambient;
	set.exposure = s_exposure;
	set.aoEnabled = s_aoEnabled;
	set.shadowsEnabled = s_shadowsEnabled;
	set.legacyShadowCastersEnabled = s_legacyShadowCasters && !(s_worldShadowCasters && NumPlayers == 1 &&
		PsyX_GetRenderBackend() == PSYX_BACKEND_VULKAN);
	set.shadowExtent = s_shadowExtent;
	set.shadowFarExtent = farExtent;
	set.legacyLightingScale = s_legacyLighting ? s_legacyLightReceptivity : 0.0f;
	if (s_positioned)
	{
		// The shadow volume and the legacy lighting term are centred on the
		// player, not on the spawn: the map is only useful where the player is,
		// and the old fixed anchor left a lit/shadowed bubble behind as soon as
		// the car drove away from it. The gallery fixtures stay fixed.
		const VECTOR* playerPosition = NULL;
		if (MainPlayer.playerCarId >= 0)
			playerPosition = (const VECTOR*)car_data[MainPlayer.playerCarId].hd.where.t;

		// The car's position storage is briefly absent while a level, mission or
		// replay re-initialises; the camera is always valid, so fall back to it
		// instead of dereferencing a null pointer once per frame.
		if (playerPosition)
		{
			set.shadowCenter[0] = (float)playerPosition->vx;
			set.shadowCenter[1] = SceneUpSign() * (float)playerPosition->vy;
			set.shadowCenter[2] = (float)playerPosition->vz;
		}
		else
		{
			set.shadowCenter[0] = (float)camera_position.vx;
			set.shadowCenter[1] = (float)camera_position.vy;
			set.shadowCenter[2] = (float)camera_position.vz;
		}
	}

	// Light 0 is always the directional sun; the point lights below are appended
	// after it, so the composite's light 0 handling stays the sun.
	const float len = sqrtf(s_lightDir[0] * s_lightDir[0] + s_lightDir[1] * s_lightDir[1] + s_lightDir[2] * s_lightDir[2]);
	const float inv = len > 1e-5f ? 1.0f / len : 1.0f;
	set.count = 1;
	set.lights[0].type = 0;
	set.lights[0].direction[0] = s_lightDir[0] * inv;
	set.lights[0].direction[1] = SceneUpSign() * s_lightDir[1] * inv;
	set.lights[0].direction[2] = s_lightDir[2] * inv;
	set.lights[0].color[0] = set.lights[0].color[1] = set.lights[0].color[2] = 1.0f;
	set.lights[0].intensity = s_lightIntensity;

	if (s_pointLights)
	{
		PointLightCandidate candidates[kMaxPointLightCandidates];
		memset(candidates, 0, sizeof(candidates));
		int candidateCount = 0;

		if (s_pointLightProbe)
		{
			AppendProbeCandidate(candidates, candidateCount, set.shadowCenter);
		}
		else
		{
			if (s_headlights && MainPlayer.playerCarId >= 0)
				AppendHeadlightCandidates(candidates, candidateCount, &car_data[MainPlayer.playerCarId], set.shadowCenter);
			if (s_sirenLights)
				AppendSirenCandidates(candidates, candidateCount, set.shadowCenter);
		}

		const int limit = PointLightLimit();
		for (int i = 0; i < candidateCount && i < limit; i++)
		{
			const PointLightCandidate& candidate = candidates[i];
			PsyXModernLight* light = &set.lights[1 + i];
			light->type = 1;
			light->position[0] = candidate.position[0];
			light->position[1] = candidate.position[1];
			light->position[2] = candidate.position[2];
			light->color[0] = candidate.color[0];
			light->color[1] = candidate.color[1];
			light->color[2] = candidate.color[2];
			light->intensity = s_pointLightIntensity;
			light->range = candidate.range;
			set.count++;
		}

		s_publishedPointLights = set.count - 1;
	}
	else
	{
		s_publishedPointLights = 0;
	}

	PsyX_ModernMesh_SetLights(&set);
	if (shadowCentre)
		memcpy(shadowCentre, set.shadowCenter, 3 * sizeof(float));
}

void DeveloperModernMesh_Initialise(void)
{
	ReadEnabledState();
	ApplyLightSet();
	PsyX_ModernMesh_SetShadowDebug(s_shadowDebug);
	PsyX_ModernMesh_SetEnabled(0);
}

void DeveloperModernMesh_Shutdown(void)
{
	DeveloperStaticScene_Shutdown();
	DeveloperWorldShadow_Clear();
	for (int i = 0; i < s_fixtureCount; i++)
	{
		Fixture* f = &s_fixtures[i];
		if (f->mesh >= 0)
		{
			PsyX_ModernMesh_Destroy(f->mesh);
			f->mesh = -1;
		}
		if (f->textures.base) PsyX_DestroyRGBATexture(f->textures.base);
		if (f->textures.normal) PsyX_DestroyRGBATexture(f->textures.normal);
		if (f->textures.mr) PsyX_DestroyRGBATexture(f->textures.mr);
		if (f->textures.emissive) PsyX_DestroyRGBATexture(f->textures.emissive);
		memset(&f->textures, 0, sizeof(f->textures));
	}
	s_fixtureCount = 0;
	s_cubeMesh = -1;
	s_positioned = 0;
	PsyX_ModernMesh_SetEnabled(0);
}

void DeveloperModernMesh_SetEnabled(int enabled)
{
	if (s_enabled == (enabled != 0))
		return;

	s_enabled = enabled != 0;
	WriteEnabledState();
}

int DeveloperModernMesh_GetEnabled(void)
{
	return s_enabled;
}

void DeveloperModernMesh_SetShadows(int enabled)
{
	s_shadowsEnabled = enabled != 0;
	ApplyLightSet();
}

int DeveloperModernMesh_GetShadows(void)
{
	return s_shadowsEnabled;
}

void DeveloperModernMesh_SetStaticUnlit(int enabled)
{
	DeveloperStaticScene_SetEnabled(enabled);
	WriteEnabledState();
}

void DeveloperModernMesh_SetStaticLighting(int enabled)
{
	DeveloperStaticScene_SetLighting(enabled);
	WriteEnabledState();
}

void DeveloperModernMesh_SetStaticLightingStrength(float strength)
{
	DeveloperStaticScene_SetLightingStrength(strength);
	WriteEnabledState();
}

void DeveloperModernMesh_SetStaticSceneLimit(int count)
{
	DeveloperStaticScene_SetObjectLimit(count);
	WriteEnabledState();
}

void DeveloperModernMesh_SetStaticRange(float range)
{
	DeveloperStaticScene_SetRange(range);
	WriteEnabledState();
}

void DeveloperModernMesh_SetWorldShadowCasters(int enabled)
{
	s_worldShadowCasters = enabled != 0;
	DeveloperWorldShadow_Clear();
	ApplyLightSet();
	WriteEnabledState();
}

int DeveloperModernMesh_GetWorldShadowCasters(void)
{
	return s_worldShadowCasters;
}

void DeveloperModernMesh_SetLegacyShadowCasters(int enabled)
{
	s_legacyShadowCasters = enabled != 0;
	WriteEnabledState();
	ApplyLightSet();
}

int DeveloperModernMesh_GetLegacyShadowCasters(void)
{
	return s_legacyShadowCasters;
}

void DeveloperModernMesh_SetAmbientOcclusion(int enabled)
{
	s_aoEnabled = enabled != 0;
	ApplyLightSet();
}

int DeveloperModernMesh_GetAmbientOcclusion(void)
{
	return s_aoEnabled;
}

void DeveloperModernMesh_SetLegacyLighting(int enabled)
{
	s_legacyLighting = enabled != 0;
	WriteEnabledState();
	ApplyLightSet();
}

int DeveloperModernMesh_GetLegacyLighting(void)
{
	return s_legacyLighting;
}

void DeveloperModernMesh_SetLegacyLightReceptivity(float scale)
{
	if (scale < 0.0f) scale = 0.0f;
	if (scale > 2.0f) scale = 2.0f;
	s_legacyLightReceptivity = scale;
	WriteEnabledState();
	ApplyLightSet();
}

float DeveloperModernMesh_GetLegacyLightReceptivity(void)
{
	return s_legacyLightReceptivity;
}

void DeveloperModernMesh_SetPointLights(int enabled)
{
	s_pointLights = enabled != 0;
	WriteEnabledState();
	ApplyLightSet();
}

int DeveloperModernMesh_GetPointLights(void)
{
	return s_pointLights;
}

void DeveloperModernMesh_SetPointLightLimit(int limit)
{
	if (limit < 1) limit = 1;
	if (limit > PSYX_MODERN_COMPOSITE_POINT_LIGHTS) limit = PSYX_MODERN_COMPOSITE_POINT_LIGHTS;
	s_pointLightLimit = limit;
	WriteEnabledState();
	ApplyLightSet();
}

int DeveloperModernMesh_GetPointLightLimit(void)
{
	return PointLightLimit();
}

void DeveloperModernMesh_SetPointLightRange(float range)
{
	if (range < 100.0f) range = 100.0f;
	if (range > 8000.0f) range = 8000.0f;
	s_pointLightRange = range;
	WriteEnabledState();
	ApplyLightSet();
}

float DeveloperModernMesh_GetPointLightRange(void)
{
	return s_pointLightRange;
}

void DeveloperModernMesh_SetPointLightIntensity(float intensity)
{
	if (intensity < 0.0f) intensity = 0.0f;
	if (intensity > 4.0f) intensity = 4.0f;
	s_pointLightIntensity = intensity;
	WriteEnabledState();
	ApplyLightSet();
}

float DeveloperModernMesh_GetPointLightIntensity(void)
{
	return s_pointLightIntensity;
}

void DeveloperModernMesh_SetHeadlights(int enabled)
{
	s_headlights = enabled != 0;
	WriteEnabledState();
	ApplyLightSet();
}

int DeveloperModernMesh_GetHeadlights(void)
{
	return s_headlights;
}

void DeveloperModernMesh_SetSirenLights(int enabled)
{
	s_sirenLights = enabled != 0;
	WriteEnabledState();
	ApplyLightSet();
}

int DeveloperModernMesh_GetSirenLights(void)
{
	return s_sirenLights;
}

void DeveloperModernMesh_SetPointLightProbe(int enabled)
{
	s_pointLightProbe = enabled != 0;
	WriteEnabledState();
	ApplyLightSet();
}

int DeveloperModernMesh_GetPointLightProbe(void)
{
	return s_pointLightProbe;
}

void DeveloperModernMesh_GetSunAngles(float* azimuthDegrees, float* elevationDegrees)
{
	const float length = sqrtf(s_lightDir[0] * s_lightDir[0] + s_lightDir[1] * s_lightDir[1] + s_lightDir[2] * s_lightDir[2]);
	const float x = length > 1e-5f ? s_lightDir[0] / length : 0.0f;
	float y = length > 1e-5f ? s_lightDir[1] / length : 1.0f;
	const float z = length > 1e-5f ? s_lightDir[2] / length : 0.0f;

	if (azimuthDegrees)
	{
		float azimuth = atan2f(x, z) * 57.2957795f;
		if (azimuth < 0.0f)
			azimuth += 360.0f;
		*azimuthDegrees = azimuth;
	}

	if (elevationDegrees)
	{
		if (y < -1.0f) y = -1.0f;
		if (y > 1.0f) y = 1.0f;
		*elevationDegrees = asinf(y) * 57.2957795f;
	}
}

void DeveloperModernMesh_SetSunAngles(float azimuthDegrees, float elevationDegrees)
{
	const float azimuth = azimuthDegrees * 0.0174532925f;
	const float elevation = elevationDegrees * 0.0174532925f;
	const float cosElevation = cosf(elevation);

	// Sun towards the light, matching the F-key controls' convention: y is the
	// elevation, x/z the compass direction the light travels from.
	s_lightDir[0] = cosElevation * sinf(azimuth);
	s_lightDir[1] = sinf(elevation);
	s_lightDir[2] = cosElevation * cosf(azimuth);

	WriteEnabledState();
	ApplyLightSet();
}

float DeveloperModernMesh_GetLightIntensity(void)
{
	return s_lightIntensity;
}

void DeveloperModernMesh_SetLightIntensity(float intensity)
{
	if (intensity < 0.0f) intensity = 0.0f;
	if (intensity > 4.0f) intensity = 4.0f;
	s_lightIntensity = intensity;
	WriteEnabledState();
	ApplyLightSet();
}

float DeveloperModernMesh_GetAmbient(void)
{
	return s_ambient;
}

void DeveloperModernMesh_SetAmbient(float ambient)
{
	if (ambient < 0.0f) ambient = 0.0f;
	if (ambient > 1.0f) ambient = 1.0f;
	s_ambient = ambient;
	WriteEnabledState();
	ApplyLightSet();
}

float DeveloperModernMesh_GetExposure(void)
{
	return s_exposure;
}

void DeveloperModernMesh_SetExposure(float exposure)
{
	if (exposure < 0.1f) exposure = 0.1f;
	if (exposure > 4.0f) exposure = 4.0f;
	s_exposure = exposure;
	WriteEnabledState();
	ApplyLightSet();
}

int DeveloperModernMesh_GetFarShadows(void) { return s_farShadows; }
void DeveloperModernMesh_SetFarShadows(int enabled)
{
	s_farShadows = enabled != 0;
	WriteEnabledState();
}

float DeveloperModernMesh_GetShadowExtent(void)
{
	return s_shadowExtent;
}

void DeveloperModernMesh_SetShadowExtent(float extent)
{
	if (extent < 200.0f) extent = 200.0f;
	if (extent > 20000.0f) extent = 20000.0f;
	s_shadowExtent = extent;
	WriteEnabledState();
	ApplyLightSet();
}

int DeveloperModernMesh_GetShadowDebug(void)
{
	return s_shadowDebug;
}

void DeveloperModernMesh_SetShadowDebugMode(int mode)
{
	if (mode < 0) mode = 0;
	if (mode > 10) mode = 10;
	s_shadowDebug = mode;
	PsyX_ModernMesh_SetShadowDebug(s_shadowDebug);
	WriteEnabledState();
}

void DeveloperModernMesh_ProcessInput(void)
{
	// Edge-detected F10 toggle; avoids synthetic click injection entirely.
	const Uint8* keys = SDL_GetKeyboardState(NULL);
	if (keys && keys[SDL_SCANCODE_F10])
	{
		if (!s_keyHeld)
		{
			s_keyHeld = 1;
			DeveloperModernMesh_SetEnabled(!s_enabled);
			printInfo("ModernMesh: %s\n", s_enabled ? "enabled" : "disabled");
		}
	}
	else
	{
		s_keyHeld = 0;
	}

	// F9 toggles legacy light receptivity. A function key is used deliberately:
	// character keys are not reliably injectable for A/B captures.
	{
		static int legacyKeyHeld = 0;
		if (keys && keys[SDL_SCANCODE_F9])
		{
			if (!legacyKeyHeld)
			{
				legacyKeyHeld = 1;
				DeveloperModernMesh_SetLegacyLighting(!s_legacyLighting);
				printInfo("ModernMesh: legacy light receptivity %s (scale %.2f)\n",
					s_legacyLighting ? "enabled" : "disabled", s_legacyLightReceptivity);
			}
		}
		else
		{
			legacyKeyHeld = 0;
		}
	}

	// Real-time light controls (R5): rotate/tilt the sun, exposure, AO.
	{
		static int pLB = 0, pRB = 0, pSC = 0, pAP = 0, pMI = 0, pEQ = 0, pS0 = 0, pS9 = 0, pS7 = 0;
		const int cLB = keys ? keys[SDL_SCANCODE_LEFTBRACKET] : 0;
		const int cRB = keys ? keys[SDL_SCANCODE_RIGHTBRACKET] : 0;
		const int cSC = keys ? keys[SDL_SCANCODE_SEMICOLON] : 0;
		const int cAP = keys ? keys[SDL_SCANCODE_APOSTROPHE] : 0;
		const int cMI = keys ? keys[SDL_SCANCODE_MINUS] : 0;
		const int cEQ = keys ? keys[SDL_SCANCODE_EQUALS] : 0;
		const int cS0 = keys ? keys[SDL_SCANCODE_0] : 0;
		const int cS9 = keys ? keys[SDL_SCANCODE_9] : 0;
		const int cS7 = keys ? keys[SDL_SCANCODE_7] : 0;
		int changed = 0;

		if (cLB && !pLB) { RotateSun(-0.12f); changed = 1; }
		if (cRB && !pRB) { RotateSun(0.12f); changed = 1; }
		if (cSC && !pSC) { TiltSun(-0.08f); changed = 1; }
		if (cAP && !pAP) { TiltSun(0.08f); changed = 1; }
		if (cMI && !pMI) { s_exposure = s_exposure > 0.2f ? s_exposure - 0.1f : s_exposure; changed = 1; }
		if (cEQ && !pEQ) { s_exposure = s_exposure < 4.0f ? s_exposure + 0.1f : s_exposure; changed = 1; }
		if (cS0 && !pS0) { s_shadowsEnabled = !s_shadowsEnabled; changed = 1; }
		if (cS9 && !pS9) { s_aoEnabled = !s_aoEnabled; changed = 1; }
		if (cS7 && !pS7)
		{
			// Diagnostics for the legacy shadow projection: 0 normal,
			// 1 scene depth, 2 shadow-volume membership.
			s_shadowDebug = (s_shadowDebug + 1) % 5;
			PsyX_ModernMesh_SetShadowDebug(s_shadowDebug);
			printInfo("ModernMesh: shadow debug mode %d\n", s_shadowDebug);
		}

		pLB = cLB; pRB = cRB; pSC = cSC; pAP = cAP; pMI = cMI; pEQ = cEQ; pS0 = cS0; pS9 = cS9; pS7 = cS7;

		if (changed)
			ApplyLightSet();
	}

}

void DeveloperModernMesh_Update(void)
{
	// One-shot baseline measurement for the R1 record: the legacy frame's
	// primitive/draw-split counts and the modern path's submission cost.
	{
		static int s_frameCount = 0;
		static int s_measured = 0;
		if (!s_measured && ++s_frameCount == 90)
		{
			s_measured = 1;
			PsyXRenderStats renderStats = { 0, 0 };
			PsyX_GetRenderStats(&renderStats);
			PsyXModernMeshStats meshStats;
			PsyX_ModernMesh_GetStats(&meshStats);
			printInfo("Baseline: legacyVertices=%d legacyDrawSplits=%d modernMeshes=%d modernVerts=%d modernCalls=%d modernMicros=%d legacyShadowPass=%d legacyLightPass=%d pointLights=%d legacyCasterCalls=%d legacyCasterTriangles=%d worldCasterTriangles=%d\n",
				renderStats.vertexCount, renderStats.drawSplitCount,
				meshStats.meshCount, meshStats.vertexCount, meshStats.drawCalls, meshStats.lastFrameMicros,
				meshStats.legacyShadowPass, meshStats.legacyLightPass, s_publishedPointLights,
				meshStats.legacyCasterDrawCalls, meshStats.legacyCasterTriangles, meshStats.worldCasterTriangles);

			// The point-light offsets and ranges are in world units, so the
			// player car's real half-extents are logged next to them: they are
			// what the offsets are scaled by and the only in-game length
			// reference the modern path has.
			if (MainPlayer.playerCarId >= 0)
			{
				const CAR_DATA* cp = &car_data[MainPlayer.playerCarId];
				printInfo("Baseline: playerCar boxLength=%d,%d,%d\n",
					cp->hd.oBox.length[0], cp->hd.oBox.length[1], cp->hd.oBox.length[2]);
			}
		}
	}

	if (!s_enabled)
	{
		PsyX_ModernMesh_SetEnabled(0);
		DeveloperWorldShadow_Clear();
		return;
	}

	// This runs after the frame's scene render (see DrawGame), so the camera
	// globals below are exactly the ones the legacy scene was built with. Using
	// them here instead of earlier in StepGame avoids a one-frame camera lag
	// that made the fixtures appear to swim when the camera moved.
	const int cameraAtOrigin = (camera_position.vx == 0 && camera_position.vy == 0 && camera_position.vz == 0);
	const int matrixEmpty = (inv_camera_matrix.m[0][0] == 0 && inv_camera_matrix.m[1][1] == 0 && inv_camera_matrix.m[2][2] == 0);
	if (cameraAtOrigin || matrixEmpty)
		return;

	if (!s_positioned)
	{
		// Anchored once, then every fixture stays fixed in the world like any
		// other static prop: nothing follows the camera or the car.
		PositionGallery();
		ApplyLightSet();	// the shadow volume centre is known now
	}

	float view[16];
	const float color[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

	CameraRotation(view);
	const float cameraPos[3] = { (float)camera_position.vx, (float)camera_position.vy, (float)camera_position.vz };
	PsyX_ModernMesh_SetCamera(view, cameraPos);

	// Pushed every frame because the shadow centre follows the player vehicle;
	// the light set itself only changes when the panel or an F-key changes it.
	float shadowCentre[3];
	const bool worldShadows = s_worldShadowCasters && s_shadowsEnabled && NumPlayers == 1 &&
		PsyX_GetRenderBackend() == PSYX_BACKEND_VULKAN;
	const float farExtent = worldShadows && s_farShadows ?
		MAX(s_shadowExtent, DeveloperStaticScene_GetRange()) : 0.0f;
	ApplyLightSet(shadowCentre, farExtent);

	PsyX_ModernMesh_SetEnabled(1);
	if (worldShadows)
	{
		const float effectiveFar = DeveloperWorldShadow_Submit(s_shadowExtent, farExtent);
		if (effectiveFar != farExtent) ApplyLightSet(shadowCentre, effectiveFar);
	}
	else
		DeveloperWorldShadow_Clear();

	for (int i = 0; i < s_fixtureCount; i++)
	{
		float world[16];
		BuildInstanceWorld(s_fixtures[i].pos, s_fixtures[i].yaw, world);
		PsyX_ModernMesh_SetInstanceWorld(s_fixtures[i].mesh, world);
		BuildInstanceView(s_fixtures[i].pos, s_fixtures[i].yaw, view);
		PsyX_ModernMesh_SetInstance(s_fixtures[i].mesh, view, color, 1);
	}
}

int DeveloperModernMesh_IsVisible(void)
{
	if (!s_enabled || s_fixtureCount == 0 || !s_positioned)
		return 0;

	PsyXModernMeshStats stats;
	PsyX_ModernMesh_GetStats(&stats);
	return stats.visibleInstances > 0;
}

#else // unsupported platform

void DeveloperModernMesh_ProcessInput(void) {}
void DeveloperModernMesh_Initialise(void) {}
void DeveloperModernMesh_Shutdown(void) {}
void DeveloperModernMesh_SetEnabled(int) {}
int  DeveloperModernMesh_GetEnabled(void) { return 0; }
void DeveloperModernMesh_SetShadows(int) {}
void DeveloperModernMesh_SetStaticUnlit(int) {}
void DeveloperModernMesh_SetWorldShadowCasters(int) {}
int  DeveloperModernMesh_GetWorldShadowCasters(void) { return 0; }
void DeveloperModernMesh_SetLegacyShadowCasters(int) {}
int  DeveloperModernMesh_GetLegacyShadowCasters(void) { return 0; }
int  DeveloperModernMesh_GetShadows(void) { return 0; }
void DeveloperModernMesh_SetAmbientOcclusion(int) {}
int  DeveloperModernMesh_GetAmbientOcclusion(void) { return 0; }
void DeveloperModernMesh_SetLegacyLighting(int) {}
int  DeveloperModernMesh_GetLegacyLighting(void) { return 0; }
void  DeveloperModernMesh_SetLegacyLightReceptivity(float) {}
float DeveloperModernMesh_GetLegacyLightReceptivity(void) { return 0.0f; }
void  DeveloperModernMesh_SetPointLights(int) {}
int   DeveloperModernMesh_GetPointLights(void) { return 0; }
void  DeveloperModernMesh_SetPointLightLimit(int) {}
int   DeveloperModernMesh_GetPointLightLimit(void) { return 0; }
void  DeveloperModernMesh_SetPointLightRange(float) {}
float DeveloperModernMesh_GetPointLightRange(void) { return 0.0f; }
void  DeveloperModernMesh_SetPointLightIntensity(float) {}
float DeveloperModernMesh_GetPointLightIntensity(void) { return 0.0f; }
void  DeveloperModernMesh_SetHeadlights(int) {}
int   DeveloperModernMesh_GetHeadlights(void) { return 0; }
void  DeveloperModernMesh_SetSirenLights(int) {}
int   DeveloperModernMesh_GetSirenLights(void) { return 0; }
void  DeveloperModernMesh_SetPointLightProbe(int) {}
int   DeveloperModernMesh_GetPointLightProbe(void) { return 0; }
void DeveloperModernMesh_GetSunAngles(float* azimuthDegrees, float* elevationDegrees)
{
	if (azimuthDegrees) *azimuthDegrees = 0.0f;
	if (elevationDegrees) *elevationDegrees = 0.0f;
}
void DeveloperModernMesh_SetSunAngles(float, float) {}
float DeveloperModernMesh_GetLightIntensity(void) { return 0.0f; }
void DeveloperModernMesh_SetLightIntensity(float) {}
float DeveloperModernMesh_GetAmbient(void) { return 0.0f; }
void DeveloperModernMesh_SetAmbient(float) {}
float DeveloperModernMesh_GetExposure(void) { return 1.0f; }
void DeveloperModernMesh_SetExposure(float) {}
int DeveloperModernMesh_GetFarShadows(void) { return 0; }
void DeveloperModernMesh_SetFarShadows(int) {}
float DeveloperModernMesh_GetShadowExtent(void) { return 0.0f; }
void DeveloperModernMesh_SetShadowExtent(float) {}
int  DeveloperModernMesh_GetShadowDebug(void) { return 0; }
void DeveloperModernMesh_SetShadowDebugMode(int) {}
void DeveloperModernMesh_Update(void) {}
int  DeveloperModernMesh_IsVisible(void) { return 0; }

#endif
