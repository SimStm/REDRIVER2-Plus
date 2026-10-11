// Standalone regression tests for the E02 native mesh record: the project-owned
// glTF/GLB writer and importer must agree on geometry, UVs, transform and
// material flags, and identical conversions must produce identical bytes.
//
// They also cover the multi-primitive record (GltfMultiMeshData /
// Gltf_WriteFileMulti / Gltf_LoadFileMulti): per-primitive geometry, indices and
// base-colour material, determinism, rejection of an empty or degenerate
// primitive with a named reason and zero partial output, that the single
// loader still reads primitive 0 of a multi-primitive file, and that a
// one-primitive mesh is byte-identical whether written through Gltf_WriteFile or
// Gltf_WriteFileMulti.
//
// The importer's WIC image path is not exercised (no embedded image), so the
// test needs no game assets or GPU context. Build and run instructions are in
// tests/README.md.

#include "../utils/GltfLoader.h"
#include "../utils/GltfWriter.h"

#include <stdio.h>
#include <string.h>
#include <string>
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

static GltfMeshData MakeTriangle()
{
	GltfMeshData mesh;
	mesh.positions = { 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f };
	mesh.normals = { 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f };
	mesh.uvs = { 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f };
	mesh.indices = { 0, 1, 2 };
	mesh.vertexCount = 3;
	mesh.triangleCount = 1;
	mesh.baseColorFactor[0] = 0.25f;
	mesh.baseColorFactor[1] = 0.5f;
	mesh.baseColorFactor[2] = 0.75f;
	mesh.baseColorFactor[3] = 1.0f;
	mesh.metallicFactor = 0.5f;
	mesh.roughnessFactor = 0.25f;

	// Translate (10, 20, 30) and scale (2, 3, 4), column-major.
	const float transform[16] = {
		2, 0, 0, 0,
		0, 3, 0, 0,
		0, 0, 4, 0,
		10, 20, 30, 1,
	};
	memcpy(mesh.transform, transform, sizeof(transform));
	return mesh;
}

static int ReadFileBytes(const char* path, std::vector<unsigned char>* out)
{
	FILE* file = fopen(path, "rb");
	if (!file)
		return 0;
	fseek(file, 0, SEEK_END);
	long size = ftell(file);
	fseek(file, 0, SEEK_SET);
	out->resize((size_t)size);
	const bool read = size > 0 && fread(out->data(), 1, (size_t)size, file) == (size_t)size;
	fclose(file);
	return read ? 1 : 0;
}

static void TestRoundTrip()
{
	const GltfMeshData source = MakeTriangle();
	char error[256];
	CHECK(Gltf_WriteFile("roundtrip.glb", source, error, sizeof(error)) == 0);

	GltfMeshData loaded;
	CHECK(Gltf_LoadFile("roundtrip.glb", &loaded, error, sizeof(error)) == 0);

	// Geometry and UVs agree.
	CHECK(loaded.vertexCount == 3);
	CHECK(loaded.positions.size() == source.positions.size());
	CHECK(memcmp(loaded.positions.data(), source.positions.data(), source.positions.size() * sizeof(float)) == 0);
	CHECK(loaded.normals.size() == source.normals.size());
	CHECK(memcmp(loaded.normals.data(), source.normals.data(), source.normals.size() * sizeof(float)) == 0);
	CHECK(loaded.uvs.size() == source.uvs.size());
	CHECK(memcmp(loaded.uvs.data(), source.uvs.data(), source.uvs.size() * sizeof(float)) == 0);
	CHECK(loaded.indices.size() == source.indices.size());
	CHECK(memcmp(loaded.indices.data(), source.indices.data(), source.indices.size() * sizeof(unsigned int)) == 0);

	// Material flags agree.
	CHECK(memcmp(loaded.baseColorFactor, source.baseColorFactor, sizeof(source.baseColorFactor)) == 0);
	CHECK(loaded.metallicFactor == source.metallicFactor);
	CHECK(loaded.roughnessFactor == source.roughnessFactor);

	// The source placement round-trips through the node matrix.
	CHECK(memcmp(loaded.transform, source.transform, sizeof(source.transform)) == 0);
}

static void TestReproducibleBytes()
{
	const GltfMeshData source = MakeTriangle();
	char error[256];
	CHECK(Gltf_WriteFile("a.glb", source, error, sizeof(error)) == 0);
	CHECK(Gltf_WriteFile("b.glb", source, error, sizeof(error)) == 0);

	std::vector<unsigned char> a, b;
	CHECK(ReadFileBytes("a.glb", &a));
	CHECK(ReadFileBytes("b.glb", &b));
	CHECK(!a.empty() && a.size() == b.size());
	CHECK(a.size() == b.size() && memcmp(a.data(), b.data(), a.size()) == 0);
}

static void TestDiagnostics()
{
	GltfMeshData empty;
	char error[256];
	CHECK(Gltf_WriteFile("empty.glb", empty, error, sizeof(error)) == -1);
	CHECK(error[0] != '\0');

	// A file that is not a GLB is rejected with a diagnostic.
	FILE* file = fopen("bad.glb", "wb");
	if (file)
	{
		const char text[] = "not a glb at all";
		fwrite(text, 1, sizeof(text) - 1, file);
		fclose(file);
	}
	GltfMeshData loaded;
	CHECK(Gltf_LoadFile("bad.glb", &loaded, error, sizeof(error)) == -1);
	CHECK(error[0] != '\0');

	// A truncated GLB (valid header, missing chunks) is rejected.
	CHECK(Gltf_WriteFile("full.glb", MakeTriangle(), error, sizeof(error)) == 0);
	std::vector<unsigned char> full;
	CHECK(ReadFileBytes("full.glb", &full));
	file = fopen("truncated.glb", "wb");
	if (file)
	{
		fwrite(full.data(), 1, full.size() / 2, file);
		fclose(file);
	}
	CHECK(Gltf_LoadFile("truncated.glb", &loaded, error, sizeof(error)) == -1);
}

static GltfPrimitiveData MakePrimitive(const std::vector<float>& positions,
	const std::vector<float>& normals, const std::vector<float>& uvs,
	const std::vector<unsigned int>& indices, float r, float g, float b, float a)
{
	GltfPrimitiveData primitive;
	primitive.positions = positions;
	primitive.normals = normals;
	primitive.uvs = uvs;
	primitive.indices = indices;
	primitive.vertexCount = (int)(positions.size() / 3);
	primitive.triangleCount = (int)(indices.size() / 3);
	primitive.baseColorFactor[0] = r;
	primitive.baseColorFactor[1] = g;
	primitive.baseColorFactor[2] = b;
	primitive.baseColorFactor[3] = a;
	return primitive;
}

// Three primitives with different vertex/index counts, different attribute sets
// (with/without normals and UVs) and different base colours.
static GltfMultiMeshData MakeMultiMesh()
{
	GltfMultiMeshData mesh;
	mesh.primitives.push_back(MakePrimitive(
		{ 0, 0, 0, 1, 0, 0, 0, 1, 0 },
		{ 0, 0, 1, 0, 0, 1, 0, 0, 1 },
		{ 0, 0, 1, 0, 0, 1 },
		{ 0, 1, 2 },
		0.25f, 0.5f, 0.75f, 1.0f));
	mesh.primitives.push_back(MakePrimitive(
		{ 0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0 },
		std::vector<float>(),
		{ 0, 0, 1, 0, 1, 1, 0, 1 },
		{ 0, 1, 2, 0, 2, 3 },
		1.0f, 0.0f, 0.0f, 1.0f));
	mesh.primitives.push_back(MakePrimitive(
		{ 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 1, 0, 0 },
		std::vector<float>(),
		std::vector<float>(),
		{ 0, 1, 2, 3, 4, 5 },
		0.0f, 1.0f, 0.0f, 0.5f));

	const float transform[16] = {
		1, 0, 0, 0,
		0, 1, 0, 0,
		0, 0, 1, 0,
		5, 6, 7, 1,
	};
	memcpy(mesh.transform, transform, sizeof(transform));
	return mesh;
}

static void TestMultiPrimitiveRoundTrip()
{
	const GltfMultiMeshData source = MakeMultiMesh();
	char error[256];
	CHECK(Gltf_WriteFileMulti("multi.glb", source, error, sizeof(error)) == 0);

	GltfMultiMeshData loaded;
	CHECK(Gltf_LoadFileMulti("multi.glb", &loaded, error, sizeof(error)) == 0);
	CHECK(loaded.primitives.size() == source.primitives.size());

	if (loaded.primitives.size() == source.primitives.size())
	{
		for (size_t i = 0; i < source.primitives.size(); i++)
		{
			const GltfPrimitiveData& s = source.primitives[i];
			const GltfPrimitiveData& l = loaded.primitives[i];

			CHECK(l.vertexCount == s.vertexCount);
			CHECK(l.triangleCount == s.triangleCount);

			CHECK(l.positions.size() == s.positions.size());
			CHECK(s.positions.empty() || memcmp(l.positions.data(), s.positions.data(), s.positions.size() * sizeof(float)) == 0);
			CHECK(l.normals.size() == s.normals.size());
			CHECK(s.normals.empty() || memcmp(l.normals.data(), s.normals.data(), s.normals.size() * sizeof(float)) == 0);
			CHECK(l.uvs.size() == s.uvs.size());
			CHECK(s.uvs.empty() || memcmp(l.uvs.data(), s.uvs.data(), s.uvs.size() * sizeof(float)) == 0);
			CHECK(l.indices.size() == s.indices.size());
			CHECK(s.indices.empty() || memcmp(l.indices.data(), s.indices.data(), s.indices.size() * sizeof(unsigned int)) == 0);

			CHECK(memcmp(l.baseColorFactor, s.baseColorFactor, sizeof(s.baseColorFactor)) == 0);
			CHECK(l.metallicFactor == s.metallicFactor);
			CHECK(l.roughnessFactor == s.roughnessFactor);
		}
	}

	CHECK(memcmp(loaded.transform, source.transform, sizeof(source.transform)) == 0);

	// The single-primitive loader reads primitive 0 of the same multi file.
	GltfMeshData first;
	CHECK(Gltf_LoadFile("multi.glb", &first, error, sizeof(error)) == 0);
	CHECK(first.vertexCount == source.primitives[0].vertexCount);
	CHECK(first.triangleCount == source.primitives[0].triangleCount);
	CHECK(memcmp(first.baseColorFactor, source.primitives[0].baseColorFactor, sizeof(first.baseColorFactor)) == 0);
}

static void TestMultiPrimitiveDeterminism()
{
	const GltfMultiMeshData source = MakeMultiMesh();
	char error[256];
	CHECK(Gltf_WriteFileMulti("multi_a.glb", source, error, sizeof(error)) == 0);
	CHECK(Gltf_WriteFileMulti("multi_b.glb", source, error, sizeof(error)) == 0);

	std::vector<unsigned char> a, b;
	CHECK(ReadFileBytes("multi_a.glb", &a));
	CHECK(ReadFileBytes("multi_b.glb", &b));
	CHECK(!a.empty() && a.size() == b.size());
	CHECK(a.size() == b.size() && memcmp(a.data(), b.data(), a.size()) == 0);
}

// Gate F4: a one-primitive mesh written through Gltf_WriteFileMulti must be
// byte-identical to the historical single-primitive Gltf_WriteFile output,
// locking the documented single-vs-multi byte-identity claim.
static void TestSingleVsMultiByteIdentity()
{
	const GltfMeshData source = MakeTriangle();
	char error[256];
	CHECK(Gltf_WriteFile("single-identity.glb", source, error, sizeof(error)) == 0);

	// The same record expressed as a one-primitive multi mesh.
	GltfMultiMeshData multi;
	GltfPrimitiveData primitive;
	primitive.positions = source.positions;
	primitive.normals = source.normals;
	primitive.uvs = source.uvs;
	primitive.indices = source.indices;
	for (int i = 0; i < 4; i++)
		primitive.baseColorFactor[i] = source.baseColorFactor[i];
	primitive.metallicFactor = source.metallicFactor;
	primitive.roughnessFactor = source.roughnessFactor;
	primitive.vertexCount = source.vertexCount;
	primitive.triangleCount = source.triangleCount;
	multi.primitives.push_back(primitive);
	memcpy(multi.transform, source.transform, sizeof(multi.transform));
	CHECK(Gltf_WriteFileMulti("multi-identity.glb", multi, error, sizeof(error)) == 0);

	std::vector<unsigned char> single, multiBytes;
	CHECK(ReadFileBytes("single-identity.glb", &single));
	CHECK(ReadFileBytes("multi-identity.glb", &multiBytes));
	CHECK(!single.empty() && single.size() == multiBytes.size());
	CHECK(single.size() == multiBytes.size() &&
		memcmp(single.data(), multiBytes.data(), single.size()) == 0);
}

static bool FileExists(const char* path)
{
	FILE* file = fopen(path, "rb");
	if (!file)
		return false;
	fclose(file);
	return true;
}

static void TestMultiPrimitiveWriterRejections()
{
	char error[256];

	// No primitives: named reason and no file.
	remove("reject-empty.glb");
	GltfMultiMeshData empty;
	CHECK(Gltf_WriteFileMulti("reject-empty.glb", empty, error, sizeof(error)) == -1);
	CHECK(error[0] != '\0');
	CHECK(strstr(error, "no primitives") != NULL);
	CHECK(!FileExists("reject-empty.glb"));

	// A primitive with no positions.
	remove("reject-degenerate.glb");
	GltfMultiMeshData noPositions;
	noPositions.primitives.push_back(GltfPrimitiveData());
	CHECK(Gltf_WriteFileMulti("reject-degenerate.glb", noPositions, error, sizeof(error)) == -1);
	CHECK(strstr(error, "primitive 0") != NULL);
	CHECK(!FileExists("reject-degenerate.glb"));

	// A primitive whose index list is not a whole number of triangles.
	remove("reject-partial-triangle.glb");
	GltfMultiMeshData partialTriangle;
	partialTriangle.primitives.push_back(MakePrimitive(
		{ 0, 0, 0, 1, 0, 0, 0, 1, 0 }, std::vector<float>(), std::vector<float>(),
		{ 0, 1 }, 1, 1, 1, 1));
	CHECK(Gltf_WriteFileMulti("reject-partial-triangle.glb", partialTriangle, error, sizeof(error)) == -1);
	CHECK(strstr(error, "primitive 0") != NULL);
	CHECK(!FileExists("reject-partial-triangle.glb"));

	// A primitive with an out-of-range index.
	remove("reject-index.glb");
	GltfMultiMeshData badIndex;
	badIndex.primitives.push_back(MakePrimitive(
		{ 0, 0, 0, 1, 0, 0, 0, 1, 0 }, std::vector<float>(), std::vector<float>(),
		{ 0, 1, 9 }, 1, 1, 1, 1));
	CHECK(Gltf_WriteFileMulti("reject-index.glb", badIndex, error, sizeof(error)) == -1);
	CHECK(strstr(error, "outside the vertex range") != NULL);
	CHECK(!FileExists("reject-index.glb"));
}

static void AppendU32(std::vector<unsigned char>& out, unsigned int value)
{
	out.push_back((unsigned char)(value & 0xff));
	out.push_back((unsigned char)((value >> 8) & 0xff));
	out.push_back((unsigned char)((value >> 16) & 0xff));
	out.push_back((unsigned char)((value >> 24) & 0xff));
}

static int WriteGlbFromJsonBin(const char* path, const char* json, const std::vector<unsigned char>& bin)
{
	std::string jsonText = json;
	while (jsonText.size() % 4 != 0)
		jsonText += " ";
	std::vector<unsigned char> binChunk = bin;
	while (binChunk.size() % 4 != 0)
		binChunk.push_back(0);

	const unsigned int jsonLength = (unsigned int)jsonText.size();
	const unsigned int binLength = (unsigned int)binChunk.size();
	const unsigned int totalLength = 12 + 8 + jsonLength + 8 + binLength;

	std::vector<unsigned char> glb;
	AppendU32(glb, 0x46546C67);
	AppendU32(glb, 2);
	AppendU32(glb, totalLength);
	AppendU32(glb, jsonLength);
	AppendU32(glb, 0x4E4F534A);
	glb.insert(glb.end(), jsonText.begin(), jsonText.end());
	AppendU32(glb, binLength);
	AppendU32(glb, 0x004E4942);
	glb.insert(glb.end(), binChunk.begin(), binChunk.end());

	FILE* file = fopen(path, "wb");
	if (!file)
		return 0;
	const bool wrote = fwrite(glb.data(), 1, glb.size(), file) == glb.size();
	fclose(file);
	return wrote ? 1 : 0;
}

// --- test-local stored-block PNG builder ------------------------------------
// Builds exactly the writer's PNG subset (8-bit RGBA, per-scanline filter None,
// one stored deflate block) so a test can corrupt one aspect at a time.

static unsigned int TestCrc32(const unsigned char* data, size_t length)
{
	unsigned int crc = 0xFFFFFFFFu;
	for (size_t i = 0; i < length; i++)
	{
		crc ^= data[i];
		for (int bit = 0; bit < 8; bit++)
			crc = (crc >> 1) ^ (0xEDB88320u & (unsigned int)(-(int)(crc & 1)));
	}
	return crc ^ 0xFFFFFFFFu;
}

static unsigned int TestAdler32(const unsigned char* data, size_t length)
{
	unsigned int a = 1, b = 0;
	for (size_t i = 0; i < length; i++)
	{
		a = (a + data[i]) % 65521u;
		b = (b + a) % 65521u;
	}
	return (b << 16) | a;
}

static void AppendU32BE(std::vector<unsigned char>& out, unsigned int value)
{
	out.push_back((unsigned char)((value >> 24) & 0xff));
	out.push_back((unsigned char)((value >> 16) & 0xff));
	out.push_back((unsigned char)((value >> 8) & 0xff));
	out.push_back((unsigned char)(value & 0xff));
}

static void AppendPngChunk(std::vector<unsigned char>& png, const char type[4],
	const unsigned char* data, size_t length)
{
	AppendU32BE(png, (unsigned int)length);
	const size_t crcStart = png.size();
	png.insert(png.end(), type, type + 4);
	if (length)
		png.insert(png.end(), data, data + length);
	AppendU32BE(png, TestCrc32(png.data() + crcStart, png.size() - crcStart));
}

// `includeAdler` lets a test omit the zlib Adler-32 trailer while keeping every
// chunk CRC valid, isolating the missing-trailer check.
static std::vector<unsigned char> MakeStoredPng(int width, int height,
	const std::vector<unsigned char>& rgba, bool includeAdler)
{
	std::vector<unsigned char> raw;
	for (int y = 0; y < height; y++)
	{
		raw.push_back(0); // filter type None
		raw.insert(raw.end(), rgba.begin() + (size_t)y * width * 4,
			rgba.begin() + (size_t)(y + 1) * width * 4);
	}

	std::vector<unsigned char> zlib;
	zlib.push_back(0x78); // CM=8 (deflate), CINFO=7
	zlib.push_back(0x01); // FCHECK, no dictionary
	const unsigned int len = (unsigned int)raw.size();
	zlib.push_back(1); // BFINAL=1, BTYPE=00 (stored)
	zlib.push_back((unsigned char)(len & 0xff));
	zlib.push_back((unsigned char)((len >> 8) & 0xff));
	const unsigned int nlen = len ^ 0xffffu;
	zlib.push_back((unsigned char)(nlen & 0xff));
	zlib.push_back((unsigned char)((nlen >> 8) & 0xff));
	zlib.insert(zlib.end(), raw.begin(), raw.end());
	if (includeAdler)
	{
		const unsigned int adler = TestAdler32(raw.data(), raw.size());
		zlib.push_back((unsigned char)((adler >> 24) & 0xff));
		zlib.push_back((unsigned char)((adler >> 16) & 0xff));
		zlib.push_back((unsigned char)((adler >> 8) & 0xff));
		zlib.push_back((unsigned char)(adler & 0xff));
	}

	std::vector<unsigned char> png;
	static const unsigned char signature[8] = { 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A };
	png.insert(png.end(), signature, signature + 8);
	unsigned char ihdr[13] = { 0 };
	ihdr[0] = (unsigned char)((width >> 24) & 0xff);
	ihdr[1] = (unsigned char)((width >> 16) & 0xff);
	ihdr[2] = (unsigned char)((width >> 8) & 0xff);
	ihdr[3] = (unsigned char)(width & 0xff);
	ihdr[4] = (unsigned char)((height >> 24) & 0xff);
	ihdr[5] = (unsigned char)((height >> 16) & 0xff);
	ihdr[6] = (unsigned char)((height >> 8) & 0xff);
	ihdr[7] = (unsigned char)(height & 0xff);
	ihdr[8] = 8;  // bit depth
	ihdr[9] = 6;  // colour type RGBA
	AppendPngChunk(png, "IHDR", ihdr, sizeof(ihdr));
	AppendPngChunk(png, "IDAT", zlib.data(), zlib.size());
	AppendPngChunk(png, "IEND", NULL, 0);
	return png;
}

// Writes a one-primitive GLB whose material's base-colour image is `png`.
static int WriteGlbWithImage(const char* path, const std::vector<unsigned char>& png)
{
	const unsigned int positionBytes = 36; // 3 * VEC3 float32
	std::vector<unsigned char> bin(positionBytes, 0);
	bin.insert(bin.end(), png.begin(), png.end());
	const unsigned int imageLength = (unsigned int)png.size();

	std::string json =
		"{\"asset\":{\"version\":\"2.0\"},"
		"\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0},\"material\":0}]}],"
		"\"materials\":[{\"pbrMetallicRoughness\":{\"baseColorTexture\":{\"index\":0}}}],"
		"\"textures\":[{\"source\":0}],"
		"\"images\":[{\"bufferView\":1,\"mimeType\":\"image/png\"}],"
		"\"accessors\":[{\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\"}],"
		"\"bufferViews\":[{\"buffer\":0,\"byteOffset\":0,\"byteLength\":36},"
		"{\"buffer\":0,\"byteOffset\":36,\"byteLength\":" + std::to_string(imageLength) + "}],"
		"\"buffers\":[{\"byteLength\":" + std::to_string(positionBytes + imageLength) + "}]}";
	return WriteGlbFromJsonBin(path, json.c_str(), bin);
}

static void TestMultiPrimitiveLoaderRejections()
{
	char error[256];
	GltfMultiMeshData loaded;

	// A mesh whose primitives array is empty.
	CHECK(WriteGlbFromJsonBin("load-empty-prims.glb",
		"{\"asset\":{\"version\":\"2.0\"},\"meshes\":[{\"primitives\":[]}]}",
		std::vector<unsigned char>()));
	loaded.primitives.push_back(GltfPrimitiveData());
	loaded.primitives[0].vertexCount = 99;
	CHECK(Gltf_LoadFileMulti("load-empty-prims.glb", &loaded, error, sizeof(error)) == -1);
	CHECK(error[0] != '\0');
	CHECK(loaded.primitives.empty()); // zero partial output

	// A primitive without a POSITION accessor.
	CHECK(WriteGlbFromJsonBin("load-no-position.glb",
		"{\"asset\":{\"version\":\"2.0\"},\"meshes\":[{\"primitives\":[{\"attributes\":{}}]}]}",
		std::vector<unsigned char>()));
	CHECK(Gltf_LoadFileMulti("load-no-position.glb", &loaded, error, sizeof(error)) == -1);
	CHECK(error[0] != '\0');
	CHECK(loaded.primitives.empty());

	// A primitive with an out-of-range index (valid POSITION, bad index list).
	std::vector<unsigned char> triangleBin(36, 0); // POSITION: 3 * VEC3 float, all zero
	AppendU32(triangleBin, 0);
	AppendU32(triangleBin, 1);
	AppendU32(triangleBin, 5); // out of range for a 3-vertex mesh
	CHECK(WriteGlbFromJsonBin("load-bad-index.glb",
		"{\"asset\":{\"version\":\"2.0\"},"
		"\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0},\"indices\":1}]}],"
		"\"accessors\":["
		"{\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\"},"
		"{\"bufferView\":1,\"componentType\":5125,\"count\":3,\"type\":\"SCALAR\"}],"
		"\"bufferViews\":["
		"{\"buffer\":0,\"byteOffset\":0,\"byteLength\":36},"
		"{\"buffer\":0,\"byteOffset\":36,\"byteLength\":12}],"
		"\"buffers\":[{\"byteLength\":48}]}",
		triangleBin));
	CHECK(Gltf_LoadFileMulti("load-bad-index.glb", &loaded, error, sizeof(error)) == -1);
	CHECK(strstr(error, "outside the vertex range") != NULL);
	CHECK(loaded.primitives.empty());

	// A primitive that references a texture the multi record cannot hold.
	const std::vector<unsigned char> positionOnly(36, 0); // POSITION: 3 * VEC3 float
	CHECK(WriteGlbFromJsonBin("load-textured.glb",
		"{\"asset\":{\"version\":\"2.0\"},"
		"\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0},\"material\":0}]}],"
		"\"materials\":[{\"pbrMetallicRoughness\":{\"baseColorTexture\":{\"index\":0}}}],"
		"\"accessors\":[{\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\"}],"
		"\"bufferViews\":[{\"buffer\":0,\"byteOffset\":0,\"byteLength\":36}],"
		"\"buffers\":[{\"byteLength\":36}]}",
		positionOnly));
	CHECK(Gltf_LoadFileMulti("load-textured.glb", &loaded, error, sizeof(error)) == -1);
	CHECK(strstr(error, "texture") != NULL);
	CHECK(loaded.primitives.empty());
}

// A synthetic RGBA8 image (4x3, a distinct byte per channel byte) round-trips
// through the multi writer/loader byte-for-byte with its dimensions, and two
// writes with an image are byte-identical.
static void TestImageRoundTrip()
{
	const int width = 4, height = 3;
	std::vector<unsigned char> image((size_t)width * height * 4);
	for (size_t i = 0; i < image.size(); i++)
		image[i] = (unsigned char)((i * 7 + 3) & 0xff);

	GltfMultiMeshData source;
	source.primitives.push_back(MakePrimitive(
		{ 0, 0, 0, 1, 0, 0, 0, 1, 0 },
		{ 0, 0, 1, 0, 0, 1, 0, 0, 1 },
		{ 0, 0, 1, 0, 0, 1 },
		{ 0, 1, 2 },
		0.25f, 0.5f, 0.75f, 1.0f));
	source.primitives[0].imageRgba = image;
	source.primitives[0].imageWidth = width;
	source.primitives[0].imageHeight = height;

	char error[256];
	CHECK(Gltf_WriteFileMulti("image.glb", source, error, sizeof(error)) == 0);

	GltfMultiMeshData loaded;
	CHECK(Gltf_LoadFileMulti("image.glb", &loaded, error, sizeof(error)) == 0);
	CHECK(loaded.primitives.size() == 1);
	if (loaded.primitives.size() == 1)
	{
		CHECK(loaded.primitives[0].imageWidth == width);
		CHECK(loaded.primitives[0].imageHeight == height);
		CHECK(loaded.primitives[0].imageRgba.size() == image.size());
		CHECK(loaded.primitives[0].imageRgba == image);
	}

	// Deterministic: a second write with the same image is byte-identical.
	CHECK(Gltf_WriteFileMulti("image_b.glb", source, error, sizeof(error)) == 0);
	std::vector<unsigned char> a, b;
	CHECK(ReadFileBytes("image.glb", &a));
	CHECK(ReadFileBytes("image_b.glb", &b));
	CHECK(!a.empty() && a.size() == b.size());
	CHECK(a.size() == b.size() && memcmp(a.data(), b.data(), a.size()) == 0);
}

// A multi mesh where primitive 0 is textured and the others are untextured: the
// image is present on exactly one primitive, and every primitive's geometry
// still round-trips.
static void TestMultiMixedImage()
{
	const int width = 2, height = 2;
	std::vector<unsigned char> image((size_t)width * height * 4);
	for (size_t i = 0; i < image.size(); i++)
		image[i] = (unsigned char)(255 - (int)i);

	GltfMultiMeshData source = MakeMultiMesh();
	source.primitives[0].imageRgba = image;
	source.primitives[0].imageWidth = width;
	source.primitives[0].imageHeight = height;

	char error[256];
	CHECK(Gltf_WriteFileMulti("mixed.glb", source, error, sizeof(error)) == 0);

	GltfMultiMeshData loaded;
	CHECK(Gltf_LoadFileMulti("mixed.glb", &loaded, error, sizeof(error)) == 0);
	CHECK(loaded.primitives.size() == source.primitives.size());
	if (loaded.primitives.size() == source.primitives.size())
	{
		CHECK(loaded.primitives[0].imageWidth == width && loaded.primitives[0].imageHeight == height);
		CHECK(loaded.primitives[0].imageRgba == image);
		CHECK(loaded.primitives[1].imageRgba.empty());
		CHECK(loaded.primitives[1].imageWidth == 0 && loaded.primitives[1].imageHeight == 0);
		CHECK(loaded.primitives[2].imageRgba.empty());

		CHECK(loaded.primitives[0].positions == source.primitives[0].positions);
		CHECK(loaded.primitives[0].indices == source.primitives[0].indices);
		CHECK(memcmp(loaded.primitives[0].baseColorFactor, source.primitives[0].baseColorFactor,
			sizeof(source.primitives[0].baseColorFactor)) == 0);
	}
}

// A PNG outside the writer's stored-block subset (a real WIC-encoded PNG whose
// IDAT uses Huffman coding) must be a named rejection with zero output, not a
// silent drop.
static void TestUnsupportedPngRejection()
{
	// A valid 1x1 red PNG (120 bytes), IDAT compressed with fixed Huffman.
	static const unsigned char png[120] = {
		0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D,
		0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
		0x08, 0x06, 0x00, 0x00, 0x00, 0x1F, 0x15, 0xC4, 0x89, 0x00, 0x00, 0x00,
		0x01, 0x73, 0x52, 0x47, 0x42, 0x00, 0xAE, 0xCE, 0x1C, 0xE9, 0x00, 0x00,
		0x00, 0x04, 0x67, 0x41, 0x4D, 0x41, 0x00, 0x00, 0xB1, 0x8F, 0x0B, 0xFC,
		0x61, 0x05, 0x00, 0x00, 0x00, 0x09, 0x70, 0x48, 0x59, 0x73, 0x00, 0x00,
		0x0E, 0xC3, 0x00, 0x00, 0x0E, 0xC3, 0x01, 0xC7, 0x6F, 0xA8, 0x64, 0x00,
		0x00, 0x00, 0x0D, 0x49, 0x44, 0x41, 0x54, 0x18, 0x57, 0x63, 0xF8, 0xCF,
		0xC0, 0xF0, 0x1F, 0x00, 0x05, 0x00, 0x01, 0xFF, 0xA6, 0x5C, 0x9B, 0x5D,
		0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82,
	};

	const unsigned int positionBytes = 36; // 3 * VEC3 float32
	std::vector<unsigned char> bin(positionBytes, 0);
	bin.insert(bin.end(), png, png + sizeof(png));
	const unsigned int imageLength = (unsigned int)sizeof(png);

	std::string json =
		"{\"asset\":{\"version\":\"2.0\"},"
		"\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0},\"material\":0}]}],"
		"\"materials\":[{\"pbrMetallicRoughness\":{\"baseColorTexture\":{\"index\":0}}}],"
		"\"textures\":[{\"source\":0}],"
		"\"images\":[{\"bufferView\":1,\"mimeType\":\"image/png\"}],"
		"\"accessors\":[{\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\"}],"
		"\"bufferViews\":[{\"buffer\":0,\"byteOffset\":0,\"byteLength\":36},"
		"{\"buffer\":0,\"byteOffset\":36,\"byteLength\":" + std::to_string(imageLength) + "}],"
		"\"buffers\":[{\"byteLength\":" + std::to_string(positionBytes + imageLength) + "}]}";

	CHECK(WriteGlbFromJsonBin("unsupported-png.glb", json.c_str(), bin));

	char error[256];
	GltfMultiMeshData loaded;
	loaded.primitives.push_back(GltfPrimitiveData());
	CHECK(Gltf_LoadFileMulti("unsupported-png.glb", &loaded, error, sizeof(error)) == -1);
	CHECK(error[0] != '\0');
	CHECK(strstr(error, "PNG") != NULL);
	CHECK(loaded.primitives.empty()); // zero partial output
}

// A stored-block PNG whose IDAT zlib stream is missing its Adler-32 trailer must
// be a named rejection, not a tolerated truncation.
static void TestStoredPngTruncatedTrailer()
{
	const std::vector<unsigned char> rgba(4, 200); // one opaque pixel
	const std::vector<unsigned char> png = MakeStoredPng(1, 1, rgba, false);
	CHECK(WriteGlbWithImage("png-no-trailer.glb", png));

	char error[256];
	GltfMultiMeshData loaded;
	loaded.primitives.push_back(GltfPrimitiveData());
	CHECK(Gltf_LoadFileMulti("png-no-trailer.glb", &loaded, error, sizeof(error)) == -1);
	CHECK(error[0] != '\0');
	CHECK(strstr(error, "Adler") != NULL);
	CHECK(loaded.primitives.empty()); // zero partial output
}

// A PNG whose chunk CRC does not match its type+data must be rejected rather than
// trusted.
static void TestPngCorruptedChunkCrc()
{
	const std::vector<unsigned char> rgba(4, 10); // one opaque pixel
	std::vector<unsigned char> png = MakeStoredPng(1, 1, rgba, true);
	CHECK(png.size() > 16);
	png[16] ^= 0xff; // corrupt the IHDR payload without updating its CRC
	CHECK(WriteGlbWithImage("png-bad-crc.glb", png));

	char error[256];
	GltfMultiMeshData loaded;
	loaded.primitives.push_back(GltfPrimitiveData());
	CHECK(Gltf_LoadFileMulti("png-bad-crc.glb", &loaded, error, sizeof(error)) == -1);
	CHECK(error[0] != '\0');
	CHECK(strstr(error, "CRC") != NULL);
	CHECK(loaded.primitives.empty()); // zero partial output
}

// A material whose `extensions` carries a texture reference (KHR spec-gloss
// diffuseTexture) must be rejected, not silently dropped.
static void TestMaterialExtensionTextureRejection()
{
	const std::vector<unsigned char> positionOnly(36, 0); // POSITION: 3 * VEC3 float
	CHECK(WriteGlbFromJsonBin("load-extension-texture.glb",
		"{\"asset\":{\"version\":\"2.0\"},"
		"\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0},\"material\":0}]}],"
		"\"materials\":[{\"extensions\":{\"KHR_materials_pbrSpecularGlossiness\":"
		"{\"diffuseTexture\":{\"index\":0}}}}],"
		"\"accessors\":[{\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\"}],"
		"\"bufferViews\":[{\"buffer\":0,\"byteOffset\":0,\"byteLength\":36}],"
		"\"buffers\":[{\"byteLength\":36}]}",
		positionOnly));

	char error[256];
	GltfMultiMeshData loaded;
	loaded.primitives.push_back(GltfPrimitiveData());
	CHECK(Gltf_LoadFileMulti("load-extension-texture.glb", &loaded, error, sizeof(error)) == -1);
	CHECK(error[0] != '\0');
	CHECK(strstr(error, "texture") != NULL);
	CHECK(loaded.primitives.empty()); // zero partial output
}

// P2: a primitive's optional per-vertex COLOR_0 attribute (RGBA8) round-trips
// through both the multi and single loaders, and a primitive without colours
// writes no COLOR_0 (the no-colour byte-identity is covered by
// TestSingleVsMultiByteIdentity / TestReproducibleBytes).
static void TestColorRoundTrip()
{
	GltfMultiMeshData source;
	GltfPrimitiveData primitive = MakePrimitive(
		{ 0, 0, 0, 1, 0, 0, 0, 1, 0 },
		{ 0, 0, 1, 0, 0, 1, 0, 0, 1 },
		{ 0, 0, 1, 0, 0, 1 },
		{ 0, 1, 2 },
		0.25f, 0.5f, 0.75f, 1.0f);
	primitive.colors = { 10, 20, 30, 255, 40, 50, 60, 255, 70, 80, 90, 255 };
	source.primitives.push_back(primitive);

	char error[256];
	CHECK(Gltf_WriteFileMulti("color.glb", source, error, sizeof(error)) == 0);

	GltfMultiMeshData loaded;
	CHECK(Gltf_LoadFileMulti("color.glb", &loaded, error, sizeof(error)) == 0);
	CHECK(loaded.primitives.size() == 1);
	if (loaded.primitives.size() == 1)
	{
		CHECK(loaded.primitives[0].colors == primitive.colors);
		CHECK(loaded.primitives[0].positions == primitive.positions);
		CHECK(loaded.primitives[0].indices == primitive.indices);
	}

	// The single loader reads the same COLOR_0 attribute.
	GltfMeshData single;
	CHECK(Gltf_LoadFile("color.glb", &single, error, sizeof(error)) == 0);
	CHECK(single.colors == primitive.colors);
}

static void TestWorldShadeRoundTrip()
{
	GltfMeshData source = MakeTriangle();
	source.worldShadeIndices = { -1, 0, 31 };
	char error[256];
	CHECK(Gltf_WriteFile("world-shade.glb", source, error, sizeof(error)) == 0);
	GltfMeshData single;
	GltfMultiMeshData multi;
	CHECK(Gltf_LoadFile("world-shade.glb", &single, error, sizeof(error)) == 0);
	CHECK(single.worldShadeIndices == source.worldShadeIndices);
	CHECK(Gltf_LoadFileMulti("world-shade.glb", &multi, error, sizeof(error)) == 0);
	CHECK(multi.primitives.size() == 1 && multi.primitives[0].worldShadeIndices == source.worldShadeIndices);
	CHECK(Gltf_WriteFile("world-shade-again.glb", single, error, sizeof(error)) == 0);
	std::vector<unsigned char> first, second;
	CHECK(ReadFileBytes("world-shade.glb", &first) == 1 && ReadFileBytes("world-shade-again.glb", &second) == 1);
	CHECK(first == second);
	source.worldShadeIndices = { 0, 1 };
	CHECK(Gltf_WriteFile("invalid-shade.glb", source, error, sizeof(error)) != 0 && strstr(error, "count"));
	const float invalidValues[] = { -2, 32, 1.5f };
	for (int i = 0; i < 3; ++i)
	{
		source.worldShadeIndices = { 0, invalidValues[i], 31 };
		CHECK(Gltf_WriteFile("invalid-shade.glb", source, error, sizeof(error)) != 0 && strstr(error, "index"));
	}
	// Reader rejection is independent of writer validation: construct malformed
	// metadata directly, including wrong attribute type/count/value.
	const char* json = "{\"asset\":{\"version\":\"2.0\"},\"buffers\":[{\"byteLength\":48}],"
		"\"bufferViews\":[{\"buffer\":0,\"byteOffset\":0,\"byteLength\":36},{\"buffer\":0,\"byteOffset\":36,\"byteLength\":12}],"
		"\"accessors\":[{\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\"},"
		"{\"bufferView\":1,\"componentType\":5126,\"count\":3,\"type\":\"SCALAR\"}],"
		"\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0,\"_RED2_WORLD_SHADE\":1}}]}]}";
	std::vector<unsigned char> bin(48, 0);
	const float badShade = 32;
	memcpy(bin.data() + 36, &badShade, sizeof(badShade));
	CHECK(WriteGlbFromJsonBin("malformed-shade.glb", json, bin) == 1);
	CHECK(Gltf_LoadFileMulti("malformed-shade.glb", &multi, error, sizeof(error)) != 0 && multi.primitives.empty() && strstr(error, "shade"));
	std::string wrongCount(json);
	const size_t countAt = wrongCount.rfind("\"count\":3");
	wrongCount.replace(countAt, 9, "\"count\":2");
	CHECK(WriteGlbFromJsonBin("malformed-shade.glb", wrongCount.c_str(), bin) == 1);
	CHECK(Gltf_LoadFileMulti("malformed-shade.glb", &multi, error, sizeof(error)) != 0 && multi.primitives.empty() && strstr(error, "count"));
}

int main()
{
	TestRoundTrip();
	TestReproducibleBytes();
	TestDiagnostics();
	TestMultiPrimitiveRoundTrip();
	TestMultiPrimitiveDeterminism();
	TestSingleVsMultiByteIdentity();
	TestMultiPrimitiveWriterRejections();
	TestMultiPrimitiveLoaderRejections();
	TestImageRoundTrip();
	TestMultiMixedImage();
	TestUnsupportedPngRejection();
	TestStoredPngTruncatedTrailer();
	TestPngCorruptedChunkCrc();
	TestMaterialExtensionTextureRejection();
	TestColorRoundTrip();
	TestWorldShadeRoundTrip();

	printf("%d checks, %d failures\n", g_checks, g_failures);
	return g_failures == 0 ? 0 : 1;
}
