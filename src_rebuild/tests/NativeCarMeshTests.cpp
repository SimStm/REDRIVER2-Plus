#include "../utils/NativeCarMesh.h"
#include "../utils/NativeCarScene.h"
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

int main()
{
	CaptureOwnedOriginalPages(); CaptureRejectsBoundsAtomically(); FullCarTransform(); ActualPublicationIdentitySeam();
#ifdef _WIN32
	ActualCombinedSubmissionSeam();
#endif
	printf("NativeCarMeshTests: %d failures\n", failures);
	return failures ? 1 : 0;
}
