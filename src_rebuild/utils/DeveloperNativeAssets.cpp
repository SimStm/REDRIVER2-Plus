#include "DeveloperNativeAssets.h"

#if !defined(PSX) && !defined(__ANDROID__) && !defined(__EMSCRIPTEN__)
#include "NativeAssetCache.h"
#include "DeveloperNativeAssetFixture.h"
#include "DeveloperNativeRegion.h"
#include "driver2.h"
#include "C/camera.h"
#include "C/draw.h"
#include "C/main.h"
#include "C/models.h"
#include "PsyX/PsyX_public.h"
#include <cstdio>
#include <cstring>
#include <new>

namespace
{
NativeAsset::GeometryCache s_geometry;
bool s_requested = false;
unsigned int s_observed = 0, s_rejected = 0;
unsigned int s_generation = 0;

int ModelSlot(const MODEL* model, int original)
{
	if (original >= 0 && original < MAX_MODEL_SLOTS && modelpointers[original] == model) return original;
	for (int slot = 0; slot < MAX_MODEL_SLOTS; ++slot) if (modelpointers[slot] == model) return slot;
	return -1;
}
int LodLink(const unsigned short* table, int index)
{
	return table && table[index] != 0xffff && table[index] < MAX_MODEL_SLOTS ? table[index] : -1;
}

const NativeAsset::CachedGeometry* CaptureModel(int slot)
{
	if (!s_requested || slot < 0 || slot >= MAX_MODEL_SLOTS) return NULL;
	const MODEL* model = modelpointers[slot];
	if (!model || model == &dummyModel) return NULL;
	const int vertexSlot = model->instance_number == -1 ? slot : model->instance_number;
	if (vertexSlot < 0 || vertexSlot >= MAX_MODEL_SLOTS) return NULL;
	NativeAsset::GeometryKey key;
	key.model = AssetCatalog_GetModelHandle(AssetCatalog_FindModel(slot));
	key.vertices = AssetCatalog_GetModelHandle(AssetCatalog_FindModel(vertexSlot));
	key.lodParent = model->instance_number;
	key.highDetail = LodLink(Low2HighDetailTable, slot);
	key.lowerDetail = LodLink(Low2LowerDetailTable, slot);
	const NativeAsset::CachedGeometry* cached = s_geometry.Find(key);
	if (cached) return cached;
	if (model->tri_verts & 0x80)
	{
		// Never invent raw source evidence by decoding already converted polygons.
		PsyX_Log_Warning("Native assets: model%d unavailable before GTE; native source unavailable\n", slot);
		return NULL;
	}
	const NativeAsset::CachedGeometry* entry = NULL;
	const unsigned int bytes = Models_GetSourceSize(slot), sourceBytes = Models_GetSourceSize(vertexSlot);
	const NativeAsset::Result result = s_geometry.Capture(key,
		NativeAsset::Bytes(model, bytes), NativeAsset::Bytes(modelpointers[vertexSlot], sourceBytes), &entry);
	if (!entry)
	{
		PsyX_Log_Warning("Native assets: model%d %s spans%u/%u; native source unavailable\n", slot,
			NativeAsset::ResultName(result), bytes, sourceBytes);
		return NULL;
	}
	if (result == NativeAsset::Result::Complete)
		PsyX_Log_Info("Native assets: model%d nativeBounds %.0f,%.0f,%.0f to %.0f,%.0f,%.0f\n", slot,
			entry->mesh.boundsMin[0], entry->mesh.boundsMin[1], entry->mesh.boundsMin[2],
			entry->mesh.boundsMax[0], entry->mesh.boundsMax[1], entry->mesh.boundsMax[2]);
	PsyX_Log_Info("Native assets: %s %s model%d vertices%d spans%u/%u source%u/%u triangles%u parts%u "
		"lod=%d,%d sourceHash=%llu vertexHash=%llu reason=%s\n",
		entry->stableId.c_str(), NativeAsset::ResultName(result), slot, vertexSlot, bytes, sourceBytes,
		entry->mesh.sourceVertices, entry->mesh.sourcePolygons, entry->mesh.triangles, unsigned(entry->mesh.parts.size()),
		key.highDetail, key.lowerDetail,
		(unsigned long long)entry->sourceHash, (unsigned long long)entry->vertexSourceHash, entry->failure.reason.c_str());
	for (size_t i = 0; i < entry->mesh.parts.size(); ++i)
	{
		const NativeAsset::Part& part = entry->mesh.parts[i];
		PsyX_Log_Info("Native assets: %s part%u page%u texture%u family%u plane%u indices%u firstUV=%.8f,%.8f\n",
			entry->stableId.c_str(), unsigned(i), part.material.textureSet, part.material.textureIndex,
			part.material.polygonType, part.material.normalDirection, unsigned(part.indices.size()),
			part.vertices[0].uv[0], part.vertices[0].uv[1]);
	}
	return entry;
}
void Observe(const NativeAsset::Placement& placement, const MODEL* model)
{
	if (!s_requested || !model || model == &dummyModel) return;
	++s_observed;
	const NativeAsset::CachedGeometry* entry = CaptureModel(ModelSlot(model, placement.modelIndex));
	if (!entry || entry->failure.result != NativeAsset::Result::Complete) ++s_rejected;
	else DeveloperNativeAssetFixture_Observe(*entry, placement);
}
}

const NativeAsset::CachedGeometry* DeveloperNativeAssets_CaptureModel(int sourceSlot) { return CaptureModel(sourceSlot); }

void DeveloperNativeAssets_Initialise(int requested)
{
	s_requested = requested != 0;
	s_geometry.Clear(); s_generation = 0;
	DeveloperNativeAssetFixture_Initialise(requested == 2);
	DeveloperNativeRegion_Initialise(requested == 4 ? DeveloperNativeRegionMode::Render :
		requested == 3 ? DeveloperNativeRegionMode::Inventory : DeveloperNativeRegionMode::Off);
	if (s_requested) PsyX_Log_Info("Native assets: read-only pre-GTE collection; use -nativeassetview for the R02 import fixture\n");
}
void DeveloperNativeAssets_Shutdown() { DeveloperNativeRegion_Shutdown(); DeveloperNativeAssetFixture_Shutdown(); s_geometry.Clear(); s_requested = false; }
void DeveloperNativeAssets_PrepareFrame()
{
	if (!s_requested) return;
	s_observed = s_rejected = 0;
	const unsigned int generation = AssetCatalog_GetGeneration();
	if (generation != s_generation) { s_geometry.Clear(); s_generation = generation; }
	s_geometry.PruneStale();
	DeveloperNativeRegion_PrepareFrame();
	DeveloperNativeAssetFixture_PrepareFrame();
}
void DeveloperNativeAssets_ObserveBuilding(const void* object, const void* sourceModel)
{
	if (!s_requested || !object) return;
	const CELL_OBJECT& cell = *static_cast<const CELL_OBJECT*>(object);
	NativeAsset::Placement placement = {};
	placement.modelIndex = cell.type; placement.yaw = cell.yang & 63;
	placement.position[0] = cell.pos.vx; placement.position[1] = cell.pos.vy; placement.position[2] = cell.pos.vz;
	// Retain the actual fixed-point yaw basis, converted once to native Y up.
	for (int row = 0; row < 3; ++row)
		for (int column = 0; column < 3; ++column)
			placement.world[column * 4 + row] = matrixtable[placement.yaw].m[row][column] / 4096.0f *
				(row == 1 ? -1.0f : 1.0f) * (column == 1 ? -1.0f : 1.0f);
	placement.world[12] = float(cell.pos.vx); placement.world[13] = -float(cell.pos.vy);
	placement.world[14] = float(cell.pos.vz); placement.world[15] = 1;
	Observe(placement, static_cast<const MODEL*>(sourceModel));
}
void DeveloperNativeAssets_ObserveTile(const void* packed, const void* sourceModel)
{
	if (!s_requested || !packed) return;
	NativeAsset::Placement placement;
	if (NativeAsset::DecodePlacement(NativeAsset::Bytes(packed, sizeof(PACKED_CELL_OBJECT)),
		camera_position.vx, camera_position.vz, placement) != NativeAsset::Result::Complete) { ++s_rejected; return; }
	Observe(placement, static_cast<const MODEL*>(sourceModel));
}
void DeveloperNativeAssets_ObserveRegionView(const void* packed)
{
	if (s_requested) DeveloperNativeRegion_ObserveView(packed);
}
void DeveloperNativeAssets_Update()
{
	if (!s_requested) return;
	DeveloperNativeAssetFixture_Update(s_geometry);
	DeveloperNativeRegion_Update(s_geometry);
	if (FrameCnt % 120) return;
	PsyX_Log_Info("Native assets: tick%d observed%u rejected%u cached%u ownedBytes%llu\n",
		FrameCnt, s_observed, s_rejected, unsigned(s_geometry.Count()), (unsigned long long)s_geometry.OwnedBytes());
}
#else
void DeveloperNativeAssets_Initialise(int) {}
void DeveloperNativeAssets_Shutdown() {}
void DeveloperNativeAssets_PrepareFrame() {}
void DeveloperNativeAssets_ObserveBuilding(const void*, const void*) {}
void DeveloperNativeAssets_ObserveTile(const void*, const void*) {}
void DeveloperNativeAssets_ObserveRegionView(const void*) {}
void DeveloperNativeAssets_Update() {}
#endif
