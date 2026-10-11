#include "NativePedScene.h"
#ifdef _WIN32
#include "NativePedCapture.h"
#include "NativeCarScene.h"
#include "PsyX/PsyX_globals.h"
#include <future>
#include <chrono>
#include <direct.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

namespace {
enum class State { Empty, Waiting, Converting, Ready, Failed };
struct Resource
{
	State state = State::Empty;
	unsigned incarnation = 0;
	int type = 0, palette = 0;
	bool seen = false, errorReported = false, poseReported = false;
	std::shared_ptr<const NativePedMeshCapture> capture;
	std::vector<float> positions, uvs;
	char key[NATIVE_RESOURCE_KEY_CAPACITY] = {};
};
struct Result { WorldMeshConversionResult converted = {}; int code = ASSET_CONVERTER_ERR_ARGUMENT; char error[256] = {}; };
static Resource resources[NATIVE_PED_SLOTS];
static char retired[NATIVE_PED_SLOTS][NATIVE_RESOURCE_KEY_CAPACITY];
static int retiredCount = 0, taskSlot = -1;
static unsigned generation = 0, taskGeneration = 0, serial = 0;
static std::shared_ptr<const NativePedMeshCapture> taskCapture;
static std::future<Result> task;
static NativePedSceneCounts counts = {};

void RetireOld()
{
	for (int i = 0; i < retiredCount; ++i) NativeRenderer_UnregisterResource(retired[i]);
	retiredCount = 0;
}
void Release()
{
	for (int slot = 0; slot < NATIVE_PED_SLOTS; ++slot) { if (resources[slot].key[0]) NativeRenderer_UnregisterResource(resources[slot].key); resources[slot] = Resource(); }
	RetireOld();
}
bool SameActor(const Resource& resource, const NativePedFrame& frame)
{
	return NativePedMesh_IsCurrentActor(frame, resource.incarnation, resource.type, resource.palette, generation, generation);
}
void Report(Resource& resource, int slot, const char* error)
{
	if (!resource.errorReported) PsyX_Log_Warning("native pedestrian: unavailable slot=%d incarnation=%u type=%d palette=%d: %s\n", slot, resource.incarnation, resource.type, resource.palette, error);
	resource.errorReported = true;
}
void Poll(const NativePedFrame* frames)
{
	if (!task.valid() || task.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
	const Result result = task.get();
	if (taskGeneration != generation || taskSlot < 0 || taskSlot >= NATIVE_PED_SLOTS) return;
	Resource& resource = resources[taskSlot];
	if (resource.state != State::Converting || resource.capture != taskCapture) return;
	if (!frames || !SameActor(resource, frames[taskSlot]) || (frames[taskSlot].selected && !NativePedMesh_SamePoseSource(frames[taskSlot], *taskCapture)))
		{ resource.state = State::Waiting; return; }
	char error[256] = {};
	if (result.code == ASSET_CONVERTER_OK)
	{
		snprintf(resource.key, sizeof(resource.key), "ped:%u:%d:%u:%s", generation, taskSlot, ++serial, result.converted.resourceKey);
		if (NativeRenderer_RegisterDynamicResource(resource.key, result.converted.asset.glbPath, error, sizeof(error)))
		{
			resource.state = State::Ready;
			PsyX_Log_Info("native pedestrian: ready slot=%d incarnation=%u type=%d palette=%d pad=%d corners=%d components=%d key=%s\n", taskSlot,
				resource.incarnation, resource.type, resource.palette, frames[taskSlot].pad, (int)resource.capture->bindings.size(), (int)resource.capture->identities.size(), resource.key);
			return;
		}
	}
	resource.key[0] = 0; resource.state = State::Failed;
	Report(resource, taskSlot, result.code == ASSET_CONVERTER_OK ? error : result.error);
}
void Start(int slot)
{
	Resource& resource = resources[slot];
	try
	{
		_mkdir("native_cache");
		const std::shared_ptr<const NativePedMeshCapture> owned = resource.capture;
		task = std::async(std::launch::async, [owned]() {
			Result result;
			try { result.code = WorldMeshConversion_Publish("native_cache", owned->conversion, &result.converted, result.error, sizeof(result.error)); }
			catch (const std::exception& e) { snprintf(result.error, sizeof(result.error), "%s", e.what()); }
			catch (...) { snprintf(result.error, sizeof(result.error), "unknown pedestrian conversion exception"); }
			return result;
		});
		taskSlot = slot; taskGeneration = generation; taskCapture = owned; resource.state = State::Converting;
	}
	catch (const std::exception& e) { resource.state = State::Failed; Report(resource, slot, e.what()); }
	catch (...) { resource.state = State::Failed; Report(resource, slot, "pedestrian conversion task creation failed"); }
}
bool Replace(Resource& resource)
{
	if (resource.key[0])
	{
		if (retiredCount == NATIVE_PED_SLOTS) { PsyX_Log_Warning("native pedestrian: retirement capacity exceeded\n"); return false; }
		snprintf(retired[retiredCount++], NATIVE_RESOURCE_KEY_CAPACITY, "%s", resource.key);
	}
	resource = Resource(); return true;
}
}

int NativePedScene_ResourceReserve() { return NATIVE_PED_SLOTS * 2; }
void NativePedScene_ClearFrameCounts() { counts = NativePedSceneCounts(); }
NativePedSceneCounts NativePedScene_ReadCounts() { return counts; }
bool NativePedScene_Append(unsigned current, int view, int level, int variant, NativeInstance* combined, int* count)
{
	counts = NativePedSceneCounts();
	if (generation != current) { Release(); generation = current; }
	const NativePedFrame* frames = nullptr;
	NativePedCapture_Consume(view, &frames);
	Poll(frames);
	for (int slot = 0; slot < NATIVE_PED_SLOTS; ++slot) resources[slot].seen = false;
	if (!frames) return true; // missed/no-ped frame never reuses old evaluated poses
	try
	{
		for (int slot = 0; slot < NATIVE_PED_SLOTS; ++slot)
		{
			const NativePedFrame& frame = frames[slot]; Resource& resource = resources[slot];
			if (!frame.active) continue;
			++counts.active;
			if (resource.state != State::Empty && (!SameActor(resource, frame) || (resource.capture && frame.selected && !frame.parts.empty() && !NativePedMesh_SamePoseSource(frame, *resource.capture))))
				if (!Replace(resource)) return false;
			resource.incarnation = frame.incarnation; resource.type = frame.type; resource.palette = frame.palette; resource.seen = true;
			if (resource.state == State::Ready) ++counts.ready;
			if (!frame.selected || frame.hidden) continue;
			if (frame.failed) { ++counts.failed; Report(resource, slot, frame.error); continue; }
			if (frame.parts.empty()) { ++counts.waiting; continue; }
			if (resource.state == State::Empty)
			{
				std::shared_ptr<NativePedMeshCapture> capture(new NativePedMeshCapture); char error[192] = {};
				if (!NativePedMesh_Build(frame, capture.get(), error, sizeof(error)))
					{ resource.state = State::Failed; ++counts.failed; Report(resource, slot, error); continue; }
				resource.capture = capture; resource.state = State::Waiting;
			}
			if (resource.state == State::Waiting && !task.valid()) Start(slot);
			if (resource.state == State::Failed) { ++counts.failed; continue; }
			if (resource.state != State::Ready) { ++counts.waiting; continue; }
			char error[192] = {};
			if (!NativePedMesh_ExpandPose(frame, *resource.capture, &resource.positions, &resource.uvs, error, sizeof(error)) ||
				!NativeRenderer_UpdateResourceVertices(resource.key, resource.positions.data(), resource.uvs.data(), (int)resource.positions.size() / 3, error, sizeof(error)))
				{ resource.state = State::Failed; ++counts.failed; Report(resource, slot, error); return false; }
			if (!resource.poseReported && getenv("RED2_NATIVE_PERF") && strcmp(getenv("RED2_NATIVE_PERF"), "1") == 0)
			{
				float minY = resource.positions[1], maxY = minY;
				for (size_t v = 1; v < resource.positions.size() / 3; ++v) { const float y = resource.positions[v * 3 + 1]; if (y < minY) minY = y; if (y > maxY) maxY = y; }
				PsyX_Log_Info("native pedestrian: first pose slot=%d incarnation=%u type=%d pad=%d position=(%d,%d,%d) localY=(%.1f,%.1f) worldY=(%.1f,%.1f)\n", slot, frame.incarnation, frame.type, frame.pad, frame.position[0], frame.position[1], frame.position[2], minY, maxY, frame.position[1] + minY, frame.position[1] + maxY);
				resource.poseReported = true;
			}
			NativeInstance instance = {}; snprintf(instance.resourceKey, sizeof(instance.resourceKey), "%s", resource.key);
			instance.transform[0] = instance.transform[5] = instance.transform[10] = instance.transform[15] = 1;
			for (int axis = 0; axis < 3; ++axis) instance.transform[12 + axis] = (float)frame.position[axis];
			instance.level = level; instance.variant = variant; instance.sourceObjectIndex = slot; instance.modelSlot = frame.type; instance.modelRevision = (int)frame.incarnation;
			if (!NativeCarScene_Append(instance, combined, NATIVE_MAX_INSTANCES, count)) { PsyX_Log_Warning("native pedestrian: combined instance capacity exceeded\n"); return false; }
			++counts.submitted;
		}
	}
	catch (const std::exception& e) { PsyX_Log_Warning("native pedestrian: frame capture/update failed: %s\n", e.what()); ++counts.failed; return false; }
	catch (...) { PsyX_Log_Warning("native pedestrian: unknown frame capture/update failure\n"); ++counts.failed; return false; }
	return true;
}
void NativePedScene_Commit()
{
	RetireOld();
	for (int slot = 0; slot < NATIVE_PED_SLOTS; ++slot)
		if (!resources[slot].seen && resources[slot].state != State::Empty)
			{ if (resources[slot].key[0]) NativeRenderer_UnregisterResource(resources[slot].key); resources[slot] = Resource(); }
}
void NativePedScene_RejectFrame() { counts.submitted = 0; RetireOld(); }
void NativePedScene_Shutdown()
{
	if (task.valid()) task.get(); // explicit shutdown boundary; never a normal display frame
	taskCapture.reset(); Release(); NativePedCapture_Shutdown();
}
#endif
