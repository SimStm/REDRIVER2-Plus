#include "../utils/NativeSkyMesh.h"
#include "../utils/NativeCameraView.h"
#include <stdio.h>
#include <math.h>
#include <limits>
static int checks = 0, failures = 0;
static void Check(bool ok, const char* message) { ++checks; if (!ok) { ++failures; printf("FAIL %s\n", message); } }
static NativeSkyAssets Fixture()
{
	NativeSkyAssets assets; assets.raw.resize(65536, 0x11); assets.generation = 2; assets.revision = 4;
	for (int t = 0; t < 28; ++t)
	{
		assets.tiles[t].page = 5; assets.tiles[t].clut = (unsigned short)((252 << 6) | 20);
		for (int c = 0; c < 8; ++c) assets.tiles[t].uv[c] = (unsigned char)(11 + c * 17);
	}
	// CLUT entry1 is red. TextureDecode preserves original 5-bit expansion.
	const unsigned offset = (252 * 128 + 1) * 2; assets.raw[offset] = 31; assets.raw[offset + 1] = 0;
	NativeSkyQuad quad = {}; quad.model = 2;
	const float positions[12] = { -12,-8,100, 15,-7,110, -13,9,120, 16,10,130 };
	for (int i = 0; i < 12; ++i) quad.positions[i] = positions[i];
	assets.quads.push_back(quad); quad.reflection = true; quad.model = 1; assets.quads.push_back(quad);
	return assets;
}
static void CaptureAndBounds()
{
	NativeSkyAssets assets = Fixture(); WorldMeshConversionInput out; char error[192] = {};
	Check(NativeSkyMesh_Build(assets, &out, error, sizeof(error)), "owned original sky builds");
	Check(out.source.positions.size() == 36 && out.source.indices.size() == 12 && out.pixels.size() == 1 && out.pixels[0].size() == 256u * 256u * 4u, "shared page is decoded once, complete quad inventory retained");
	const int expectedVertex[6] = { 0,1,3,1,2,3 }, expectedUv[6] = { 2,3,0,3,1,0 };
	for (int c = 0; c < 6; ++c)
	{
		Check(out.source.positions[c * 3 + 2] == assets.quads[0].positions[expectedVertex[c] * 3 + 2], "asymmetric quad uses original GPU diagonal");
		Check(out.source.uvs[c * 2] == assets.tiles[0].uv[expectedUv[c] * 2] && out.source.uvs[c * 2 + 1] == assets.tiles[0].uv[expectedUv[c] * 2 + 1], "asymmetric sky UV order matches packet corners");
	}
	Check(out.legacyColors[0] == 128 && out.legacyColors[18] == 64 && out.worldShadeIndices[0] == 2 && out.worldShadeIndices[6] == 1, "original model selection and reflection provenance survive grouping");
	Check(out.pixels[0][0] > 240 && out.pixels[0][1] == 0 && out.pixels[0][3] == 255, "sky block CLUT and page addressing decode original red");
	assets.raw[2] = 0; Check(out.pixels[0][0] > 240, "decoded ownership survives source mutation");
	assets.raw.pop_back(); Check(!NativeSkyMesh_Build(assets, &out, error, sizeof(error)) && out.source.positions.empty(), "short SKY segment fails without partial output");
	assets = Fixture(); assets.quads[0].positions[11] = std::numeric_limits<float>::quiet_NaN(); Check(!NativeSkyMesh_Build(assets, &out, error, sizeof(error)), "late nonfinite corner rejected before decode");
	assets = Fixture(); assets.quads[0].tile = 28; Check(!NativeSkyMesh_Build(assets, &out, error, sizeof(error)), "invalid tile selector rejected");
	assets = Fixture(); assets.tiles[0].page = 4; Check(!NativeSkyMesh_Build(assets, &out, error, sizeof(error)), "page outside owned block fails explicitly");
	assets = Fixture(); assets.quads.resize(161, assets.quads[0]); Check(!NativeSkyMesh_Build(assets, &out, error, sizeof(error)), "quad inventory is bounded before allocation");
}
static void PoseAndLifetime()
{
	const short rotation[3][3] = { {0,0,4096}, {0,4096,0}, {-4096,0,0} };
	const short gates[4][3] = { {-4,0,0}, {-3,0,0}, {20,0,0}, {-32000,0,0} };
	NativeSkyFrame pose; NativeSkyMesh_Pose(rotation, gates, -17, &pose);
	Check(pose.enabled[0] && !pose.enabled[1] && !pose.enabled[2] && pose.enabled[3], "SZ3 divided by four eligibility includes threshold and saturation cases");
	const short saturatedRotation[3][3] = { {4096,0,0}, {0,4096,0}, {32767,0,0} };
	const short saturatedGates[4][3] = { {32767,0,0}, {-32768,0,0}, {0,0,0}, {1,0,0} };
	NativeSkyFrame saturated; NativeSkyMesh_Pose(saturatedRotation, saturatedGates, -17, &saturated);
	Check(saturated.enabled[0] && !saturated.enabled[1] && !saturated.enabled[2] && saturated.enabled[3], "GTE upper/lower depth saturation preserves visibility predicate");
	Check(pose.rotation[12] == 0 && pose.rotation[13] == -17 && pose.rotation[14] == 0, "offset is postrotation camera-space with no camera world translation");
	float projection[16]; NativeCameraView_BuildProjection(256, 320, 240, 160, 120, 4.0f / 3.0f, 0.1f, 1000, projection);
	// (-100,20,50) transforms to (50,3,100), then y projects as h*3/100.
	const float x = pose.rotation[0] * -100 + pose.rotation[8] * 50;
	const float y = pose.rotation[5] * 20 + pose.rotation[13], z = pose.rotation[2] * -100;
	const float pixelY = (1 - projection[5] * y / z) * 120;
	Check(fabsf(pixelY - (120 + 256 * 3.0f / 100)) < 0.001f && x == 50, "known sky point reproduces original rotation/offset projection");
	const int rgb[3] = { 127,91,55 }; float color[3]; NativeSkyMesh_Color(rgb, true, color);
	Check(fabsf(color[0] - 126.0f / 255) < 0.0001f && fabsf(color[1] - 90.0f / 255) < 0.0001f && fabsf(color[2] - 54.0f / 255) < 0.0001f, "reflection truncates each odd original RGB byte before modulation");
	NativeSkyEpoch epoch; epoch.Begin(); epoch.Select(0); epoch.captured = pose; epoch.captured.selected = true; epoch.captured.epoch = epoch.frame; epoch.captured.view = 0; epoch.captured.generation = 2; epoch.captured.revision = 4;
	NativeSkyFrame consumed; Check(epoch.Consume(0, 2, 4, &consumed), "current source/frame/view is available");
	Check(epoch.Consume(0, 2, 4, &consumed), "screenshot and present share the same current sky epoch");
	epoch.captured = pose; epoch.captured.selected = true; epoch.captured.epoch = epoch.frame; epoch.captured.view = 0; epoch.Select(1);
	Check(!epoch.Consume(1, 2, 4, &consumed), "skipped second view cannot reuse first-view sky");
	epoch.captured.selected = true; epoch.Begin(); Check(!epoch.Consume(0, 2, 4, &consumed), "early return or missed sky draw clears prior frame");
	epoch.Select(0); epoch.captured.selected = true; epoch.captured.epoch = epoch.frame; epoch.captured.view = 0; epoch.captured.generation = 2; epoch.captured.revision = 4;
	Check(!epoch.Consume(0, 3, 4, &consumed), "new level generation rejects old painted sky");
	epoch.captured.selected = true; epoch.captured.epoch = epoch.frame; epoch.captured.view = 0; epoch.captured.generation = 3; epoch.captured.revision = 4;
	Check(!epoch.Consume(0, 3, 5, &consumed), "day/weather reload revision rejects old texture pose");
}
static void OriginalModelSpans()
{
	std::vector<unsigned char> vertices(18 * 8), records(12 * 20), selectors(40);
	for (unsigned v = 0; v < 18; ++v) { vertices[v * 8] = (unsigned char)(v + 1); vertices[v * 8 + 4] = (unsigned char)(v + 20); }
	for (unsigned q = 0; q < 12; ++q) { records[q * 20] = 21; for (int c = 0; c < 4; ++c) records[q * 20 + 4 + c] = (unsigned char)(q + c); selectors[q] = (unsigned char)q; }
	NativeSkyModelInput input = { vertices.data(), vertices.size(), records.data(), records.size(), 18, 12, selectors.data(), 40, 0, 3 };
	std::vector<NativeSkyQuad> quads; short gate[3] = {}; char error[192] = {};
	Check(NativeSkyMesh_CopyModel(input, &quads, gate, error, sizeof(error)) && quads.size() == 12, "production horizon copy accepts complete original FT4 spans");
	Check(quads[3].positions[0] == 4 && !quads[3].reflection && quads[4].reflection && quads[11].model == 3 && gate[2] == 25, "production copy uses source indices, last eight reflection quads and RTPT gate vertex");
	input.polygonBytes--; Check(!NativeSkyMesh_CopyModel(input, &quads, gate, error, sizeof(error)) && quads.empty(), "short final polygon span rejects all copied output"); input.polygonBytes++;
	records[11 * 20 + 7] = 18; Check(!NativeSkyMesh_CopyModel(input, &quads, gate, error, sizeof(error)) && quads.empty(), "late out-of-range vertex fails complete production copy"); records[11 * 20 + 7] = 14;
	input.vertexBytes--; Check(!NativeSkyMesh_CopyModel(input, &quads, gate, error, sizeof(error)), "short actual vertex allocation rejected"); input.vertexBytes++;
	input.textureOffset = 29; Check(!NativeSkyMesh_CopyModel(input, &quads, gate, error, sizeof(error)), "horizon lookup plus polygon count cannot escape texture table"); input.textureOffset = 0;
	records.assign(12 * 16, 0); for (unsigned q = 0; q < 12; ++q) { records[q * 16] = 11; records[q * 16 + 5] = 1; records[q * 16 + 6] = 2; records[q * 16 + 7] = 3; }
	input.polygons = records.data(); input.polygonBytes = records.size();
	Check(NativeSkyMesh_CopyModel(input, &quads, gate, error, sizeof(error)), "original PLFT4 horizon lists copy the shared vertex prefix without reading absent RGB");
	// Havana uses the same vertex prefix with PolySizes[5] == 20. Record UVs
	// and colors deliberately differ from the separate horizon tile selectors.
	vertices.assign(30 * 8, 0); records.assign(20 * 20, 0xa5);
	for (unsigned v = 0; v < 30; ++v) { vertices[v * 8] = (unsigned char)(v + 1); vertices[v * 8 + 4] = (unsigned char)(v + 40); }
	for (unsigned q = 0; q < 20; ++q)
	{
		records[q * 20] = 5; selectors[20 + q] = (unsigned char)((q + 7) % 28);
		for (int c = 0; c < 4; ++c) records[q * 20 + 4 + c] = (unsigned char)(q + c);
	}
	input = NativeSkyModelInput{ vertices.data(), vertices.size(), records.data(), records.size(), 30, 20, selectors.data(), 40, 20, 0 };
	Check(NativeSkyMesh_CopyModel(input, &quads, gate, error, sizeof(error)) && quads.size() == 20, "Havana type5 horizon accepts exact thirty-vertex twenty-record spans and offset20");
	bool mapped = quads.size() == 20;
	for (unsigned q = 0; mapped && q < 20; ++q)
	{
		mapped = quads[q].tile == selectors[20 + q] && quads[q].reflection == (q >= 12);
		for (int c = 0; mapped && c < 4; ++c)
			mapped = quads[q].positions[c * 3] == (float)(q + c + 1) && quads[q].positions[c * 3 + 2] == (float)(q + c + 40);
	}
	Check(mapped && gate[0] == 18 && gate[2] == 57, "Havana shared prefix preserves every corner, external tile, reflection boundary and original gate");
	input.polygonBytes--; Check(!NativeSkyMesh_CopyModel(input, &quads, gate, error, sizeof(error)) && quads.empty(), "Havana truncated final type5 record rejects complete copy"); input.polygonBytes++;
	records[19 * 20] = 21; Check(!NativeSkyMesh_CopyModel(input, &quads, gate, error, sizeof(error)) && quads.empty(), "Havana late mixed record type rejects complete copy"); records[19 * 20] = 5;
	records[19 * 20 + 7] = 30; Check(!NativeSkyMesh_CopyModel(input, &quads, gate, error, sizeof(error)) && quads.empty(), "Havana late vertex index outside owned span rejects complete copy");
}
int main() { CaptureAndBounds(); PoseAndLifetime(); OriginalModelSpans(); printf("%d checks, %d failures\n", checks, failures); return failures ? 1 : 0; }
