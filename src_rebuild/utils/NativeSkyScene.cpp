#include "NativeSkyScene.h"
#ifdef _WIN32
#include "NativeSkyCapture.h"
#include "../native/NativeRenderer.h"
#include "PsyX/PsyX_globals.h"
#include <future>
#include <chrono>
#include <direct.h>
#include <stdio.h>
#include <string.h>
namespace {
enum class State { Waiting, Converting, Ready, Failed };
struct Result { WorldMeshConversionResult converted = {}; int code = ASSET_CONVERTER_ERR_ARGUMENT; char error[256] = {}; };
State state = State::Waiting;
unsigned generation = 0, revision = 0, taskGeneration = 0, taskRevision = 0;
std::future<Result> task;
char key[NATIVE_RESOURCE_KEY_CAPACITY] = {}, retired[NATIVE_RESOURCE_KEY_CAPACITY] = {};
bool reported = false, poseReported = false;
void Report(const char* error)
{
	if (!reported) PsyX_Log_Warning("native sky: unavailable generation=%u revision=%u: %s\n", generation, revision, error);
	reported = true;
}
void Retire()
{
	if (retired[0]) NativeRenderer_UnregisterResource(retired);
	retired[0] = 0;
}
void Reset(unsigned currentGeneration, unsigned currentRevision)
{
	NativeRenderer_ClearSky(); Retire();
	if (key[0]) snprintf(retired, sizeof(retired), "%s", key);
	key[0] = 0; generation = currentGeneration; revision = currentRevision; state = State::Waiting; reported = poseReported = false;
}
void Poll()
{
	if (!task.valid() || task.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
	const Result result = task.get();
	if (taskGeneration != generation || taskRevision != revision || state != State::Converting) return;
	if (result.code != ASSET_CONVERTER_OK) { state = State::Failed; Report(result.error); return; }
	char error[256] = {};
	snprintf(key, sizeof(key), "sky:%u:%u:%s", generation, revision, result.converted.resourceKey);
	if (!NativeRenderer_RegisterResource(key, result.converted.asset.glbPath, error, sizeof(error)))
		{ key[0] = 0; state = State::Failed; Report(error); return; }
	state = State::Ready;
	PsyX_Log_Info("native sky: ready generation=%u revision=%u triangles=%d key=%s\n", generation, revision, result.converted.built.triangleCount, key);
}
void Start(const std::shared_ptr<const NativeSkyAssets>& owned)
{
	try
	{
		_mkdir("native_cache");
		PsyX_Log_Info("native sky: converting level=%d segment=%d generation=%u revision=%u\n", owned->level, owned->segment, generation, revision);
		task = std::async(std::launch::async, [owned]() {
			Result result;
			try { WorldMeshConversionInput input; if (NativeSkyMesh_Build(*owned, &input, result.error, sizeof(result.error)))
				result.code = WorldMeshConversion_Publish("native_cache", input, &result.converted, result.error, sizeof(result.error)); }
			catch (const std::exception& e) { snprintf(result.error, sizeof(result.error), "%s", e.what()); }
			catch (...) { snprintf(result.error, sizeof(result.error), "original sky conversion failed"); }
			return result;
		});
		taskGeneration = generation; taskRevision = revision; state = State::Converting;
	}
	catch (const std::exception& e) { state = State::Failed; Report(e.what()); }
	catch (...) { state = State::Failed; Report("original sky worker creation failed"); }
}
}
int NativeSkyScene_ResourceReserve() { return 2; }
void NativeSkyScene_Update(unsigned current, int view)
{
	unsigned sourceRevision = 0; const char* sourceError = nullptr;
	const std::shared_ptr<const NativeSkyAssets> owned = NativeSkyCapture_Read(&sourceRevision, &sourceError);
	NativeRenderer_ClearSky(); // no captured current pose means no sky, even with a resident key
	if (generation != current || revision != sourceRevision) Reset(current, sourceRevision);
	Poll();
	NativeSkyFrame frame;
	const bool selected = NativeSkyCapture_Consume(view, current, sourceRevision, &frame);
	if (!owned || owned->generation != current || owned->revision != sourceRevision)
	{
		if (sourceError && sourceError[0]) Report(sourceError);
		Retire(); return;
	}
	if (selected && state == State::Waiting && !task.valid()) Start(owned);
	if (state == State::Ready && !NativeRenderer_ResourceResident(key))
		{ key[0] = 0; state = State::Waiting; reported = false; }
	if (selected && state == State::Ready)
	{
		char error[256] = {};
		if (!NativeRenderer_SetSky(key, frame.rotation, frame.rgb, frame.enabled, error, sizeof(error)))
			{ state = State::Failed; Report(error); }
		else if (!poseReported)
		{
			PsyX_Log_Info("native sky: submitted generation=%u revision=%u view=%d epoch=%u rgb=%d,%d,%d models=%u,%u,%u,%u yOffset=%.0f key=%s\n",
				generation, revision, view, frame.epoch, frame.rgb[0], frame.rgb[1], frame.rgb[2], frame.enabled[0], frame.enabled[1], frame.enabled[2], frame.enabled[3], frame.rotation[13], key);
			poseReported = true;
		}
	}
	Retire(); // ClearSky/SetSky removed every reference to the previous publication
}
void NativeSkyScene_Shutdown()
{
	NativeRenderer_ClearSky();
	if (task.valid()) task.wait(); // shutdown only, never in presentation frames
	if (key[0]) NativeRenderer_UnregisterResource(key);
	key[0] = 0; Retire(); NativeSkyCapture_Shutdown(); state = State::Waiting; generation = revision = 0;
}
#else
int NativeSkyScene_ResourceReserve() { return 0; }
void NativeSkyScene_Update(unsigned, int) {}
void NativeSkyScene_Shutdown() {}
#endif
