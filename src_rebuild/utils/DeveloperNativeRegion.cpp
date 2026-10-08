#include "DeveloperNativeRegion.h"
#if !defined(PSX) && !defined(__ANDROID__) && !defined(__EMSCRIPTEN__)
#include "DeveloperNativeCamera.h"
#include "DeveloperNativeBackdrop.h"
#include "DeveloperNativeScene.h"
#include "NativeOriginalMaterials.h"
#include "driver2.h"
#include "C/camera.h"
#include "C/cell.h"
#include "C/draw.h"
#include "C/main.h"
#include "C/map.h"
#include "C/mission.h"
#include "C/models.h"
#include "C/spool.h"
#include "C/system.h"
#include "PsyX/PsyX_globals.h"
#include "PsyX/PsyX_public.h"
#include <algorithm>
#include <bitset>
#include <chrono>
#include <cstring>
#include <new>
#include <set>

namespace
{
// Chicago block containing the accepted R02 spawn. Inclusion is by source cell
// membership, including straddlers, rather than by camera/PVS or model names.
const int kMinX = -8192, kMaxX = 16384;
const int kMinZ = -237568, kMaxZ = -212992;
const unsigned int kMaxObjects = 4096;
enum class Surface { Opaque, Cutout, Water, Unknown };
struct SourceInstance
{
	NativeAsset::GeometryKey geometry;
	NativeAsset::Placement placement;
	unsigned int sourceSlot; // current packed-cell slot, diagnostic only
	uint64_t identity; // logical placement; shared by every material part/pass
	bool billboard;
	Surface surface;
	unsigned int triangles, parts;
};
struct Inventory
{
	unsigned int cells, residentCells, unavailableCells;
	unsigned int references, duplicateReferences, invalidReferences;
	unsigned int objects, missingModels, unavailableSources, rejectedSources;
	unsigned int animatedObjects, opaque, cutout, water, unknown;
	unsigned int triangles, partInstances;
	bool capacityFailure, usagePreserved;
};
DeveloperNativeRegionMode s_mode = DeveloperNativeRegionMode::Off;
Inventory s_inventory = {};
NativeAsset::RegionCoverage s_coverage;
uint64_t s_coverageTick = 0;
bool s_published = false;
std::vector<SourceInstance> s_instances;
struct PickFrame
{
	uint64_t scene=0, tick=0;
	std::vector<DeveloperNativePickSource> sources;
};
PickFrame s_pickFrames[2];
unsigned int s_pickFrame=0;
uint64_t s_resolvedPick=0;
DeveloperNativePickSource s_pickSource={};

void ClearPickSources()
{
	PsyX_Native_CancelPick(); s_resolvedPick=0;
	for (unsigned int i=0;i<2;++i) s_pickFrames[i]=PickFrame();
}
NativeAsset::GpuAssetCache s_gpu;
NativeAsset::OriginalMaterialAdapter s_materials;
PsyXNativeResult s_lastFailure = PSYX_NATIVE_OK;
struct PreparationCost
{
	double total = 0, peak = 0;
	double recent[120] = {};
	unsigned int samples = 0;
	void Add(double milliseconds)
	{
		recent[samples%120] = milliseconds;
		total += milliseconds; peak = std::max(peak, milliseconds); ++samples;
	}
	double RecentMean() const
	{
		const unsigned int count = std::min(samples,120u);
		double sum = 0;
		for (unsigned int i = 0; i < count; ++i) sum += recent[i];
		return sum/std::max(count,1u);
	}
	double RecentP95() const
	{
		const unsigned int count = std::min(samples,120u);
		if (!count) return 0;
		double sorted[120]; memcpy(sorted,recent,count*sizeof(double));
		std::sort(sorted,sorted+count);
		return sorted[(count*95+99)/100-1];
	}
};
PreparationCost s_collectionCost, s_assemblyCost, s_backdropCost;
typedef std::chrono::steady_clock PreparationClock;
double ElapsedMilliseconds(PreparationClock::time_point start)
{
	return std::chrono::duration<double, std::milli>(PreparationClock::now()-start).count();
}

bool CellResident(int x, int z)
{
	const int rx = x / MAP_REGION_SIZE, rz = z / MAP_REGION_SIZE;
	const int slot = (rx & 1) + (rz & 1) * 2;
	return loading_region[slot] == -1 && RoadMapRegions[slot] == rx + rz * regions_across;
}
unsigned int PackedSourceSlot(const void* packed)
{
	// Compare addresses without subtracting potentially unrelated C++ pointers.
	// Never dereference or retain the diagnostic input.
	const uintptr_t address = reinterpret_cast<uintptr_t>(packed);
	const uintptr_t base = reinterpret_cast<uintptr_t>(cell_objects);
	const uintptr_t offset = packed && cell_objects && address >= base ? address-base : UINTPTR_MAX;
	return offset % sizeof(PACKED_CELL_OBJECT) == 0 &&
		offset / sizeof(PACKED_CELL_OBJECT) < NativeAsset::RegionCoverage::Capacity ?
		unsigned(offset / sizeof(PACKED_CELL_OBJECT)) : NativeAsset::RegionCoverage::Capacity;
}
Surface Classify(const MODEL& model)
{
	if (model.shape_flags & SHAPE_FLAG_WATER) return Surface::Water;
	// This flag participates in collision/smash processing; it does not establish a
	// raster blend mode. Until the source draw policy is known, refuse the view.
	if (model.shape_flags & SHAPE_FLAG_TRANS) return Surface::Unknown;
	if (model.shape_flags & (SHAPE_FLAG_ALPHA | SHAPE_FLAG_SPRITE)) return Surface::Cutout;
	return Surface::Opaque;
}
void Observe(const PACKED_CELL_OBJECT& packed, const XZPAIR& centre, unsigned int sourceSlot)
{
	++s_inventory.objects;
	NativeAsset::Placement placement;
	if (NativeAsset::DecodePlacement(NativeAsset::Bytes(&packed, sizeof(packed)), centre.x, centre.z, placement) != NativeAsset::Result::Complete)
	{ ++s_inventory.invalidReferences; return; }
	if (placement.modelIndex < 0 || placement.modelIndex >= MAX_MODEL_SLOTS)
	{ ++s_inventory.missingModels; return; }
	MODEL* model = modelpointers[placement.modelIndex];
	if (!model || model == &dummyModel) { ++s_inventory.missingModels; return; }
	const Surface surface = Classify(*model);
	switch (surface)
	{
	case Surface::Opaque: ++s_inventory.opaque; break;
	case Surface::Cutout: ++s_inventory.cutout; break;
	case Surface::Water: ++s_inventory.water; break;
	case Surface::Unknown: ++s_inventory.unknown; break;
	}
	// Match the original quantized placement basis exactly; source trig floats
	// are useful for pure decoding, but the game already owns this fixed basis.
	for (int row = 0; row < 3; ++row) for (int column = 0; column < 3; ++column)
		placement.world[column * 4 + row] = matrixtable[placement.yaw].m[row][column] / 4096.0f *
			(row == 1 ? -1.0f : 1.0f) * (column == 1 ? -1.0f : 1.0f);
	if (model->flags2 & MODEL_FLAG_ANIMOBJ) ++s_inventory.animatedObjects;
	int sourceModel = placement.modelIndex;
	if (Low2HighDetailTable)
	{
		const unsigned int high = Low2HighDetailTable[sourceModel];
		if (high < MAX_MODEL_SLOTS && modelpointers[high] && modelpointers[high] != &dummyModel) sourceModel = int(high);
	}
	const NativeAsset::CachedGeometry* geometry = DeveloperNativeAssets_CaptureModel(sourceModel);
	if (!geometry) { ++s_inventory.unavailableSources; return; }
	if (geometry->failure.result != NativeAsset::Result::Complete) { ++s_inventory.rejectedSources; return; }
	if (s_instances.size() == kMaxObjects) { s_inventory.capacityFailure = true; return; }
	SourceInstance instance;
	instance.geometry = geometry->key; instance.placement = placement;
	instance.sourceSlot = sourceSlot; instance.surface = surface;
	instance.identity = 0; instance.billboard = (model->shape_flags & SHAPE_FLAG_SPRITE) != 0;
	instance.triangles = geometry->mesh.triangles; instance.parts = unsigned(geometry->mesh.parts.size());
	if (!s_coverage.Copy(sourceSlot, instance.parts)) { ++s_inventory.invalidReferences; return; }
	s_instances.push_back(instance); // owned keys/transforms; no recycled game pointer
	s_inventory.triangles += instance.triangles; s_inventory.partInstances += instance.parts;
}
int ComparePlacement(const SourceInstance& a, const SourceInstance& b)
{
	if (a.placement.modelIndex != b.placement.modelIndex) return a.placement.modelIndex < b.placement.modelIndex ? -1 : 1;
	for (int axis = 0; axis < 3; ++axis)
		if (a.placement.position[axis] != b.placement.position[axis]) return a.placement.position[axis] < b.placement.position[axis] ? -1 : 1;
	return a.placement.yaw == b.placement.yaw ? 0 : a.placement.yaw < b.placement.yaw ? -1 : 1;
}
void AssignIdentities()
{
	std::sort(s_instances.begin(), s_instances.end(), [](const SourceInstance& a, const SourceInstance& b)
	{
		const int order = ComparePlacement(a, b);
		return order < 0 || (order == 0 && a.sourceSlot < b.sourceSlot);
	});
	unsigned int occurrence = 0;
	std::set<uint64_t> identities;
	for (size_t i = 0; i < s_instances.size(); ++i)
	{
		SourceInstance& source = s_instances[i];
		occurrence = i && ComparePlacement(source, s_instances[i-1]) == 0 ? occurrence+1 : 0;
		char logical[ASSET_CATALOG_ID_CAPACITY];
		if (!AssetCatalog_MakeModelId(source.placement.modelIndex, logical, sizeof(logical)))
		{ ++s_inventory.unavailableSources; continue; }
		uint64_t hash = UINT64_C(14695981039346656037);
		for (const char* c = logical; *c; ++c) { hash ^= (unsigned char)*c; hash *= UINT64_C(1099511628211); }
		const uint32_t words[] = { uint32_t(source.placement.position[0]), uint32_t(source.placement.position[1]),
			uint32_t(source.placement.position[2]), source.placement.yaw, occurrence };
		for (size_t word = 0; word < sizeof(words)/sizeof(words[0]); ++word)
			for (unsigned int byte = 0; byte < 4; ++byte)
			{ hash ^= (words[word] >> (byte*8)) & 255; hash *= UINT64_C(1099511628211); }
		source.identity = hash;
		// Identity hashes are verified, not assumed collision-free. Reject the
		// whole view on a collision; do not silently select an unrelated object.
		if (!identities.insert(hash).second) ++s_inventory.invalidReferences;
	}
}
void Collect()
{
	// The opt-in original fixture is fixed to Chicago's ordinary cell list.
	// Event-specific topology/animation remains explicitly outside this probe.
	if (GameLevel != 0 || NumPlayers != 1 || !cell_ptrs || !cells || !cell_objects ||
		MAP_CELL_SIZE <= 0 || MAP_REGION_SIZE <= 0 || cells_across <= 0 || cells_down <= 0) return;
	const int x0 = MAX(0, (kMinX + units_across_halved) / MAP_CELL_SIZE);
	const int x1 = MIN(cells_across - 1, (kMaxX - 1 + units_across_halved) / MAP_CELL_SIZE);
	const int z0 = MAX(0, (kMinZ + units_down_halved) / MAP_CELL_SIZE);
	const int z1 = MIN(cells_down - 1, (kMaxZ - 1 + units_down_halved) / MAP_CELL_SIZE);
	for (int z = z0; z <= z1; ++z) for (int x = x0; x <= x1; ++x)
	{
		++s_inventory.cells;
		if (!CellResident(x, z)) { ++s_inventory.unavailableCells; continue; }
		++s_inventory.residentCells;
		CELL_ITERATOR iterator;
		// use_computed=0 neither consults nor changes the gameplay usage bitset.
		for (PACKED_CELL_OBJECT* packed = GetFirstPackedCop(x, z, &iterator, 0, -1);
			packed; packed = GetNextPackedCop(&iterator))
		{
			++s_inventory.references;
			const unsigned int slot = PackedSourceSlot(packed);
			if (slot >= NativeAsset::RegionCoverage::Capacity)
			{ ++s_inventory.invalidReferences; continue; }
			if (!s_coverage.Select(slot)) { ++s_inventory.duplicateReferences; continue; }
			Observe(*packed, iterator.nearCell, slot);
		}
	}
}
}
void DeveloperNativeRegion_Initialise(DeveloperNativeRegionMode mode)
{
	ClearPickSources();
	s_mode = mode;
	DeveloperNativeBackdrop_SetEnabled(mode == DeveloperNativeRegionMode::Render);
	s_collectionCost = PreparationCost(); s_assemblyCost = PreparationCost();
	s_backdropCost = PreparationCost();
	s_inventory = Inventory(); s_instances.clear();
	s_coverage.BeginFrame(); s_coverageTick = 0; s_published = false;
	if (mode != DeveloperNativeRegionMode::Off) PsyX_Log_Info("Native region probe: Chicago source cells x[%d,%d) z[%d,%d); mode%d, R03 incomplete\n",
		kMinX, kMaxX, kMinZ, kMaxZ, int(mode));
}
void DeveloperNativeRegion_Shutdown()
{
	ClearPickSources();
	DeveloperNativeBackdrop_Clear(); DeveloperNativeBackdrop_SetEnabled(0);
	s_gpu.Clear(); s_materials.Clear();
	if (s_mode == DeveloperNativeRegionMode::Render) PsyX_Native_SetRequested(0);
	s_mode = DeveloperNativeRegionMode::Off; s_inventory = Inventory(); std::vector<SourceInstance>().swap(s_instances);
	s_coverage.BeginFrame(); s_coverageTick = 0; s_published = false;
}
void DeveloperNativeRegion_PrepareFrame()
{
	if (s_mode == DeveloperNativeRegionMode::Off) return;
	DeveloperNativeBackdrop_BeginFrame();
	s_inventory = Inventory(); s_instances.clear();
	s_coverage.BeginFrame(); s_coverageTick = uint64_t(FrameCnt); s_published = false;
	const PreparationClock::time_point start = PreparationClock::now();
	unsigned char usage[sizeof(cell_object_computed_values)];
	memcpy(usage, cell_object_computed_values, sizeof(usage));
	try { Collect(); AssignIdentities(); }
	catch (const std::bad_alloc&) { s_inventory.capacityFailure = true; }
	s_inventory.usagePreserved = memcmp(usage, cell_object_computed_values, sizeof(usage)) == 0;
	s_collectionCost.Add(ElapsedMilliseconds(start));
	if (FrameCnt % 120) return;
	PsyX_Log_Info("Native region probe: tick%d cells%u resident%u unavailable%u refs%u duplicateRefs%u invalidRefs%u objects%u copied%u missingModels%u unavailableSources%u rejected%u animated%u opaque%u cutout%u water%u unknownBlendPolicy%u triangles%u partInstances%u capacityFailure%d usagePreserved%d\n",
		FrameCnt, s_inventory.cells, s_inventory.residentCells, s_inventory.unavailableCells,
		s_inventory.references, s_inventory.duplicateReferences, s_inventory.invalidReferences,
		s_inventory.objects, unsigned(s_instances.size()), s_inventory.missingModels,
		s_inventory.unavailableSources, s_inventory.rejectedSources, s_inventory.animatedObjects,
		s_inventory.opaque, s_inventory.cutout, s_inventory.water, s_inventory.unknown,
		s_inventory.triangles, s_inventory.partInstances, int(s_inventory.capacityFailure), int(s_inventory.usagePreserved));
}

namespace
{
struct PreparedResource
{
	NativeAsset::GeometryKey key;
	std::vector<PsyXNativeMeshHandle> meshes;
	std::vector<PsyXNativeMaterialHandle> materials;
};
PsyXNativeResult Assemble(const NativeAsset::GeometryCache& geometry, std::vector<PsyXNativeInstance>& draws,
	std::vector<DeveloperNativePickSource>& pickSources)
{
	std::vector<PreparedResource> resources;
	for (size_t object = 0; object < s_instances.size(); ++object)
	{
		const SourceInstance& source = s_instances[object];
		const NativeAsset::CachedGeometry* cached = geometry.Find(source.geometry);
		if (!cached) return PSYX_NATIVE_STALE;
		size_t resource = 0;
		while (resource < resources.size() && !(resources[resource].key == source.geometry)) ++resource;
		if (resource == resources.size())
		{
			std::vector<NativeAsset::ResolvedMaterial> materials;
			const NativeAsset::Result resolved = s_materials.Resolve(*cached, materials);
			if (resolved != NativeAsset::Result::Complete)
			{
				PsyX_Log_Warning("Native region: material resolution failed for %s (%s)\n", cached->stableId.c_str(), NativeAsset::ResultName(resolved));
				return PSYX_NATIVE_UNSUPPORTED;
			}
			const NativeAsset::GpuGeometry* gpu = NULL;
			const PsyXNativeResult uploaded = s_gpu.Ensure(*cached, materials, &gpu);
			if (uploaded != PSYX_NATIVE_OK && uploaded != PSYX_NATIVE_PENDING) return uploaded;
			PreparedResource ready;
			ready.key = source.geometry; ready.meshes = gpu->meshes; ready.materials = gpu->materials;
			resources.push_back(std::move(ready)); // no pointer into mutating GPU cache
		}
		const PreparedResource& ready = resources[resource];
		if (!s_coverage.Publish(source.sourceSlot, unsigned(ready.meshes.size()))) return PSYX_NATIVE_INVALID;
		if (ready.meshes.size() > PSYX_NATIVE_MAX_INSTANCES-draws.size()) return PSYX_NATIVE_OUT_OF_BUDGET;
		for (size_t part = 0; part < ready.meshes.size(); ++part)
		{
			PsyXNativeInstance draw = {};
			draw.sampling=g_cfg_bilinearFiltering ? PSYX_NATIVE_SAMPLE_TRILINEAR : PSYX_NATIVE_SAMPLE_NEAREST;
			draw.mesh = ready.meshes[part]; draw.identity = source.identity;
			memcpy(draw.world, source.placement.world, sizeof(draw.world));
			if (source.billboard)
				for (int row = 0; row < 3; ++row) for (int column = 0; column < 3; ++column)
					draw.world[column*4+row] = face_camera_work.m[row][column]/4096.0f *
						(row == 1 ? -1.0f : 1.0f) * (column == 1 ? -1.0f : 1.0f);
			for (unsigned int channel = 0; channel < 4; ++channel) draw.tint[channel] = 1;
			draws.push_back(draw);
			DeveloperNativePickSource pick={};
			pick.logicalModel=AssetCatalog_GetModelHandle(AssetCatalog_FindModel(source.placement.modelIndex));
			pick.renderedModel=source.geometry.model; pick.vertexModel=source.geometry.vertices;
			pick.mesh=draw.mesh; pick.material=ready.materials[part]; pick.identity=draw.identity;
			pick.modelIndex=source.placement.modelIndex;
			memcpy(pick.position,source.placement.position,sizeof(pick.position));
			pick.yaw=source.placement.yaw; pick.part=unsigned(part);
			pick.sourceVertices=cached->mesh.sourceVertices; pick.sourcePolygons=cached->mesh.sourcePolygons;
			pick.artwork=cached->mesh.parts[part].material;
			pickSources.push_back(pick);
		}
	}
	return PSYX_NATIVE_OK;
}
void Report(PsyXNativeResult failure)
{
	if (failure != PSYX_NATIVE_OK) { s_coverage.BeginPublication(); s_published = false; }
	PsyX_Native_SetFrameStatus(failure);
	if (failure != s_lastFailure)
	{
		if (failure == PSYX_NATIVE_OK) PsyX_Log_Info("Native region: snapshot ready; GPU effective mode must be checked separately\n");
		else PsyX_Log_Warning("Native region: %s; cells%u unavailable%u objects%u missingModels%u unavailableSources%u rejected%u invalidReferences%u; legacy world disabled\n",
			PsyX_Native_ResultName(failure), s_inventory.cells, s_inventory.unavailableCells, s_inventory.objects,
			s_inventory.missingModels, s_inventory.unavailableSources, s_inventory.rejectedSources, s_inventory.invalidReferences);
	}
	s_lastFailure = failure;
}
}
void DeveloperNativeRegion_Update(const NativeAsset::GeometryCache& geometry)
{
	if (s_mode != DeveloperNativeRegionMode::Render) return;
	// Resolve a completed click even when the inspector tab is hidden, before
	// the two bounded publication tables roll over. The retained copy still
	// revalidates both catalog and GPU identities when presented later.
	PsyXNativePickResult completedPick={};
	if (PsyX_Native_GetPickResult(&completedPick)==PSYX_NATIVE_OK && completedPick.hit)
	{
		DeveloperNativePickSource retained={};
		DeveloperNativeRegion_GetPickSource(completedPick,retained);
	}
	if (DeveloperNativeScene_GetRequested()) { Report(PSYX_NATIVE_UNSUPPORTED); return; }
	PsyX_Native_SetRequested(1);
	if (PsyX_GetRenderBackend() != PSYX_BACKEND_VULKAN || GameLevel != 0 || NumPlayers != 1)
	{ Report(PSYX_NATIVE_UNSUPPORTED); return; }
	if (s_inventory.water || s_inventory.unknown)
	{ Report(PSYX_NATIVE_UNSUPPORTED); return; }
	if (s_inventory.missingModels || s_inventory.unavailableSources ||
		s_inventory.rejectedSources || s_inventory.invalidReferences || s_inventory.capacityFailure || !s_inventory.usagePreserved)
	{ Report(PSYX_NATIVE_STALE); return; }
	if (!s_inventory.cells || s_inventory.unavailableCells) { Report(PSYX_NATIVE_PENDING); return; }
	try
	{
		s_coverage.BeginPublication();
		s_gpu.Prune(); s_materials.BeginFrame();
		std::vector<PsyXNativeInstance> draws;
		draws.reserve(s_inventory.partInstances);
		std::vector<DeveloperNativePickSource> pickSources;
		pickSources.reserve(s_inventory.partInstances);
		const PreparationClock::time_point start = PreparationClock::now();
		const PsyXNativeResult assembled = Assemble(geometry, draws, pickSources);
		s_assemblyCost.Add(ElapsedMilliseconds(start));
		if (assembled != PSYX_NATIVE_OK) { Report(assembled); return; }
		if (!s_coverage.CompletePublication()) { Report(PSYX_NATIVE_INVALID); return; }
		float rotation[9];
		for (int row = 0; row < 3; ++row) for (int column = 0; column < 3; ++column)
			rotation[row*3+column] = inv_camera_matrix.m[row][column]/4096.0f;
		const float camera[] = { float(camera_position.vx), float(camera_position.vy), float(camera_position.vz) };
		PsyXNativeSnapshot snapshot = {};
		snapshot.size = sizeof(snapshot); snapshot.version = PSYX_NATIVE_VERSION;
		snapshot.sceneGeneration = PsyX_Native_GetSceneGeneration(); snapshot.simulationTick = uint64_t(FrameCnt);
		if (!DeveloperNativeCamera_Build(&snapshot.view, rotation, camera, float(scr_z),
			float(current->disp.disp.w), float(current->disp.disp.h), float(g_windowWidth), float(g_windowHeight)))
		{ Report(PSYX_NATIVE_INVALID); return; }
		const PreparationClock::time_point backdropStart = PreparationClock::now();
		const PsyXNativeResult backdrop = DeveloperNativeBackdrop_Append(snapshot.view, draws);
		s_backdropCost.Add(ElapsedMilliseconds(backdropStart));
		if (backdrop != PSYX_NATIVE_OK) { Report(backdrop); return; }
		snapshot.instances = draws.data(); snapshot.instanceCount = unsigned(draws.size());
		const PsyXNativeResult published=PsyX_Native_Publish(&snapshot);
		if (published==PSYX_NATIVE_OK)
		{
			s_published = true;
			s_pickFrame=(s_pickFrame+1)%2;
			PickFrame& frame=s_pickFrames[s_pickFrame];
			frame.scene=snapshot.sceneGeneration; frame.tick=snapshot.simulationTick;
			frame.sources.swap(pickSources); // allocated before Publish; no source pointers
		}
		Report(published);
		if (!(FrameCnt%120))
		{
			const NativeAsset::RegionCoverageCounts& counts = s_coverage.Counts();
			PsyX_Log_Info("Native region coverage: tick%llu selected%u copied%u published%u worldParts%u/%u viewInside%u viewOutside%u viewUncopied%u duplicateView%u invalid%u; outside sources are omitted, never legacy recovery; GPU submission checked separately\n",
				(unsigned long long)s_coverageTick, counts.selected, counts.copied, counts.published,
				counts.publishedParts, counts.copiedParts, counts.viewInside, counts.viewOutside,
				counts.viewUncopied, counts.duplicateView, counts.invalid);
		}
		if (!(FrameCnt%120)) PsyX_Log_Info("Native region: tick%d sourceObjects%u worldAndBackdropInstances%u worldSourceTriangles%u worldGpuResources%u; region/visual acceptance pending\n",
			FrameCnt, unsigned(s_instances.size()), unsigned(draws.size()), s_inventory.triangles, unsigned(s_gpu.Count()));
		if (!(FrameCnt%120)) PsyX_Log_Info("Native backdrop CPU preparation: mean%.3f peak%.3f ms/%u samples; recent120 mean%.3f p95%.3f ms (not GPU timing)\n",
			s_backdropCost.total/std::max(1u,s_backdropCost.samples), s_backdropCost.peak, s_backdropCost.samples,
			s_backdropCost.RecentMean(), s_backdropCost.RecentP95());
		if (!(FrameCnt%120)) PsyX_Log_Info("Native region CPU preparation: collection mean%.3f peak%.3f ms/%u samples; material/instance assembly mean%.3f peak%.3f ms/%u samples (includes initial uploads)\n",
			s_collectionCost.total/std::max(1u,s_collectionCost.samples), s_collectionCost.peak, s_collectionCost.samples,
			s_assemblyCost.total/std::max(1u,s_assemblyCost.samples), s_assemblyCost.peak, s_assemblyCost.samples);
		if (!(FrameCnt%120)) PsyX_Log_Info("Native region CPU preparation recent120: collection mean%.3f p95%.3f ms; material/instance assembly mean%.3f p95%.3f ms (bounded recent window, not GPU timing)\n",
			s_collectionCost.RecentMean(), s_collectionCost.RecentP95(), s_assemblyCost.RecentMean(), s_assemblyCost.RecentP95());
	}
	catch (const std::bad_alloc&) { Report(PSYX_NATIVE_OUT_OF_BUDGET); }
}

void DeveloperNativeRegion_ObserveView(const void* packedObject)
{
	if (s_mode == DeveloperNativeRegionMode::Off) return;
	s_coverage.ObserveView(PackedSourceSlot(packedObject));
}
bool DeveloperNativeRegion_GetCoverage(NativeAsset::RegionCoverageCounts& counts, uint64_t& tick, bool& published)
{
	counts = s_coverage.Counts(); tick = s_coverageTick; published = s_published;
	return s_mode != DeveloperNativeRegionMode::Off;
}
PsyXNativeResult DeveloperNativeRegion_GetPickSource(const PsyXNativePickResult& pick,
	DeveloperNativePickSource& source)
{
	source=DeveloperNativePickSource();
	if (pick.reason!=PSYX_NATIVE_OK || !pick.hit) return pick.reason==PSYX_NATIVE_OK ? PSYX_NATIVE_NO_SNAPSHOT : pick.reason;
	PsyXNativePickResult live={};
	if (PsyX_Native_GetPickResult(&live)!=PSYX_NATIVE_OK || live.requestSerial!=pick.requestSerial ||
		live.submittedSerial!=pick.submittedSerial || !live.hit) return PSYX_NATIVE_STALE;
	if (s_resolvedPick!=pick.requestSerial)
	{
		const DeveloperNativePickSource* matched=NULL;
		for (unsigned int i=0;i<2;++i)
		{
			const PickFrame& frame=s_pickFrames[i];
			if (frame.scene!=pick.sceneGeneration || frame.tick!=pick.simulationTick || pick.instanceIndex>=frame.sources.size()) continue;
			const DeveloperNativePickSource& candidate=frame.sources[pick.instanceIndex];
			if (candidate.identity==pick.identity && candidate.mesh.slot==pick.mesh.slot && candidate.mesh.generation==pick.mesh.generation &&
				candidate.material.slot==pick.material.slot && candidate.material.generation==pick.material.generation) matched=&candidate;
		}
		if (!matched) return PSYX_NATIVE_STALE;
		s_pickSource=*matched; s_resolvedPick=pick.requestSerial;
	}
	if (!AssetCatalog_HandleValid(s_pickSource.logicalModel) || !AssetCatalog_HandleValid(s_pickSource.renderedModel) ||
		!AssetCatalog_HandleValid(s_pickSource.vertexModel)) return PSYX_NATIVE_STALE;
	source=s_pickSource;
	return PSYX_NATIVE_OK;
}
#endif
