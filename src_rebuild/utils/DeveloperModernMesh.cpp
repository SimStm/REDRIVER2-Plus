#include "DeveloperModernMesh.h"

#include "driver2.h"

#include "C/camera.h"
#include "C/cars.h"
#include "C/draw.h"
#include "C/dr2roads.h"
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
float s_shadowExtent = 2500.0f;

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
		else if (sscanf(line, " shadowextent=%f", &s_shadowExtent) == 1)
			;
		else if (sscanf(line, " shadowdebug=%d", &value) == 1 || sscanf(line, "shadowdebug=%d", &value) == 1)
			s_shadowDebug = value;
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
	fprintf(file, "shadowextent=%.1f\n", s_shadowExtent);
	fprintf(file, "shadowdebug=%d\n", s_shadowDebug);
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
		pos[1] = GroundHeightAt(pos[0], pos[2]) + kHalfSize;	// sphere rests on the ground
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
	if (height == 0 && (camera_position.vy > 200 || camera_position.vy < -200))
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
	pos[1] = GroundHeightAt(pos[0], pos[2]) - minY;	// base rests on the surface
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
	pos[1] = GroundHeightAt(pos[0], pos[2]) + kHalfSize;
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
	world[1] = 0.0f; world[5] = 1.0f; world[9] = 0.0f; world[13] = pos[1];
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
	if (MainPlayer.playerCarId >= 0)
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

void ApplyLightSet()
{
	PsyXModernLightSet set;
	memset(&set, 0, sizeof(set));
	set.ambient[0] = set.ambient[1] = set.ambient[2] = s_ambient;
	set.exposure = s_exposure;
	set.aoEnabled = s_aoEnabled;
	set.shadowsEnabled = s_shadowsEnabled;
	set.shadowExtent = s_shadowExtent;
	if (s_positioned)
	{
		set.shadowCenter[0] = s_anchor[0];
		set.shadowCenter[1] = s_anchor[1];
		set.shadowCenter[2] = s_anchor[2];
	}

	// One directional sun; additional point lights could be appended here.
	const float len = sqrtf(s_lightDir[0] * s_lightDir[0] + s_lightDir[1] * s_lightDir[1] + s_lightDir[2] * s_lightDir[2]);
	const float inv = len > 1e-5f ? 1.0f / len : 1.0f;
	set.count = 1;
	set.lights[0].type = 0;
	set.lights[0].direction[0] = s_lightDir[0] * inv;
	set.lights[0].direction[1] = s_lightDir[1] * inv;
	set.lights[0].direction[2] = s_lightDir[2] * inv;
	set.lights[0].color[0] = set.lights[0].color[1] = set.lights[0].color[2] = 1.0f;
	set.lights[0].intensity = s_lightIntensity;

	PsyX_ModernMesh_SetLights(&set);
}

/* The modern-mesh renderer owns OpenGL programs, framebuffers and buffers, and
   it builds them lazily from this module. On the Vulkan backend those GL entry
   points were never loaded, so the module stays off instead of crashing; the
   Vulkan modern-mesh path is a separate later milestone. */
static int ModernMeshVulkanDisabled(void)
{
	static int warned = 0;

	if (PsyX_GetRenderBackend() != PSYX_BACKEND_VULKAN)
		return 0;

	if (!warned)
	{
		warned = 1;
		printWarning("ModernMesh: disabled on the Vulkan backend (OpenGL-only for now)\n");
	}

	return 1;
}

void DeveloperModernMesh_Initialise(void)
{
	if (ModernMeshVulkanDisabled())
		return;

	ReadEnabledState();
	ApplyLightSet();
	PsyX_ModernMesh_SetShadowDebug(s_shadowDebug);
	PsyX_ModernMesh_SetEnabled(0);
}

void DeveloperModernMesh_Shutdown(void)
{
	if (ModernMeshVulkanDisabled())
		return;

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

void DeveloperModernMesh_SetAmbientOcclusion(int enabled)
{
	s_aoEnabled = enabled != 0;
	ApplyLightSet();
}

int DeveloperModernMesh_GetAmbientOcclusion(void)
{
	return s_aoEnabled;
}

void DeveloperModernMesh_Update(void)
{
	if (ModernMeshVulkanDisabled())
		return;

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
			printInfo("Baseline: legacyVertices=%d legacyDrawSplits=%d modernMeshes=%d modernVerts=%d modernCalls=%d modernMicros=%d legacyShadowPass=%d\n",
				renderStats.vertexCount, renderStats.drawSplitCount,
				meshStats.meshCount, meshStats.vertexCount, meshStats.drawCalls, meshStats.lastFrameMicros,
				meshStats.legacyShadowPass);
		}
	}

	if (!s_enabled)
	{
		PsyX_ModernMesh_SetEnabled(0);
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
	PsyX_ModernMesh_SetEnabled(1);

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
	if (ModernMeshVulkanDisabled())
		return 0;

	if (!s_enabled || s_fixtureCount == 0 || !s_positioned)
		return 0;

	PsyXModernMeshStats stats;
	PsyX_ModernMesh_GetStats(&stats);
	return stats.visibleInstances > 0;
}

#else // unsupported platform

void DeveloperModernMesh_Initialise(void) {}
void DeveloperModernMesh_Shutdown(void) {}
void DeveloperModernMesh_SetEnabled(int) {}
int  DeveloperModernMesh_GetEnabled(void) { return 0; }
void DeveloperModernMesh_SetShadows(int) {}
int  DeveloperModernMesh_GetShadows(void) { return 0; }
void DeveloperModernMesh_SetAmbientOcclusion(int) {}
int  DeveloperModernMesh_GetAmbientOcclusion(void) { return 0; }
void DeveloperModernMesh_Update(void) {}
int  DeveloperModernMesh_IsVisible(void) { return 0; }

#endif
