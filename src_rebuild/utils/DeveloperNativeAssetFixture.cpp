#include "DeveloperNativeAssetFixture.h"
#if !defined(PSX) && !defined(__ANDROID__) && !defined(__EMSCRIPTEN__)
#include "NativeOriginalMaterials.h"
#include "DeveloperNativeCamera.h"
#include "DeveloperNativeScene.h"
#include "driver2.h"
#include "C/camera.h"
#include "C/draw.h"
#include "C/main.h"
#include "C/models.h"
#include "C/glaunch.h"
#include "C/mission.h"
#include "C/system.h"
#include "PsyX/PsyX_globals.h"
#include "PsyX/PsyX_public.h"
#include <cstring>
#include <new>

namespace
{
// R02 import acceptance fixture, not an R03 complete-region renderer. Keep
// real transforms and every face of each selected original resource. Chicago
// model37 is a six-triangle road. Select one tall original facade from the
// resources actually observed after the startup camera settles. This fixture
// follows actual source keys; logical placement indices remain diagnostic data.
const unsigned int kMaxPlacementsPerResource = 12;
int s_models[] = { 37, -1 };
struct SelectedPlacement { NativeAsset::GeometryKey key; NativeAsset::Placement placement; };
SelectedPlacement s_selected[2][kMaxPlacementsPerResource];
unsigned int s_counts[2] = {}, s_excluded = 0;
bool s_requested = false;
unsigned int s_catalogGeneration = 0;
NativeAsset::GpuAssetCache s_gpu;
NativeAsset::OriginalMaterialAdapter s_materials;
PsyXNativeResult s_lastFailure = PSYX_NATIVE_OK;
void Failure(PsyXNativeResult result)
{
	PsyX_Native_SetFrameStatus(result);
	if (result != PSYX_NATIVE_OK && result != s_lastFailure)
		PsyX_Log_Warning("Native asset fixture: %s; legacy world disabled\n", PsyX_Native_ResultName(result));
	s_lastFailure = result;
}
PsyXNativeResult AppendResourceInstances(int kind, const NativeAsset::GeometryCache& geometry,
	PsyXNativeInstance* instances, unsigned int& count)
{
	const NativeAsset::CachedGeometry* source = geometry.Find(s_selected[kind][0].key);
	if (!source) { return PSYX_NATIVE_STALE; }
	std::vector<NativeAsset::ResolvedMaterial> materials;
	const NativeAsset::Result decoded = s_materials.Resolve(*source, materials);
	if (decoded != NativeAsset::Result::Complete) { return PSYX_NATIVE_UNSUPPORTED; }
	const NativeAsset::GpuGeometry* gpu = NULL;
	const PsyXNativeResult result = s_gpu.Ensure(*source, materials, &gpu);
	if (result != PSYX_NATIVE_OK && result != PSYX_NATIVE_PENDING)
	{
		if (result != s_lastFailure)
			for (size_t part = 0; part < materials.size(); ++part)
			{
				float minU = 256, minV = 256, maxU = 0, maxV = 0;
				const NativeAsset::Part& mesh = source->mesh.parts[part];
				for (size_t vertex = 0; vertex < mesh.vertices.size(); ++vertex)
				{
					const float u = mesh.vertices[vertex].uv[0]*256, v = mesh.vertices[vertex].uv[1]*256;
					if (u < minU) minU = u; if (u > maxU) maxU = u;
					if (v < minV) minV = v; if (v > maxV) maxV = v;
				}
				const NativeAsset::ResolvedMaterial& material = materials[part];
				PsyX_Log_Warning("Native asset fixture: model%d part%u page%u id%u tpage%u clut%u region%u,%u,%u,%u image%u,%u UV=%.3f..%.3f,%.3f..%.3f\n",
					s_models[kind], unsigned(part), mesh.material.textureSet, mesh.material.textureIndex,
					material.image.tpage, material.image.clut, material.regionU, material.regionV, material.regionWidth,
					material.regionHeight, material.image.width, material.image.height, minU, maxU, minV, maxV);
			}
		return result;
	}
	if (result == PSYX_NATIVE_PENDING)
	{
		int sourceSlot = -1;
		AssetCatalog_GetModel(source->key.model.index, &sourceSlot, NULL, 0, NULL, NULL, NULL);
		const char* name = GetModelNameByIndex(sourceSlot);
		PsyX_Log_Info("Native asset fixture: logical%d sourceModel%d name=%s source%u/%u triangles%u parts%u uploaded with region UVs, filter%d revision%llu\n",
			s_selected[kind][0].placement.modelIndex, sourceSlot, name ? name : "unnamed", source->mesh.sourceVertices, source->mesh.sourcePolygons, source->mesh.triangles,
			unsigned(gpu->meshes.size()), int(materials[0].filter), (unsigned long long)materials[0].revision);
		for (size_t part = 0; part < gpu->uploads.size(); ++part)
		{
			const NativeAsset::Part& mesh = source->mesh.parts[part];
			const NativeAsset::ResolvedMaterial& material = materials[part];
			const NativeAsset::GpuGeometry::UploadEvidence& bytes = gpu->uploads[part];
			PsyX_Log_Info("Native upload evidence: source%d part%u vertices%u indices%u page%u id%u tpage%u clut%u region%u,%u,%u,%u image%u,%u hashes=%016llx,%016llx,%016llx\n",
				sourceSlot, unsigned(part), unsigned(mesh.vertices.size()), unsigned(mesh.indices.size()), mesh.material.textureSet,
				mesh.material.textureIndex, material.image.tpage, material.image.clut, material.regionU, material.regionV,
				material.regionWidth, material.regionHeight, material.image.width, material.image.height,
				(unsigned long long)bytes.vertices, (unsigned long long)bytes.indices, (unsigned long long)bytes.rgba);
		}
	}
	for (unsigned int placement = 0; placement < s_counts[kind]; ++placement)
	{
		if (!(s_selected[kind][placement].key == source->key)) { return PSYX_NATIVE_STALE; }
		if (gpu->meshes.size() > PSYX_NATIVE_MAX_INSTANCES-count) { return PSYX_NATIVE_OUT_OF_BUDGET; }
		for (size_t part = 0; part < gpu->meshes.size(); ++part)
		{
			PsyXNativeInstance& instance = instances[count++];
			instance.sampling=g_cfg_bilinearFiltering ? PSYX_NATIVE_SAMPLE_TRILINEAR : PSYX_NATIVE_SAMPLE_NEAREST;
			instance.mesh = gpu->meshes[part];
			memcpy(instance.world, s_selected[kind][placement].placement.world, sizeof(instance.world));
			for (int i = 0; i < 4; ++i) instance.tint[i] = 1;
			instance.identity = uint64_t(kind+1)*100000 + placement*100 + part;
		}
	}
	return PSYX_NATIVE_OK;
}

}
void DeveloperNativeAssetFixture_Initialise(bool requested)
{
	s_requested = requested;
	s_models[1] = -1;
	s_catalogGeneration = AssetCatalog_GetGeneration();
	if (requested) PsyX_Log_Info("Native asset fixture: Chicago road37/observed facade, at most12 placements each; incomplete diagnostic scene, R03 pending\n");
}
void DeveloperNativeAssetFixture_Shutdown()
{
	s_gpu.Clear(); s_materials.Clear();
	if (s_requested) PsyX_Native_SetRequested(0);
	s_requested = false;
}
void DeveloperNativeAssetFixture_PrepareFrame()
{
	if (!s_requested) return;
	s_counts[0] = s_counts[1] = s_excluded = 0;
	const unsigned int generation = AssetCatalog_GetGeneration();
	if (generation != s_catalogGeneration)
	{
		s_gpu.Clear(); s_materials.Clear(); s_models[1] = -1;
		s_catalogGeneration = generation;
	}
	s_gpu.Prune(); s_materials.BeginFrame();
}
void DeveloperNativeAssetFixture_Observe(const NativeAsset::CachedGeometry& geometry, const NativeAsset::Placement& placement)
{
	if (!s_requested || GameLevel != 0 || geometry.failure.result != NativeAsset::Result::Complete) return;
	int slot = -1;
	if (!AssetCatalog_GetModel(geometry.key.model.index, &slot, NULL, 0, NULL, NULL, NULL)) return;
	if (FrameCnt >= 30 && s_models[1] < 0 && geometry.mesh.boundsMax[1]-geometry.mesh.boundsMin[1] >= 2500 &&
		geometry.mesh.boundsMax[0]-geometry.mesh.boundsMin[0] >= 1500 && geometry.mesh.sourcePolygons >= 6 &&
		geometry.mesh.parts.size() <= 3)
	{
		s_models[1] = slot;
		PsyX_Log_Info("Native asset fixture: selected facade source%d logical%d bounds%.0f,%.0f,%.0f to %.0f,%.0f,%.0f\n", slot,
			placement.modelIndex, geometry.mesh.boundsMin[0], geometry.mesh.boundsMin[1], geometry.mesh.boundsMin[2],
			geometry.mesh.boundsMax[0], geometry.mesh.boundsMax[1], geometry.mesh.boundsMax[2]);
	}
	const int kind = slot == s_models[0] ? 0 : slot == s_models[1] ? 1 : -1;
	if (kind < 0 || s_counts[kind] == kMaxPlacementsPerResource) { ++s_excluded; return; }
	if (s_counts[kind] && !(s_selected[kind][0].key == geometry.key)) { ++s_excluded; return; }
	SelectedPlacement& selected = s_selected[kind][s_counts[kind]++];
	selected.key = geometry.key; selected.placement = placement;
}
void DeveloperNativeAssetFixture_Update(const NativeAsset::GeometryCache& geometry)
{
	if (!s_requested) return;
	// Analytic F11 mode and this diagnostic cannot publish competing snapshots.
	if (DeveloperNativeScene_GetRequested()) { Failure(PSYX_NATIVE_UNSUPPORTED); return; }
	PsyX_Native_SetRequested(1);
	if (PsyX_GetRenderBackend() != PSYX_BACKEND_VULKAN || GameLevel != 0 || NumPlayers != 1)
	{ Failure(PSYX_NATIVE_UNSUPPORTED); return; }
	if (FrameCnt < 30 || !s_counts[0] || !s_counts[1])
	{
		Failure(FrameCnt < 30 ? PSYX_NATIVE_PENDING : PSYX_NATIVE_NO_SNAPSHOT);
		if (!(FrameCnt%120)) PsyX_Log_Info("Native asset fixture: awaiting representative sources tick%d road%u building%u excluded%u\n",
			FrameCnt, s_counts[0], s_counts[1], s_excluded);
		return;
	}
	try
	{
		std::vector<PsyXNativeInstance> instances(PSYX_NATIVE_MAX_INSTANCES);
		unsigned int count = 0;
		for (int kind = 0; kind < 2; ++kind)
		{
			const PsyXNativeResult result = AppendResourceInstances(kind, geometry, instances.data(), count);
			if (result != PSYX_NATIVE_OK) { Failure(result); return; }
		}
		float rotation[9];
		for (int row = 0; row < 3; ++row) for (int col = 0; col < 3; ++col)
			rotation[row*3+col] = inv_camera_matrix.m[row][col]/4096.0f;
		const float camera[] = { float(camera_position.vx), float(camera_position.vy), float(camera_position.vz) };
		PsyXNativeSnapshot snapshot = {};
		snapshot.size = sizeof(snapshot); snapshot.version = PSYX_NATIVE_VERSION;
		snapshot.sceneGeneration = PsyX_Native_GetSceneGeneration(); snapshot.simulationTick = uint64_t(FrameCnt);
		snapshot.instances = instances.data(); snapshot.instanceCount = count;
		if (!DeveloperNativeCamera_Build(&snapshot.view, rotation, camera, float(scr_z),
			float(current->disp.disp.w), float(current->disp.disp.h), float(g_windowWidth), float(g_windowHeight)))
		{ Failure(PSYX_NATIVE_INVALID); return; }
		Failure(PsyX_Native_Publish(&snapshot));
		if (!(FrameCnt%120)) PsyX_Log_Info("Native asset fixture: tick%d road%u building%u selectedDraws%u excluded%u resources%u\n",
			FrameCnt, s_counts[0], s_counts[1], count, s_excluded, unsigned(s_gpu.Count()));
	}
	catch (const std::bad_alloc&) { Failure(PSYX_NATIVE_OUT_OF_BUDGET); }
}
#endif
