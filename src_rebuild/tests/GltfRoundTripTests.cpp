// Standalone regression tests for the E02 native mesh record: the project-owned
// glTF/GLB writer and importer must agree on geometry, UVs, transform and
// material flags, and identical conversions must produce identical bytes.
//
// The importer's WIC image path is not exercised (no embedded image), so the
// test needs no game assets or GPU context. Build and run instructions are in
// tests/README.md.

#include "../utils/GltfLoader.h"
#include "../utils/GltfWriter.h"

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

int main()
{
	TestRoundTrip();
	TestReproducibleBytes();
	TestDiagnostics();

	printf("%d checks, %d failures\n", g_checks, g_failures);
	return g_failures == 0 ? 0 : 1;
}
