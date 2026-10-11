#include "NativePedCapture.h"
#ifdef _WIN32
#include "../Game/driver2.h"
#include "../Game/C/pedest.h"
#include "../Game/C/players.h"
#include "../Game/C/camera.h"
#include "../Game/C/draw.h"
#include "../Game/C/models.h"
#include "../Game/C/assetcatalog.h"
#include "../Game/C/assetcatalog_game.h"
#include "../Game/C/texture.h"
#include "../Game/C/main.h"
#include "../Game/C/system.h"
#include "../native/NativeRenderer.h"
#include "PsyX/PsyX_render.h"
#include "TextureDecode.h"
#include "psx/gtereg.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <exception>

namespace {
struct CachedPart
{
	unsigned incarnation = 0;
	int palette = 0, type = 0, modelSlot = -1;
	NativePedComponentIdentity identity = {};
	bool failed = false;
	char error[192] = {};
	std::shared_ptr<const NativePedComponent> part;
};
static NativePedFrame frames[NATIVE_PED_SLOTS];
static CachedPart cache[NATIVE_PED_SLOTS][NATIVE_PED_COMPONENTS];
static unsigned incarnations[NATIVE_PED_SLOTS] = {}, generation = 0;
static NativePedFrameEpoch epoch;
static_assert(MAX_PEDESTRIANS == NATIVE_PED_SLOTS, "native pedestrian pool capacity differs");
int Slot(const PEDESTRIAN* ped)
{
	const uintptr_t address = (uintptr_t)ped, begin = (uintptr_t)pedestrians;
	return address >= begin && address < begin + sizeof(pedestrians) && (address - begin) % sizeof(PEDESTRIAN) == 0 ? (int)((address - begin) / sizeof(PEDESTRIAN)) : -1;
}
void Failure(int slot, const char* error)
{
	frames[slot].failed = true; snprintf(frames[slot].error, sizeof(frames[slot].error), "%s", error);
}
bool DecodePage(unsigned short page, unsigned short clut, const std::vector<unsigned short>& vram,
	std::vector<unsigned char>* pixels, char* error, int capacity)
{
	const int format = (page >> 7) & 3, x = (page & 15) * 64, y = (page & 16) ? 256 : 0;
	const int words = format == 0 ? 64 : format == 1 ? 128 : 256, entries = format == 0 ? 16 : format == 1 ? 256 : 0;
	const int cx = (clut & 63) * 16, cy = clut >> 6;
	if (format == 3 || x + words > 1024 || (entries && (cx + entries > 1024 || cy >= 512)))
		{ snprintf(error, capacity, "pedestrian page/CLUT is outside original VRAM"); return false; }
	pixels->resize(256u * 256u * 4u);
	unsigned char palette[1024]; TextureDecodeRequest request = {};
	const size_t offset = (size_t)y * 1024 + x;
	request.format = (TextureDecodeFormat)format; request.page = (const unsigned char*)vram.data() + offset * 2;
	request.pageBytes = (int)((vram.size() - offset) * 2); request.widthTexels = request.heightTexels = 256; request.rowStrideHalfwords = 1024;
	if (entries)
	{
		request.clut = (const unsigned char*)vram.data() + ((size_t)cy * 1024 + cx) * 2; request.clutBytes = entries * 2;
		request.paletteEntryCount = entries; request.paletteVariantCount = 1; request.paletteOut = palette; request.paletteCapacity = entries * 4;
	}
	request.rgbaOut = pixels->data(); request.rgbaCapacity = (int)pixels->size(); TextureDecodeResult result = {};
	return TextureDecode_Decode(&request, &result, error, capacity) == TEXTURE_DECODE_OK;
}
bool Bake(int slot, NativePedComponent* part, int customClut, char* error, int capacity)
{
	if (part->conversion.source.submeshes.size() > 64) { snprintf(error, capacity, "pedestrian material capacity exceeded"); return false; }
	std::vector<unsigned short> vram(1024u * 512u); GR_ReadVRAM(vram.data(), 0, 0, 1024, 512);
	std::vector<unsigned> materials;
	size_t cachedBytes = 0;
	for (int component = 0; component < NATIVE_PED_COMPONENTS; ++component)
		if (cache[slot][component].part) for (size_t image = 0; image < cache[slot][component].part->conversion.pixels.size(); ++image)
			cachedBytes += cache[slot][component].part->conversion.pixels[image].size();
	for (size_t s = 0; s < part->conversion.source.submeshes.size(); ++s)
	{
		const SourceSubmesh& submesh = part->conversion.source.submeshes[s];
		unsigned short page = part->identity.page, clut = part->identity.clut;
		if (part->identity.source)
		{
			if (submesh.textureSet >= 128 || submesh.textureId >= 32) { snprintf(error, capacity, "pedestrian texture identity invalid"); return false; }
			page = texture_pages[submesh.textureSet]; clut = customClut >= 0 ? (unsigned short)customClut : texture_cluts[submesh.textureSet][submesh.textureId];
		}
		const unsigned material = ((unsigned)page << 16) | clut;
		size_t index = 0; while (index < materials.size() && materials[index] != material) ++index;
		if (index == materials.size())
		{
			if (cachedBytes + part->conversion.pixels.size() * 256u * 256u * 4u + 256u * 256u * 4u > 16u * 1024u * 1024u)
				{ snprintf(error, capacity, "pedestrian cached texture capacity exceeded"); return false; }
			std::vector<unsigned char> pixels;
			if (!DecodePage(page, clut, vram, &pixels, error, capacity)) return false;
			materials.push_back(material); part->conversion.pixels.push_back(std::move(pixels));
		}
		part->conversion.images.push_back({ (int)index, 256, 256 });
	}
	return true;
}
bool View(MODEL* model, SourceModelView* view, const unsigned char** begin, int* span, unsigned* revision, int* slotHint, unsigned* baseRevision)
{
	int slot = *slotHint;
	if (slot < 0 || slot >= MAX_MODEL_SLOTS || modelpointers[slot] != model)
		{ slot = 0; while (slot < MAX_MODEL_SLOTS && modelpointers[slot] != model) ++slot; }
	if (slot == MAX_MODEL_SLOTS || model_byte_sizes[slot] <= 0) return false;
	*slotHint = slot;
	const int record = AssetCatalog_FindModel(slot); if (record < 0) return false;
	*revision = AssetCatalog_GetModelHandle(record).revision; *span = model_byte_sizes[slot]; *begin = (const unsigned char*)model;
	*view = SourceModelView(); view->instanceNumber = model->instance_number; view->numVertices = model->num_vertices;
	view->numPolys = model->num_polys; view->numPointNormals = model->num_point_normals; view->vertices = model->vertices;
	view->polyBlock = model->poly_block; view->pointNormals = model->point_normals;
	if (model->instance_number != -1)
	{
		const int base = model->instance_number; if (base < 0 || base >= MAX_MODEL_SLOTS || !modelpointers[base] || model_byte_sizes[base] <= 0) return false;
		MODEL* baseModel = modelpointers[base]; view->baseBegin = (const unsigned char*)baseModel; view->baseEnd = view->baseBegin + model_byte_sizes[base];
		const int baseRecord = AssetCatalog_FindModel(base); if (baseRecord < 0) return false;
		*baseRevision = AssetCatalog_GetModelHandle(baseRecord).revision;
		view->baseVertices = baseModel->vertices; view->basePointNormals = baseModel->point_normals;
		view->baseNumVertices = baseModel->num_vertices; view->baseNumPointNormals = baseModel->num_point_normals;
	}
	return true;
}
unsigned long long MaterialIdentity(const NativePedComponent& part, int customClut)
{
	unsigned long long hash = 1469598103934665603ull;
	for (size_t s = 0; s < part.conversion.source.submeshes.size(); ++s)
	{
		const SourceSubmesh& submesh = part.conversion.source.submeshes[s];
		if (submesh.textureSet >= 128 || submesh.textureId >= 32) return 0;
		const unsigned page = texture_pages[submesh.textureSet], clut = customClut >= 0 ? (unsigned)customClut : texture_cluts[submesh.textureSet][submesh.textureId];
		hash = (hash ^ page) * 1099511628211ull; hash = (hash ^ clut) * 1099511628211ull;
	}
	return hash;
}
CachedPart* Cache(int slot, int component, const NativePedComponentIdentity& identity)
{
	CachedPart& entry = cache[slot][component]; const NativePedFrame& frame = frames[slot];
	if (entry.incarnation != frame.incarnation || entry.type != frame.type || entry.palette != frame.palette || !NativePedMesh_SameComponent(entry.identity, identity))
		{ entry = CachedPart(); entry.incarnation = frame.incarnation; entry.type = frame.type; entry.palette = frame.palette; entry.identity = identity; }
	return &entry;
}
void Record(int slot, NativePedComponentPose&& pose)
{
	for (size_t p = 0; p < frames[slot].parts.size(); ++p)
		if (frames[slot].parts[p].component->identity.component == pose.component->identity.component) { frames[slot].parts[p] = std::move(pose); return; }
	if (frames[slot].parts.size() == NATIVE_PED_COMPONENTS) { Failure(slot, "pedestrian component capacity exceeded"); return; }
	frames[slot].parts.push_back(std::move(pose));
}
}

void NativePedCapture_BeginFrame()
{
	epoch.Begin(NativeRenderer_IsActive() != 0);
	for (int slot = 0; slot < NATIVE_PED_SLOTS; ++slot) frames[slot] = NativePedFrame();
	const unsigned current = AssetCatalogGame_LevelGeneration();
	if (generation != current) { for (int s = 0; s < NATIVE_PED_SLOTS; ++s) for (int c = 0; c < NATIVE_PED_COMPONENTS; ++c) cache[s][c] = CachedPart(); generation = current; }
}
void NativePedCapture_Created(PEDESTRIAN* ped) { const int slot = Slot(ped); if (slot >= 0) ++incarnations[slot]; }
bool NativePedCapture_Enabled() { return epoch.enabled && epoch.valid; }
void NativePedCapture_SelectView(int view)
{
	if (!epoch.enabled) return;
	epoch.Select(view);
	for (int slot = 0; slot < NATIVE_PED_SLOTS; ++slot) frames[slot] = NativePedFrame();
	int visited = 0;
	for (PEDESTRIAN* ped = pUsedPeds; ped && visited++ < NATIVE_PED_SLOTS; ped = ped->pNext)
	{
		const int slot = Slot(ped); if (slot < 0) break;
		NativePedFrame& frame = frames[slot]; frame.active = true; frame.incarnation = incarnations[slot];
		frame.type = ped->pedType; frame.palette = ped->pallet; frame.pad = ped->padId;
		for (int c = 0; c < NATIVE_PED_COMPONENTS; ++c)
			if (cache[slot][c].incarnation != frame.incarnation || cache[slot][c].type != frame.type || cache[slot][c].palette != frame.palette) cache[slot][c] = CachedPart();
		frame.position[0] = ped->position.vx; frame.position[1] = ped->position.vy; frame.position[2] = ped->position.vz;
		frame.hidden = ped->pedType < OTHER_SPRITE && ped->padId == view && player[view].cameraView == 2;
	}
}
void NativePedCapture_Select(PEDESTRIAN* ped) { const int slot = Slot(ped); if (epoch.enabled && epoch.valid && slot >= 0) frames[slot].selected = true; }

void NativePedCapture_RecordModel(PEDESTRIAN* ped, int component, MODEL* model, const short rotation[3][3], const int offset[3], int customClut)
{
	const int slot = Slot(ped);
	if (!epoch.enabled || !epoch.valid || slot < 0 || !frames[slot].selected || frames[slot].hidden || component < 0 || component >= NATIVE_PED_COMPONENTS) return;
	char error[192] = {};
	try
	{
		SourceModelView view = {}; const unsigned char* begin = nullptr; int span = 0; unsigned revision = 0, baseRevision = 0;
		int modelSlot = cache[slot][component].modelSlot;
		if (!model || !View(model, &view, &begin, &span, &revision, &modelSlot, &baseRevision)) { Failure(slot, "pedestrian model has no resident bounded source span"); return; }
		NativePedComponentIdentity identity = { component, model, revision, 0, (unsigned short)(customClut < 0 ? 0 : customClut), 0, view.baseBegin, baseRevision };
		if (cache[slot][component].part) identity.uvIdentity = MaterialIdentity(*cache[slot][component].part, customClut);
		CachedPart* entry = Cache(slot, component, identity);
		entry->modelSlot = modelSlot;
		if (entry->failed) { Failure(slot, entry->error); return; }
		if (!entry->part)
		{
			std::shared_ptr<NativePedComponent> part(new NativePedComponent);
			if (!NativePedMesh_Decode(begin, begin + span, view, part.get(), error, sizeof(error))) { entry->failed = true; snprintf(entry->error, sizeof(entry->error), "%s", error); Failure(slot, error); return; }
			identity.uvIdentity = MaterialIdentity(*part, customClut); entry->identity = identity; part->identity = identity;
			entry->part = part; // retains bounded source metadata for a stable failure latch
			if (!Bake(slot, part.get(), customClut, error, sizeof(error))) { entry->failed = true; snprintf(entry->error, sizeof(entry->error), "%s", error); Failure(slot, error); return; }
		}
		const unsigned count = view.instanceNumber == -1 ? view.numVertices : view.baseNumVertices;
		if (!count || count > 256) { Failure(slot, "pedestrian evaluated vertex capacity exceeded"); return; }
		const SVECTOR* vertices = GET_MODEL_DATA(SVECTOR, model, vertices);
		NativePedComponentPose pose; pose.component = entry->part; pose.positions.resize(count * 3);
		for (unsigned v = 0; v < count; ++v)
		{
			const int source[3] = { vertices[v].vx, vertices[v].vy, vertices[v].vz };
			for (int axis = 0; axis < 3; ++axis)
			{
				float value = rotation ? 0 : (float)source[axis];
				if (rotation) for (int c = 0; c < 3; ++c) value += rotation[axis][c] * source[c] / 4096.0f;
				pose.positions[v * 3 + axis] = value + offset[axis];
			}
		}
		Record(slot, std::move(pose));
	}
	catch (const std::exception& e) { Failure(slot, e.what()); }
	catch (...) { Failure(slot, "pedestrian owned model capture failed"); }
}

void NativePedCapture_RecordSprite(PEDESTRIAN* ped, int component, const NativePedSprite& sprite)
{
	const int slot = Slot(ped);
	if (!epoch.enabled || !epoch.valid || slot < 0 || !frames[slot].selected || frames[slot].hidden || component < 0 || component >= NATIVE_PED_COMPONENTS) return;
	try
	{
		const float depth[2] = { (float)sprite.depth[0], (float)sprite.depth[1] };
		const int camera[3] = { camera_position.vx, camera_position.vy, camera_position.vz };
		float positions[12];
		if (!NativePedMesh_UnprojectSprite(sprite.xy, depth, (float)scr_z, (float)(gteRegs.CP2C.n.ofx >> 16), (float)(gteRegs.CP2C.n.ofy >> 16), inv_camera_matrix.m, camera, frames[slot].position, positions))
			{ Failure(slot, "pedestrian sprite projection unavailable"); return; }
		const unsigned char* uv = sprite.uv;
		unsigned long long uvIdentity = 1469598103934665603ULL; unsigned char canonical[8];
		for (int c = 0; c < 8; ++c) { canonical[c] = (unsigned char)(uv[c] - (c % 2 ? 0 : sprite.canonicalUvOffset)); uvIdentity = (uvIdentity ^ canonical[c]) * 1099511628211ULL; }
		NativePedComponentIdentity identity = { component, nullptr, 0, sprite.page, sprite.clut, uvIdentity };
		CachedPart* entry = Cache(slot, component, identity); char error[192] = {};
		if (entry->failed) { Failure(slot, entry->error); return; }
		if (!entry->part)
		{
			std::shared_ptr<NativePedComponent> part(new NativePedComponent); part->identity = identity;
			part->conversion.source.positions.assign(positions, positions + 12);
			for (int c = 0; c < 8; ++c) part->conversion.source.uvs.push_back((float)canonical[c]);
			part->conversion.source.indices = { 0,1,2, 1,2,3 }; part->poseVertices = { 0,1,2,3 };
			SourceSubmesh submesh = {}; submesh.indexCount = 6; submesh.primitiveClass = SourcePrimitiveClass::TexturedFlatQuad;
			part->conversion.source.submeshes.push_back(submesh);
			if (!Bake(slot, part.get(), -1, error, sizeof(error))) { entry->failed = true; snprintf(entry->error, sizeof(entry->error), "%s", error); Failure(slot, error); return; }
			entry->part = part;
		}
		NativePedComponentPose pose; pose.component = entry->part; pose.positions.assign(positions, positions + 12);
		for (int c = 0; c < 8; ++c) pose.uvs.push_back((float)uv[c]); Record(slot, std::move(pose));
	}
	catch (const std::exception& e) { Failure(slot, e.what()); }
	catch (...) { Failure(slot, "pedestrian owned sprite capture failed"); }
}
bool NativePedCapture_Consume(int view, const NativePedFrame** out)
{
	if (!out || !epoch.Consume(view)) return false;
	*out = frames; return true;
}
void NativePedCapture_Shutdown()
{
	epoch.Begin(false);
	for (int slot = 0; slot < NATIVE_PED_SLOTS; ++slot) { frames[slot] = NativePedFrame(); for (int c = 0; c < NATIVE_PED_COMPONENTS; ++c) cache[slot][c] = CachedPart(); }
}
#endif
