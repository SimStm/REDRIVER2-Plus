#include "../utils/NativeCarMesh.h"
#include "../utils/NativeCarScene.h"
#include "../utils/NativeWheelMesh.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static int failures = 0;
#define CHECK(value) do { if (!(value)) { ++failures; printf("FAIL line %d: %s\n", __LINE__, #value); } } while (0)
static bool Near(float a, float b) { return fabsf(a - b) < 0.001f; }

static NativeCarTriangle Triangle(unsigned short clut)
{
	NativeCarTriangle triangle = {};
	triangle.vertices[0] = 2; triangle.vertices[1] = 0; triangle.vertices[2] = 1;
	triangle.uv[0] = 17; triangle.uv[1] = 29;
	triangle.uv[2] = 31; triangle.uv[3] = 43;
	triangle.uv[4] = 53; triangle.uv[5] = 61;
	triangle.clut = clut;
	triangle.textured = true;
	return triangle;
}

static void CaptureOwnedOriginalPages()
{
	std::vector<float> positions = { 1,2,3, 4,5,6, 7,8,9 };
	std::vector<unsigned short> vram(1024u * 512u, 0);
	vram[0] = 0x1111;
	vram[300u * 1024u + 1] = 0x001f;
	vram[301u * 1024u + 1] = 0x03e0;
	std::vector<NativeCarTriangle> triangles;
	triangles.push_back(Triangle(300 << 6));
	triangles.push_back(Triangle(301 << 6));
	NativeCarTriangle black = Triangle(0); black.textured = false;
	triangles.push_back(black);
	WorldMeshConversionInput captured;
	char error[256];
	CHECK(NativeCarMesh_Capture(positions, triangles, vram, &captured, error, sizeof(error)));
	CHECK(captured.source.submeshes.size() == 3);
	CHECK(captured.source.indices.size() == 9);
	CHECK(captured.source.positions[0] == 7 && captured.source.positions[1] == 8);
	CHECK(captured.source.uvs[0] == 17 && captured.source.uvs[1] == 29);
	CHECK(captured.images.size() == 3 && captured.pixels.size() == 2);
	CHECK(captured.images[0].width == 256 && captured.images[0].height == 256);
	CHECK(captured.images[2].pixelIndex == -1);
	CHECK(captured.pixels[0][0] == 255 && captured.pixels[0][1] == 0 && captured.pixels[0][3] == 255);
	CHECK(captured.pixels[1][0] == 0 && captured.pixels[1][1] == 255);
	CHECK(captured.legacyColors[0] == 128 && captured.legacyColors[18] == 40);
	CHECK(captured.worldShadeIndices.empty());
	WorldMeshConversionInput repeated;
	CHECK(NativeCarMesh_Capture(positions, triangles, vram, &repeated, error, sizeof(error)));
	CHECK(repeated.source.positions == captured.source.positions && repeated.pixels == captured.pixels);
	positions[6] = -999; triangles[0].uv[0] = 0; vram.assign(vram.size(), 0);
	CHECK(captured.source.positions[0] == 7 && captured.source.uvs[0] == 17 && captured.pixels[0][0] == 255);
}

static void CaptureRejectsBoundsAtomically()
{
	std::vector<float> positions = { 1,2,3, 4,5,6, 7,8,9 };
	std::vector<unsigned short> vram(1024u * 512u, 0);
	std::vector<NativeCarTriangle> triangles(1, Triangle(0));
	WorldMeshConversionInput out;
	char error[256];
	triangles[0].vertices[0] = 3;
	out.source.positions.push_back(99);
	CHECK(!NativeCarMesh_Capture(positions, triangles, vram, &out, error, sizeof(error)));
	CHECK(error[0] && out.source.positions.empty() && out.pixels.empty());
	triangles[0] = Triangle(0); triangles[0].tpage = 3 << 7;
	CHECK(!NativeCarMesh_Capture(positions, triangles, vram, &out, error, sizeof(error)));
	triangles[0].tpage = (2 << 7) | 15;
	CHECK(!NativeCarMesh_Capture(positions, triangles, vram, &out, error, sizeof(error)));
	triangles[0].tpage = 1 << 7; triangles[0].clut = (300 << 6) | 63;
	CHECK(!NativeCarMesh_Capture(positions, triangles, vram, &out, error, sizeof(error)));
	triangles.assign(1025, Triangle(0));
	CHECK(!NativeCarMesh_Capture(positions, triangles, vram, &out, error, sizeof(error)));
	triangles.clear();
	for (int i = 0; i < 65; ++i) triangles.push_back(Triangle((300 + i) << 6));
	CHECK(!NativeCarMesh_Capture(positions, triangles, vram, &out, error, sizeof(error)));
	CHECK(out.source.indices.empty() && out.images.empty());
	triangles.assign(1, Triangle(0)); positions.resize(257 * 3);
	CHECK(!NativeCarMesh_Capture(positions, triangles, vram, &out, error, sizeof(error)));
}

static void FullCarTransform()
{
	const int position[3] = { 100,200,300 };
	const short cog[3] = { 10,20,30 };
	const short rotation[3][3] = { { 0,4096,0 }, { 0,0,4096 }, { 4096,0,0 } };
	float matrix[16];
	NativeCarMesh_ComposeTransform(position, rotation, cog, false, matrix);
	CHECK(Near(matrix[4], -1) && Near(matrix[9], 1) && Near(matrix[2], -1));
	CHECK(Near(matrix[12], 80) && Near(matrix[13], -170) && Near(matrix[14], 290));
	CHECK(matrix[15] == 1 && matrix[3] == 0 && matrix[7] == 0);
	NativeCarMesh_ComposeTransform(position, rotation, cog, true, matrix);
	CHECK(Near(matrix[4], -0.5f) && Near(matrix[9], 0.5f));
	CHECK(Near(matrix[12], 80) && Near(matrix[13], -170) && Near(matrix[14], 290));
}

static void OriginalPerCarDamageAndMaterialOrder()
{
	const std::vector<float> clean = { 10,20,30, 40,50,60, 70,80,90 };
	std::vector<unsigned short> vram(1024u * 512u, 0);
	std::vector<NativeCarTriangle> triangles;
	NativeCarTriangle a = Triangle(300 << 6); a.originalIndex = 5;
	NativeCarTriangle b = Triangle(301 << 6); b.originalIndex = 1;
	NativeCarTriangle c = a; c.originalIndex = 3; c.vertices[0] = 1;
	NativeCarTriangle black = a; black.textured = false; black.originalIndex = 999;
	// A,B,A,untextured source order becomes A,A,B,untextured primitive order.
	triangles.push_back(a); triangles.push_back(b); triangles.push_back(c); triangles.push_back(black);
	NativeCarBodyCapture capture;
	char error[256] = {};
	CHECK(NativeCarMesh_Capture(clean, triangles, vram, &capture.conversion, error, sizeof(error), &capture.bindings));
	CHECK(capture.bindings.size() == 12);
	CHECK(capture.bindings[0].originalIndex == 5 && capture.bindings[3].originalIndex == 3 &&
		capture.bindings[6].originalIndex == 1 && !capture.bindings[9].textured);
	for (size_t corner = 0; corner < capture.bindings.size(); ++corner)
		for (int axis = 0; axis < 3; ++axis)
			CHECK(capture.conversion.source.positions[corner * 3 + axis] == clean[capture.bindings[corner].vertex * 3 + axis]);
	const std::vector<float> canonicalPositions = capture.conversion.source.positions;
	const std::vector<float> canonicalUvs = capture.conversion.source.uvs;
	std::vector<unsigned char> firstDamage(6, 0), secondDamage(6, 0);
	firstDamage[5] = 192; firstDamage[3] = 64; firstDamage[1] = 128;
	std::vector<float> dented = clean;
	dented[6] = -321; dented[7] = 456;
	std::vector<float> firstXyz, firstUvs, secondXyz, secondUvs;
	CHECK(NativeCarMesh_ExpandDamage(capture, dented, firstDamage, &firstXyz, &firstUvs, error, sizeof(error)));
	CHECK(NativeCarMesh_ExpandDamage(capture, clean, secondDamage, &secondXyz, &secondUvs, error, sizeof(error)));
	CHECK(firstXyz[0] == -321 && firstXyz[1] == 456 && secondXyz[0] == 70 && secondXyz[1] == 80);
	CHECK(firstUvs[0] == 209 && firstUvs[6] == 81 && firstUvs[12] == 145 && firstUvs[18] == 17);
	CHECK(secondUvs[0] == 17 && secondUvs[6] == 17 && secondUvs[12] == 17);
	// Low detail receives the original low-model positions, never HD dents.
	capture.lowDetail = true;
	const std::vector<float> low = { 1,2,3, 4,5,6, 7,8,9 };
	CHECK(NativeCarMesh_ExpandDamage(capture, low, firstDamage, &secondXyz, &secondUvs, error, sizeof(error)));
	CHECK(secondXyz[0] == 7 && firstXyz[0] == -321);
	CHECK(capture.conversion.source.positions == canonicalPositions && capture.conversion.source.uvs == canonicalUvs);
	// Packed addition includes a carry from U into V, rather than saturating U.
	capture.bindings[0].uv = (7 << 8) | 250;
	firstDamage[5] = 64;
	CHECK(NativeCarMesh_ExpandDamage(capture, low, firstDamage, &secondXyz, &secondUvs, error, sizeof(error)));
	CHECK(secondUvs[0] == 58 && secondUvs[1] == 8);
	// Late malformed bindings are rejected before either output is populated.
	capture.bindings.back().vertex = 3;
	CHECK(!NativeCarMesh_ExpandDamage(capture, low, firstDamage, &secondXyz, &secondUvs, error, sizeof(error)));
	CHECK(secondXyz.empty() && secondUvs.empty() && error[0]);
	capture.bindings.back().vertex = 1;
	capture.bindings[0].originalIndex = 6;
	CHECK(!NativeCarMesh_ExpandDamage(capture, low, firstDamage, &secondXyz, &secondUvs, error, sizeof(error)));
	capture.bindings[0].originalIndex = 5; capture.bindings[0].uv = 17;
	std::vector<float> nonFinite = low; nonFinite.back() = HUGE_VALF;
	CHECK(!NativeCarMesh_ExpandDamage(capture, nonFinite, firstDamage, &secondXyz, &secondUvs, error, sizeof(error)));
	CHECK(secondXyz.empty() && secondUvs.empty());
	CHECK(!NativeCarMesh_SelectLowDetail(10000, 10000, false, false));
	CHECK(NativeCarMesh_SelectLowDetail(10001, 10000, false, false));
	CHECK(NativeCarMesh_SelectLowDetail(10, 10000, true, false));
	CHECK(!NativeCarMesh_SelectLowDetail(10001, 10000, true, true));
	CHECK(!NativeCarMesh_SelectLowDetail(-10, 10000, false, false));
}

static float SampleAtlasChannel(const NativeCarBodyCapture& capture, float rawU, float rawV, int channel, bool linear = true)
{
	const WorldConversionImage& image = capture.conversion.images[0];
	const std::vector<unsigned char>& pixels = capture.conversion.pixels[image.pixelIndex];
	if (!linear)
	{
		int x = (int)floorf(rawU / 256.0f * image.width), y = (int)floorf(rawV / 256.0f * image.height);
		if (x < 0) x = 0; if (x >= image.width) x = image.width - 1;
		if (y < 0) y = 0; if (y >= image.height) y = image.height - 1;
		return pixels[((size_t)y * image.width + x) * 4 + channel];
	}
	float x = rawU / 256.0f * image.width - 0.5f, y = rawV / 256.0f * image.height - 0.5f;
	if (x < 0) x = 0; if (y < 0) y = 0;
	if (x > image.width - 1) x = (float)image.width - 1;
	if (y > image.height - 1) y = (float)image.height - 1;
	const int x0 = (int)floorf(x), y0 = (int)floorf(y);
	const int x1 = x0 + 1 < image.width ? x0 + 1 : x0;
	const int y1 = y0 + 1 < image.height ? y0 + 1 : y0;
	const float fx = x - x0, fy = y - y0;
	const float top = pixels[((size_t)y0 * image.width + x0) * 4 + channel] * (1 - fx) +
		pixels[((size_t)y0 * image.width + x1) * 4 + channel] * fx;
	const float bottom = pixels[((size_t)y1 * image.width + x0) * 4 + channel] * (1 - fx) +
		pixels[((size_t)y1 * image.width + x1) * 4 + channel] * fx;
	return top * (1 - fy) + bottom * fy;
}

static void OriginalPackedPacketCarryAndAtlasEdges()
{
	const std::vector<float> positions = { 1,2,3, 4,5,6, 7,8,9 };
	std::vector<unsigned short> vram(1024u * 512u, 0);
	for (int y = 0; y < 256; ++y)
	{
		for (int x = 0; x < 64; ++x) vram[y * 1024 + x] = 0x1111;
		for (int x = 64; x < 128; ++x) vram[y * 1024 + x] = 0x2222;
	}
	vram[300 * 1024 + 1] = 0x001f; vram[300 * 1024 + 2] = 0x7c00;
	vram[300 * 1024 + 17] = 0x03e0; vram[300 * 1024 + 18] = 0x7fff;
	vram[300 * 1024 + 33] = 0x001f; vram[300 * 1024 + 34] = 0x03e0;
	// Vary edge texels to detect a shifted sample convention, not only bleed.
	vram[63] = 0x2111; vram[127] = 0x1222;
	vram[255 * 1024] = 0x2222; vram[255 * 1024 + 64] = 0x1111;
	char error[256] = {};
	// Full packet words distinguish FT3 addition from GT3 palette-CLUT OR.
	for (int mode = 0; mode < 4; ++mode)
	{
		NativeCarTriangle triangle = Triangle((300 << 6) + (mode & 1));
		triangle.paletteClut = mode >= 2;
		triangle.uv[0] = triangle.uv[2] = 250;
		triangle.uv[1] = triangle.uv[3] = 255;
		triangle.uv[4] = 0; triangle.uv[5] = 0;
		NativeCarBodyCapture capture;
		CHECK(NativeCarMesh_Capture(positions, std::vector<NativeCarTriangle>(1, triangle), vram,
			&capture.conversion, error, sizeof(error), &capture.bindings));
		const bool clutChanges = !(triangle.paletteClut && (triangle.clut & 1));
		CHECK(capture.conversion.images[0].width == 516 && capture.conversion.images[0].height == (clutChanges ? 516 : 256));
		std::vector<unsigned char> damage(1, 64);
		std::vector<float> xyz, uvs;
		CHECK(NativeCarMesh_ExpandDamage(capture, positions, damage, &xyz, &uvs, error, sizeof(error)));
		const unsigned uv0 = 0xfffa, uv1 = 0xfffa, uv2 = 0;
		const unsigned packet0 = triangle.paletteClut ? ((unsigned)triangle.clut << 16) | (uv0 + 64) :
			(((unsigned)triangle.clut << 16) | uv0) + 64;
		const unsigned packet1 = ((unsigned)triangle.tpage << 16 | uv1) + 64;
		const unsigned packet2 = uv2 + 64;
		CHECK((packet1 >> 16) == triangle.tpage + 1u);
		CHECK((packet0 >> 16) == triangle.clut + (clutChanges ? 1u : 0u));
		// Compare each reachable quadrant with an ordinary native256 page using
		// both actual supported filters, including asymmetric edge texels.
		for (int row = 0; row < (clutChanges ? 2 : 1); ++row)
		for (int column = 0; column < 2; ++column)
		{
			NativeCarTriangle plain = Triangle((unsigned short)(triangle.clut + row)); plain.tpage = (unsigned short)(triangle.tpage + column);
			NativeCarBodyCapture baseline;
			CHECK(NativeCarMesh_Capture(positions, std::vector<NativeCarTriangle>(1, plain), vram,
				&baseline.conversion, error, sizeof(error)));
			const int edges[4] = { 0,1,254,255 };
			for (int u = 0; u < 4; ++u)
			for (int v = 0; v < 4; ++v)
			for (int filter = 0; filter < 2; ++filter)
			for (int channel = 0; channel < 3; ++channel)
			{
				const float mappedU = (float)(1 + edges[u] + column * 258) / 516 * 256;
				const float mappedV = clutChanges ? (float)(1 + edges[v] + row * 258) / 516 * 256 : (float)edges[v];
				CHECK(fabsf(SampleAtlasChannel(capture, mappedU, mappedV, channel, filter != 0) -
					SampleAtlasChannel(baseline, (float)edges[u], (float)edges[v], channel, filter != 0)) < 0.02f);
			}
		}
		const unsigned packets[3] = { packet0, packet1, packet2 };
		for (int corner = 0; corner < 3; ++corner)
		{
			const float expectedU = (float)(1 + (packets[corner] & 255) + 258) / 516 * 256;
			const float expectedV = clutChanges ? (float)(1 + ((packets[corner] >> 8) & 255) + 258) / 516 * 256 :
				(float)((packets[corner] >> 8) & 255);
			CHECK(Near(uvs[corner * 2], expectedU) && Near(uvs[corner * 2 + 1], expectedV));
		}
		// The selected page's palette index2 is white unless FT3 oddCLUT adds
		// to the next CLUT, whose index2 is green. Corner2 lies at V=0, so a
		// missing row gutter would mix the preceding CLUT under bilinear.
		const bool green = mode == 1;
		CHECK(Near(SampleAtlasChannel(capture, uvs[4], uvs[5], 0), green ? 0 : 255));
		CHECK(Near(SampleAtlasChannel(capture, uvs[4], uvs[5], 1), 255));
		CHECK(Near(SampleAtlasChannel(capture, uvs[4], uvs[5], 2), green ? 0 : 255));
		// Static template and zero-damage update use exactly the same raw domain.
		damage[0] = 0;
		CHECK(NativeCarMesh_ExpandDamage(capture, positions, damage, &xyz, &uvs, error, sizeof(error)));
		CHECK(uvs == capture.conversion.source.uvs);
		for (int channel = 0; channel < 3; ++channel)
			CHECK(Near(SampleAtlasChannel(capture, uvs[4], uvs[5], channel), channel == (mode & 1 ? 1 : 0) ? 255 : 0));
	}
	// A UV2 high16 carry writes packet padding; it cannot change page/CLUT.
	NativeCarTriangle onlyUv2 = Triangle(300 << 6);
	onlyUv2.uv[4] = 250; onlyUv2.uv[5] = 255;
	NativeCarBodyCapture ordinary;
	CHECK(NativeCarMesh_Capture(positions, std::vector<NativeCarTriangle>(1, onlyUv2), vram,
		&ordinary.conversion, error, sizeof(error), &ordinary.bindings));
	CHECK(ordinary.conversion.images[0].width == 256 && ordinary.conversion.images[0].height == 256);
	std::vector<float> xyz, uvs;
	CHECK(NativeCarMesh_ExpandDamage(ordinary, positions, std::vector<unsigned char>(1, 64), &xyz, &uvs, error, sizeof(error)));
	CHECK(uvs[4] == 58 && uvs[5] == 0);
	NativeCarTriangle clutOnly = Triangle(300 << 6);
	clutOnly.uv[0] = 250; clutOnly.uv[1] = 255;
	NativeCarBodyCapture vertical;
	CHECK(NativeCarMesh_Capture(positions, std::vector<NativeCarTriangle>(1, clutOnly), vram,
		&vertical.conversion, error, sizeof(error), &vertical.bindings));
	CHECK(vertical.conversion.images[0].width == 256 && vertical.conversion.images[0].height == 516);
	CHECK(NativeCarMesh_ExpandDamage(vertical, positions, std::vector<unsigned char>(1, 64), &xyz, &uvs, error, sizeof(error)));
	CHECK(uvs[0] == 58 && Near(uvs[1], 259.0f / 516 * 256));
	std::vector<NativeCarTriangle> overBudget;
	for (int clut = 0; clut < 64; ++clut)
	{
		NativeCarTriangle triangle = Triangle((unsigned short)((300 << 6) + clut));
		triangle.uv[2] = 250; triangle.uv[3] = 255;
		overBudget.push_back(triangle);
	}
	CHECK(!NativeCarMesh_Capture(positions, overBudget, vram, &vertical.conversion, error, sizeof(error), &vertical.bindings));
	CHECK(vertical.conversion.pixels.empty() && vertical.bindings.empty() && error[0]);
}

static void ActualPublicationIdentitySeam()
{
	// Opaque addresses are only compared; no original MODEL/game storage needed.
	int first = 0, second = 0;
	NativeCarSourceIdentity captured = { 4, 2, 19, reinterpret_cast<const MODEL*>(&first) };
	NativeCarSourceIdentity current = captured;
	CHECK(NativeCarMesh_IsCurrentPublication(current, captured, 7, 7));
	CHECK(!NativeCarMesh_IsCurrentPublication(current, captured, 8, 7));
	current.revision++;
	CHECK(!NativeCarMesh_IsCurrentPublication(current, captured, 7, 7));
	current = captured; current.palette++;
	CHECK(!NativeCarMesh_SameSource(current, captured));
	current = captured; current.model++;
	CHECK(!NativeCarMesh_SameSource(current, captured));
	current = captured; current.cleanModel = reinterpret_cast<const MODEL*>(&second);
	CHECK(!NativeCarMesh_SameSource(current, captured));
	current = captured; current.lowModel = reinterpret_cast<const MODEL*>(&second);
	CHECK(!NativeCarMesh_SameSource(current, captured));
	current = captured;
	CHECK(NativeCarMesh_IsCurrentPublication(current, captured, 7, 7));
}

#ifdef _WIN32
static void ActualCombinedSubmissionSeam()
{
	CHECK(NativeCarScene_HideBody(2, 1, 1));
	CHECK(!NativeCarScene_HideBody(2, 1, 0));
	CHECK(!NativeCarScene_HideBody(0, 1, 1));
	CHECK(!NativeCarScene_HideBody(1, 1, 1));
	NativeInstance world[2] = {}, combined[4] = {}, car = {};
	strcpy(world[0].resourceKey, "world-a"); strcpy(world[1].resourceKey, "world-b");
	strcpy(car.resourceKey, "vehicle"); car.transform[12] = 42;
	int count = -1;
	CHECK(NativeCarScene_PrepareWorld(world, 2, true, 2, combined, 4, &count));
	CHECK(NativeCarScene_Append(car, combined, 4, &count));
	CHECK(count == 3 && !strcmp(combined[0].resourceKey, "world-a") && !strcmp(combined[1].resourceKey, "world-b"));
	CHECK(!strcmp(combined[2].resourceKey, "vehicle") && combined[2].transform[12] == 42 && combined[2].worldShadeOffset == 0);
	car.transform[12] = 84;
	CHECK(combined[2].transform[12] == 42);
	CHECK(NativeCarScene_PrepareWorld(NULL, 0, true, 2, combined, 4, &count));
	CHECK(NativeCarScene_Append(car, combined, 4, &count) && count == 1);
	CHECK(!NativeCarScene_PrepareWorld(world, 2, false, 2, combined, 4, &count) && count == 0);
	CHECK(!NativeCarScene_PrepareWorld(world, 2, true, 3, combined, 4, &count) && count == 0);
	count = 4;
	CHECK(!NativeCarScene_Append(car, combined, 4, &count) && count == 4);
}
#endif

static void OriginalWheelProceduralGeometry()
{
	NativeWheelCapture capture;
	capture.originalPositions.resize(24 * 3);
	for (int v = 0; v < 24; ++v)
	{
		capture.originalPositions[v * 3] = (float)(v + 1);
		capture.originalPositions[v * 3 + 1] = 70;
		capture.originalPositions[v * 3 + 2] = 90;
		capture.cornerVertices.push_back(v);
	}
	std::vector<float> zero, quarter;
	char error[256];
	CHECK(NativeWheelMesh_Deform(capture, 100, 0, 4096, &zero, error, sizeof(error)));
	CHECK(NativeWheelMesh_Deform(capture, 100, 4096, 0, &quarter, error, sizeof(error)));
	CHECK(zero[8 * 3 + 1] == 141 && zero[8 * 3 + 2] == 0);
	CHECK(quarter[8 * 3 + 1] == 0 && quarter[8 * 3 + 2] == 141);
	CHECK(zero[11 * 3 + 1] == 0 && zero[11 * 3 + 2] == -141);
	CHECK(quarter[11 * 3 + 1] == 141 && quarter[11 * 3 + 2] == 0);
	for (int v = 0; v < 24; ++v) CHECK(zero[v * 3] == quarter[v * 3] && zero[v * 3] == v + 1);
	for (int v = 16; v < 24; ++v)
		CHECK(zero[v * 3 + 1] == quarter[v * 3 + 1] && zero[v * 3 + 2] == quarter[v * 3 + 2]);
	CHECK(zero[0] == 1 && zero[1] == 70 && zero[2] == 90);
	CHECK(zero[16 * 3 + 1] == -100 && zero[16 * 3 + 2] == 0);
	CHECK(zero[17 * 3 + 1] == -100 && zero[17 * 3 + 2] == 0);
	CHECK(zero[18 * 3 + 1] == 0 && zero[18 * 3 + 2] == -100);
	CHECK(zero[19 * 3 + 1] == 0 && zero[19 * 3 + 2] == -100);
	CHECK(zero[20 * 3 + 1] == 0 && zero[20 * 3 + 2] == 100);
	CHECK(zero[21 * 3 + 1] == 0 && zero[21 * 3 + 2] == 100);
	CHECK(zero[22 * 3 + 1] == 100 && zero[22 * 3 + 2] == 0);
	CHECK(zero[23 * 3 + 1] == 100 && zero[23 * 3 + 2] == 0);
	CHECK(NativeWheelMesh_Deform(capture, 100, 2048, -2048, &quarter, error, sizeof(error)));
	CHECK(quarter[8 * 3 + 1] == -70 && quarter[8 * 3 + 2] == 71);
	const unsigned int indices[4] = { 8,3,19,21 };
	const unsigned char uv[8] = { 11,22,33,44,55,66,77,88 };
	NativeCarTriangle emitted[2];
	NativeWheelMesh_TriangulateQuad(indices, uv, 17, 23, emitted);
	CHECK(emitted[0].vertices[0] == 8 && emitted[0].vertices[1] == 3 && emitted[0].vertices[2] == 21);
	CHECK(emitted[1].vertices[0] == 3 && emitted[1].vertices[1] == 21 && emitted[1].vertices[2] == 19);
	CHECK(emitted[0].uv[0] == 11 && emitted[0].uv[2] == 33 && emitted[0].uv[4] == 77);
	CHECK(emitted[1].uv[0] == 33 && emitted[1].uv[2] == 77 && emitted[1].uv[4] == 55);
	CHECK(emitted[1].uv[1] == 44 && emitted[1].uv[3] == 88 && emitted[1].uv[5] == 66);
	CHECK(emitted[0].tpage == 17 && emitted[1].clut == 23 && emitted[1].textured);
	std::vector<unsigned char> colors(5 * 6 * 3, 128);
	CHECK(NativeWheelMesh_ApplyConstantColors(5, &colors, error, sizeof(error)));
	for (int q = 0; q < 5; ++q)
		for (int channel = 0; channel < 18; ++channel)
			CHECK(colors[q * 18 + channel] == (q < 2 ? 128 : 0));
	const std::vector<unsigned char> previousColors = colors;
	CHECK(!NativeWheelMesh_ApplyConstantColors(4, &colors, error, sizeof(error)) && colors == previousColors);
	CHECK(!NativeWheelMesh_ApplyConstantColors(17, &colors, error, sizeof(error)) && colors == previousColors);
	capture.cornerVertices[0] = 24;
	CHECK(!NativeWheelMesh_Deform(capture, 100, 0, 4096, &quarter, error, sizeof(error)) && quarter.empty());
	capture.cornerVertices[0] = 0;
	CHECK(!NativeWheelMesh_Deform(capture, -1, 0, 4096, &quarter, error, sizeof(error)));
	const float body[16] = { -1,0,0,0, 0,1,0,0, 0,0,-1,0, 100,-200,300,1 };
	const short displacement[3] = { 10,20,30 };
	float wheel[16];
	for (int slot = 0; slot < 4; ++slot)
	{
		NativeWheelMesh_ComposeTransform(body, displacement, 100, 5, slot, 4096, 0, wheel);
		CHECK(Near(wheel[12], slot & 2 ? 127 : 93));
		CHECK(Near(wheel[13], -311) && Near(wheel[14], 330));
		CHECK(Near(wheel[0], slot & 1 ? -1 : 0));
		CHECK(Near(wheel[2], slot & 1 ? 0 : 1));
		CHECK(wheel[15] == 1);
	}
	NativeWheelSourceIdentity first = {}, second = {};
	first.wheelSize = second.wheelSize = 100;
	CHECK(NativeWheelMesh_SameSource(first, second));
	second.wheelSize = 101;
	CHECK(!NativeWheelMesh_SameSource(first, second));
}

int main()
{
	CaptureOwnedOriginalPages(); CaptureRejectsBoundsAtomically(); FullCarTransform(); ActualPublicationIdentitySeam();
	OriginalWheelProceduralGeometry();
	OriginalPerCarDamageAndMaterialOrder();
	OriginalPackedPacketCarryAndAtlasEdges();
#ifdef _WIN32
	ActualCombinedSubmissionSeam();
#endif
	printf("NativeCarMeshTests: %d failures\n", failures);
	return failures ? 1 : 0;
}
