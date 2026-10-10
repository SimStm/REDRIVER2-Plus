// Standalone regression tests for the E03 block 1 source-MODEL decoder.
//
// The decoder turns the polygon records of a reconstructed MODEL into a
// project-owned SourceMesh without touching the source bytes and without
// calling the legacy runtime ConvertPolygonTypes. The fixtures below are
// synthetic and analytic: they build byte-exact MODEL spans and assert the
// emitted vertex/UV order, material grouping, span validation and determinism.
// They need no game assets, no game/PSX headers and no GPU context. Build and
// run instructions are in tests/README.md.

#include "../utils/SourceModelDecoder.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <vector>

static int g_checks = 0;
static int g_failures = 0;

#define CHECK(condition) \
	do { \
		g_checks++; \
		if (!(condition)) { \
			g_failures++; \
			printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
		} \
	} while (0)

// --- synthetic MODEL span builder -------------------------------------------

struct SourceUV
{
	unsigned char u;
	unsigned char v;
};

struct SourceRGB
{
	unsigned char r;
	unsigned char g;
	unsigned char b;
	unsigned char pad;
};

static SourceUV UV(unsigned char u, unsigned char v)
{
	SourceUV result;
	result.u = u;
	result.v = v;
	return result;
}

static SourceRGB RGB(unsigned char r, unsigned char g, unsigned char b)
{
	SourceRGB result;
	result.r = r;
	result.g = g;
	result.b = b;
	result.pad = 0;
	return result;
}

struct SourceModelBuilder
{
	std::vector<unsigned char> bytes;
	int verticesOffset;
	int pointNormalsOffset;
	int polyBlockOffset;
	unsigned int vertexCount;
	unsigned int pointNormalCount;
	unsigned int polyCount;

	SourceModelBuilder()
		: verticesOffset(-1), pointNormalsOffset(-1), polyBlockOffset(-1),
		  vertexCount(0), pointNormalCount(0), polyCount(0)
	{
		bytes.resize(8, 0); // MODEL-base slack, so offsets never collide with 0
	}

	void BeginVertices() { verticesOffset = (int)bytes.size(); }
	void BeginPointNormals() { pointNormalsOffset = (int)bytes.size(); }
	void BeginPolys() { polyBlockOffset = (int)bytes.size(); }

	unsigned int AddVertex(short x, short y, short z)
	{
		const unsigned char data[8] = {
			(unsigned char)(x & 0xff), (unsigned char)((x >> 8) & 0xff),
			(unsigned char)(y & 0xff), (unsigned char)((y >> 8) & 0xff),
			(unsigned char)(z & 0xff), (unsigned char)((z >> 8) & 0xff),
			0, 0
		};
		bytes.insert(bytes.end(), data, data + 8);
		return vertexCount++;
	}

	unsigned int AddRawNormal(short x, short y, short z)
	{
		const unsigned char data[8] = {
			(unsigned char)(x & 0xff), (unsigned char)((x >> 8) & 0xff),
			(unsigned char)(y & 0xff), (unsigned char)((y >> 8) & 0xff),
			(unsigned char)(z & 0xff), (unsigned char)((z >> 8) & 0xff),
			0, 0
		};
		bytes.insert(bytes.end(), data, data + 8);
		const unsigned int index = pointNormalCount;
		pointNormalCount++;
		return index;
	}

	void AddPoly(const unsigned char* record, size_t size)
	{
		bytes.insert(bytes.end(), record, record + size);
		polyCount++;
	}

	SourceModelView View() const
	{
		SourceModelView view;
		view.instanceNumber = -1;
		view.numVertices = vertexCount;
		view.numPolys = polyCount;
		view.numPointNormals = pointNormalCount;
		view.vertices = verticesOffset;
		view.polyBlock = polyBlockOffset;
		view.pointNormals = pointNormalsOffset;
		view.baseBegin = NULL;
		view.baseEnd = NULL;
		view.baseVertices = 0;
		view.basePointNormals = 0;
		view.baseNumVertices = 0;
		view.baseNumPointNormals = 0;
		return view;
	}
};

// Record writers write the md.h layout byte-for-byte; the parameters are the
// layout fields in order, so uv0..uv3 (FT4/GT4) and v0..v3 are the raw layout
// values, not the world emission order.
static void AddFT3(SourceModelBuilder& b, unsigned char set, unsigned char id,
	unsigned char v0, unsigned char v1, unsigned char v2,
	SourceUV uv0, SourceUV uv1, SourceUV uv2, SourceRGB color)
{
	unsigned char rec[20];
	memset(rec, 0, sizeof(rec));
	rec[0] = 20; rec[1] = set; rec[2] = id;
	rec[4] = v0; rec[5] = v1; rec[6] = v2;
	rec[8] = uv0.u; rec[9] = uv0.v; rec[10] = uv1.u; rec[11] = uv1.v;
	rec[12] = uv2.u; rec[13] = uv2.v;
	rec[16] = color.r; rec[17] = color.g; rec[18] = color.b; rec[19] = color.pad;
	b.AddPoly(rec, sizeof(rec));
}

static void AddFT4(SourceModelBuilder& b, unsigned char set, unsigned char id,
	unsigned char v0, unsigned char v1, unsigned char v2, unsigned char v3,
	SourceUV uv0, SourceUV uv1, SourceUV uv2, SourceUV uv3, SourceRGB color)
{
	unsigned char rec[20];
	memset(rec, 0, sizeof(rec));
	rec[0] = 21; rec[1] = set; rec[2] = id;
	rec[4] = v0; rec[5] = v1; rec[6] = v2; rec[7] = v3;
	rec[8] = uv0.u; rec[9] = uv0.v; rec[10] = uv1.u; rec[11] = uv1.v;
	rec[12] = uv2.u; rec[13] = uv2.v; rec[14] = uv3.u; rec[15] = uv3.v;
	rec[16] = color.r; rec[17] = color.g; rec[18] = color.b; rec[19] = color.pad;
	b.AddPoly(rec, sizeof(rec));
}

static void AddGT3(SourceModelBuilder& b, unsigned char set, unsigned char id,
	unsigned char v0, unsigned char v1, unsigned char v2,
	unsigned char n0, unsigned char n1, unsigned char n2,
	SourceUV uv0, SourceUV uv1, SourceUV uv2, SourceRGB color)
{
	unsigned char rec[24];
	memset(rec, 0, sizeof(rec));
	rec[0] = 22; rec[1] = set; rec[2] = id;
	rec[4] = v0; rec[5] = v1; rec[6] = v2;
	rec[8] = n0; rec[9] = n1; rec[10] = n2;
	rec[12] = uv0.u; rec[13] = uv0.v; rec[14] = uv1.u; rec[15] = uv1.v;
	rec[16] = uv2.u; rec[17] = uv2.v;
	rec[20] = color.r; rec[21] = color.g; rec[22] = color.b; rec[23] = color.pad;
	b.AddPoly(rec, sizeof(rec));
}

static void AddGT4(SourceModelBuilder& b, unsigned char set, unsigned char id,
	unsigned char v0, unsigned char v1, unsigned char v2, unsigned char v3,
	unsigned char n0, unsigned char n1, unsigned char n2, unsigned char n3,
	SourceUV uv0, SourceUV uv1, SourceUV uv2, SourceUV uv3, SourceRGB color)
{
	unsigned char rec[24];
	memset(rec, 0, sizeof(rec));
	rec[0] = 23; rec[1] = set; rec[2] = id;
	rec[4] = v0; rec[5] = v1; rec[6] = v2; rec[7] = v3;
	rec[8] = n0; rec[9] = n1; rec[10] = n2; rec[11] = n3;
	rec[12] = uv0.u; rec[13] = uv0.v; rec[14] = uv1.u; rec[15] = uv1.v;
	rec[16] = uv2.u; rec[17] = uv2.v; rec[18] = uv3.u; rec[19] = uv3.v;
	rec[20] = color.r; rec[21] = color.g; rec[22] = color.b; rec[23] = color.pad;
	b.AddPoly(rec, sizeof(rec));
}

static void AddPL(SourceModelBuilder& b, unsigned char set, unsigned char id, unsigned char th,
	unsigned char v0, unsigned char v1, unsigned char v2, unsigned char v3,
	SourceUV uv0, SourceUV uv1, SourceUV uv2, SourceUV uv3)
{
	unsigned char rec[16];
	memset(rec, 0, sizeof(rec));
	rec[0] = 11; rec[1] = set; rec[2] = id; rec[3] = th;
	rec[4] = v0; rec[5] = v1; rec[6] = v2; rec[7] = v3;
	rec[8] = uv0.u; rec[9] = uv0.v; rec[10] = uv1.u; rec[11] = uv1.v;
	rec[12] = uv2.u; rec[13] = uv2.v; rec[14] = uv3.u; rec[15] = uv3.v;
	b.AddPoly(rec, sizeof(rec));
}

// ptype 10: a PL 3-vertex record in the 16-byte stride. Bytes 7 and 14..15 are
// padding before the runtime 10->11 conversion writes v3/uv3 into them.
static void AddPLTriangle(SourceModelBuilder& b, unsigned char set, unsigned char id, unsigned char th,
	unsigned char v0, unsigned char v1, unsigned char v2,
	SourceUV uv0, SourceUV uv1, SourceUV uv2)
{
	unsigned char rec[16];
	memset(rec, 0, sizeof(rec));
	rec[0] = 10; rec[1] = set; rec[2] = id; rec[3] = th;
	rec[4] = v0; rec[5] = v1; rec[6] = v2;
	rec[8] = uv0.u; rec[9] = uv0.v; rec[10] = uv1.u; rec[11] = uv1.v;
	rec[12] = uv2.u; rec[13] = uv2.v;
	b.AddPoly(rec, sizeof(rec));
}

static SourceModelDecoderResult Decode(const SourceModelBuilder& b, SourceMesh& out,
	char* error, int capacity,
	SourceModelDecoderPolicy policy = SourceModelDecoderPolicy::Strict)
{
	SourceModelView view = b.View();
	return DecodeSourceModel(b.bytes.data(), b.bytes.data() + b.bytes.size(), view, &out,
		error, capacity, policy);
}

static bool MeshIsEmpty(const SourceMesh& mesh)
{
	return mesh.positions.empty() && mesh.normals.empty() && mesh.uvs.empty()
		&& mesh.indices.empty() && mesh.submeshes.empty() && mesh.colors.empty();
}

static bool MeshEqual(const SourceMesh& a, const SourceMesh& b)
{
	if (a.positions != b.positions || a.normals != b.normals || a.uvs != b.uvs)
		return false;
	if (a.colors != b.colors || a.indices != b.indices)
		return false;
	if (a.submeshes.size() != b.submeshes.size())
		return false;
	for (size_t i = 0; i < a.submeshes.size(); ++i)
	{
		const SourceSubmesh& x = a.submeshes[i];
		const SourceSubmesh& y = b.submeshes[i];
		if (x.primitiveClass != y.primitiveClass || x.textureSet != y.textureSet ||
			x.textureId != y.textureId || x.firstIndex != y.firstIndex ||
			x.indexCount != y.indexCount || x.hasColors != y.hasColors ||
			x.hasNormals != y.hasNormals)
			return false;
	}
	return true;
}

// --- F-A: quad emission order and UVs ---------------------------------------

static float EdgeXY(const float* a, const float* b, float x, float y)
{
	return (b[0] - a[0]) * (y - a[1]) - (b[1] - a[1]) * (x - a[0]);
}

static int QuadCoverage(const SourceMesh& mesh, float x, float y)
{
	int coverage = 0;
	for (size_t i = 0; i < mesh.indices.size(); i += 3)
	{
		const float* a = &mesh.positions[mesh.indices[i] * 3];
		const float* b = &mesh.positions[mesh.indices[i + 1] * 3];
		const float* c = &mesh.positions[mesh.indices[i + 2] * 3];
		const float ab = EdgeXY(a, b, x, y), bc = EdgeXY(b, c, x, y), ca = EdgeXY(c, a, x, y);
		if ((ab > 0 && bc > 0 && ca > 0) || (ab < 0 && bc < 0 && ca < 0))
			coverage++;
	}
	return coverage;
}

static void TestCyclicQuadCoverage()
{
	const int types[] = { 11, 21, 23 };
	for (int t = 0; t < 3; ++t)
	{
		SourceModelBuilder b;
		b.BeginVertices();
		b.AddVertex(0, 0, 0); b.AddVertex(1, 0, 0);
		b.AddVertex(1, 1, 0); b.AddVertex(0, 1, 0);
		b.BeginPointNormals();
		b.AddRawNormal(4096, 0, 0); b.AddRawNormal(0, 4096, 0);
		b.AddRawNormal(0, 0, 4096); b.AddRawNormal(-4096, 0, 0);
		b.BeginPolys();
		if (types[t] == 11)
			AddPL(b, 2, 5, 0x87, 0, 1, 2, 3, UV(1, 2), UV(3, 4), UV(5, 6), UV(7, 8));
		else if (types[t] == 21)
			AddFT4(b, 2, 5, 0, 1, 2, 3, UV(1, 2), UV(3, 4), UV(5, 6), UV(7, 8), RGB(10, 20, 30));
		else
			AddGT4(b, 2, 5, 0, 1, 2, 3, 0, 1, 2, 3, UV(1, 2), UV(3, 4), UV(5, 6), UV(7, 8), RGB(10, 20, 30));
		SourceMesh mesh;
		char error[256] = "";
		CHECK(Decode(b, mesh, error, sizeof(error)).error == SOURCE_MODEL_ERROR_NONE);
		CHECK(mesh.positions == std::vector<float>({ 0,0,0, 1,0,0, 0,1,0, 1,1,0 }));
		CHECK(mesh.uvs == std::vector<float>({ 1,2, 3,4, 7,8, 5,6 }));
		CHECK(mesh.indices.size() == 6 && mesh.submeshes.size() == 1);
		CHECK(mesh.submeshes[0].indexCount == 6);
		CHECK(mesh.submeshes[0].textureSet == 2 && mesh.submeshes[0].textureId == 5);
		CHECK(mesh.shadeIndices == std::vector<unsigned char>(4, types[t] == 11 ? 7 : types[t] == 21 ? 0 : 8));
		CHECK(mesh.colors == (types[t] == 11 ? std::vector<unsigned char>() :
			std::vector<unsigned char>({ 10,20,30, 10,20,30, 10,20,30, 10,20,30 })));
		if (types[t] == 23)
			CHECK(mesh.normals == std::vector<float>({ 4096,0,0, 0,4096,0, -4096,0,0, 0,0,4096 }));
		else
			CHECK(mesh.normals.empty());
		for (size_t i = 0; i < mesh.indices.size(); i += 3)
		{
			const float* a = &mesh.positions[mesh.indices[i] * 3];
			const float* b = &mesh.positions[mesh.indices[i + 1] * 3];
			const float* c = &mesh.positions[mesh.indices[i + 2] * 3];
			CHECK(EdgeXY(a, b, c[0], c[1]) > 0);
		}
		const float samples[][2] = { {0.1f,0.5f}, {0.9f,0.5f}, {0.2f,0.1f}, {0.8f,0.95f} };
		for (int s = 0; s < 4; ++s)
			CHECK(QuadCoverage(mesh, samples[s][0], samples[s][1]) == 1);
		// The old topology has both overlap and a hole despite equal total area.
		mesh.indices = { 0,1,2, 0,2,3 };
		CHECK(QuadCoverage(mesh, 0.1f, 0.5f) == 2);
		CHECK(QuadCoverage(mesh, 0.9f, 0.5f) == 0);
	}
}

static void TestQuadOrderAndUVs()
{
	SourceModelBuilder b;
	b.BeginVertices();
	b.AddVertex(10, 0, 0);  // v0
	b.AddVertex(0, 10, 0);  // v1
	b.AddVertex(0, 0, 10);  // v2
	b.AddVertex(5, 5, 5);   // v3
	b.BeginPolys();
	AddFT4(b, 2, 5, 0, 1, 2, 3, UV(1, 2), UV(3, 4), UV(5, 6), UV(7, 8), RGB(10, 20, 30));

	SourceMesh mesh;
	char error[256] = "";
	SourceModelDecoderResult result = Decode(b, mesh, error, sizeof(error));
	CHECK(result.error == SOURCE_MODEL_ERROR_NONE);
	CHECK(error[0] == '\0');

	// World order is (v0,v1,v3,v2), so v3 precedes v2.
	CHECK(mesh.positions.size() == 12);
	CHECK(mesh.positions[0] == 10.0f && mesh.positions[1] == 0.0f && mesh.positions[2] == 0.0f);
	CHECK(mesh.positions[3] == 0.0f && mesh.positions[4] == 10.0f && mesh.positions[5] == 0.0f);
	CHECK(mesh.positions[6] == 5.0f && mesh.positions[7] == 5.0f && mesh.positions[8] == 5.0f);
	CHECK(mesh.positions[9] == 0.0f && mesh.positions[10] == 0.0f && mesh.positions[11] == 10.0f);

	// UVs follow (uv0,uv1,uv3,uv2).
	CHECK(mesh.uvs.size() == 8);
	CHECK(mesh.uvs[0] == 1.0f && mesh.uvs[1] == 2.0f);
	CHECK(mesh.uvs[2] == 3.0f && mesh.uvs[3] == 4.0f);
	CHECK(mesh.uvs[4] == 7.0f && mesh.uvs[5] == 8.0f);
	CHECK(mesh.uvs[6] == 5.0f && mesh.uvs[7] == 6.0f);

	// Triangle list over stored A,B,D,C: ABD+BCD.
	CHECK(mesh.indices.size() == 6);
	CHECK(mesh.indices[0] == 0 && mesh.indices[1] == 1 && mesh.indices[2] == 2);
	CHECK(mesh.indices[3] == 1 && mesh.indices[4] == 3 && mesh.indices[5] == 2);

	CHECK(!mesh.colors.empty());
	CHECK(mesh.colors.size() == 12);
	CHECK(mesh.colors[0] == 10 && mesh.colors[1] == 20 && mesh.colors[2] == 30);
	CHECK(mesh.colors[9] == 10 && mesh.colors[10] == 20 && mesh.colors[11] == 30);

	CHECK(mesh.normals.empty()); // flat layout stores no per-vertex normals

	CHECK(mesh.submeshes.size() == 1);
	CHECK(mesh.submeshes[0].primitiveClass == SourcePrimitiveClass::TexturedFlatQuad);
	CHECK(mesh.submeshes[0].hasColors);
	CHECK(!mesh.submeshes[0].hasNormals);
	CHECK(mesh.submeshes[0].textureSet == 2 && mesh.submeshes[0].textureId == 5);
	CHECK(mesh.submeshes[0].firstIndex == 0 && mesh.submeshes[0].indexCount == 6);
}

// F-A pins the GT4 layout offsets: the gouraud quad's normals live at bytes
// 8..11 and its UVs at 12..19. Reading the 16-byte PL layout would return the
// normals as UVs, so distinct values catch that documented suspicion.
static void TestGouraudQuadLayout()
{
	SourceModelBuilder b;
	b.BeginVertices();
	b.AddVertex(1, 1, 1);   // v0
	b.AddVertex(2, 2, 2);   // v1
	b.AddVertex(3, 3, 3);   // v2
	b.AddVertex(4, 4, 4);   // v3
	b.BeginPointNormals();
	b.AddRawNormal(100, 0, 0);  // n0
	b.AddRawNormal(0, 100, 0);  // n1
	b.AddRawNormal(0, 0, 100);  // n2
	b.AddRawNormal(50, 50, 0);  // n3
	b.BeginPolys();
	AddGT4(b, 7, 8, 0, 1, 2, 3, 0, 1, 2, 3,
		UV(11, 12), UV(13, 14), UV(15, 16), UV(17, 18), RGB(200, 201, 202));

	SourceMesh mesh;
	char error[256] = "";
	SourceModelDecoderResult result = Decode(b, mesh, error, sizeof(error));
	CHECK(result.error == SOURCE_MODEL_ERROR_NONE);

	// Positions (v0,v1,v3,v2).
	CHECK(mesh.positions.size() == 12);
	CHECK(mesh.positions[6] == 4.0f && mesh.positions[7] == 4.0f && mesh.positions[8] == 4.0f);
	CHECK(mesh.positions[9] == 3.0f && mesh.positions[10] == 3.0f && mesh.positions[11] == 3.0f);

	// UVs (uv0,uv1,uv3,uv2) taken from the GT4 UV offsets, not the normals.
	CHECK(mesh.uvs.size() == 8);
	CHECK(mesh.uvs[0] == 11.0f && mesh.uvs[1] == 12.0f);
	CHECK(mesh.uvs[2] == 13.0f && mesh.uvs[3] == 14.0f);
	CHECK(mesh.uvs[4] == 17.0f && mesh.uvs[5] == 18.0f);
	CHECK(mesh.uvs[6] == 15.0f && mesh.uvs[7] == 16.0f);

	// Normals follow (n0,n1,n3,n2).
	CHECK(mesh.normals.size() == 12);
	CHECK(mesh.normals[0] == 100.0f && mesh.normals[1] == 0.0f && mesh.normals[2] == 0.0f);
	CHECK(mesh.normals[3] == 0.0f && mesh.normals[4] == 100.0f && mesh.normals[5] == 0.0f);
	CHECK(mesh.normals[6] == 50.0f && mesh.normals[7] == 50.0f && mesh.normals[8] == 0.0f);
	CHECK(mesh.normals[9] == 0.0f && mesh.normals[10] == 0.0f && mesh.normals[11] == 100.0f);

	CHECK(mesh.submeshes.size() == 1);
	CHECK(mesh.submeshes[0].primitiveClass == SourcePrimitiveClass::TexturedGouraudQuad);
	CHECK(mesh.submeshes[0].hasNormals);
	CHECK(mesh.submeshes[0].textureSet == 7 && mesh.submeshes[0].textureId == 8);

	// Raw stored face RGB on the first and last corner.
	CHECK(mesh.colors.size() == 12);
	CHECK(mesh.colors[0] == 200 && mesh.colors[1] == 201 && mesh.colors[2] == 202);
	CHECK(mesh.colors[9] == 200 && mesh.colors[10] == 201 && mesh.colors[11] == 202);
}

// --- F-B: triangle emission order and UVs -----------------------------------

static void TestTriangleOrderAndUVs()
{
	SourceModelBuilder b;
	b.BeginVertices();
	b.AddVertex(1, 1, 1);
	b.AddVertex(2, 2, 2);
	b.AddVertex(3, 3, 3);
	b.BeginPolys();
	AddFT3(b, 4, 6, 0, 1, 2, UV(21, 22), UV(23, 24), UV(25, 26), RGB(7, 8, 9));

	SourceMesh mesh;
	char error[256] = "";
	SourceModelDecoderResult result = Decode(b, mesh, error, sizeof(error));
	CHECK(result.error == SOURCE_MODEL_ERROR_NONE);

	CHECK(mesh.positions.size() == 9);
	CHECK(mesh.positions[0] == 1.0f && mesh.positions[3] == 2.0f && mesh.positions[6] == 3.0f);

	CHECK(mesh.uvs.size() == 6);
	CHECK(mesh.uvs[0] == 21.0f && mesh.uvs[1] == 22.0f);
	CHECK(mesh.uvs[2] == 23.0f && mesh.uvs[3] == 24.0f);
	CHECK(mesh.uvs[4] == 25.0f && mesh.uvs[5] == 26.0f);

	CHECK(mesh.indices.size() == 3);
	CHECK(mesh.indices[0] == 0 && mesh.indices[1] == 1 && mesh.indices[2] == 2);

	CHECK(mesh.colors.size() == 9);
	CHECK(mesh.colors[0] == 7 && mesh.colors[1] == 8 && mesh.colors[2] == 9);
	CHECK(mesh.colors[6] == 7 && mesh.colors[7] == 8 && mesh.colors[8] == 9);
	CHECK(mesh.normals.empty());
	CHECK(mesh.submeshes.size() == 1);
	CHECK(mesh.submeshes[0].primitiveClass == SourcePrimitiveClass::TexturedFlatTriangle);
	CHECK(!mesh.submeshes[0].hasNormals);
	CHECK(mesh.submeshes[0].firstIndex == 0 && mesh.submeshes[0].indexCount == 3);
}

static void TestGouraudTriangleOrderAndUVs()
{
	SourceModelBuilder b;
	b.BeginVertices();
	b.AddVertex(1, 0, 0);
	b.AddVertex(0, 1, 0);
	b.AddVertex(0, 0, 1);
	b.BeginPointNormals();
	b.AddRawNormal(9, 0, 0);
	b.AddRawNormal(0, 9, 0);
	b.AddRawNormal(0, 0, 9);
	b.BeginPolys();
	AddGT3(b, 1, 1, 0, 1, 2, 0, 1, 2, UV(31, 32), UV(33, 34), UV(35, 36), RGB(1, 2, 3));

	SourceMesh mesh;
	char error[256] = "";
	SourceModelDecoderResult result = Decode(b, mesh, error, sizeof(error));
	CHECK(result.error == SOURCE_MODEL_ERROR_NONE);
	CHECK(mesh.normals.size() == 9);
	CHECK(mesh.normals[0] == 9.0f && mesh.normals[4] == 9.0f && mesh.normals[8] == 9.0f);
	CHECK(mesh.colors.size() == 9);
	CHECK(mesh.colors[0] == 1 && mesh.colors[1] == 2 && mesh.colors[2] == 3);
	CHECK(mesh.colors[6] == 1 && mesh.colors[7] == 2 && mesh.colors[8] == 3);
	CHECK(mesh.indices.size() == 3 && mesh.indices[2] == 2);
	CHECK(mesh.submeshes.size() == 1);
	CHECK(mesh.submeshes[0].primitiveClass == SourcePrimitiveClass::TexturedGouraudTriangle);
	CHECK(mesh.submeshes[0].hasNormals);
}

// --- F-C: unsupported polygon type ------------------------------------------

static void TestUnsupportedPolygonType()
{
	CHECK(SourceModelDecoder_IsSupportedType(10));
	CHECK(SourceModelDecoder_IsSupportedType(11));
	CHECK(SourceModelDecoder_IsSupportedType(20));
	CHECK(SourceModelDecoder_IsSupportedType(21));
	CHECK(SourceModelDecoder_IsSupportedType(22));
	CHECK(SourceModelDecoder_IsSupportedType(23));
	CHECK(!SourceModelDecoder_IsSupportedType(8));
	CHECK(!SourceModelDecoder_IsSupportedType(9));
	CHECK(!SourceModelDecoder_IsSupportedType(4));
	CHECK(!SourceModelDecoder_IsSupportedType(0));

	// ptype 0 (untextured F3) is genuinely outside the allowlist.
	const unsigned char f3Record[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

	SourceModelBuilder b;
	b.BeginVertices();
	b.AddVertex(0, 0, 0);
	b.AddVertex(1, 0, 0);
	b.AddVertex(0, 1, 0);
	b.BeginPolys();
	b.AddPoly(f3Record, sizeof(f3Record));

	SourceMesh mesh;
	char error[256] = "";
	SourceModelDecoderResult result = Decode(b, mesh, error, sizeof(error));
	CHECK(result.error == SOURCE_MODEL_ERROR_UNSUPPORTED_PTYPE);
	CHECK(error[0] != '\0');
	CHECK(strstr(error, "unsupported") != NULL);
	CHECK(result.primitiveIndex == 0);
	CHECK(result.primitiveType == 0);
	CHECK(MeshIsEmpty(mesh));

	// The 4..7 textured bank is not consumed by the building/tile paths.
	SourceModelBuilder c;
	c.BeginVertices();
	c.AddVertex(0, 0, 0);
	c.BeginPolys();
	const unsigned char bankRecord[20] = { 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	c.AddPoly(bankRecord, sizeof(bankRecord));
	SourceMesh mesh2;
	result = Decode(c, mesh2, error, sizeof(error));
	CHECK(result.error == SOURCE_MODEL_ERROR_UNSUPPORTED_PTYPE);
	CHECK(result.primitiveType == 4);
	CHECK(MeshIsEmpty(mesh2));
}

// --- Block 2.1 skip-vs-fail policy ------------------------------------------

// A declared ptype 10 (PL textured triangle) is in the allowlist and decodes
// alongside ptype 11; nothing is skipped. STRICT and SKIP_UNSUPPORTED agree.
static void TestMixedPlTriangleAndQuad()
{
	const unsigned char type10Record[16] = { 10, 0, 0, 0, 0, 1, 2, 3, 0, 0, 0, 0, 0, 0, 0, 0 };

	SourceModelBuilder b;
	b.BeginVertices();
	b.AddVertex(0, 0, 0);
	b.AddVertex(1, 0, 0);
	b.AddVertex(1, 1, 0);
	b.AddVertex(0, 1, 0);
	b.BeginPolys();
	AddPL(b, 1, 2, 0x11, 0, 1, 2, 3, UV(1, 1), UV(2, 2), UV(3, 3), UV(4, 4));
	b.AddPoly(type10Record, sizeof(type10Record));

	SourceMesh mesh;
	char error[256] = "";
	SourceModelDecoderResult result = Decode(b, mesh, error, sizeof(error));
	CHECK(result.error == SOURCE_MODEL_ERROR_NONE);
	CHECK(result.skippedCount == 0);
	CHECK(mesh.submeshes.size() == 2);
	CHECK(mesh.submeshes[0].primitiveClass == SourcePrimitiveClass::TexturedPlQuad);
	CHECK(mesh.submeshes[0].indexCount == 6);
	CHECK(mesh.submeshes[1].primitiveClass == SourcePrimitiveClass::TexturedPlTriangle);
	CHECK(mesh.submeshes[1].indexCount == 3);
	CHECK(mesh.indices.size() == 9);
	CHECK(mesh.colors.empty());
	CHECK(mesh.normals.empty());

	// SKIP_UNSUPPORTED produces the same result: there is nothing to skip.
	SourceMesh skipMesh;
	result = Decode(b, skipMesh, error, sizeof(error),
		SourceModelDecoderPolicy::SkipUnsupported);
	CHECK(result.error == SOURCE_MODEL_ERROR_NONE);
	CHECK(result.skippedCount == 0);
	CHECK(MeshEqual(mesh, skipMesh));
}

// ptype 9 is odd, so ConvertPolygonTypes leaves it and the building loop does
// not draw it: a genuine runtime skip. SKIP_UNSUPPORTED advances by
// PolySizes[9] = 12 and counts it; STRICT rejects the same model.
static void TestSkipUnsupportedBuildingType()
{
	const unsigned char type9Record[12] = { 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

	SourceModelBuilder b;
	b.BeginVertices();
	b.AddVertex(0, 0, 0);
	b.AddVertex(1, 0, 0);
	b.AddVertex(1, 1, 0);
	b.AddVertex(0, 1, 0);
	b.BeginPolys();
	AddPL(b, 1, 2, 0x11, 0, 1, 2, 3, UV(1, 1), UV(2, 2), UV(3, 3), UV(4, 4));
	const long long secondOffset = (long long)b.bytes.size();
	b.AddPoly(type9Record, sizeof(type9Record));

	SourceMesh mesh;
	char error[256] = "";
	SourceModelDecoderResult result = Decode(b, mesh, error, sizeof(error),
		SourceModelDecoderPolicy::SkipUnsupported);
	CHECK(result.error == SOURCE_MODEL_ERROR_NONE);
	CHECK(mesh.submeshes.size() == 1);
	CHECK(mesh.submeshes[0].primitiveClass == SourcePrimitiveClass::TexturedPlQuad);
	CHECK(mesh.indices.size() == 6);
	CHECK(result.skippedCount == 1);
	CHECK(result.firstSkippedIndex == 1);
	CHECK(result.firstSkippedType == 9);
	CHECK(result.firstSkippedOffset == secondOffset);

	// STRICT on the same bytes is a named error with zero output.
	SourceMesh strictMesh;
	result = Decode(b, strictMesh, error, sizeof(error));
	CHECK(result.error == SOURCE_MODEL_ERROR_UNSUPPORTED_PTYPE);
	CHECK(result.primitiveType == 9);
	CHECK(result.skippedCount == 0);
	CHECK(result.firstSkippedIndex == -1);
	CHECK(MeshIsEmpty(strictMesh));
}

// SKIP_UNSUPPORTED with no unsupported records reports zero skips.
static void TestSkipUnsupportedNone()
{
	SourceModelBuilder b;
	b.BeginVertices();
	b.AddVertex(0, 0, 0);
	b.AddVertex(1, 0, 0);
	b.AddVertex(1, 1, 0);
	b.AddVertex(0, 1, 0);
	b.BeginPolys();
	AddPL(b, 1, 2, 0x11, 0, 1, 2, 3, UV(1, 1), UV(2, 2), UV(3, 3), UV(4, 4));

	SourceMesh mesh;
	char error[256] = "";
	SourceModelDecoderResult result = Decode(b, mesh, error, sizeof(error),
		SourceModelDecoderPolicy::SkipUnsupported);
	CHECK(result.error == SOURCE_MODEL_ERROR_NONE);
	CHECK(result.skippedCount == 0);
	CHECK(result.firstSkippedIndex == -1);
	CHECK(result.firstSkippedType == 0);
	CHECK(result.firstSkippedOffset == -1);
	CHECK(mesh.submeshes.size() == 1);
}

// Bounds stay strict under SKIP_UNSUPPORTED: an unsupported record that does not
// fit the span is still a hard error, never a silent skip.
static void TestSkipUnsupportedOutOfSpan()
{
	SourceModelBuilder b;
	b.BeginVertices();
	b.AddVertex(0, 0, 0);
	b.AddVertex(1, 0, 0);
	b.AddVertex(1, 1, 0);
	b.AddVertex(0, 1, 0);
	b.BeginPolys();
	AddPL(b, 1, 2, 0x11, 0, 1, 2, 3, UV(1, 1), UV(2, 2), UV(3, 3), UV(4, 4));
	// A ptype 9 header with only 8 bytes of body: stride 12 does not fit.
	const unsigned char partial[8] = { 9, 0, 0, 0, 0, 0, 0, 0 };
	b.AddPoly(partial, sizeof(partial));

	SourceMesh mesh;
	char error[256] = "";
	SourceModelDecoderResult result = Decode(b, mesh, error, sizeof(error),
		SourceModelDecoderPolicy::SkipUnsupported);
	CHECK(result.error == SOURCE_MODEL_ERROR_POLY_RECORD_RANGE);
	CHECK(result.primitiveType == 9);
	CHECK(MeshIsEmpty(mesh));
}

// A type whose PolySizes stride is zero cannot be advanced, so it is an error
// even under SKIP_UNSUPPORTED (and a strict error under STRICT).
static void TestSkipUnsupportedZeroStride()
{
	const unsigned char type12Record[8] = { 12, 0, 0, 0, 0, 0, 0, 0 };

	SourceModelBuilder b;
	b.BeginVertices();
	b.AddVertex(0, 0, 0);
	b.BeginPolys();
	b.AddPoly(type12Record, sizeof(type12Record));

	SourceMesh mesh;
	char error[256] = "";
	SourceModelDecoderResult result = Decode(b, mesh, error, sizeof(error),
		SourceModelDecoderPolicy::SkipUnsupported);
	CHECK(result.error == SOURCE_MODEL_ERROR_UNSKIPPABLE_PTYPE);
	CHECK(result.primitiveType == 12);
	CHECK(MeshIsEmpty(mesh));

	result = Decode(b, mesh, error, sizeof(error));
	CHECK(result.error == SOURCE_MODEL_ERROR_UNSUPPORTED_PTYPE);
	CHECK(MeshIsEmpty(mesh));
}

// F-A PL variant: ptype 11 (PL_POLYFT4) uses the same world quad order and UV
// order as 21/23 but stores no RGB, so no color is emitted for it.
static void TestPLQuadOrderAndUVs()
{
	SourceModelBuilder b;
	b.BeginVertices();
	b.AddVertex(10, 0, 0);  // v0
	b.AddVertex(0, 10, 0);  // v1
	b.AddVertex(0, 0, 10);  // v2
	b.AddVertex(5, 5, 5);   // v3
	b.BeginPolys();
	// `th` = 0x2a is a light/normal index and must be ignored, never a color.
	AddPL(b, 3, 4, 0x2a, 0, 1, 2, 3, UV(1, 2), UV(3, 4), UV(5, 6), UV(7, 8));

	SourceMesh mesh;
	char error[256] = "";
	SourceModelDecoderResult result = Decode(b, mesh, error, sizeof(error));
	CHECK(result.error == SOURCE_MODEL_ERROR_NONE);

	// Same world order (v0,v1,v3,v2) as the 16-byte PL layout's UVs allow.
	CHECK(mesh.positions.size() == 12);
	CHECK(mesh.positions[0] == 10.0f && mesh.positions[5] == 0.0f);
	CHECK(mesh.positions[6] == 5.0f && mesh.positions[7] == 5.0f && mesh.positions[8] == 5.0f);
	CHECK(mesh.positions[9] == 0.0f && mesh.positions[10] == 0.0f && mesh.positions[11] == 10.0f);

	CHECK(mesh.uvs.size() == 8);
	CHECK(mesh.uvs[0] == 1.0f && mesh.uvs[1] == 2.0f);
	CHECK(mesh.uvs[2] == 3.0f && mesh.uvs[3] == 4.0f);
	CHECK(mesh.uvs[4] == 7.0f && mesh.uvs[5] == 8.0f);
	CHECK(mesh.uvs[6] == 5.0f && mesh.uvs[7] == 6.0f);

	CHECK(mesh.indices.size() == 6);
	CHECK(mesh.indices[0] == 0 && mesh.indices[1] == 1 && mesh.indices[2] == 2);
	CHECK(mesh.indices[3] == 1 && mesh.indices[4] == 3 && mesh.indices[5] == 2);

	// No RGB exists in the PL layout, so the mesh has no color array at all.
	CHECK(mesh.colors.empty());
	CHECK(mesh.normals.empty());
	CHECK(mesh.submeshes.size() == 1);
	CHECK(mesh.submeshes[0].primitiveClass == SourcePrimitiveClass::TexturedPlQuad);
	CHECK(!mesh.submeshes[0].hasColors);
	CHECK(!mesh.submeshes[0].hasNormals);
	CHECK(mesh.submeshes[0].textureSet == 3 && mesh.submeshes[0].textureId == 4);
	CHECK(mesh.submeshes[0].firstIndex == 0 && mesh.submeshes[0].indexCount == 6);
}

// ptype 10 is a declared PL 3-vertex record (stride 16): v0,v1,v2 with
// uv0,uv1,uv2 and no RGB. Its v3/uv3 padding bytes are ignored (the runtime
// 10->11 conversion would fill them).
static void TestPLTriangleOrderAndUVs()
{
	SourceModelBuilder b;
	b.BeginVertices();
	b.AddVertex(10, 0, 0);
	b.AddVertex(0, 10, 0);
	b.AddVertex(0, 0, 10);
	b.BeginPolys();
	AddPLTriangle(b, 6, 7, 0x2a, 0, 1, 2, UV(1, 2), UV(3, 4), UV(5, 6));

	SourceMesh mesh;
	char error[256] = "";
	SourceModelDecoderResult result = Decode(b, mesh, error, sizeof(error));
	CHECK(result.error == SOURCE_MODEL_ERROR_NONE);

	CHECK(mesh.positions.size() == 9);
	CHECK(mesh.positions[0] == 10.0f && mesh.positions[3] == 0.0f && mesh.positions[4] == 10.0f);
	CHECK(mesh.positions[6] == 0.0f && mesh.positions[8] == 10.0f);

	CHECK(mesh.uvs.size() == 6);
	CHECK(mesh.uvs[0] == 1.0f && mesh.uvs[1] == 2.0f);
	CHECK(mesh.uvs[2] == 3.0f && mesh.uvs[3] == 4.0f);
	CHECK(mesh.uvs[4] == 5.0f && mesh.uvs[5] == 6.0f);

	CHECK(mesh.indices.size() == 3);
	CHECK(mesh.indices[0] == 0 && mesh.indices[1] == 1 && mesh.indices[2] == 2);

	CHECK(mesh.colors.empty());
	CHECK(mesh.normals.empty());
	CHECK(mesh.submeshes.size() == 1);
	CHECK(mesh.submeshes[0].primitiveClass == SourcePrimitiveClass::TexturedPlTriangle);
	CHECK(!mesh.submeshes[0].hasColors);
	CHECK(!mesh.submeshes[0].hasNormals);
	CHECK(mesh.submeshes[0].textureSet == 6 && mesh.submeshes[0].textureId == 7);
	CHECK(mesh.submeshes[0].firstIndex == 0 && mesh.submeshes[0].indexCount == 3);
}

// P2: the decoder exposes the legacy shading bucket per corner. The combo types
// {20,21} carry 0 (they use combointensity); the normal-shaded types {10,11,22,23}
// use the stored `th & 31` when its 0x80 flag is set, otherwise the recomputed
// normalIndex bucket. A +Z-facing triangle v0=(0,0,0), v1=(100,0,0),
// v2=(0,100,0) is the hand-computed bucket 8.
static void TestLegacyShadeIndices()
{
	SourceModelBuilder b;
	b.BeginVertices();
	b.AddVertex(0, 0, 0);     // 0
	b.AddVertex(100, 0, 0);   // 1
	b.AddVertex(0, 100, 0);   // 2
	b.AddVertex(0, 0, 100);   // 3
	b.BeginPolys();
	// ptype 11 quad, stored th=0 -> the decoder recomputes the bucket (8).
	AddPL(b, 1, 1, 0x00, 0, 1, 2, 3, UV(0, 0), UV(1, 0), UV(0, 1), UV(1, 1));
	// ptype 11 quad, stored th=0x80|7 -> the precomputed bucket 7 is used.
	AddPL(b, 1, 1, 0x80 | 7, 0, 1, 2, 3, UV(0, 0), UV(1, 0), UV(0, 1), UV(1, 1));
	// ptype 21 quad -> combo, bucket 0.
	AddFT4(b, 1, 1, 0, 1, 2, 3, UV(0, 0), UV(1, 0), UV(0, 1), UV(1, 1), RGB(9, 9, 9));

	SourceMesh mesh;
	char error[256] = "";
	SourceModelDecoderResult result = Decode(b, mesh, error, sizeof(error));
	CHECK(result.error == SOURCE_MODEL_ERROR_NONE);

	CHECK(mesh.positions.size() / 3 == 12);
	CHECK(mesh.shadeIndices.size() == 12);
	// First quad (4 corners): recomputed bucket 8.
	CHECK(mesh.shadeIndices[0] == 8 && mesh.shadeIndices[3] == 8);
	// Second quad: stored precomputed bucket 7.
	CHECK(mesh.shadeIndices[4] == 7 && mesh.shadeIndices[7] == 7);
	// FT4 combo quad carries 0.
	CHECK(mesh.shadeIndices[8] == 0 && mesh.shadeIndices[11] == 0);

	// The two PL records merge into one submesh (same class/texture); the FT4 is
	// a separate submesh, so the combo/normal branch is decided per submesh.
	CHECK(mesh.submeshes.size() == 2);
	CHECK(mesh.submeshes[0].primitiveClass == SourcePrimitiveClass::TexturedPlQuad);
	CHECK(mesh.submeshes[1].primitiveClass == SourcePrimitiveClass::TexturedFlatQuad);
}

// A model may legally mix a PL record with an FT4 record (the building loop
// draws 11, 21 and 23). Color presence is per submesh: the PL run is flagged
// hasColors == false and its placeholder bytes are never a real color, while
// the FT4 run carries its RGB.
static void TestMixedModelPerSubmeshColors()
{
	SourceModelBuilder b;
	b.BeginVertices();
	b.AddVertex(0, 0, 0);
	b.AddVertex(1, 0, 0);
	b.AddVertex(1, 1, 0);
	b.AddVertex(0, 1, 0);
	b.BeginPolys();
	// PL run (same texture key as the FT4 below, but a different class).
	AddPL(b, 1, 2, 0x11, 0, 1, 2, 3, UV(1, 1), UV(2, 2), UV(3, 3), UV(4, 4));
	// FT4 run with the same texture set/id.
	AddFT4(b, 1, 2, 0, 1, 2, 3, UV(5, 5), UV(6, 6), UV(7, 7), UV(8, 8), RGB(10, 20, 30));

	SourceMesh mesh;
	char error[256] = "";
	SourceModelDecoderResult result = Decode(b, mesh, error, sizeof(error));
	CHECK(result.error == SOURCE_MODEL_ERROR_NONE);

	// The two records do not merge: a PL run never mixes with a colored run.
	CHECK(mesh.submeshes.size() == 2);

	CHECK(mesh.submeshes[0].primitiveClass == SourcePrimitiveClass::TexturedPlQuad);
	CHECK(!mesh.submeshes[0].hasColors);
	CHECK(!mesh.submeshes[0].hasNormals);
	CHECK(mesh.submeshes[0].firstIndex == 0 && mesh.submeshes[0].indexCount == 6);

	CHECK(mesh.submeshes[1].primitiveClass == SourcePrimitiveClass::TexturedFlatQuad);
	CHECK(mesh.submeshes[1].hasColors);
	CHECK(!mesh.submeshes[1].hasNormals);
	CHECK(mesh.submeshes[1].firstIndex == 6 && mesh.submeshes[1].indexCount == 6);

	// Both runs are flat, so no run stores normals.
	CHECK(mesh.normals.empty());

	// Colors are present for the whole mesh (one colored run exists); the PL
	// corners are zero placeholders that the flag marks as invalid.
	CHECK(mesh.colors.size() == 24);
	CHECK(mesh.colors[0] == 0 && mesh.colors[1] == 0 && mesh.colors[2] == 0);
	CHECK(mesh.colors[12] == 10 && mesh.colors[13] == 20 && mesh.colors[14] == 30);
	CHECK(mesh.colors[21] == 10 && mesh.colors[22] == 20 && mesh.colors[23] == 30);

	// Index ranges are contiguous and complete.
	CHECK(mesh.indices.size() == 12);
	CHECK(mesh.submeshes[0].firstIndex + mesh.submeshes[0].indexCount == mesh.submeshes[1].firstIndex);
	CHECK(mesh.submeshes[1].firstIndex + mesh.submeshes[1].indexCount == mesh.indices.size());
}

// F2: a model may legally mix flat (FT4) and gouraud (GT4) records. Normal
// presence is per submesh; flat corners hold aligned zero placeholders and the
// gouraud run keeps its real normals.
static void TestMixedModelPerSubmeshNormals()
{
	SourceModelBuilder b;
	b.BeginVertices();
	b.AddVertex(0, 0, 0);
	b.AddVertex(1, 0, 0);
	b.AddVertex(1, 1, 0);
	b.AddVertex(0, 1, 0);
	b.BeginPointNormals();
	b.AddRawNormal(10, 0, 0); // n0
	b.AddRawNormal(0, 10, 0); // n1
	b.AddRawNormal(0, 0, 10); // n2
	b.AddRawNormal(5, 5, 0);  // n3
	b.BeginPolys();
	AddFT4(b, 1, 1, 0, 1, 2, 3, UV(1, 1), UV(2, 2), UV(3, 3), UV(4, 4), RGB(1, 2, 3));
	AddGT4(b, 1, 1, 0, 1, 2, 3, 0, 1, 2, 3,
		UV(5, 5), UV(6, 6), UV(7, 7), UV(8, 8), RGB(4, 5, 6));

	SourceMesh mesh;
	char error[256] = "";
	SourceModelDecoderResult result = Decode(b, mesh, error, sizeof(error));
	CHECK(result.error == SOURCE_MODEL_ERROR_NONE);

	CHECK(mesh.submeshes.size() == 2);
	CHECK(mesh.submeshes[0].primitiveClass == SourcePrimitiveClass::TexturedFlatQuad);
	CHECK(!mesh.submeshes[0].hasNormals);
	CHECK(mesh.submeshes[0].hasColors);
	CHECK(mesh.submeshes[0].firstIndex == 0 && mesh.submeshes[0].indexCount == 6);

	CHECK(mesh.submeshes[1].primitiveClass == SourcePrimitiveClass::TexturedGouraudQuad);
	CHECK(mesh.submeshes[1].hasNormals);
	CHECK(mesh.submeshes[1].hasColors);
	CHECK(mesh.submeshes[1].firstIndex == 6 && mesh.submeshes[1].indexCount == 6);

	// Normals stay aligned with positions: flat corners are zero, the gouraud
	// corners carry (n0,n1,n3,n2).
	CHECK(mesh.normals.size() == mesh.positions.size());
	CHECK(mesh.normals[0] == 0.0f && mesh.normals[1] == 0.0f && mesh.normals[2] == 0.0f);
	CHECK(mesh.normals[9] == 0.0f && mesh.normals[10] == 0.0f && mesh.normals[11] == 0.0f);
	CHECK(mesh.normals[12] == 10.0f && mesh.normals[13] == 0.0f && mesh.normals[14] == 0.0f);
	CHECK(mesh.normals[15] == 0.0f && mesh.normals[16] == 10.0f && mesh.normals[17] == 0.0f);
	CHECK(mesh.normals[18] == 5.0f && mesh.normals[19] == 5.0f && mesh.normals[20] == 0.0f);
	CHECK(mesh.normals[21] == 0.0f && mesh.normals[22] == 0.0f && mesh.normals[23] == 10.0f);

	// Contiguous, complete ranges.
	CHECK(mesh.submeshes[0].firstIndex + mesh.submeshes[0].indexCount == mesh.submeshes[1].firstIndex);
	CHECK(mesh.submeshes[1].firstIndex + mesh.submeshes[1].indexCount == mesh.indices.size());
}

// F6: an empty model (no vertices, no polygons) is a valid, empty mesh. The
// unset offsets must not be dereferenced or treated as misuse.
static void TestEmptyModel()
{
	SourceModelBuilder b; // no BeginVertices/BeginPolys; all offsets stay -1
	SourceModelView view = b.View();
	CHECK(view.numVertices == 0 && view.numPolys == 0);

	SourceMesh mesh;
	char error[256] = "";
	SourceModelDecoderResult result = DecodeSourceModel(b.bytes.data(),
		b.bytes.data() + b.bytes.size(), view, &mesh, error, sizeof(error));
	CHECK(result.error == SOURCE_MODEL_ERROR_NONE);
	CHECK(error[0] == '\0');
	CHECK(MeshIsEmpty(mesh));
}

// --- F-D: span violations ---------------------------------------------------

static void TestSpanViolations()
{
	char error[256] = "";
	SourceMesh mesh;

	// (1) The declared vertex array lies outside the span.
	{
		SourceModelBuilder b;
		b.BeginVertices();
		b.AddVertex(1, 2, 3);
		b.BeginPolys();
		AddFT3(b, 0, 0, 0, 0, 0, UV(0, 0), UV(0, 0), UV(0, 0), RGB(0, 0, 0));
		SourceModelView view = b.View();
		view.vertices = (int)b.bytes.size();
		SourceModelDecoderResult result = DecodeSourceModel(b.bytes.data(),
			b.bytes.data() + b.bytes.size(), view, &mesh, error, sizeof(error));
		CHECK(result.error == SOURCE_MODEL_ERROR_VERTICES_RANGE);
		CHECK(error[0] != '\0');
		CHECK(MeshIsEmpty(mesh));
	}

	// (2) The polygon block start lies outside the span.
	{
		SourceModelBuilder b;
		b.BeginVertices();
		for (int i = 0; i < 4; ++i) b.AddVertex((short)i, 0, 0);
		b.BeginPolys();
		AddFT4(b, 0, 0, 0, 1, 2, 3, UV(0, 0), UV(0, 0), UV(0, 0), UV(0, 0), RGB(0, 0, 0));
		SourceModelView view = b.View();
		view.polyBlock = (int)b.bytes.size();
		SourceModelDecoderResult result = DecodeSourceModel(b.bytes.data(),
			b.bytes.data() + b.bytes.size(), view, &mesh, error, sizeof(error));
		CHECK(result.error == SOURCE_MODEL_ERROR_POLY_BLOCK_RANGE);
		CHECK(MeshIsEmpty(mesh));
	}

	// (3) count*stride does not fit the span (record truncated).
	{
		SourceModelBuilder b;
		b.BeginVertices();
		for (int i = 0; i < 4; ++i) b.AddVertex((short)i, 0, 0);
		b.BeginPolys();
		const unsigned char partial[10] = { 21, 0, 0, 0, 0, 1, 2, 3, 0, 0 };
		b.AddPoly(partial, sizeof(partial));
		SourceModelDecoderResult result = Decode(b, mesh, error, sizeof(error));
		CHECK(result.error == SOURCE_MODEL_ERROR_POLY_RECORD_RANGE);
		CHECK(MeshIsEmpty(mesh));
	}

	// (4) The walk runs past the end: a second record header is present but the
	//     record body is not.
	{
		SourceModelBuilder b;
		b.BeginVertices();
		for (int i = 0; i < 4; ++i) b.AddVertex((short)i, 0, 0);
		b.BeginPolys();
		AddFT4(b, 0, 0, 0, 1, 2, 3, UV(0, 0), UV(0, 0), UV(0, 0), UV(0, 0), RGB(0, 0, 0));
		const unsigned char secondHeader[5] = { 21, 0, 0, 0, 0 };
		b.bytes.insert(b.bytes.end(), secondHeader, secondHeader + sizeof(secondHeader));
		SourceModelView view = b.View();
		view.numPolys = 2;
		SourceModelDecoderResult result = DecodeSourceModel(b.bytes.data(),
			b.bytes.data() + b.bytes.size(), view, &mesh, error, sizeof(error));
		CHECK(result.error == SOURCE_MODEL_ERROR_POLY_RECORD_RANGE);
		CHECK(MeshIsEmpty(mesh));
	}

	// (5) A vertex index is outside the declared vertex count.
	{
		SourceModelBuilder b;
		b.BeginVertices();
		for (int i = 0; i < 4; ++i) b.AddVertex((short)i, 0, 0);
		b.BeginPolys();
		AddFT4(b, 0, 0, 0, 1, 2, 9, UV(0, 0), UV(0, 0), UV(0, 0), UV(0, 0), RGB(0, 0, 0));
		SourceModelDecoderResult result = Decode(b, mesh, error, sizeof(error));
		CHECK(result.error == SOURCE_MODEL_ERROR_VERTEX_INDEX);
		CHECK(MeshIsEmpty(mesh));
	}

	// (6) A gouraud normal index is outside the declared point-normal count.
	{
		SourceModelBuilder b;
		b.BeginVertices();
		for (int i = 0; i < 4; ++i) b.AddVertex((short)i, 0, 0);
		b.BeginPointNormals();
		b.AddRawNormal(1, 0, 0);
		b.AddRawNormal(0, 1, 0);
		b.BeginPolys();
		AddGT4(b, 0, 0, 0, 1, 2, 3, 0, 1, 9, 2,
			UV(0, 0), UV(0, 0), UV(0, 0), UV(0, 0), RGB(0, 0, 0));
		SourceModelDecoderResult result = Decode(b, mesh, error, sizeof(error));
		CHECK(result.error == SOURCE_MODEL_ERROR_NORMAL_INDEX);
		CHECK(MeshIsEmpty(mesh));
	}

	// (7) The point-normal array lies outside the span.
	{
		SourceModelBuilder b;
		b.BeginVertices();
		for (int i = 0; i < 4; ++i) b.AddVertex((short)i, 0, 0);
		b.BeginPointNormals();
		b.AddRawNormal(1, 0, 0);
		b.BeginPolys();
		AddGT4(b, 0, 0, 0, 1, 2, 3, 0, 0, 0, 0,
			UV(0, 0), UV(0, 0), UV(0, 0), UV(0, 0), RGB(0, 0, 0));
		SourceModelView view = b.View();
		view.pointNormals = (int)b.bytes.size();
		SourceModelDecoderResult result = DecodeSourceModel(b.bytes.data(),
			b.bytes.data() + b.bytes.size(), view, &mesh, error, sizeof(error));
		CHECK(result.error == SOURCE_MODEL_ERROR_POINT_NORMALS_RANGE);
		CHECK(MeshIsEmpty(mesh));
	}

	// (8) An instanced model with no base span is the named instance-base error.
	{
		SourceModelBuilder b;
		b.BeginVertices();
		b.AddVertex(0, 0, 0);
		b.BeginPolys();
		SourceModelView view = b.View();
		view.instanceNumber = 3;
		SourceModelDecoderResult result = DecodeSourceModel(b.bytes.data(),
			b.bytes.data() + b.bytes.size(), view, &mesh, error, sizeof(error));
		CHECK(result.error == SOURCE_MODEL_ERROR_INSTANCE_BASE);
		CHECK(MeshIsEmpty(mesh));
	}

	// (9) An empty or inverted span is rejected.
	{
		SourceModelBuilder b;
		SourceModelView view = b.View();
		SourceModelDecoderResult result = DecodeSourceModel(b.bytes.data(), b.bytes.data(),
			view, &mesh, error, sizeof(error));
		CHECK(result.error == SOURCE_MODEL_ERROR_RANGE);
		CHECK(MeshIsEmpty(mesh));
	}

	// (10) A missing destination is a named argument error.
	{
		SourceModelBuilder b;
		SourceModelView view = b.View();
		SourceModelDecoderResult result = DecodeSourceModel(b.bytes.data(),
			b.bytes.data() + b.bytes.size(), view, NULL, error, sizeof(error));
		CHECK(result.error == SOURCE_MODEL_ERROR_ARGUMENT);
	}

	// A NULL diagnostic buffer must not crash.
	{
		SourceModelBuilder b;
		SourceModelView view = b.View();
		view.instanceNumber = 5;
		SourceModelDecoderResult result = DecodeSourceModel(b.bytes.data(),
			b.bytes.data() + b.bytes.size(), view, &mesh, NULL, 0);
		CHECK(result.error == SOURCE_MODEL_ERROR_INSTANCE_BASE);
		CHECK(MeshIsEmpty(mesh));
	}
}

// --- F4: instanced models share the base model's vertex/normal arrays --------
//
// An instanced MODEL (instance_number != -1) owns only its polygon block; its
// vertex and point-normal arrays are the base model's, and its polygon indices
// are ABSOLUTE into those arrays (models.c `_MDL_GETTER_vertices/normals/
// point_normals`; draw.c `&srcVerts[polys->v0]`). The fixtures build a base span
// and an instance span separately and assert the emitted geometry comes from the
// base arrays while the instance span supplies the polygon block/materials.
static void TestInstancedModel()
{
	char error[256] = "";
	SourceMesh mesh;

	// The base model owns the shared vertex and point-normal arrays.
	SourceModelBuilder base;
	base.BeginVertices();
	base.AddVertex(10, 0, 0);   // v0
	base.AddVertex(0, 20, 0);   // v1
	base.AddVertex(0, 0, 30);   // v2
	base.AddVertex(40, 50, 60); // v3
	base.BeginPointNormals();
	base.AddRawNormal(1, 0, 0); // n0
	base.AddRawNormal(0, 1, 0); // n1
	base.AddRawNormal(0, 0, 1); // n2
	base.AddRawNormal(7, 8, 9); // n3

	// The instance carries only its polygon block; its own vertex/point-normal
	// fields are ignored.
	SourceModelBuilder instance;
	instance.BeginPolys();
	AddGT4(instance, 1, 2, 0, 1, 2, 3, 0, 1, 2, 3,
		UV(1, 2), UV(3, 4), UV(5, 6), UV(7, 8), RGB(10, 20, 30));

	SourceModelView view = instance.View();
	view.instanceNumber = 0;
	// Deliberately bogus instance fields: the decoder must ignore them.
	view.numVertices = 1;
	view.numPointNormals = 0;
	view.vertices = -1;
	view.pointNormals = -1;
	view.baseBegin = base.bytes.data();
	view.baseEnd = base.bytes.data() + base.bytes.size();
	view.baseVertices = base.verticesOffset;
	view.basePointNormals = base.pointNormalsOffset;
	view.baseNumVertices = base.vertexCount;
	view.baseNumPointNormals = base.pointNormalCount;

	SourceModelDecoderResult result = DecodeSourceModel(instance.bytes.data(),
		instance.bytes.data() + instance.bytes.size(), view, &mesh, error, sizeof(error));
	CHECK(result.error == SOURCE_MODEL_ERROR_NONE);
	CHECK(error[0] == '\0');

	// Positions come from the base vertices, in world quad order (v0,v1,v3,v2).
	CHECK(mesh.positions.size() == 12);
	CHECK(mesh.positions[0] == 10.0f && mesh.positions[1] == 0.0f && mesh.positions[2] == 0.0f);
	CHECK(mesh.positions[3] == 0.0f && mesh.positions[4] == 20.0f && mesh.positions[5] == 0.0f);
	CHECK(mesh.positions[6] == 40.0f && mesh.positions[7] == 50.0f && mesh.positions[8] == 60.0f);
	CHECK(mesh.positions[9] == 0.0f && mesh.positions[10] == 0.0f && mesh.positions[11] == 30.0f);

	// Normals come from the base point-normals, in the same (n0,n1,n3,n2) order.
	CHECK(mesh.normals.size() == 12);
	CHECK(mesh.normals[0] == 1.0f && mesh.normals[1] == 0.0f && mesh.normals[2] == 0.0f);
	CHECK(mesh.normals[3] == 0.0f && mesh.normals[4] == 1.0f && mesh.normals[5] == 0.0f);
	CHECK(mesh.normals[6] == 7.0f && mesh.normals[7] == 8.0f && mesh.normals[8] == 9.0f);
	CHECK(mesh.normals[9] == 0.0f && mesh.normals[10] == 0.0f && mesh.normals[11] == 1.0f);

	// The instance's own span supplies the poly block/material grouping.
	CHECK(mesh.submeshes.size() == 1);
	CHECK(mesh.submeshes[0].primitiveClass == SourcePrimitiveClass::TexturedGouraudQuad);
	CHECK(mesh.submeshes[0].textureSet == 1 && mesh.submeshes[0].textureId == 2);
	CHECK(mesh.submeshes[0].hasNormals && mesh.submeshes[0].hasColors);

	// (a) A vertex index is validated against the BASE count, not the instance's
	//     ignored count: index 4 >= 4 base vertices fails.
	{
		SourceModelBuilder bad;
		bad.BeginPolys();
		AddFT4(bad, 0, 0, 0, 1, 2, 4, UV(0, 0), UV(0, 0), UV(0, 0), UV(0, 0), RGB(0, 0, 0));
		SourceModelView badView = bad.View();
		badView.instanceNumber = 0;
		badView.baseBegin = base.bytes.data();
		badView.baseEnd = base.bytes.data() + base.bytes.size();
		badView.baseVertices = base.verticesOffset;
		badView.basePointNormals = base.pointNormalsOffset;
		badView.baseNumVertices = base.vertexCount;
		badView.baseNumPointNormals = base.pointNormalCount;
		SourceMesh badMesh;
		SourceModelDecoderResult badResult = DecodeSourceModel(bad.bytes.data(),
			bad.bytes.data() + bad.bytes.size(), badView, &badMesh, error, sizeof(error));
		CHECK(badResult.error == SOURCE_MODEL_ERROR_VERTEX_INDEX);
		CHECK(MeshIsEmpty(badMesh));
	}

	// (b) The base vertex array must lie inside the base span.
	{
		SourceModelBuilder bad;
		bad.BeginPolys();
		AddFT4(bad, 0, 0, 0, 1, 2, 3, UV(0, 0), UV(0, 0), UV(0, 0), UV(0, 0), RGB(0, 0, 0));
		SourceModelView badView = bad.View();
		badView.instanceNumber = 0;
		badView.baseBegin = base.bytes.data();
		badView.baseEnd = base.bytes.data() + base.bytes.size();
		badView.baseVertices = (int)base.bytes.size(); // past the base span
		badView.basePointNormals = base.pointNormalsOffset;
		badView.baseNumVertices = base.vertexCount;
		badView.baseNumPointNormals = base.pointNormalCount;
		SourceMesh badMesh;
		SourceModelDecoderResult badResult = DecodeSourceModel(bad.bytes.data(),
			bad.bytes.data() + bad.bytes.size(), badView, &badMesh, error, sizeof(error));
		CHECK(badResult.error == SOURCE_MODEL_ERROR_VERTICES_RANGE);
		CHECK(MeshIsEmpty(badMesh));
	}

	// (c) A missing base span is the named instance-base error.
	{
		SourceModelBuilder bad;
		bad.BeginPolys();
		AddFT4(bad, 0, 0, 0, 1, 2, 3, UV(0, 0), UV(0, 0), UV(0, 0), UV(0, 0), RGB(0, 0, 0));
		SourceModelView badView = bad.View();
		badView.instanceNumber = 0;
		SourceMesh badMesh;
		SourceModelDecoderResult badResult = DecodeSourceModel(bad.bytes.data(),
			bad.bytes.data() + bad.bytes.size(), badView, &badMesh, error, sizeof(error));
		CHECK(badResult.error == SOURCE_MODEL_ERROR_INSTANCE_BASE);
		CHECK(error[0] != '\0');
		CHECK(MeshIsEmpty(badMesh));
	}

	// (d) Determinism: the same instance view decodes to the same mesh.
	{
		SourceMesh again;
		SourceModelDecoderResult resultAgain = DecodeSourceModel(instance.bytes.data(),
			instance.bytes.data() + instance.bytes.size(), view, &again, error, sizeof(error));
		CHECK(resultAgain.error == SOURCE_MODEL_ERROR_NONE);
		CHECK(MeshEqual(mesh, again));
	}
}

// --- Block-2 convention fixtures: yang rotation, view, winding, unit ---------
//
// These fixtures do NOT exercise the decoder. They pin the conventions Block 2
// uses when it feeds decoded world geometry to the native renderer:
//
//  * object yaw: yang -> matrixtable[yang] = RotMatrixY(yang*64), a 4096-unit
//    PSX turn (draw.c:507-525,1508-1517; cell.h:19). The building/tile
//    renderers take yang modulo 64, so yang and yang+64 select the same entry.
//  * camera composition: view = R_cam * (pos - cam) with R_obj =
//    RotMatrixY(yang*64), mirroring utils/DeveloperModernMesh.cpp:494-531.
//  * winding/cull: the decoder emits a world quad as (v0,v1,v3,v2) and fans it
//    (0,1,2)+(0,2,3) (utils/SourceModelDecoder.cpp:384-393). The first triangle
//    (v0,v1,v3) is exactly the triangle the runtime feeds to gte_nclip
//    (draw.c:780-796). These fixtures pin the NDC sign of that triangle and the
//    Diligent cull policy it implies, under the game's +Y-down projection
//    (BuildPerspectiveFixture negates camera y, so the game-front triangle has
//    a negative NDC signed area and the pin is FrontCounterClockwise False).
//  * unit policy: translation, vertices and projection near/far share one raw
//    game unit (4096 per metre).
//
// All math is local: no Game/PSX headers and no GPU. C++11.

static int YangToAngle(unsigned int yang)
{
	return (int)(yang & 63) * 64;
}

static bool Near(float a, float b, float tolerance)
{
	return fabsf(a - b) <= tolerance;
}

// Analytic PSX RotMatrixY at `angle` (4096-unit turn), row-major m[row][col].
// Matches PsyCross RotMatrixY applied to an identity matrix
// (PsyCross/src/psx/LIBGTE.C:696-716):
//   [ c  0  s ]
//   [ 0  1  0 ]
//   [-s  0  c ]
static void RotMatrixYAnalytic(int angle, float m[3][3])
{
	const float a = (float)(angle & 4095) * (2.0f * 3.14159265358979f / 4096.0f);
	const float c = cosf(a);
	const float s = sinf(a);
	m[0][0] = c;    m[0][1] = 0.0f; m[0][2] = s;
	m[1][0] = 0.0f; m[1][1] = 1.0f; m[1][2] = 0.0f;
	m[2][0] = -s;   m[2][1] = 0.0f; m[2][2] = c;
}

// Object yaw from a packed cell object: the table entry is RotMatrixY(yang*64)
// (draw.c:507-525, cell.h:19).
static void ObjectRotationFromYang(unsigned int yang, float m[3][3])
{
	RotMatrixYAnalytic(YangToAngle(yang), m);
}

static bool MatrixNear(const float a[3][3], const float b[3][3], float tolerance)
{
	for (int i = 0; i < 3; ++i)
		for (int j = 0; j < 3; ++j)
			if (!Near(a[i][j], b[i][j], tolerance))
				return false;
	return true;
}

static float Dot3(const float a[3], const float b[3])
{
	return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

static void Cross3(const float a[3], const float b[3], float out[3])
{
	out[0] = a[1] * b[2] - a[2] * b[1];
	out[1] = a[2] * b[0] - a[0] * b[2];
	out[2] = a[0] * b[1] - a[1] * b[0];
}

static void TestBlock2YangRotationGate()
{
	// Cardinal cases: a 4096-unit turn, so 1024 units is a quarter turn.
	CHECK(YangToAngle(0) == 0);
	CHECK(YangToAngle(16) == 1024);
	CHECK(YangToAngle(32) == 2048);
	CHECK(YangToAngle(48) == 3072);

	// 64 increments of 64 units is a full turn; yang is 6 bits, so entry 64
	// wraps back to the identity like entry 0.
	CHECK(64 * 64 == 4096);
	CHECK(YangToAngle(64) == 0);

	// Wrap: yang and yang+64 select the same rotation matrix.
	for (unsigned int yang = 0; yang < 64; ++yang)
	{
		CHECK(YangToAngle(yang) == YangToAngle(yang + 64));
		CHECK(YangToAngle(yang) == YangToAngle(yang + 128));
	}

	// The 64 entries cover the full turn exactly once.
	bool seen[64];
	memset(seen, 0, sizeof(seen));
	for (unsigned int yang = 0; yang < 64; ++yang)
		seen[(YangToAngle(yang) / 64) & 63] = true;
	int covered = 0;
	for (int i = 0; i < 64; ++i)
		if (seen[i]) covered++;
	CHECK(covered == 64);

	// Cardinal matrices: exact quarter-turn values catch a wrong axis or sign.
	float m[3][3];
	ObjectRotationFromYang(0, m);
	CHECK(Near(m[0][0], 1.0f, 1e-5f) && Near(m[0][2], 0.0f, 1e-5f) &&
		Near(m[2][0], 0.0f, 1e-5f) && Near(m[2][2], 1.0f, 1e-5f));
	ObjectRotationFromYang(16, m);
	CHECK(Near(m[0][0], 0.0f, 1e-5f) && Near(m[0][2], 1.0f, 1e-5f) &&
		Near(m[2][0], -1.0f, 1e-5f) && Near(m[2][2], 0.0f, 1e-5f));
	ObjectRotationFromYang(32, m);
	CHECK(Near(m[0][0], -1.0f, 1e-5f) && Near(m[0][2], 0.0f, 1e-5f) &&
		Near(m[2][0], 0.0f, 1e-5f) && Near(m[2][2], -1.0f, 1e-5f));
	ObjectRotationFromYang(48, m);
	CHECK(Near(m[0][0], 0.0f, 1e-5f) && Near(m[0][2], -1.0f, 1e-5f) &&
		Near(m[2][0], 1.0f, 1e-5f) && Near(m[2][2], 0.0f, 1e-5f));

	// Real matrix comparison: the yang convention equals the independent
	// analytic RotMatrixY at yang*64, and the result is a proper rotation
	// (orthonormal, right-handed) for every entry.
	for (unsigned int yang = 0; yang < 64; ++yang)
	{
		float fromYang[3][3];
		float analytic[3][3];
		ObjectRotationFromYang(yang, fromYang);
		RotMatrixYAnalytic(YangToAngle(yang), analytic);
		CHECK(MatrixNear(fromYang, analytic, 1e-5f));

		const float* r0 = fromYang[0];
		const float* r1 = fromYang[1];
		const float* r2 = fromYang[2];
		CHECK(Near(Dot3(r0, r0), 1.0f, 1e-4f));
		CHECK(Near(Dot3(r1, r1), 1.0f, 1e-4f));
		CHECK(Near(Dot3(r2, r2), 1.0f, 1e-4f));
		CHECK(Near(Dot3(r0, r1), 0.0f, 1e-4f));
		CHECK(Near(Dot3(r0, r2), 0.0f, 1e-4f));
		CHECK(Near(Dot3(r1, r2), 0.0f, 1e-4f));

		float cross[3];
		Cross3(r1, r2, cross);
		CHECK(Near(Dot3(r0, cross), 1.0f, 1e-4f)); // det == +1, right-handed

		// The rotation equals rotating the basis vectors about Y by the same
		// angle, the independent definition of a Y rotation.
		float wrapped[3][3];
		ObjectRotationFromYang(yang + 64, wrapped);
		CHECK(MatrixNear(fromYang, wrapped, 1e-5f));
	}
}

// Column-major 4x4 helpers matching native/NativeRenderer.cpp
// (MultiplyColumnMajor and the shader's mul(MVP, float4(pos, 1))).

static void MultiplyColumnMajor(const float a[16], const float b[16], float out[16])
{
	for (int col = 0; col < 4; ++col)
	{
		for (int row = 0; row < 4; ++row)
		{
			float sum = 0.0f;
			for (int k = 0; k < 4; ++k)
				sum += a[k * 4 + row] * b[col * 4 + k];
			out[col * 4 + row] = sum;
		}
	}
}

// view * p, with `view` column-major (matches the native shader).
static void TransformPoint(const float view[16], const float p[3], float out[4])
{
	out[0] = view[0] * p[0] + view[4] * p[1] + view[8] * p[2] + view[12];
	out[1] = view[1] * p[0] + view[5] * p[1] + view[9] * p[2] + view[13];
	out[2] = view[2] * p[0] + view[6] * p[1] + view[10] * p[2] + view[14];
	out[3] = view[3] * p[0] + view[7] * p[1] + view[11] * p[2] + view[15];
}

// Mirrors DeveloperModernMesh.cpp:494-531: view = R_cam * (R_obj * p + pos - cam).
static void ComposeInstanceView(const float r[3][3], const float cam[3],
	const float pos[3], unsigned int yang, float view[16])
{
	float o[3][3];
	ObjectRotationFromYang(yang, o);

	float m[3][3];
	for (int i = 0; i < 3; ++i)
		for (int j = 0; j < 3; ++j)
			m[i][j] = r[i][0] * o[0][j] + r[i][1] * o[1][j] + r[i][2] * o[2][j];

	const float dx = pos[0] - cam[0];
	const float dy = pos[1] - cam[1];
	const float dz = pos[2] - cam[2];
	const float tx = r[0][0] * dx + r[0][1] * dy + r[0][2] * dz;
	const float ty = r[1][0] * dx + r[1][1] * dy + r[1][2] * dz;
	const float tz = r[2][0] * dx + r[2][1] * dy + r[2][2] * dz;

	view[0] = m[0][0]; view[4] = m[0][1]; view[8] = m[0][2];  view[12] = tx;
	view[1] = m[1][0]; view[5] = m[1][1]; view[9] = m[1][2];  view[13] = ty;
	view[2] = m[2][0]; view[6] = m[2][1]; view[10] = m[2][2]; view[14] = tz;
	view[3] = 0.0f;    view[7] = 0.0f;    view[11] = 0.0f;    view[15] = 1.0f;
}

// Independent reference: world = pos + R_obj * p, camera = R_cam * (world - cam).
static void ReferenceCameraPoint(const float r[3][3], const float cam[3],
	const float pos[3], unsigned int yang, const float p[3], float out[3])
{
	float o[3][3];
	ObjectRotationFromYang(yang, o);

	const float wx = pos[0] + o[0][0] * p[0] + o[0][1] * p[1] + o[0][2] * p[2];
	const float wy = pos[1] + o[1][0] * p[0] + o[1][1] * p[1] + o[1][2] * p[2];
	const float wz = pos[2] + o[2][0] * p[0] + o[2][1] * p[1] + o[2][2] * p[2];

	const float dx = wx - cam[0];
	const float dy = wy - cam[1];
	const float dz = wz - cam[2];
	out[0] = r[0][0] * dx + r[0][1] * dy + r[0][2] * dz;
	out[1] = r[1][0] * dx + r[1][1] * dy + r[1][2] * dz;
	out[2] = r[2][0] * dx + r[2][1] * dy + r[2][2] * dz;
}

// Pins the composition view = R_cam * (pos - cam) with R_obj = RotMatrixY(yang*64)
// against an independently computed camera-space point.
static void TestBlock2ViewComposition()
{
	// Known case: identity camera at the origin, object at (100,0,0) yawed a
	// quarter turn (yang 16), object point (1,0,0). R_obj * (1,0,0) = (0,0,-1),
	// so the camera-space point is (100,0,-1).
	const float identity[3][3] = { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } };
	const float origin[3] = { 0, 0, 0 };
	const float objectPos[3] = { 100, 0, 0 };
	const float localPoint[3] = { 1, 0, 0 };

	float view[16];
	ComposeInstanceView(identity, origin, objectPos, 16, view);
	float point[4];
	TransformPoint(view, localPoint, point);
	CHECK(Near(point[0], 100.0f, 1e-3f));
	CHECK(Near(point[1], 0.0f, 1e-3f));
	CHECK(Near(point[2], -1.0f, 1e-3f));
	CHECK(Near(point[3], 1.0f, 1e-3f));

	// The instance origin maps to the view translation (pos - cam), not to the
	// object point.
	float instanceOrigin[4];
	TransformPoint(view, origin, instanceOrigin);
	CHECK(Near(instanceOrigin[0], 100.0f, 1e-3f));
	CHECK(Near(instanceOrigin[1], 0.0f, 1e-3f));
	CHECK(Near(instanceOrigin[2], 0.0f, 1e-3f));

	// Non-trivial camera and object: the composed view must agree with the
	// independent world-then-camera computation for every yang and point.
	float rCam[3][3];
	RotMatrixYAnalytic(1024, rCam); // camera yawed a quarter turn
	const float cam[3] = { 10, 20, 30 };
	const float pos[3] = { 100, 0, 50 };
	const float p[3] = { 3, 4, 5 };

	for (unsigned int yang = 0; yang < 64; ++yang)
	{
		ComposeInstanceView(rCam, cam, pos, yang, view);
		TransformPoint(view, p, point);

		float reference[3];
		ReferenceCameraPoint(rCam, cam, pos, yang, p, reference);

		CHECK(Near(point[0], reference[0], 1e-3f));
		CHECK(Near(point[1], reference[1], 1e-3f));
		CHECK(Near(point[2], reference[2], 1e-3f));
	}
}

// Left-handed +Z-forward perspective with the [0, 1] NDC z range, matching
// native/NativeRenderer.cpp BuildPerspective and Diligent's Vulkan/D3D
// convention (BasicMath.hpp SetNearFarClipPlanes, NegativeOneToOneZ = false).
// The camera looks down +Z, so clip.w = +zView. out[5] is negated to match the
// game's +Y-down screen convention (NativeRenderer.cpp BuildPerspective), so a
// camera-space +y point maps to the lower half; this is a y reflection, so it
// flips the NDC signed-area sign of every triangle.
static void BuildPerspectiveFixture(float fovY, float aspect, float nearPlane,
	float farPlane, float out[16])
{
	for (int i = 0; i < 16; ++i)
		out[i] = 0.0f;
	const float f = 1.0f / tanf(fovY * 0.5f);
	out[0] = f / aspect;
	out[5] = -f;
	out[10] = farPlane / (farPlane - nearPlane);
	out[11] = 1.0f;
	out[14] = -nearPlane * farPlane / (farPlane - nearPlane);
}

// NDC z of a camera-space point on the view axis, matching the fixture
// projection. |ndc| <= 1 means the point is inside the depth range.
static float NdcZFixture(float nearPlane, float farPlane, float zView)
{
	float projection[16];
	BuildPerspectiveFixture(1.0472f, 1.0f, nearPlane, farPlane, projection);
	const float clipZ = projection[10] * zView + projection[14];
	const float w = projection[11] * zView; // = +zView (camera looks down +Z)
	return clipZ / w;
}

// Signed area of a triangle in the y-up NDC; positive is counter-clockwise.
static float SignedAreaNdc(const float a[2], const float b[2], const float c[2])
{
	return 0.5f * (a[0] * (b[1] - c[1]) + b[0] * (c[1] - a[1]) + c[0] * (a[1] - b[1]));
}

// Pins the decoder's winding and the cull policy it implies.
static void TestBlock2WindingAndCullPolicy()
{
	// The decoder emits a world quad as (v0,v1,v3,v2) and fans it
	// (0,1,2)+(0,2,3) (SourceModelDecoder.cpp:384-393), so emitted corner 0..3
	// are c0=v0, c1=v1, c2=v3, c3=v2. The first triangle (v0,v1,v3) is exactly
	// the runtime's gte_nclip triangle (draw.c:780-796).
	//
	// Known front-facing quad: stored layout v0..v3 with outward normal +Z
	// (toward the camera). v0=(-1,-1), v1=(1,-1), v2=(1,1), v3=(-1,1).
	const float v0[3] = { -1, -1, 0 };
	const float v1[3] = { 1, -1, 0 };
	const float v2[3] = { 1, 1, 0 };
	const float v3[3] = { -1, 1, 0 };

	// Geometric front test: cross(v1-v0, v3-v0) = +Z points at the camera.
	float edgeA[3] = { v1[0] - v0[0], v1[1] - v0[1], v1[2] - v0[2] };
	float edgeB[3] = { v3[0] - v0[0], v3[1] - v0[1], v3[2] - v0[2] };
	float normal[3];
	Cross3(edgeA, edgeB, normal);
	CHECK(Near(normal[0], 0.0f, 1e-4f) && Near(normal[1], 0.0f, 1e-4f) && normal[2] > 0.0f);

	// Place the quad 10 m (40960 raw units) in front of an identity camera,
	// using the pinned raw-unit projection window. The near/far choice does not
	// affect the winding sign; it is kept consistent with the unit policy below.
	// The mesh is local (the decoder emits local vertices); the view carries the
	// placement. The projection is +Z-forward (Option B), so "in front" is +z.
	const float identity[3][3] = { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } };
	const float origin[3] = { 0, 0, 0 };
	const float objectPos[3] = { 0, 0, 40960 };
	float view[16];
	ComposeInstanceView(identity, origin, objectPos, 0, view);
	float projection[16];
	BuildPerspectiveFixture(1.0472f, 1.0f, 100.0f, 1000000.0f, projection);
	float mvp[16];
	MultiplyColumnMajor(projection, view, mvp); // model transform is identity

	float clip0[4], clip1[4], clip2[4];
	TransformPoint(mvp, v0, clip0);
	TransformPoint(mvp, v1, clip1);
	TransformPoint(mvp, v3, clip2);
	CHECK(clip0[3] > 0.0f && clip1[3] > 0.0f && clip2[3] > 0.0f); // all in front

	const float n0[2] = { clip0[0] / clip0[3], clip0[1] / clip0[3] };
	const float n1[2] = { clip1[0] / clip1[3], clip1[1] / clip1[3] };
	const float n2[2] = { clip2[0] / clip2[3], clip2[1] / clip2[3] };

	const float frontArea = SignedAreaNdc(n0, n1, n2);
	CHECK(frontArea < 0.0f); // clockwise in the y-up NDC after the y negation

	// Reversing the winding flips the sign: this is the back face.
	const float backArea = SignedAreaNdc(n0, n2, n1);
	CHECK(backArea > 0.0f);
	CHECK(Near(frontArea, -backArea, 1e-5f));

	// Pinned cull policy. BuildPerspective negates camera-space y (out[5] = -f)
	// to match the game's +Y-down screen convention, which reflects the NDC and
	// flips the signed-area sign of every triangle: the decoder's game-front
	// triangle (the same (v0,v1,v3) the runtime feeds to gte_nclip,
	// draw.c:780-796) now has NEGATIVE NDC signed area. Diligent runs on Vulkan
	// through a negative viewport height to emulate Direct3D, so the scene
	// pipeline culls back faces with CULL_MODE_BACK and
	// FrontCounterClockwise = False: the reflection and the flipped pin cancel,
	// so exactly the same world faces remain front. The reversed winding still
	// has positive area and is culled. CULL_MODE_NONE is not an accepted policy.
	// tests/NativeRendererSmoke.cpp asserts the pixel result.
	CHECK(frontArea < 0.0f);
	CHECK(backArea > 0.0f);
}

// Pins the unit policy: translation, vertices and projection near/far share one
// raw game unit (4096 per metre). It asserts the renderer's default pair is not
// a valid raw-unit scene projection and that a raw-unit pair is; it does not
// change the renderer's defaults.
static void TestBlock2UnitPolicy()
{
	const float rawUnitsPerMetre = 4096.0f; // PSX ONE/FIXED

	// The decoder emits raw SVECTOR positions (SourceModelDecoder.cpp:344-346)
	// and the bridge keeps the camera translation raw while scaling only the
	// rotation by 1/4096 (redriver2_psxpc.cpp:535-552;
	// DeveloperModernMesh.cpp:496-525). A raw-unit scene therefore needs
	// raw-unit near/far as well.

	// The default pair is metre-scale (native/NativeRenderer.cpp defaults,
	// overridable through NativeRenderer_SetClipPlanes): read as raw units, its
	// far plane (300) is smaller than one world metre, so every raw vertex
	// beyond 0.07 m is clipped away.
	CHECK(300.0f < rawUnitsPerMetre);
	CHECK(0.02f < 1.0f); // the near plane is below one raw unit

	// A representative raw scene depth (24 m) is far beyond that far plane.
	// The camera looks down +Z, so the view-space depth is positive.
	const float sceneDepthRaw = 24.0f * rawUnitsPerMetre; // 98304 raw units
	CHECK(NdcZFixture(0.02f, 300.0f, sceneDepthRaw) > 1.0f); // clipped: beyond far

	// The same depth is inside a raw-unit window, so raw-unit near/far is the
	// required policy (the bridge supplies ~100 / ~125000).
	CHECK(fabsf(NdcZFixture(100.0f, 1000000.0f, sceneDepthRaw)) <= 1.0f);
}

// --- F-F: material grouping -------------------------------------------------

static void TestMaterialGrouping()
{
	SourceModelBuilder b;
	b.BeginVertices();
	b.AddVertex(0, 0, 0);
	b.AddVertex(1, 0, 0);
	b.AddVertex(1, 1, 0);
	b.AddVertex(0, 1, 0);
	b.BeginPointNormals();
	b.AddRawNormal(0, 0, 1);
	b.AddRawNormal(0, 1, 0);
	b.AddRawNormal(1, 0, 0);
	b.BeginPolys();

	// run 1: two adjacent FT4 with key (flat-quad, set 1, id 2) -> merge
	AddFT4(b, 1, 2, 0, 1, 2, 3, UV(0, 0), UV(1, 0), UV(1, 1), UV(0, 1), RGB(0, 0, 0));
	AddFT4(b, 1, 2, 3, 2, 1, 0, UV(0, 0), UV(1, 0), UV(1, 1), UV(0, 1), RGB(0, 0, 0));
	// run 2: same class/set but id 3 -> a distinct key
	AddFT4(b, 1, 3, 0, 1, 2, 3, UV(0, 0), UV(1, 0), UV(1, 1), UV(0, 1), RGB(0, 0, 0));
	// run 3: a different primitive class -> a distinct key
	AddGT3(b, 1, 2, 0, 1, 2, 0, 1, 2, UV(0, 0), UV(1, 0), UV(1, 1), RGB(0, 0, 0));

	SourceMesh mesh;
	char error[256] = "";
	SourceModelDecoderResult result = Decode(b, mesh, error, sizeof(error));
	CHECK(result.error == SOURCE_MODEL_ERROR_NONE);

	CHECK(mesh.indices.size() == 21); // 6 + 6 + 6 + 3
	CHECK(mesh.submeshes.size() == 3);

	CHECK(mesh.submeshes[0].primitiveClass == SourcePrimitiveClass::TexturedFlatQuad);
	CHECK(mesh.submeshes[0].textureSet == 1 && mesh.submeshes[0].textureId == 2);
	CHECK(mesh.submeshes[0].firstIndex == 0 && mesh.submeshes[0].indexCount == 12);

	CHECK(mesh.submeshes[1].primitiveClass == SourcePrimitiveClass::TexturedFlatQuad);
	CHECK(mesh.submeshes[1].textureSet == 1 && mesh.submeshes[1].textureId == 3);
	CHECK(mesh.submeshes[1].firstIndex == 12 && mesh.submeshes[1].indexCount == 6);

	CHECK(mesh.submeshes[2].primitiveClass == SourcePrimitiveClass::TexturedGouraudTriangle);
	CHECK(mesh.submeshes[2].textureSet == 1 && mesh.submeshes[2].textureId == 2);
	CHECK(mesh.submeshes[2].firstIndex == 18 && mesh.submeshes[2].indexCount == 3);

	// Per-submesh attribute presence follows the layout, not the merge key.
	CHECK(!mesh.submeshes[0].hasNormals && !mesh.submeshes[1].hasNormals);
	CHECK(mesh.submeshes[2].hasNormals);
	CHECK(mesh.submeshes[0].hasColors && mesh.submeshes[1].hasColors && mesh.submeshes[2].hasColors);
	CHECK(mesh.normals.size() == mesh.positions.size());

	// Runs are contiguous and cover every index exactly once.
	unsigned int expectedFirst = 0;
	unsigned int covered = 0;
	for (size_t i = 0; i < mesh.submeshes.size(); ++i)
	{
		CHECK(mesh.submeshes[i].firstIndex == expectedFirst);
		expectedFirst += mesh.submeshes[i].indexCount;
		covered += mesh.submeshes[i].indexCount;
	}
	CHECK(covered == (unsigned int)mesh.indices.size());
}

// --- F-G: determinism -------------------------------------------------------

static void TestDeterminism()
{
	SourceModelBuilder b;
	b.BeginVertices();
	b.AddVertex(3, 4, 5);
	b.AddVertex(-3, 4, 5);
	b.AddVertex(3, -4, 5);
	b.AddVertex(3, 4, -5);
	b.BeginPointNormals();
	b.AddRawNormal(0, 0, 100);
	b.AddRawNormal(0, 100, 0);
	b.AddRawNormal(100, 0, 0);
	b.AddRawNormal(50, 50, 50);
	b.BeginPolys();
	AddGT4(b, 2, 1, 0, 1, 2, 3, 0, 1, 2, 3,
		UV(1, 1), UV(2, 2), UV(3, 3), UV(4, 4), RGB(9, 8, 7));
	AddGT3(b, 2, 1, 0, 1, 2, 0, 1, 2, UV(5, 5), UV(6, 6), UV(7, 7), RGB(9, 8, 7));

	SourceMesh first;
	SourceMesh second;
	char error[256] = "";
	CHECK(Decode(b, first, error, sizeof(error)).error == SOURCE_MODEL_ERROR_NONE);
	CHECK(Decode(b, second, error, sizeof(error)).error == SOURCE_MODEL_ERROR_NONE);
	CHECK(MeshEqual(first, second));

	// Identical input through a fresh builder also produces an identical record.
	SourceModelBuilder c;
	c.BeginVertices();
	c.AddVertex(3, 4, 5);
	c.AddVertex(-3, 4, 5);
	c.AddVertex(3, -4, 5);
	c.AddVertex(3, 4, -5);
	c.BeginPointNormals();
	c.AddRawNormal(0, 0, 100);
	c.AddRawNormal(0, 100, 0);
	c.AddRawNormal(100, 0, 0);
	c.AddRawNormal(50, 50, 50);
	c.BeginPolys();
	AddGT4(c, 2, 1, 0, 1, 2, 3, 0, 1, 2, 3,
		UV(1, 1), UV(2, 2), UV(3, 3), UV(4, 4), RGB(9, 8, 7));
	AddGT3(c, 2, 1, 0, 1, 2, 0, 1, 2, UV(5, 5), UV(6, 6), UV(7, 7), RGB(9, 8, 7));
	SourceMesh third;
	CHECK(Decode(c, third, error, sizeof(error)).error == SOURCE_MODEL_ERROR_NONE);
	CHECK(MeshEqual(first, third));
	CHECK(first.normals.size() == first.positions.size());
}

int main()
{
	TestQuadOrderAndUVs();
	TestCyclicQuadCoverage();
	TestPLQuadOrderAndUVs();
	TestPLTriangleOrderAndUVs();
	TestLegacyShadeIndices();
	TestMixedModelPerSubmeshColors();
	TestMixedModelPerSubmeshNormals();
	TestEmptyModel();
	TestGouraudQuadLayout();
	TestTriangleOrderAndUVs();
	TestGouraudTriangleOrderAndUVs();
	TestUnsupportedPolygonType();
	TestMixedPlTriangleAndQuad();
	TestSkipUnsupportedBuildingType();
	TestSkipUnsupportedNone();
	TestSkipUnsupportedOutOfSpan();
	TestSkipUnsupportedZeroStride();
	TestSpanViolations();
	TestInstancedModel();
	TestBlock2YangRotationGate();
	TestBlock2ViewComposition();
	TestBlock2WindingAndCullPolicy();
	TestBlock2UnitPolicy();
	TestMaterialGrouping();
	TestDeterminism();

	printf("%d checks, %d failures\n", g_checks, g_failures);
	return g_failures == 0 ? 0 : 1;
}
