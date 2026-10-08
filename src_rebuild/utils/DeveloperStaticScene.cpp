#include "DeveloperStaticScene.h"
#include "DeveloperModernMesh.h"
#include "driver2.h"
#include "C/assetcatalog.h"
#include "C/camera.h"
#include "C/cell.h"
#include "C/draw.h"
#include "C/map.h"
#include "C/mission.h"
#include "C/models.h"
#include "C/spool.h"
#include "C/system.h"
#include "C/texture.h"
#include "PsyX/PsyX_public.h"
#include <vector>
#include <bitset>
#include <algorithm>
#include <math.h>
#include <string.h>

#if !defined(PSX) && !defined(__ANDROID__) && !defined(__EMSCRIPTEN__)
#include <SDL.h>
namespace
{
const int kMaxMaterials = 4;
const int kMaxPolygons = 256;
const int kMaxResources = 8;
const int kMaxTextures = 16;
const int kMaxObjects = PSYX_MODERN_MAX_FRAME_INSTANCES;
const int kPageWords = 64 * 256;

struct Texture
{
	int set = 0, id = 0, references = 0;
	unsigned short page = 0, clut = 0;
	unsigned int handle = 0;
	std::vector<unsigned short> words;
};
struct Material
{
	int mesh = -1, textureIndex = -1;
	std::vector<float> positions, normals, uvs;
	std::vector<unsigned char> colors;
};
struct Resource
{
	bool live = false;
	int modelIndex = -1, yaw = 0;
	AssetCatalogHandle model = {}, vertices = {};
	int colourTable[32], combo = 0, dither = 0;
	unsigned int lastColourFrame = 0;
	std::vector<Material> materials;
};
struct Object
{
	CELL_OBJECT placement;
	AssetCatalogHandle original;
	int resource;
	bool colour;
};
struct FrameStats
{
	double cpuMicros = 0;
	int colour = 0, extended = 0, budgetFallbacks = 0;
	int unavailableCells = 0, residentCells = 0;
	int created = 0, released = 0;
};
Texture s_textures[kMaxTextures];
Resource s_resources[kMaxResources];
std::vector<Object> s_objects;
std::vector<CELL_OBJECT> s_legacyObjects;
std::vector<CELL_OBJECT> s_candidates;
std::vector<unsigned short> s_textureScratch;
int s_enabled = 0, s_lighting = 1, s_objectLimit = 64;
float s_lightingStrength = 0.75f, s_range = 18000.0f;
bool s_active = false;
int s_attempts = 0;
unsigned int s_generation = 0, s_frame = 0;
AssetCatalogHandle s_rejected[MAX_MODEL_SLOTS] = {};
FrameStats s_stats;
bool s_budgetFailure = false;

struct ScopedCpuTimer
{
	Uint64 started = SDL_GetPerformanceCounter();
	~ScopedCpuTimer()
	{
		s_stats.cpuMicros += (double)(SDL_GetPerformanceCounter() - started) *
			1000000.0 / SDL_GetPerformanceFrequency();
	}
};

bool PolicyAllowsMigration()
{
	PsyXTextureOverrideStats overrides = {};
	PsyX_GetTextureOverrideStats(&overrides);
	return s_enabled && DeveloperModernMesh_GetEnabled() && NumPlayers == 1 &&
		PsyX_GetRenderBackend() == PSYX_BACKEND_VULKAN &&
		g_cfg_pgxpZBuffer && g_cfg_pgxpTextureCorrection && !g_cfg_bilinearFiltering &&
		!(overrides.enabled && overrides.registeredCount);
}

bool CellResident(int x, int z)
{
	const int rx = x / MAP_REGION_SIZE, rz = z / MAP_REGION_SIZE;
	const int slot = (rx & 1) + (rz & 1) * 2;
	return loading_region[slot] == -1 && RoadMapRegions[slot] == rx + rz * (cells_across / MAP_REGION_SIZE);
}

bool MapReady()
{
	return cell_ptrs && cells && cell_objects && MAP_CELL_SIZE > 0 && MAP_REGION_SIZE > 0;
}

bool Resident(const CELL_OBJECT& object)
{
	if (!MapReady()) return false;
	const int x = (object.pos.vx + units_across_halved) / MAP_CELL_SIZE;
	const int z = (object.pos.vz + units_down_halved) / MAP_CELL_SIZE;
	return x >= 0 && z >= 0 && x < cells_across && z < cells_down && CellResident(x, z);
}

float CameraDistanceSquared(const CELL_OBJECT& object)
{
	const float dx = (float)object.pos.vx - camera_position.vx;
	const float dz = (float)object.pos.vz - camera_position.vz;
	return dx * dx + dz * dz;
}

bool SameObject(const CELL_OBJECT& a, const CELL_OBJECT& b)
{
	return a.type == b.type && a.yang == b.yang && a.pos.vx == b.pos.vx &&
		a.pos.vy == b.pos.vy && a.pos.vz == b.pos.vz;
}

void ReleaseResource(int index)
{
	Resource& resource = s_resources[index];
	const bool wasLive = resource.live;
	for (Material& material : resource.materials)
	{
		if (material.mesh >= 0) PsyX_ModernMesh_Destroy(material.mesh);
		if (material.textureIndex >= 0)
		{
			Texture& texture = s_textures[material.textureIndex];
			if (--texture.references == 0)
			{
				if (texture.handle) PsyX_DestroyRGBATexture(texture.handle);
				texture = Texture();
			}
		}
	}
	resource = Resource();
	s_objects.erase(std::remove_if(s_objects.begin(), s_objects.end(),
		[index](const Object& object) { return object.resource == index; }), s_objects.end());
	if (wasLive) s_stats.released++;
}

void ReleaseAll()
{
	for (int i = 0; i < kMaxResources; i++) ReleaseResource(i);
	s_objects.clear();
}

bool ReadTexture(const Texture& texture, std::vector<unsigned short>& words)
{
	const unsigned short page = texture_pages[texture.set];
	const unsigned short clut = texture_cluts[texture.set][texture.id];
	if (page != texture.page || clut != texture.clut || ((page >> 7) & 3) != 0 || (clut >> 6) >= 512)
		return false;
	words.resize(kPageWords + 16);
	RECT16 rect = { (short)((page & 15) * 64), (short)(((page >> 4) & 1) * 256), 64, 256 };
	StoreImage(&rect, (u_long*)words.data());
	rect.x = (clut & 63) * 16; rect.y = clut >> 6; rect.w = 16; rect.h = 1;
	StoreImage(&rect, (u_long*)(words.data() + kPageWords));
	return true;
}

unsigned short Pixel(const Texture& texture, int u, int v)
{
	const unsigned short word = texture.words[v * 64 + u / 4];
	return texture.words[kPageWords + ((word >> ((u & 3) * 4)) & 15)];
}

int AcquireTexture(int set, int id)
{
	if (set < 0 || set >= 128 || id < 0 || id >= 32) return -1;
	for (int i = 0; i < kMaxTextures; i++)
		if (s_textures[i].references && s_textures[i].set == set && s_textures[i].id == id)
		{
			s_textures[i].references++;
			return i;
		}
	for (int i = 0; i < kMaxTextures; i++)
	{
		Texture& texture = s_textures[i];
		if (texture.references) continue;
		texture.set = set; texture.id = id;
		texture.page = texture_pages[set]; texture.clut = texture_cluts[set][id];
		if (!ReadTexture(texture, texture.words)) { texture = Texture(); return -1; }
		texture.references = 1;
		return i;
	}
	s_budgetFailure = true;
	return -1;
}

int MaterialFor(Resource& resource, int set, int id)
{
	for (size_t i = 0; i < resource.materials.size(); i++)
	{
		const Texture& texture = s_textures[resource.materials[i].textureIndex];
		if (texture.set == set && texture.id == id) return (int)i;
	}
	if (resource.materials.size() == kMaxMaterials) return -1;
	const int texture = AcquireTexture(set, id);
	if (texture < 0) return -1;
	Material material; material.textureIndex = texture;
	resource.materials.push_back(material);
	return (int)resource.materials.size() - 1;
}

bool OpaqueRectangle(const Texture& texture, const UV_INFO* uv, int count)
{
	int u0 = 255, v0 = 255, u1 = 0, v1 = 0;
	for (int i = 0; i < count; i++)
	{
		u0 = MIN(u0, uv[i].u); v0 = MIN(v0, uv[i].v);
		u1 = MAX(u1, uv[i].u); v1 = MAX(v1, uv[i].v);
	}
	for (int v = v0; v <= v1; v++)
		for (int u = u0; u <= u1; u++)
			if (Pixel(texture, u, v) == 0) return false;
	return true;
}

void AppendTriangle(Material& material, const SVECTOR* vertices, const PL_POLYFT4& polygon,
	int a, int b, int c, unsigned int colour)
{
	const unsigned char* indices = &polygon.v0;
	const UV_INFO* uv = &polygon.uv0;
	const SVECTOR& p = vertices[indices[a]], &q = vertices[indices[b]], &r = vertices[indices[c]];
	const float ax = q.vx - p.vx, ay = q.vy - p.vy, az = q.vz - p.vz;
	const float bx = r.vx - p.vx, by = r.vy - p.vy, bz = r.vz - p.vz;
	float normal[3] = { ay * bz - az * by, az * bx - ax * bz, ax * by - ay * bx };
	const float length = sqrtf(normal[0]*normal[0] + normal[1]*normal[1] + normal[2]*normal[2]);
	if (length < 1.0f) return;
	const int corners[3] = { a, b, c };
	for (int corner : corners)
	{
		const SVECTOR& point = vertices[indices[corner]];
		material.positions.push_back(point.vx); material.positions.push_back(point.vy); material.positions.push_back(point.vz);
		for (int axis = 0; axis < 3; axis++) material.normals.push_back(normal[axis] / length);
		// Keep the original texel coordinate; exact nearest sampling matches PSX.
		material.uvs.push_back(uv[corner].u / 256.0f); material.uvs.push_back(uv[corner].v / 256.0f);
		material.colors.push_back(colour & 255); material.colors.push_back((colour >> 8) & 255);
		material.colors.push_back((colour >> 16) & 255); material.colors.push_back(255);
	}
}

bool Decode(Resource& resource, MODEL* model, int modelIndex, const CELL_OBJECT& object, const _pct& context)
{
	const int sourceIndex = model->instance_number == -1 ? modelIndex : model->instance_number;
	if (sourceIndex < 0 || sourceIndex >= MAX_MODEL_SLOTS) return false;
	MODEL* source = modelpointers[sourceIndex];
	resource.model = AssetCatalog_GetModelHandle(AssetCatalog_FindModel(modelIndex));
	resource.vertices = AssetCatalog_GetModelHandle(AssetCatalog_FindModel(sourceIndex));
	if (!source || source == &dummyModel || !source->vertices || !source->num_vertices ||
		!model->poly_block || !AssetCatalog_HandleValid(resource.model) || !AssetCatalog_HandleValid(resource.vertices)) return false;
	const SVECTOR* vertices = GET_MODEL_DATA(SVECTOR, model, vertices);
	const unsigned char* data = GET_MODEL_DATA(unsigned char, model, poly_block);
	for (int i = 0; i < model->num_polys; i++)
	{
		const PL_POLYFT4& polygon = *(const PL_POLYFT4*)data;
		const int type = polygon.id & 31;
		if (type != 11 && type != 21 && type != 23) return false;
		const unsigned char* indices = &polygon.v0;
		for (int v = 0; v < 4; v++) if (indices[v] >= source->num_vertices) return false;
		const int index = MaterialFor(resource, polygon.texture_set, polygon.texture_id);
		if (index < 0) return false;
		Material& material = resource.materials[index];
		if (!OpaqueRectangle(s_textures[material.textureIndex], &polygon.uv0, 4)) return false;
		const unsigned int colour = type == 21 ? combointensity : context.f4colourTable[(((object.yang >> 3) * 4) - polygon.th) & 31];
		AppendTriangle(material, vertices, polygon, 0, 1, 3, colour);
		if (polygon.v3 != polygon.v2) AppendTriangle(material, vertices, polygon, 3, 1, 2, colour);
		data += PolySizes[type];
	}
	return !resource.materials.empty();
}

bool UploadTexture(Texture& texture)
{
	if (texture.handle) return true;
	std::vector<unsigned char> rgba(256 * 256 * 4);
	for (int v = 0; v < 256; v++) for (int u = 0; u < 256; u++)
	{
		const unsigned short pixel = Pixel(texture, u, v);
		const int offset = (v * 256 + u) * 4;
		rgba[offset] = (pixel & 31) << 3; rgba[offset + 1] = ((pixel >> 5) & 31) << 3;
		rgba[offset + 2] = ((pixel >> 10) & 31) << 3; rgba[offset + 3] = 255;
	}
	texture.handle = PsyX_CreateRGBATexture(256, 256, rgba.data());
	if (!texture.handle) s_budgetFailure = true;
	return texture.handle != 0;
}

bool Upload(Resource& resource, int modelIndex, const CELL_OBJECT& object, const _pct& context)
{
	for (Material& material : resource.materials)
	{
		if (material.positions.empty()) return false;
		Texture& texture = s_textures[material.textureIndex];
		if (!UploadTexture(texture)) return false;
		PsyXModernMeshDesc description = {};
		description.positions = material.positions.data(); description.normals = material.normals.data();
		description.uvs = material.uvs.data(); description.colors = material.colors.data();
		description.vertexCount = (int)material.positions.size() / 3;
		description.baseColorTexture = texture.handle; description.roughnessFactor = 1;
		description.unlit = s_lighting ? 2 : 1; description.unlitColorScale = 2;
		description.originalLightingStrength = s_lightingStrength;
		description.unlitDither = context.current->draw.dtd || ((context.current->draw.tpage >> 9) & 1);
		material.mesh = PsyX_ModernMesh_CreateEx(&description);
		if (material.mesh < 0 || !PsyX_ModernMesh_SetFrameInstances(material.mesh, NULL, 0))
		{
			s_budgetFailure = true;
			return false;
		}
	}
	resource.modelIndex = modelIndex; resource.yaw = object.yang;
	memcpy(resource.colourTable, context.f4colourTable, sizeof(resource.colourTable));
	resource.combo = combointensity;
	resource.dither = context.current->draw.dtd || ((context.current->draw.tpage >> 9) & 1);
	resource.live = true; resource.lastColourFrame = s_frame;
	s_stats.created++;
	return true;
}

int ModelIndex(MODEL* model, int original)
{
	if (model == modelpointers[original]) return original;
	for (int i = 0; i < MAX_MODEL_SLOTS; i++) if (modelpointers[i] == model) return i;
	return -1;
}

bool Eligible(const MODEL* model)
{
	return model && model != &dummyModel && (model->tri_verts & 0x80) &&
		model->num_polys >= 8 && model->num_polys <= kMaxPolygons &&
		!(model->shape_flags & (SHAPE_FLAG_SPRITE | SHAPE_FLAG_ALPHA | SHAPE_FLAG_TRANS | SHAPE_FLAG_WATER | SHAPE_FLAG_TILE)) &&
		!(model->flags2 & (MODEL_FLAG_ANIMOBJ | MODEL_FLAG_SMASHABLE | MODEL_FLAG_PATH | MODEL_FLAG_GRASS));
}

int ResourceFor(MODEL* model, const CELL_OBJECT& object, const _pct& context)
{
	const int modelIndex = ModelIndex(model, object.type);
	if (modelIndex < 0 || AssetCatalog_HandleValid(s_rejected[modelIndex])) return -1;
	const int dither = context.current->draw.dtd || ((context.current->draw.tpage >> 9) & 1);
	for (int i = 0; i < kMaxResources; i++)
	{
		Resource& resource = s_resources[i];
		if (resource.live && resource.modelIndex == modelIndex && resource.yaw == object.yang &&
			resource.dither == dither && resource.combo == combointensity &&
			!memcmp(resource.colourTable, context.f4colourTable, sizeof(resource.colourTable))) return i;
	}
	if (s_attempts++ >= 2) return -1;
	int index = -1;
	for (int i = 0; i < kMaxResources; i++)
		if (!s_resources[i].live) { index = i; break; }
	if (index < 0)
	{
		unsigned int oldest = s_frame;
		for (int i = 0; i < kMaxResources; i++)
			if (s_resources[i].lastColourFrame + 60 < s_frame && s_resources[i].lastColourFrame < oldest)
				{ oldest = s_resources[i].lastColourFrame; index = i; }
		if (index < 0) { s_stats.budgetFallbacks++; return -1; }
		ReleaseResource(index);
	}
	Resource& resource = s_resources[index];
	s_budgetFailure = false;
	if (Decode(resource, model, modelIndex, object, context) && Upload(resource, modelIndex, object, context)) return index;
	ReleaseResource(index);
	if (s_budgetFailure) s_stats.budgetFallbacks++;
	else s_rejected[modelIndex] = AssetCatalog_GetModelHandle(AssetCatalog_FindModel(modelIndex));
	return -1;
}

void BuildWorld(const CELL_OBJECT& object, float world[16])
{
	memset(world, 0, sizeof(float) * 16);
	const MATRIX2& rotation = matrixtable[object.yang & 63];
	for (int row = 0; row < 3; row++)
		for (int column = 0; column < 3; column++) world[column * 4 + row] = rotation.m[row][column] / 4096.0f;
	world[12] = (float)object.pos.vx; world[13] = (float)object.pos.vy; world[14] = (float)object.pos.vz; world[15] = 1;
}

void PublishResource(int index)
{
	PsyXModernMeshInstance instances[kMaxObjects];
	int count = 0;
	for (const Object& object : s_objects)
	{
		if (object.resource != index) continue;
		PsyXModernMeshInstance& instance = instances[count++];
		BuildWorld(object.placement, instance.world);
		for (int channel = 0; channel < 4; channel++) instance.color[channel] = 1;
		instance.colorVisible = object.colour;
		instance.shadowVisible = 1;
	}
	for (const Material& material : s_resources[index].materials)
	{
		PsyX_ModernMesh_SetOriginalLighting(material.mesh, s_lighting, s_lightingStrength);
		if (!PsyX_ModernMesh_SetFrameInstances(material.mesh, instances, count))
			printWarning("StaticScene: frame instance publication rejected for mesh %d\n", material.mesh);
	}
}

bool SubmitObject(const CELL_OBJECT& object, MODEL* model, const _pct& context)
{
	if (object.type < 0 || object.type >= MAX_MODEL_SLOTS || !Eligible(model) || !Resident(object) ||
		CameraDistanceSquared(object) > s_range * s_range) return false;
	const int resource = ResourceFor(model, object, context);
	if (resource < 0) return false;
	for (Object& cached : s_objects)
	{
		if (!SameObject(cached.placement, object)) continue;
		if (cached.resource != resource)
		{
			const int previous = cached.resource;
			cached.resource = resource;
			PublishResource(previous);
		}
		cached.colour = true;
		s_resources[resource].lastColourFrame = s_frame;
		PublishResource(resource);
		return true;
	}
	if ((int)s_objects.size() >= s_objectLimit) { s_stats.budgetFallbacks++; return false; }
	Object cached; cached.placement = object; cached.resource = resource; cached.colour = true;
	cached.original = AssetCatalog_GetModelHandle(AssetCatalog_FindModel(object.type));
	if (!AssetCatalog_HandleValid(cached.original)) return false;
	s_objects.push_back(cached);
	s_resources[resource].lastColourFrame = s_frame;
	PublishResource(resource);
	return true;
}

bool InCamera(const CELL_OBJECT& object, int radius)
{
	const float delta[3] = { (float)object.pos.vx - camera_position.vx,
		(float)object.pos.vy - camera_position.vy, (float)object.pos.vz - camera_position.vz };
	float view[3] = {};
	for (int row = 0; row < 3; row++) for (int col = 0; col < 3; col++)
		view[row] += inv_camera_matrix.m[row][col] * delta[col] / 4096.0f;
	const float screen = (float)gCameraDefaultScrZ;
	// Conservative sphere test in the final camera basis; no GTE/game state writes.
	return view[2] + radius > 0 && fabsf(view[0]) * screen <= MAX(view[2], 0.0f) * 160 + radius * (screen + 160) &&
		fabsf(view[1]) * screen <= MAX(view[2], 0.0f) * 120 + radius * (screen + 120);
}

void CollectExtended(const _pct& context)
{
	if (!MapReady()) return;
	const int x0 = MAX(0, (int)floorf((camera_position.vx - s_range + units_across_halved) / MAP_CELL_SIZE));
	const int x1 = MIN(cells_across - 1, (int)floorf((camera_position.vx + s_range + units_across_halved) / MAP_CELL_SIZE));
	const int z0 = MAX(0, (int)floorf((camera_position.vz - s_range + units_down_halved) / MAP_CELL_SIZE));
	const int z1 = MIN(cells_down - 1, (int)floorf((camera_position.vz + s_range + units_down_halved) / MAP_CELL_SIZE));
	std::bitset<16384> seen;
	s_candidates.clear();
	for (int z = z0; z <= z1; z++) for (int x = x0; x <= x1; x++)
	{
		if (!CellResident(x, z)) { s_stats.unavailableCells++; continue; }
		s_stats.residentCells++;
		CELL_ITERATOR iterator;
		for (PACKED_CELL_OBJECT* packed = GetFirstPackedCop(x, z, &iterator, 0); packed; packed = GetNextPackedCop(&iterator))
		{
			const ptrdiff_t index = packed - cell_objects;
			if (index < 0 || index >= 16384 || seen.test((size_t)index)) continue;
			seen.set((size_t)index);
			CELL_OBJECT object; QuickUnpackCellObject(packed, &iterator.nearCell, &object);
			if (object.type < 0 || object.type >= MAX_MODEL_SLOTS || CameraDistanceSquared(object) > s_range * s_range) continue;
			MODEL* model = modelpointers[object.type];
			if (!Eligible(model) || !InCamera(object, model->bounding_sphere)) continue;
			const bool legacy = std::any_of(s_legacyObjects.begin(), s_legacyObjects.end(),
				[&object](const CELL_OBJECT& drawn) { return SameObject(drawn, object); });
			if (!legacy) s_candidates.push_back(object);
		}
	}
	std::sort(s_candidates.begin(), s_candidates.end(), [](const CELL_OBJECT& a, const CELL_OBJECT& b)
		{ return CameraDistanceSquared(a) < CameraDistanceSquared(b); });
	for (const CELL_OBJECT& object : s_candidates)
		if (SubmitObject(object, modelpointers[object.type], context)) s_stats.extended++;
}

void LogFrame()
{
	static double micros = 0, peak = 0;
	static unsigned int samples = 0;
	const double elapsed = s_stats.cpuMicros;
	micros += elapsed; peak = MAX(peak, elapsed);
	if (++samples != 120) return;
	int resources = 0, textures = 0, vertices = 0;
	for (const Resource& resource : s_resources) if (resource.live)
	{
		resources++;
		for (const Material& material : resource.materials) vertices += (int)material.positions.size() / 3;
	}
	for (const Texture& texture : s_textures) if (texture.references) textures++;
	printInfo("StaticScene: resources=%d objects=%d colour=%d extended=%d textures=%d vertices=%d range=%.0f "
		"budgetFallbacks=%d residentCells=%d unavailableCells=%d created=%d released=%d cpu_us=%.1f peak_us=%.1f\n",
		resources, (int)s_objects.size(), s_stats.colour, s_stats.extended, textures, vertices, s_range,
		s_stats.budgetFallbacks, s_stats.residentCells, s_stats.unavailableCells, s_stats.created, s_stats.released, micros / 120, peak);
	micros = peak = 0; samples = 0;
}
}

void DeveloperStaticScene_SetEnabled(int enabled)
{
	if (s_enabled != (enabled != 0)) memset(s_rejected, 0, sizeof(s_rejected));
	s_enabled = enabled != 0;
}
int DeveloperStaticScene_GetEnabled(void) { return s_enabled; }
void DeveloperStaticScene_SetLighting(int enabled) { s_lighting = enabled != 0; }
int DeveloperStaticScene_GetLighting(void) { return s_lighting; }
void DeveloperStaticScene_SetLightingStrength(float strength) { s_lightingStrength = fmaxf(0, fminf(strength, 1)); }
float DeveloperStaticScene_GetLightingStrength(void) { return s_lightingStrength; }
void DeveloperStaticScene_SetObjectLimit(int count) { s_objectLimit = MAX(1, MIN(count, kMaxObjects)); }
int DeveloperStaticScene_GetObjectLimit(void) { return s_objectLimit; }
void DeveloperStaticScene_SetRange(float range) { s_range = fmaxf(10000, fminf(range, 40000)); }
float DeveloperStaticScene_GetRange(void) { return s_range; }
void DeveloperStaticScene_Shutdown(void) { ReleaseAll(); s_active = false; }

void DeveloperStaticScene_PrepareFrame(void)
{
	s_stats = FrameStats();
	ScopedCpuTimer timer;
	s_active = PolicyAllowsMigration(); s_attempts = 0; s_frame++;
	s_legacyObjects.clear();
	if (!s_active) { ReleaseAll(); return; }
	if (s_candidates.capacity() < 16384) s_candidates.reserve(16384);
	if (s_objects.capacity() < kMaxObjects) s_objects.reserve(kMaxObjects);
	if (s_legacyObjects.capacity() < MAX_DRAWN_BUILDINGS) s_legacyObjects.reserve(MAX_DRAWN_BUILDINGS);
	if (s_generation != AssetCatalog_GetGeneration())
	{
		ReleaseAll(); s_generation = AssetCatalog_GetGeneration();
		memset(s_rejected, 0, sizeof(s_rejected));
	}
	for (int i = 0; i < kMaxResources; i++)
		if (s_resources[i].live && (!AssetCatalog_HandleValid(s_resources[i].model) || !AssetCatalog_HandleValid(s_resources[i].vertices))) ReleaseResource(i);
	for (const Texture& texture : s_textures)
		if (texture.references && (!ReadTexture(texture, s_textureScratch) || texture.words != s_textureScratch))
			{ ReleaseAll(); memset(s_rejected, 0, sizeof(s_rejected)); break; }
	s_objects.erase(std::remove_if(s_objects.begin(), s_objects.end(), [](const Object& object)
		{ return !AssetCatalog_HandleValid(object.original) || !Resident(object.placement) ||
			CameraDistanceSquared(object.placement) > (s_range + 4000) * (s_range + 4000); }), s_objects.end());
	if ((int)s_objects.size() > s_objectLimit) s_objects.resize(s_objectLimit);
	for (Object& object : s_objects) object.colour = false;
	for (int i = 0; i < kMaxResources; i++) if (s_resources[i].live) PublishResource(i);
}

int DeveloperStaticScene_TryDraw(const CELL_OBJECT* object, MODEL* model, const _pct* context)
{
	if (!s_active || !object || !model || !context) return 0;
	ScopedCpuTimer timer;
	// Remember even fallback/LOD objects to avoid duplicating them in the extension.
	if (s_legacyObjects.size() < MAX_DRAWN_BUILDINGS) s_legacyObjects.push_back(*object);
	const bool submitted = SubmitObject(*object, model, *context);
	if (submitted) s_stats.colour++;
	return submitted;
}

void DeveloperStaticScene_SubmitExtended(void)
{
	if (!s_active) return;
	{
		ScopedCpuTimer timer;
		_pct context = plotContext;
		context.current = current;
		for (int i = 0; i < 8; i++)
		{
			context.f4colourTable[i * 4] = context.planeColours[i] | 0x2C000000;
			context.f4colourTable[i * 4 + 1] = context.planeColours[0] | 0x2C000000;
			context.f4colourTable[i * 4 + 2] = context.planeColours[5] | 0x2C000000;
			context.f4colourTable[i * 4 + 3] = context.planeColours[0] | 0x2C000000;
		}
		CollectExtended(context);
	}
	LogFrame();
}

int DeveloperStaticScene_Owns(const CELL_OBJECT* object)
{
	if (!s_active || !object) return 0;
	return std::any_of(s_objects.begin(), s_objects.end(), [object](const Object& cached)
		{ return SameObject(cached.placement, *object); });
}
#else
void DeveloperStaticScene_SetEnabled(int) {}
int DeveloperStaticScene_GetEnabled(void) { return 0; }
void DeveloperStaticScene_SetLighting(int) {}
int DeveloperStaticScene_GetLighting(void) { return 0; }
void DeveloperStaticScene_SetLightingStrength(float) {}
float DeveloperStaticScene_GetLightingStrength(void) { return 0; }
void DeveloperStaticScene_SetObjectLimit(int) {}
int DeveloperStaticScene_GetObjectLimit(void) { return 0; }
void DeveloperStaticScene_SetRange(float) {}
float DeveloperStaticScene_GetRange(void) { return 0; }
void DeveloperStaticScene_PrepareFrame(void) {}
void DeveloperStaticScene_SubmitExtended(void) {}
int DeveloperStaticScene_TryDraw(const CELL_OBJECT*, MODEL*, const _pct*) { return 0; }
int DeveloperStaticScene_Owns(const CELL_OBJECT*) { return 0; }
void DeveloperStaticScene_Shutdown(void) {}
#endif
