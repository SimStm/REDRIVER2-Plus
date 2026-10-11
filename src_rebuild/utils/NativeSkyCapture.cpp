#include "NativeSkyCapture.h"
#ifdef _WIN32
#include "../Game/driver2.h"
#include "../Game/C/models.h"
#include "../Game/C/main.h"
#include "../Game/C/mission.h"
#include "../Game/C/assetcatalog_game.h"
#include "../native/NativeRenderer.h"
#include "PsyX/PsyX_globals.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <exception>
namespace {
std::shared_ptr<const NativeSkyAssets> assets;
NativeSkyEpoch epoch;
unsigned revision = 0;
char failure[192] = {};
bool active = false;
bool Span(MODEL* model, int bytes, const void* pointer, size_t length)
{
	const uintptr_t begin = (uintptr_t)model, address = (uintptr_t)pointer;
	return bytes >= (int)sizeof(MODEL) && bytes <= 1024 * 1024 && address >= begin &&
		address - begin <= (size_t)bytes && length <= (size_t)bytes - (address - begin);
}
bool CaptureModels(NativeSkyAssets& owned, const unsigned char* lookup, const unsigned char* textures)
{
	const int slots[4] = { 0, 2, 3, 1 };
	for (int m = 0; m < 4; ++m)
	{
		const int slot = slots[m]; MODEL* model = modelpointers[slot];
		if (!model || model_byte_sizes[slot] < (int)sizeof(MODEL))
		{
			snprintf(failure, sizeof(failure), "horizon slot=%d MODEL header unavailable bytes=%d", slot, model_byte_sizes[slot]);
			return false;
		}
		const unsigned count = model->num_vertices, polygons = model->num_polys;
		const unsigned offset = lookup[m];
		const int parent = model->instance_number == -1 ? slot : model->instance_number;
		const auto fail = [&](const char* reason) {
			snprintf(failure, sizeof(failure), "horizon slot=%d parent=%d vertices=%u polys=%u offset=%u bytes=%d: %s",
				slot, parent, count, polygons, offset, model_byte_sizes[slot], reason);
			return false;
		};
		if (count < 15 || count > 256 || count % 3 || polygons <= 8 || offset + polygons > 40) return fail("counts/selectors");
		if (parent < 0 || parent >= MAX_MODEL_SLOTS || !modelpointers[parent]) return fail("parent unavailable");
		if (!Span(modelpointers[parent], model_byte_sizes[parent], modelpointers[parent], sizeof(MODEL)) ||
			count > modelpointers[parent]->num_vertices) return fail("parent header/vertex count");
		const SVECTOR* vertices = GET_MODEL_DATA(SVECTOR, model, vertices);
		const unsigned char* polys = GET_MODEL_DATA(unsigned char, model, poly_block);
		if (!Span(modelpointers[parent], model_byte_sizes[parent], vertices, count * sizeof(SVECTOR))) return fail("vertex allocation span");
		if (!Span(model, model_byte_sizes[slot], polys, 1)) return fail("polygon header allocation span");
		const unsigned type = polys[0], stride = NativeSkyMesh_ModelStride(type);
		if (!stride || !Span(model, model_byte_sizes[slot], polys, polygons * stride))
		{
			char reason[80]; snprintf(reason, sizeof(reason), "polygon type=%u stride=%u allocation span", type, stride);
			return fail(reason);
		}
		static_assert(sizeof(SVECTOR) == 8, "original horizon SVECTOR layout differs");
		NativeSkyModelInput input = { (const unsigned char*)vertices, count * sizeof(SVECTOR), polys, polygons * stride,
			count, polygons, textures, 40, offset, (unsigned char)m };
		std::vector<NativeSkyQuad> quads;
		char modelError[96] = {};
		if (!NativeSkyMesh_CopyModel(input, &quads, owned.gateVertices[m], modelError, sizeof(modelError))) return fail(modelError);
		owned.quads.insert(owned.quads.end(), quads.begin(), quads.end());
	}
	return true;
}
}
void NativeSkyCapture_BeginFrame()
{
	epoch.Begin(); active = NativeRenderer_IsActive() && AssetCatalogGame_RegionDrawRequested();
}
void NativeSkyCapture_SelectView(int view) { epoch.Select(view); }
void NativeSkyCapture_Load(const unsigned char* raw, int bytes, int level, int segment,
	const unsigned char* uv, const short* pages, const short* cluts, const unsigned char* lookup, const unsigned char* textures)
{
	++revision; assets.reset(); failure[0] = 0;
	if (!NativeRenderer_IsActive()) return;
	try
	{
		if (!raw || bytes != 65536 || level < 0 || level > 3 || !uv || !pages || !cluts || !lookup || !textures)
			{ snprintf(failure, sizeof(failure), "original sky segment read is incomplete or invalid (%d bytes)", bytes); return; }
		std::shared_ptr<NativeSkyAssets> owned(new NativeSkyAssets);
		owned->generation = AssetCatalogGame_LevelGeneration(); owned->revision = revision; owned->level = level; owned->segment = segment;
		owned->raw.assign(raw, raw + bytes);
		for (int t = 0; t < 28; ++t) { memcpy(owned->tiles[t].uv, uv + t * 8, 8); owned->tiles[t].page = (unsigned short)pages[t]; owned->tiles[t].clut = (unsigned short)cluts[t]; }
		if (!CaptureModels(*owned, lookup, textures)) return;
		assets = owned;
		PsyX_Log_Info("native sky: captured level=%d segment=%d generation=%u revision=%u bytes=%d quads=%d\n",
			level, segment, owned->generation, revision, bytes, (int)owned->quads.size());
	}
	catch (const std::exception& e) { snprintf(failure, sizeof(failure), "%s", e.what()); }
	catch (...) { snprintf(failure, sizeof(failure), "original sky owned capture failed"); }
}
void NativeSkyCapture_Record(const short rotation[3][3], int yOffset, const int rgb[3])
{
	if (!active || !assets || assets->generation != AssetCatalogGame_LevelGeneration() || assets->level != GameLevel) return;
	NativeSkyFrame& frame = epoch.captured; frame = NativeSkyFrame();
	frame.epoch = epoch.frame; frame.view = epoch.view; frame.generation = assets->generation; frame.revision = assets->revision; frame.selected = true;
	for (int c = 0; c < 3; ++c) frame.rgb[c] = rgb[c];
	NativeSkyMesh_Pose(rotation, assets->gateVertices, yOffset, &frame);
}
std::shared_ptr<const NativeSkyAssets> NativeSkyCapture_Read(unsigned* outRevision, const char** error)
{
	if (outRevision) *outRevision = revision;
	if (error) *error = failure;
	return assets;
}
bool NativeSkyCapture_Consume(int view, unsigned generation, unsigned currentRevision, NativeSkyFrame* out) { return epoch.Consume(view, generation, currentRevision, out); }
void NativeSkyCapture_Shutdown() { assets.reset(); epoch.Begin(); active = false; failure[0] = 0; }
#else
void NativeSkyCapture_BeginFrame() {}
void NativeSkyCapture_SelectView(int) {}
void NativeSkyCapture_Load(const unsigned char*, int, int, int, const unsigned char*, const short*, const short*, const unsigned char*, const unsigned char*) {}
void NativeSkyCapture_Record(const short[3][3], int, const int[3]) {}
std::shared_ptr<const NativeSkyAssets> NativeSkyCapture_Read(unsigned* r, const char** e) { if (r) *r = 0; if (e) *e = "native sky unavailable on this target"; return {}; }
bool NativeSkyCapture_Consume(int, unsigned, unsigned, NativeSkyFrame*) { return false; }
void NativeSkyCapture_Shutdown() {}
#endif
