#include "NativeWheelScene.h"
#ifdef _WIN32
#include "../Game/driver2.h"
#include "../Game/C/cars.h"
#include "NativeWheelMesh.h"
#include "NativeCarScene.h"
#include "PsyX/PsyX_public.h"
#include <future>
#include <chrono>
#include <memory>
#include <direct.h>
#include <stdio.h>
#include <string.h>

namespace
{
enum class State { Empty, Waiting, Converting, Ready, Failed };
struct WheelResource
{
	State state = State::Empty;
	NativeWheelSourceIdentity source = {};
	std::shared_ptr<NativeWheelCapture> capture;
	std::vector<float> positions;
	bool seen = false;
	char key[NATIVE_RESOURCE_KEY_CAPACITY] = {};
};
struct ConversionResult
{
	WorldMeshConversionResult converted = {};
	int code = ASSET_CONVERTER_ERR_ARGUMENT;
	char error[256] = {};
};
static WheelResource resources[MAX_CARS * 4];
static char retired[MAX_CARS * 4][NATIVE_RESOURCE_KEY_CAPACITY];
static int retiredCount = 0;
static unsigned generation = 0, taskGeneration = 0, serial = 0;
static int taskIndex = -1;
static NativeWheelSourceIdentity taskSource = {};
static std::future<ConversionResult> task;
static NativeWheelSceneCounts counts = {};
static bool missing[MAX_CARS * 4] = {};

void ReleaseResources()
{
	for (int i = 0; i < MAX_CARS * 4; ++i)
	{
		if (resources[i].key[0]) NativeRenderer_UnregisterResource(resources[i].key);
		resources[i] = WheelResource();
	}
	for (int i = 0; i < retiredCount; ++i) NativeRenderer_UnregisterResource(retired[i]);
	retiredCount = 0;
	memset(missing, 0, sizeof(missing));
}

void Poll()
{
	if (!task.valid() || task.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
	const ConversionResult result = task.get();
	if (taskGeneration != generation || taskIndex < 0) return;
	WheelResource& resource = resources[taskIndex];
	NativeWheelSourceIdentity current = {};
	if (resource.state != State::Converting || !NativeWheelMesh_SameSource(resource.source, taskSource)) return;
	if (!Cars_GetNativeWheelSource(taskIndex / 4, taskIndex % 4, &current) || !NativeWheelMesh_SameSource(current, taskSource))
	{
		// The completed task was consumed. A rejected frame may retain this
		// slot, so leave it retryable rather than waiting on a nonexistent task.
		resource.state = State::Waiting;
		return;
	}
	char error[256] = {};
	if (result.code == ASSET_CONVERTER_OK)
	{
		snprintf(resource.key, sizeof(resource.key), "wheel:%u:%d:%u:%s", generation, taskIndex, ++serial, result.converted.resourceKey);
		if (NativeRenderer_RegisterDynamicResource(resource.key, result.converted.asset.glbPath, error, sizeof(error)))
		{
			resource.state = State::Ready;
			PsyX_Log_Info("native wheel: ready car=%d slot=%d vertices=%d key=%s\n", taskIndex / 4, taskIndex % 4,
				(int)resource.capture->cornerVertices.size(), resource.key);
			return;
		}
	}
	resource.key[0] = 0;
	resource.state = State::Failed;
	PsyX_Log_Warning("native wheel: resource unavailable car=%d slot=%d: %s\n", taskIndex / 4, taskIndex % 4,
		result.code == ASSET_CONVERTER_OK ? error : result.error);
}

void Start(int index)
{
	WheelResource& resource = resources[index];
	try
	{
		resource.capture.reset(new NativeWheelCapture);
		char error[256] = {};
		if (!Cars_CaptureNativeWheel(index / 4, index % 4, resource.capture.get(), error, sizeof(error)))
		{
			resource.state = State::Failed;
			PsyX_Log_Warning("native wheel: capture failed car=%d slot=%d: %s\n", index / 4, index % 4, error);
			return;
		}
		_mkdir("native_cache");
		const std::shared_ptr<const NativeWheelCapture> owned = resource.capture;
		task = std::async(std::launch::async, [owned]() {
			ConversionResult result;
			try { result.code = WorldMeshConversion_Publish("native_cache", owned->conversion, &result.converted, result.error, sizeof(result.error)); }
			catch (const std::exception& e) { snprintf(result.error, sizeof(result.error), "%s", e.what()); }
			catch (...) { snprintf(result.error, sizeof(result.error), "unknown wheel conversion exception"); }
			return result;
		});
		taskGeneration = generation; taskIndex = index; taskSource = resource.source;
		resource.state = State::Converting;
	}
	catch (const std::exception& e) { resource.state = State::Failed; PsyX_Log_Warning("native wheel: capture/task failed: %s\n", e.what()); }
	catch (...) { resource.state = State::Failed; PsyX_Log_Warning("native wheel: unknown capture/task failure\n"); }
}
}

int NativeWheelScene_ResourceReserve() { return MAX_CARS * 4 * 2; }
void NativeWheelScene_ClearFrameCounts() { counts = NativeWheelSceneCounts(); }
void NativeWheelScene_RejectFrame()
{
	counts.submitted = 0;
	// The caller has published an empty scene with ClearInstances; old keys are
	// now safe to release even when the replacement update/submission failed.
	for (int i = 0; i < retiredCount; ++i) NativeRenderer_UnregisterResource(retired[i]);
	retiredCount = 0;
}
NativeWheelSceneCounts NativeWheelScene_ReadCounts() { return counts; }

void NativeWheelScene_Begin(unsigned currentGeneration)
{
	counts = NativeWheelSceneCounts();
	if (generation != currentGeneration) { ReleaseResources(); generation = currentGeneration; }
	Poll();
	for (int i = 0; i < MAX_CARS * 4; ++i) resources[i].seen = false;
}

bool NativeWheelScene_Append(int carId, bool hidden, const NativeInstance& car, NativeInstance* combined, int* count)
{
	if (carId < 0 || carId >= MAX_CARS) return false;
	for (int slot = 0; slot < 4; ++slot)
	{
		const int index = carId * 4 + slot;
		WheelResource& resource = resources[index];
		NativeWheelSourceIdentity source = {};
		if (!Cars_GetNativeWheelSource(carId, slot, &source))
		{
			++counts.failed;
			if (!missing[index]) PsyX_Log_Warning("native wheel: original source missing car=%d slot=%d\n", carId, slot);
			missing[index] = true;
			continue;
		}
		missing[index] = false;
		if (resource.state != State::Empty && !NativeWheelMesh_SameSource(resource.source, source))
		{
			if (resource.key[0])
			{
				if (retiredCount == MAX_CARS * 4) { PsyX_Log_Warning("native wheel: retirement capacity exceeded\n"); return false; }
				snprintf(retired[retiredCount++], NATIVE_RESOURCE_KEY_CAPACITY, "%s", resource.key);
			}
			resource = WheelResource();
		}
		if (resource.state == State::Empty)
		{
			if (hidden) continue;
			resource.source = source; resource.state = State::Waiting;
		}
		resource.seen = true;
		if (resource.state == State::Waiting && !hidden && !task.valid()) Start(index);
		if (resource.state == State::Failed) { ++counts.failed; continue; }
		if (resource.state != State::Ready) continue;
		++counts.ready;
		if (hidden) continue;
		NativeInstance instance = {};
		int sine = 0, cosine = 0;
		char error[256] = {};
		if (!Cars_GetNativeWheelPose(carId, slot, instance.transform, &sine, &cosine) ||
			!NativeWheelMesh_Deform(*resource.capture, source.wheelSize, sine, cosine, &resource.positions, error, sizeof(error)) ||
			!NativeRenderer_UpdateResourcePositions(resource.key, resource.positions.data(), (int)resource.positions.size() / 3, error, sizeof(error)))
		{
			resource.state = State::Failed; ++counts.failed;
			PsyX_Log_Warning("native wheel: pose/update failed car=%d slot=%d: %s\n", carId, slot, error[0] ? error : "wheel pose unavailable");
			return false;
		}
		snprintf(instance.resourceKey, sizeof(instance.resourceKey), "%s", resource.key);
		instance.level = car.level; instance.variant = car.variant;
		instance.sourceObjectIndex = carId; instance.modelSlot = source.car.model;
		instance.modelRevision = (int)source.car.revision;
		if (!NativeCarScene_Append(instance, combined, NATIVE_MAX_INSTANCES, count))
			{ PsyX_Log_Warning("native wheel: combined instance capacity exceeded\n"); return false; }
		++counts.submitted;
	}
	return true;
}

void NativeWheelScene_Commit()
{
	for (int i = 0; i < retiredCount; ++i) NativeRenderer_UnregisterResource(retired[i]);
	retiredCount = 0;
	for (int i = 0; i < MAX_CARS * 4; ++i)
		if (resources[i].state != State::Empty && !resources[i].seen)
		{
			if (resources[i].key[0]) NativeRenderer_UnregisterResource(resources[i].key);
			resources[i] = WheelResource();
		}
}
void NativeWheelScene_Shutdown()
{
	if (task.valid()) task.get(); // explicit shutdown boundary, never a display frame
	ReleaseResources();
}
#endif
