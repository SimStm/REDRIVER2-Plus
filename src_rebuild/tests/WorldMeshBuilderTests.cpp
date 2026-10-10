// Standalone regression tests for the E03 block 2 item 3 world mesh builder
// (SourceMesh -> GltfMeshData) that feeds the single-primitive E02 GLB pipeline.
//
// It pins the documented invariants: flat per-triangle normal synthesis, base
// colour from stored raw face colours (or the neutral when absent), the named
// multi-submesh diagnostic with zero output, bounds/argument errors, output
// determinism (two builds byte-identical, equal conversion content hash) and a
// GltfLoader reload of the built mesh. It needs no game assets or GPU context.
// Build and run instructions are in tests/README.md.

#include "../utils/WorldMeshBuilder.h"
#include "../utils/GltfWriter.h"
#include "../utils/GltfLoader.h"
#include "../utils/AssetConverter.h"
#include "../Game/C/conversioncache.h"

#include <direct.h>
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

static bool NearlyEqual(float a, float b, float epsilon = 1e-5f)
{
	return fabsf(a - b) <= epsilon;
}

// One quad in the XY plane, decoder emission order (v0,v1,v3,v2) already
// flattened to (0,1,2)+(0,2,3), with UVs but no stored RGB (a PL record).
static SourceMesh MakePlainQuad()
{
	SourceMesh mesh;
	mesh.positions = {
		0.0f, 0.0f, 0.0f,
		1.0f, 0.0f, 0.0f,
		1.0f, 1.0f, 0.0f,
		0.0f, 1.0f, 0.0f,
	};
	mesh.uvs = { 0.0f, 0.0f, 8.0f, 0.0f, 8.0f, 8.0f, 0.0f, 8.0f };
	mesh.indices = { 0, 1, 2, 0, 2, 3 };

	SourceSubmesh submesh;
	submesh.primitiveClass = SourcePrimitiveClass::TexturedPlQuad;
	submesh.textureSet = 1;
	submesh.textureId = 2;
	submesh.firstIndex = 0;
	submesh.indexCount = 6;
	submesh.hasColors = false;
	submesh.hasNormals = false;
	mesh.submeshes.push_back(submesh);
	return mesh;
}

// The same quad with a stored raw RGB face colour per vertex.
static SourceMesh MakeColoredQuad()
{
	SourceMesh mesh = MakePlainQuad();
	mesh.colors = {
		255, 0, 0,
		0, 255, 0,
		0, 0, 255,
		255, 255, 0,
	};
	mesh.submeshes[0].hasColors = true;
	return mesh;
}

static bool FileExists(const char* path)
{
	FILE* file = fopen(path, "rb");
	if (!file)
		return false;
	fclose(file);
	return true;
}

static int ReadFileBytes(const char* path, std::vector<unsigned char>* out)
{
	FILE* file = fopen(path, "rb");
	if (!file)
		return 0;
	fseek(file, 0, SEEK_END);
	const long size = ftell(file);
	fseek(file, 0, SEEK_SET);
	if (size <= 0) { fclose(file); return 0; }
	out->resize((size_t)size);
	const bool read = fread(out->data(), 1, (size_t)size, file) == (size_t)size;
	fclose(file);
	return read ? 1 : 0;
}

static void TestFlatNormals()
{
	SourceMesh source = MakePlainQuad();
	GltfMeshData mesh;
	WorldMeshBuilderResult result;
	char error[256];

	CHECK(WorldMeshBuilder_Build(source, &mesh, &result, error, sizeof(error)) == WORLD_MESH_ERROR_NONE);
	CHECK(result.error == WORLD_MESH_ERROR_NONE);
	CHECK(result.submeshCount == 1);
	CHECK(result.vertexCount == 6);
	CHECK(result.triangleCount == 2);
	CHECK(result.primitiveClass == SourcePrimitiveClass::TexturedPlQuad);
	CHECK(result.textureSet == 1 && result.textureId == 2);
	CHECK(result.hasColors == false);

	// Every triangle corner carries the same flat, unit, +Z normal.
	CHECK(mesh.normals.size() == 6 * 3);
	for (int vertex = 0; vertex < 6; ++vertex)
	{
		const float* n = &mesh.normals[(size_t)vertex * 3];
		CHECK(NearlyEqual(n[0], 0.0f));
		CHECK(NearlyEqual(n[1], 0.0f));
		CHECK(NearlyEqual(n[2], 1.0f));
		CHECK(NearlyEqual(sqrtf(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]), 1.0f));
	}

	// The two triangles of a planar quad share the same face normal.
	CHECK(memcmp(&mesh.normals[0], &mesh.normals[3], sizeof(float) * 3) == 0);

	// UVs are copied to the expanded per-triangle corners, raw (unnormalised).
	CHECK(mesh.uvs.size() == 6 * 2);
	CHECK(NearlyEqual(mesh.uvs[0], 0.0f) && NearlyEqual(mesh.uvs[1], 0.0f));
	CHECK(NearlyEqual(mesh.uvs[2], 8.0f) && NearlyEqual(mesh.uvs[3], 0.0f));

	// Output indices are a sequential triangle list over the expanded vertices.
	CHECK(mesh.indices.size() == 6);
	for (int i = 0; i < 6; ++i)
		CHECK(mesh.indices[i] == (unsigned int)i);
}

static void TestFlatNormalOfVerticalTriangle()
{
	// A triangle in the XZ plane gets a +/-Y normal; this pins that the normal
	// is derived from the geometry rather than a fixed axis.
	SourceMesh source;
	source.positions = { 0, 0, 0, 1, 0, 0, 0, 0, 1 };
	source.uvs = { 0, 0, 1, 0, 0, 1 };
	source.indices = { 0, 1, 2 };

	SourceSubmesh submesh;
	submesh.primitiveClass = SourcePrimitiveClass::TexturedFlatTriangle;
	submesh.textureSet = 0;
	submesh.textureId = 0;
	submesh.firstIndex = 0;
	submesh.indexCount = 3;
	submesh.hasColors = false;
	submesh.hasNormals = false;
	source.submeshes.push_back(submesh);

	GltfMeshData mesh;
	WorldMeshBuilderResult result;
	char error[256];
	CHECK(WorldMeshBuilder_Build(source, &mesh, &result, error, sizeof(error)) == WORLD_MESH_ERROR_NONE);
	CHECK(mesh.vertexCount == 3 && mesh.triangleCount == 1);
	CHECK(NearlyEqual(mesh.normals[0], 0.0f));
	CHECK(NearlyEqual(fabsf(mesh.normals[1]), 1.0f));
	CHECK(NearlyEqual(mesh.normals[2], 0.0f));
}

static void TestBaseColor()
{
	char error[256];

	// Stored raw face colours become the mean material colour, no shading.
	{
		SourceMesh source = MakeColoredQuad();
		GltfMeshData mesh;
		WorldMeshBuilderResult result;
		CHECK(WorldMeshBuilder_Build(source, &mesh, &result, error, sizeof(error)) == WORLD_MESH_ERROR_NONE);
		CHECK(result.hasColors == true);
		CHECK(NearlyEqual(mesh.baseColorFactor[0], 0.5f));
		CHECK(NearlyEqual(mesh.baseColorFactor[1], 85.0f / 255.0f));
		CHECK(NearlyEqual(mesh.baseColorFactor[2], 85.0f / 255.0f));
		CHECK(NearlyEqual(mesh.baseColorFactor[3], 1.0f));
	}

	// A record with no stored RGB (a PL record) uses the documented neutral.
	{
		SourceMesh source = MakePlainQuad();
		GltfMeshData mesh;
		WorldMeshBuilderResult result;
		CHECK(WorldMeshBuilder_Build(source, &mesh, &result, error, sizeof(error)) == WORLD_MESH_ERROR_NONE);
		CHECK(result.hasColors == false);
		CHECK(NearlyEqual(mesh.baseColorFactor[0], 1.0f));
		CHECK(NearlyEqual(mesh.baseColorFactor[1], 1.0f));
		CHECK(NearlyEqual(mesh.baseColorFactor[2], 1.0f));
		CHECK(NearlyEqual(mesh.baseColorFactor[3], 1.0f));
	}
}

static void TestMultipleSubmeshesDiagnostic()
{
	SourceMesh source = MakePlainQuad();

	SourceSubmesh second;
	second.primitiveClass = SourcePrimitiveClass::TexturedFlatQuad;
	second.textureSet = 1;
	second.textureId = 3;
	second.firstIndex = 6;
	second.indexCount = 6;
	second.hasColors = false;
	second.hasNormals = false;
	source.submeshes.push_back(second);

	GltfMeshData mesh;
	mesh.positions = { 1, 2, 3 };
	WorldMeshBuilderResult result;
	char error[256];
	CHECK(WorldMeshBuilder_Build(source, &mesh, &result, error, sizeof(error)) == WORLD_MESH_ERROR_MULTIPLE_SUBMESHES);
	CHECK(result.error == WORLD_MESH_ERROR_MULTIPLE_SUBMESHES);
	CHECK(strcmp(WorldMeshBuilder_ErrorName(result.error), "multiple-submeshes-unsupported") == 0);

	// Zero partial output.
	CHECK(mesh.positions.empty());
	CHECK(mesh.normals.empty());
	CHECK(mesh.uvs.empty());
	CHECK(mesh.indices.empty());
	CHECK(mesh.vertexCount == 0 && mesh.triangleCount == 0);
	CHECK(error[0] != '\0');
}

static void TestBoundsAndArguments()
{
	char error[256];
	WorldMeshBuilderResult result;
	GltfMeshData mesh;

	// No destination.
	CHECK(WorldMeshBuilder_Build(MakePlainQuad(), NULL, &result, error, sizeof(error)) == WORLD_MESH_ERROR_ARGUMENT);

	// Empty positions.
	{
		SourceMesh source = MakePlainQuad();
		source.positions.clear();
		CHECK(WorldMeshBuilder_Build(source, &mesh, &result, error, sizeof(error)) == WORLD_MESH_ERROR_EMPTY);
		CHECK(mesh.positions.empty());
	}

	// Positions not a whole number of vertices.
	{
		SourceMesh source = MakePlainQuad();
		source.positions.push_back(1.0f);
		CHECK(WorldMeshBuilder_Build(source, &mesh, &result, error, sizeof(error)) == WORLD_MESH_ERROR_ATTRIBUTE_SIZE);
	}

	// UV count mismatch.
	{
		SourceMesh source = MakePlainQuad();
		source.uvs.pop_back();
		CHECK(WorldMeshBuilder_Build(source, &mesh, &result, error, sizeof(error)) == WORLD_MESH_ERROR_ATTRIBUTE_SIZE);
	}

	// Colour count mismatch.
	{
		SourceMesh source = MakeColoredQuad();
		source.colors.pop_back();
		CHECK(WorldMeshBuilder_Build(source, &mesh, &result, error, sizeof(error)) == WORLD_MESH_ERROR_ATTRIBUTE_SIZE);
	}

	// Declared colours with no colour array.
	{
		SourceMesh source = MakePlainQuad();
		source.submeshes[0].hasColors = true;
		CHECK(WorldMeshBuilder_Build(source, &mesh, &result, error, sizeof(error)) == WORLD_MESH_ERROR_ATTRIBUTE_SIZE);
	}

	// Colours present but not declared.
	{
		SourceMesh source = MakeColoredQuad();
		source.submeshes[0].hasColors = false;
		CHECK(WorldMeshBuilder_Build(source, &mesh, &result, error, sizeof(error)) == WORLD_MESH_ERROR_ATTRIBUTE_SIZE);
	}

	// No whole triangles.
	{
		SourceMesh source = MakePlainQuad();
		source.indices.pop_back();
		CHECK(WorldMeshBuilder_Build(source, &mesh, &result, error, sizeof(error)) == WORLD_MESH_ERROR_EMPTY);
	}

	// No submesh.
	{
		SourceMesh source = MakePlainQuad();
		source.submeshes.clear();
		CHECK(WorldMeshBuilder_Build(source, &mesh, &result, error, sizeof(error)) == WORLD_MESH_ERROR_EMPTY);
	}

	// Index outside the vertex range.
	{
		SourceMesh source = MakePlainQuad();
		source.indices[0] = 99;
		CHECK(WorldMeshBuilder_Build(source, &mesh, &result, error, sizeof(error)) == WORLD_MESH_ERROR_INDEX_RANGE);
		CHECK(mesh.positions.empty());
	}

	// Submesh run that is not a whole-triangle range inside the index list.
	{
		SourceMesh source = MakePlainQuad();
		source.submeshes[0].indexCount = 4;
		CHECK(WorldMeshBuilder_Build(source, &mesh, &result, error, sizeof(error)) == WORLD_MESH_ERROR_INDEX_RANGE);
	}

	// Submesh does not cover every index.
	{
		SourceMesh source = MakePlainQuad();
		source.submeshes[0].firstIndex = 3;
		source.submeshes[0].indexCount = 3;
		CHECK(WorldMeshBuilder_Build(source, &mesh, &result, error, sizeof(error)) == WORLD_MESH_ERROR_UNCOVERED_INDICES);
		CHECK(mesh.positions.empty());
	}

	// A null error buffer is allowed and never written.
	{
		SourceMesh source = MakePlainQuad();
		source.positions.clear();
		CHECK(WorldMeshBuilder_Build(source, &mesh, &result, NULL, 0) == WORLD_MESH_ERROR_EMPTY);
	}

	CHECK(strcmp(WorldMeshBuilder_ErrorName(WORLD_MESH_ERROR_NONE), "none") == 0);
	CHECK(strcmp(WorldMeshBuilder_ErrorName(WORLD_MESH_ERROR_UNCOVERED_INDICES), "uncovered-indices") == 0);
}

static void TestDeterminismAndReload()
{
	SourceMesh source = MakeColoredQuad();
	char error[256];

	GltfMeshData first;
	WorldMeshBuilderResult firstResult;
	CHECK(WorldMeshBuilder_Build(source, &first, &firstResult, error, sizeof(error)) == WORLD_MESH_ERROR_NONE);

	GltfMeshData second;
	WorldMeshBuilderResult secondResult;
	CHECK(WorldMeshBuilder_Build(source, &second, &secondResult, error, sizeof(error)) == WORLD_MESH_ERROR_NONE);

	CHECK(firstResult.vertexCount == secondResult.vertexCount);
	CHECK(firstResult.triangleCount == secondResult.triangleCount);
	CHECK(first.positions == second.positions);
	CHECK(first.normals == second.normals);
	CHECK(first.uvs == second.uvs);
	CHECK(first.indices == second.indices);

	// Two builds write byte-identical GLB files.
	CHECK(Gltf_WriteFile("world_a.glb", first, error, sizeof(error)) == 0);
	CHECK(Gltf_WriteFile("world_b.glb", second, error, sizeof(error)) == 0);
	std::vector<unsigned char> bytesA, bytesB;
	CHECK(ReadFileBytes("world_a.glb", &bytesA));
	CHECK(ReadFileBytes("world_b.glb", &bytesB));
	CHECK(!bytesA.empty() && bytesA.size() == bytesB.size());
	CHECK(bytesA.size() == bytesB.size() && memcmp(bytesA.data(), bytesB.data(), bytesA.size()) == 0);

	// The built mesh reloads through the GLB importer with the same record.
	GltfMeshData loaded;
	CHECK(Gltf_LoadFile("world_a.glb", &loaded, error, sizeof(error)) == 0);
	CHECK(loaded.vertexCount == first.vertexCount);
	CHECK(loaded.triangleCount == first.triangleCount);
	CHECK(loaded.positions.size() == first.positions.size());
	CHECK(memcmp(loaded.positions.data(), first.positions.data(), first.positions.size() * sizeof(float)) == 0);
	CHECK(loaded.normals.size() == first.normals.size());
	CHECK(memcmp(loaded.normals.data(), first.normals.data(), first.normals.size() * sizeof(float)) == 0);
	CHECK(loaded.uvs.size() == first.uvs.size());
	CHECK(memcmp(loaded.uvs.data(), first.uvs.data(), first.uvs.size() * sizeof(float)) == 0);
	CHECK(loaded.indices.size() == first.indices.size());
	CHECK(memcmp(loaded.indices.data(), first.indices.data(), first.indices.size() * sizeof(unsigned int)) == 0);
	CHECK(memcmp(loaded.baseColorFactor, first.baseColorFactor, sizeof(first.baseColorFactor)) == 0);
}

static void TestConversionHashStability()
{
	_mkdir("cache");
	char error[256];

	GltfMeshData mesh;
	WorldMeshBuilderResult result;
	CHECK(WorldMeshBuilder_Build(MakeColoredQuad(), &mesh, &result, error, sizeof(error)) == WORLD_MESH_ERROR_NONE);

	ConversionCacheKey key;
	ConversionCache_MakeKey(&key, "world-mesh", 0x1234ULL, 0ULL, 0ULL, CONVERSION_CACHE_SCHEMA_VERSION);

	AssetConverterResult first, second;
	CHECK(AssetConverter_ConvertMesh("cache", &key, mesh, &first, error, sizeof(error)) == ASSET_CONVERTER_OK);
	CHECK(first.converted == 1 && first.reused == 0);
	CHECK(FileExists(first.glbPath));

	CHECK(AssetConverter_ConvertMesh("cache", &key, mesh, &second, error, sizeof(error)) == ASSET_CONVERTER_OK);
	CHECK(second.reused == 1);
	CHECK(strcmp(first.glbPath, second.glbPath) == 0);
	CHECK(strcmp(first.contentHash, second.contentHash) == 0);
}

int main()
{
	TestFlatNormals();
	TestFlatNormalOfVerticalTriangle();
	TestBaseColor();
	TestMultipleSubmeshesDiagnostic();
	TestBoundsAndArguments();
	TestDeterminismAndReload();
	TestConversionHashStability();

	printf("%d checks, %d failures\n", g_checks, g_failures);
	return g_failures == 0 ? 0 : 1;
}
