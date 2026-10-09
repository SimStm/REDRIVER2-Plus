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

static SourceModelDecoderResult Decode(const SourceModelBuilder& b, SourceMesh& out,
	char* error, int capacity)
{
	SourceModelView view = b.View();
	return DecodeSourceModel(b.bytes.data(), b.bytes.data() + b.bytes.size(), view, &out,
		error, capacity);
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

	// Triangle list (0,1,2)+(0,2,3).
	CHECK(mesh.indices.size() == 6);
	CHECK(mesh.indices[0] == 0 && mesh.indices[1] == 1 && mesh.indices[2] == 2);
	CHECK(mesh.indices[3] == 0 && mesh.indices[4] == 2 && mesh.indices[5] == 3);

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
	CHECK(SourceModelDecoder_IsSupportedType(11));
	CHECK(SourceModelDecoder_IsSupportedType(20));
	CHECK(SourceModelDecoder_IsSupportedType(21));
	CHECK(SourceModelDecoder_IsSupportedType(22));
	CHECK(SourceModelDecoder_IsSupportedType(23));
	CHECK(!SourceModelDecoder_IsSupportedType(8));
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
	CHECK(mesh.indices[3] == 0 && mesh.indices[4] == 2 && mesh.indices[5] == 3);

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

	// (8) Instanced models are not decodable from a single span.
	{
		SourceModelBuilder b;
		b.BeginVertices();
		b.AddVertex(0, 0, 0);
		b.BeginPolys();
		SourceModelView view = b.View();
		view.instanceNumber = 3;
		SourceModelDecoderResult result = DecodeSourceModel(b.bytes.data(),
			b.bytes.data() + b.bytes.size(), view, &mesh, error, sizeof(error));
		CHECK(result.error == SOURCE_MODEL_ERROR_INSTANCE);
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
		CHECK(result.error == SOURCE_MODEL_ERROR_INSTANCE);
		CHECK(MeshIsEmpty(mesh));
	}
}

// --- Block-2 convention gate: yang -> rotation (not decoder coverage) -------
//
// This fixture does NOT exercise the decoder. It is a prospective convention
// gate for Block 2, which will consume decoded world geometry and apply object
// rotations. The building/tile renderers derive a Y rotation from a packed
// object's yang (draw.c matrixtable[i] = RotMatrixY(i*64),
// CalcObjectRotationMatrices). A full PSX turn is 4096 units, so the angle is
// yang*64 with yang taken modulo 64; a value and yang+64 are identical. It
// deliberately avoids Game/PSX headers and only pins the convention.

static int YangToAngle(unsigned int yang)
{
	return (int)(yang & 63) * 64;
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
	TestPLQuadOrderAndUVs();
	TestMixedModelPerSubmeshColors();
	TestMixedModelPerSubmeshNormals();
	TestEmptyModel();
	TestGouraudQuadLayout();
	TestTriangleOrderAndUVs();
	TestGouraudTriangleOrderAndUVs();
	TestUnsupportedPolygonType();
	TestSpanViolations();
	TestBlock2YangRotationGate();
	TestMaterialGrouping();
	TestDeterminism();

	printf("%d checks, %d failures\n", g_checks, g_failures);
	return g_failures == 0 ? 0 : 1;
}
