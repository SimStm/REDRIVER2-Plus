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

// Two separate coloured quads (each two triangles) as two submeshes: submesh 0
// at z = 0 with a solid red face colour, submesh 1 at z = 1 with a solid green
// one. The colour array is global (one RGB per vertex); the presence flag is per
// submesh. The runs are contiguous and cover the whole index list.
static SourceMesh MakeMultiSubmeshMesh()
{
	SourceMesh mesh;
	mesh.positions = {
		// quad A (z = 0)
		0.0f, 0.0f, 0.0f,
		1.0f, 0.0f, 0.0f,
		1.0f, 1.0f, 0.0f,
		0.0f, 1.0f, 0.0f,
		// quad B (z = 1)
		0.0f, 0.0f, 1.0f,
		1.0f, 0.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		0.0f, 1.0f, 1.0f,
	};
	mesh.uvs = {
		0.0f, 0.0f, 8.0f, 0.0f, 8.0f, 8.0f, 0.0f, 8.0f,
		0.0f, 0.0f, 4.0f, 0.0f, 4.0f, 4.0f, 0.0f, 4.0f,
	};
	mesh.colors = {
		255, 0, 0, 255, 0, 0, 255, 0, 0, 255, 0, 0,		// quad A: red
		0, 255, 0, 0, 255, 0, 0, 255, 0, 0, 255, 0,		// quad B: green
	};
	mesh.indices = { 0, 1, 2, 0, 2, 3, 4, 5, 6, 4, 6, 7 };

	SourceSubmesh first;
	first.primitiveClass = SourcePrimitiveClass::TexturedFlatQuad;
	first.textureSet = 1;
	first.textureId = 2;
	first.firstIndex = 0;
	first.indexCount = 6;
	first.hasColors = true;
	first.hasNormals = false;
	mesh.submeshes.push_back(first);

	SourceSubmesh second;
	second.primitiveClass = SourcePrimitiveClass::TexturedFlatQuad;
	second.textureSet = 1;
	second.textureId = 3;
	second.firstIndex = 6;
	second.indexCount = 6;
	second.hasColors = true;
	second.hasNormals = false;
	mesh.submeshes.push_back(second);
	return mesh;
}

static void TestMultiSubmeshBuild()
{
	SourceMesh source = MakeMultiSubmeshMesh();
	GltfMultiMeshData mesh;
	WorldMeshBuilderResult result;
	char error[256];

	CHECK(WorldMeshBuilder_BuildMulti(source, &mesh, &result, error, sizeof(error)) == WORLD_MESH_ERROR_NONE);
	CHECK(result.error == WORLD_MESH_ERROR_NONE);
	CHECK(result.submeshCount == 2);
	CHECK(result.vertexCount == 12);
	CHECK(result.triangleCount == 4);
	CHECK(result.primitiveClass == SourcePrimitiveClass::TexturedFlatQuad);
	CHECK(result.textureSet == 1 && result.textureId == 2);
	CHECK(result.hasColors == true);

	// One primitive per submesh, each with its own expanded geometry.
	CHECK(mesh.primitives.size() == 2);
	CHECK(mesh.primitives[0].vertexCount == 6 && mesh.primitives[0].triangleCount == 2);
	CHECK(mesh.primitives[1].vertexCount == 6 && mesh.primitives[1].triangleCount == 2);
	CHECK(mesh.primitives[0].positions.size() == 18 && mesh.primitives[0].normals.size() == 18);
	CHECK(mesh.primitives[1].positions.size() == 18 && mesh.primitives[1].normals.size() == 18);
	CHECK(mesh.primitives[0].indices.size() == 6 && mesh.primitives[1].indices.size() == 6);

	// Each primitive carries its own quad (z = 0 vs z = 1).
	CHECK(NearlyEqual(mesh.primitives[0].positions[2], 0.0f));
	CHECK(NearlyEqual(mesh.primitives[1].positions[2], 1.0f));

	// Per-primitive base colour: solid red and solid green, no shading baked in.
	CHECK(NearlyEqual(mesh.primitives[0].baseColorFactor[0], 1.0f));
	CHECK(NearlyEqual(mesh.primitives[0].baseColorFactor[1], 0.0f));
	CHECK(NearlyEqual(mesh.primitives[0].baseColorFactor[2], 0.0f));
	CHECK(NearlyEqual(mesh.primitives[0].baseColorFactor[3], 1.0f));
	CHECK(NearlyEqual(mesh.primitives[1].baseColorFactor[0], 0.0f));
	CHECK(NearlyEqual(mesh.primitives[1].baseColorFactor[1], 1.0f));
	CHECK(NearlyEqual(mesh.primitives[1].baseColorFactor[2], 0.0f));

	// Flat per-triangle normals are synthesized per primitive, as the single path.
	for (int vertex = 0; vertex < 6; ++vertex)
	{
		CHECK(NearlyEqual(mesh.primitives[0].normals[(size_t)vertex * 3 + 2], 1.0f));
		CHECK(NearlyEqual(mesh.primitives[1].normals[(size_t)vertex * 3 + 2], 1.0f));
	}
}

static void TestMultiMixedColors()
{
	// A colour-bearing submesh and a PL submesh in the same mesh: the PL submesh
	// must use the documented neutral rather than read a neighbour's colour.
	SourceMesh source = MakeMultiSubmeshMesh();
	source.submeshes[1].hasColors = false;
	source.submeshes[1].primitiveClass = SourcePrimitiveClass::TexturedPlQuad;

	GltfMultiMeshData mesh;
	WorldMeshBuilderResult result;
	char error[256];
	CHECK(WorldMeshBuilder_BuildMulti(source, &mesh, &result, error, sizeof(error)) == WORLD_MESH_ERROR_NONE);
	CHECK(result.hasColors == true); // at least one submesh carries colour
	CHECK(mesh.primitives.size() == 2);
	CHECK(NearlyEqual(mesh.primitives[0].baseColorFactor[0], 1.0f));
	CHECK(NearlyEqual(mesh.primitives[0].baseColorFactor[1], 0.0f));
	CHECK(NearlyEqual(mesh.primitives[1].baseColorFactor[0], 1.0f));
	CHECK(NearlyEqual(mesh.primitives[1].baseColorFactor[1], 1.0f));
	CHECK(NearlyEqual(mesh.primitives[1].baseColorFactor[2], 1.0f));
}

static void TestMultiErrors()
{
	char error[256];
	WorldMeshBuilderResult result;
	GltfMultiMeshData mesh;

	// No destination.
	CHECK(WorldMeshBuilder_BuildMulti(MakeMultiSubmeshMesh(), NULL, &result, error, sizeof(error)) == WORLD_MESH_ERROR_ARGUMENT);

	// No submesh.
	{
		SourceMesh source = MakeMultiSubmeshMesh();
		source.submeshes.clear();
		CHECK(WorldMeshBuilder_BuildMulti(source, &mesh, &result, error, sizeof(error)) == WORLD_MESH_ERROR_EMPTY);
		CHECK(mesh.primitives.empty());
	}

	// A gap between runs would drop geometry silently: rejected, zero output.
	{
		SourceMesh source = MakeMultiSubmeshMesh();
		source.submeshes[0].indexCount = 3;
		source.submeshes[1].firstIndex = 6;
		source.submeshes[1].indexCount = 3;
		CHECK(WorldMeshBuilder_BuildMulti(source, &mesh, &result, error, sizeof(error)) == WORLD_MESH_ERROR_UNCOVERED_INDICES);
		CHECK(mesh.primitives.empty());
		CHECK(error[0] != '\0');
	}

	// A run that extends past the index list.
	{
		SourceMesh source = MakeMultiSubmeshMesh();
		source.submeshes[1].indexCount = 12;
		CHECK(WorldMeshBuilder_BuildMulti(source, &mesh, &result, error, sizeof(error)) == WORLD_MESH_ERROR_INDEX_RANGE);
		CHECK(mesh.primitives.empty());
	}

	// A submesh declares colour but the source has none.
	{
		SourceMesh source = MakeMultiSubmeshMesh();
		source.colors.clear();
		CHECK(WorldMeshBuilder_BuildMulti(source, &mesh, &result, error, sizeof(error)) == WORLD_MESH_ERROR_ATTRIBUTE_SIZE);
		CHECK(mesh.primitives.empty());
	}

	// An index outside the vertex range.
	{
		SourceMesh source = MakeMultiSubmeshMesh();
		source.indices[6] = 99;
		CHECK(WorldMeshBuilder_BuildMulti(source, &mesh, &result, error, sizeof(error)) == WORLD_MESH_ERROR_INDEX_RANGE);
	}

	// Positions not a whole number of vertices.
	{
		SourceMesh source = MakeMultiSubmeshMesh();
		source.positions.push_back(1.0f);
		CHECK(WorldMeshBuilder_BuildMulti(source, &mesh, &result, error, sizeof(error)) == WORLD_MESH_ERROR_ATTRIBUTE_SIZE);
	}

	// A null error buffer is allowed and never written.
	{
		SourceMesh source = MakeMultiSubmeshMesh();
		source.submeshes.clear();
		CHECK(WorldMeshBuilder_BuildMulti(source, &mesh, &result, NULL, 0) == WORLD_MESH_ERROR_EMPTY);
	}
}

static void TestMultiDeterminismAndReload()
{
	SourceMesh source = MakeMultiSubmeshMesh();
	char error[256];

	GltfMultiMeshData first;
	WorldMeshBuilderResult firstResult;
	CHECK(WorldMeshBuilder_BuildMulti(source, &first, &firstResult, error, sizeof(error)) == WORLD_MESH_ERROR_NONE);

	GltfMultiMeshData second;
	WorldMeshBuilderResult secondResult;
	CHECK(WorldMeshBuilder_BuildMulti(source, &second, &secondResult, error, sizeof(error)) == WORLD_MESH_ERROR_NONE);

	CHECK(firstResult.vertexCount == secondResult.vertexCount);
	CHECK(firstResult.triangleCount == secondResult.triangleCount);
	CHECK(first.primitives.size() == second.primitives.size());
	for (size_t p = 0; p < first.primitives.size(); ++p)
	{
		CHECK(first.primitives[p].positions == second.primitives[p].positions);
		CHECK(first.primitives[p].normals == second.primitives[p].normals);
		CHECK(first.primitives[p].uvs == second.primitives[p].uvs);
		CHECK(first.primitives[p].indices == second.primitives[p].indices);
		CHECK(memcmp(first.primitives[p].baseColorFactor, second.primitives[p].baseColorFactor,
			sizeof(first.primitives[p].baseColorFactor)) == 0);
	}

	// Two builds write byte-identical multi GLB files.
	CHECK(Gltf_WriteFileMulti("world_multi_a.glb", first, error, sizeof(error)) == 0);
	CHECK(Gltf_WriteFileMulti("world_multi_b.glb", second, error, sizeof(error)) == 0);
	std::vector<unsigned char> bytesA, bytesB;
	CHECK(ReadFileBytes("world_multi_a.glb", &bytesA));
	CHECK(ReadFileBytes("world_multi_b.glb", &bytesB));
	CHECK(!bytesA.empty() && bytesA.size() == bytesB.size());
	CHECK(bytesA.size() == bytesB.size() && memcmp(bytesA.data(), bytesB.data(), bytesA.size()) == 0);

	// The built mesh reloads through the multi importer with the same primitives.
	GltfMultiMeshData loaded;
	CHECK(Gltf_LoadFileMulti("world_multi_a.glb", &loaded, error, sizeof(error)) == 0);
	CHECK(loaded.primitives.size() == first.primitives.size());
	for (size_t p = 0; p < first.primitives.size(); ++p)
	{
		CHECK(loaded.primitives[p].positions.size() == first.primitives[p].positions.size());
		CHECK(memcmp(loaded.primitives[p].positions.data(), first.primitives[p].positions.data(),
			first.primitives[p].positions.size() * sizeof(float)) == 0);
		CHECK(loaded.primitives[p].indices.size() == first.primitives[p].indices.size());
		CHECK(memcmp(loaded.primitives[p].indices.data(), first.primitives[p].indices.data(),
			first.primitives[p].indices.size() * sizeof(unsigned int)) == 0);
		CHECK(memcmp(loaded.primitives[p].baseColorFactor, first.primitives[p].baseColorFactor,
			sizeof(first.primitives[p].baseColorFactor)) == 0);
	}
}

// The multi path attaches an optional baked image per submesh: submesh 0 is
// textured, submesh 1 is untextured. The image is copied verbatim into the
// matching primitive, two builds write byte-identical multi GLB, and the image
// survives the multi GLB round trip.
static void TestMultiSubmeshImages()
{
	SourceMesh source = MakeMultiSubmeshMesh();
	const int width = 2, height = 2;
	std::vector<unsigned char> image((size_t)width * height * 4);
	for (size_t i = 0; i < image.size(); ++i)
		image[i] = (unsigned char)((i * 11 + 5) & 0xff);

	WorldSubmeshImage images[2];
	images[0].rgba = image.data();
	images[0].width = width;
	images[0].height = height;
	images[0].byteCount = image.size();
	images[1].rgba = NULL;
	images[1].width = 0;
	images[1].height = 0;
	images[1].byteCount = 0;

	GltfMultiMeshData mesh;
	WorldMeshBuilderResult result;
	char error[256];
	CHECK(WorldMeshBuilder_BuildMulti(source, &mesh, &result, error, sizeof(error),
		images, 2) == WORLD_MESH_ERROR_NONE);
	CHECK(result.error == WORLD_MESH_ERROR_NONE);
	CHECK(mesh.primitives.size() == 2);
	CHECK(mesh.primitives[0].imageWidth == width && mesh.primitives[0].imageHeight == height);
	CHECK(mesh.primitives[0].imageRgba == image);
	CHECK(mesh.primitives[1].imageRgba.empty());
	CHECK(mesh.primitives[1].imageWidth == 0 && mesh.primitives[1].imageHeight == 0);

	// Determinism: two builds write byte-identical multi GLB files.
	GltfMultiMeshData second;
	WorldMeshBuilderResult secondResult;
	CHECK(WorldMeshBuilder_BuildMulti(source, &second, &secondResult, error, sizeof(error),
		images, 2) == WORLD_MESH_ERROR_NONE);
	CHECK(Gltf_WriteFileMulti("world_img_a.glb", mesh, error, sizeof(error)) == 0);
	CHECK(Gltf_WriteFileMulti("world_img_b.glb", second, error, sizeof(error)) == 0);
	std::vector<unsigned char> bytesA, bytesB;
	CHECK(ReadFileBytes("world_img_a.glb", &bytesA));
	CHECK(ReadFileBytes("world_img_b.glb", &bytesB));
	CHECK(!bytesA.empty() && bytesA.size() == bytesB.size());
	CHECK(bytesA.size() == bytesB.size() && memcmp(bytesA.data(), bytesB.data(), bytesA.size()) == 0);

	// The image survives the multi GLB round trip.
	GltfMultiMeshData loaded;
	CHECK(Gltf_LoadFileMulti("world_img_a.glb", &loaded, error, sizeof(error)) == 0);
	CHECK(loaded.primitives.size() == 2);
	if (loaded.primitives.size() == 2)
	{
		CHECK(loaded.primitives[0].imageWidth == width && loaded.primitives[0].imageHeight == height);
		CHECK(loaded.primitives[0].imageRgba == image);
		CHECK(loaded.primitives[1].imageRgba.empty());
	}
}

static void TestImageErrors()
{
	char error[256];
	WorldMeshBuilderResult result;
	GltfMultiMeshData mesh;
	SourceMesh source = MakeMultiSubmeshMesh();
	const unsigned char pixel[4] = { 1, 2, 3, 4 };
	WorldSubmeshImage images[2];

	// An image array length that does not match the submesh count.
	images[0].rgba = pixel;
	images[0].width = 1;
	images[0].height = 1;
	images[0].byteCount = 4;
	CHECK(WorldMeshBuilder_BuildMulti(source, &mesh, &result, error, sizeof(error), images, 1)
		== WORLD_MESH_ERROR_ARGUMENT);
	CHECK(mesh.primitives.empty());
	CHECK(strcmp(WorldMeshBuilder_ErrorName(result.error), "argument") == 0);

	// A byte count that disagrees with the dimensions.
	images[0].rgba = pixel;
	images[0].width = 2;
	images[0].height = 2;
	images[0].byteCount = 4;
	images[1].rgba = NULL;
	images[1].width = 0;
	images[1].height = 0;
	images[1].byteCount = 0;
	CHECK(WorldMeshBuilder_BuildMulti(source, &mesh, &result, error, sizeof(error), images, 2)
		== WORLD_MESH_ERROR_IMAGE);
	CHECK(mesh.primitives.empty());
	CHECK(strcmp(WorldMeshBuilder_ErrorName(result.error), "image") == 0);
	CHECK(error[0] != '\0');

	// Non-positive dimensions.
	images[0].rgba = pixel;
	images[0].width = 0;
	images[0].height = 1;
	images[0].byteCount = 0;
	CHECK(WorldMeshBuilder_BuildMulti(source, &mesh, &result, error, sizeof(error), images, 2)
		== WORLD_MESH_ERROR_IMAGE);
	CHECK(mesh.primitives.empty());
}

// P2: explicit per-vertex colours become an RGBA8 COLOR_0 attribute and the
// base-colour factor becomes the neutral white, so the renderer's
// texel * vertexColour product is the only modulation. Two builds are
// byte-identical and the colour survives the multi GLB round trip.
static void TestMultiVertexColors()
{
	SourceMesh source = MakeMultiSubmeshMesh();
	const int vertexCount = (int)(source.positions.size() / 3);
	std::vector<unsigned char> colors((size_t)vertexCount * 3);
	for (int v = 0; v < vertexCount; ++v)
	{
		colors[(size_t)v * 3 + 0] = (unsigned char)(10 + v);
		colors[(size_t)v * 3 + 1] = (unsigned char)(20 + v);
		colors[(size_t)v * 3 + 2] = (unsigned char)(30 + v);
	}

	GltfMultiMeshData mesh;
	WorldMeshBuilderResult result;
	char error[256];
	CHECK(WorldMeshBuilder_BuildMulti(source, &mesh, &result, error, sizeof(error),
		NULL, 0, colors.data(), colors.size()) == WORLD_MESH_ERROR_NONE);
	CHECK(result.error == WORLD_MESH_ERROR_NONE);
	CHECK(mesh.primitives.size() == 2);

	for (size_t p = 0; p < mesh.primitives.size(); ++p)
	{
		// One RGBA colour per expanded corner, neutral base colour.
		CHECK(mesh.primitives[p].colors.size() == (size_t)mesh.primitives[p].vertexCount * 4);
		CHECK(NearlyEqual(mesh.primitives[p].baseColorFactor[0], 1.0f));
		CHECK(NearlyEqual(mesh.primitives[p].baseColorFactor[1], 1.0f));
		CHECK(NearlyEqual(mesh.primitives[p].baseColorFactor[2], 1.0f));
		CHECK(NearlyEqual(mesh.primitives[p].baseColorFactor[3], 1.0f));
	}

	// Primitive 0's first corner is source vertex 0 (indices 0,1,2,0,2,3).
	CHECK(mesh.primitives[0].colors[0] == 10 && mesh.primitives[0].colors[1] == 20 &&
		mesh.primitives[0].colors[2] == 30 && mesh.primitives[0].colors[3] == 255);
	// The per-polygon colour stays flat: both triangles of the quad repeat it.
	CHECK(mesh.primitives[0].colors[0] == mesh.primitives[0].colors[3 * 4 + 0]);

	// A byte count that does not match the vertex count is rejected, zero output.
	{
		GltfMultiMeshData rejected;
		CHECK(WorldMeshBuilder_BuildMulti(source, &rejected, &result, error, sizeof(error),
			NULL, 0, colors.data(), colors.size() - 1) == WORLD_MESH_ERROR_ATTRIBUTE_SIZE);
		CHECK(rejected.primitives.empty());
	}

	// Determinism and the multi GLB round trip.
	GltfMultiMeshData second;
	CHECK(WorldMeshBuilder_BuildMulti(source, &second, &result, error, sizeof(error),
		NULL, 0, colors.data(), colors.size()) == WORLD_MESH_ERROR_NONE);
	CHECK(Gltf_WriteFileMulti("world_colors_a.glb", mesh, error, sizeof(error)) == 0);
	CHECK(Gltf_WriteFileMulti("world_colors_b.glb", second, error, sizeof(error)) == 0);
	std::vector<unsigned char> bytesA, bytesB;
	CHECK(ReadFileBytes("world_colors_a.glb", &bytesA));
	CHECK(ReadFileBytes("world_colors_b.glb", &bytesB));
	CHECK(!bytesA.empty() && bytesA.size() == bytesB.size() &&
		memcmp(bytesA.data(), bytesB.data(), bytesA.size()) == 0);

	GltfMultiMeshData loaded;
	CHECK(Gltf_LoadFileMulti("world_colors_a.glb", &loaded, error, sizeof(error)) == 0);
	CHECK(loaded.primitives.size() == mesh.primitives.size());
	for (size_t p = 0; p < mesh.primitives.size(); ++p)
		CHECK(loaded.primitives[p].colors == mesh.primitives[p].colors);
}

// Decode actual cyclic MODEL records before BuildMulti, rather than supplying
// an already-triangulated fixture that could conceal a decoder topology error.
static void TestDecodedCyclicQuadCoverage()
{
	const unsigned char types[] = { 11, 21, 23 };
	for (int t = 0; t < 3; ++t)
	{
		std::vector<unsigned char> bytes(72, 0);
		bytes[16] = 1; // B=(1,0,0)
		bytes[24] = 1; bytes[26] = 1; // C=(1,1,0)
		bytes[34] = 1; // D=(0,1,0)
		for (int n = 0; n < 4; ++n)
			bytes[40 + n * 8 + 5] = 16; // +Z point normal, 4096
		const int stride = types[t] == 11 ? 16 : types[t] == 21 ? 20 : 24;
		bytes.resize(72 + stride, 0);
		unsigned char* record = &bytes[72];
		record[0] = types[t]; record[1] = 2; record[2] = 5; record[3] = 0x87;
		for (int v = 0; v < 4; ++v)
		{
			record[4 + v] = (unsigned char)v;
			if (types[t] == 23) record[8 + v] = (unsigned char)v;
			const int uvOffset = types[t] == 23 ? 12 : 8;
			record[uvOffset + v * 2] = (unsigned char)(v * 2 + 1);
			record[uvOffset + v * 2 + 1] = (unsigned char)(v * 2 + 2);
		}
		if (types[t] != 11)
		{
			const int rgbOffset = types[t] == 23 ? 20 : 16;
			record[rgbOffset] = 10; record[rgbOffset + 1] = 20; record[rgbOffset + 2] = 30;
		}
		SourceModelView view = {};
		view.instanceNumber = -1; view.numVertices = 4; view.numPolys = 1;
		view.vertices = 8; view.pointNormals = 40; view.numPointNormals = 4; view.polyBlock = 72;
		SourceMesh source;
		char error[256] = "";
		CHECK(DecodeSourceModel(bytes.data(), bytes.data() + bytes.size(), view,
			&source, error, sizeof(error)).error == SOURCE_MODEL_ERROR_NONE);
		// HashSourceMesh includes these exact index bytes in the world cache
		// source hash: corrected indices cannot reuse the old topology's key.
		const unsigned int oldIndices[] = { 0,1,2, 0,2,3 };
		CHECK(ConversionCache_HashBytes(source.indices.data(), (int)(source.indices.size() * sizeof(unsigned int))) !=
			ConversionCache_HashBytes(oldIndices, sizeof(oldIndices)));
		GltfMultiMeshData mesh;
		WorldMeshBuilderResult result;
		CHECK(WorldMeshBuilder_BuildMulti(source, &mesh, &result, error, sizeof(error)) == WORLD_MESH_ERROR_NONE);
		CHECK(mesh.primitives.size() == 1);
		const GltfPrimitiveData& primitive = mesh.primitives[0];
		CHECK(primitive.vertexCount == 6 && primitive.triangleCount == 2);
		for (int v = 0; v < 6; ++v)
		{
			const unsigned int original = source.indices[v];
			CHECK(NearlyEqual(primitive.normals[v * 3 + 2], 1.0f));
			CHECK(NearlyEqual(primitive.uvs[v * 2], source.uvs[original * 2]));
			CHECK(NearlyEqual(primitive.uvs[v * 2 + 1], source.uvs[original * 2 + 1]));
		}
		const float samples[][2] = { {0.1f,0.5f}, {0.9f,0.5f}, {0.2f,0.1f}, {0.8f,0.95f} };
		for (int s = 0; s < 4; ++s)
		{
			int coverage = 0;
			for (int i = 0; i < 6; i += 3)
			{
				float edges[3];
				for (int e = 0; e < 3; ++e)
				{
					const float* a = &primitive.positions[primitive.indices[i + e] * 3];
					const float* b = &primitive.positions[primitive.indices[i + (e + 1) % 3] * 3];
					edges[e] = (b[0] - a[0]) * (samples[s][1] - a[1]) -
						(b[1] - a[1]) * (samples[s][0] - a[0]);
				}
				if ((edges[0] > 0 && edges[1] > 0 && edges[2] > 0) ||
					(edges[0] < 0 && edges[1] < 0 && edges[2] < 0)) coverage++;
			}
			CHECK(coverage == 1);
		}
	}
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
	TestMultiSubmeshBuild();
	TestMultiMixedColors();
	TestMultiErrors();
	TestMultiDeterminismAndReload();
	TestMultiSubmeshImages();
	TestImageErrors();
	TestMultiVertexColors();
	TestDecodedCyclicQuadCoverage();

	printf("%d checks, %d failures\n", g_checks, g_failures);
	return g_failures == 0 ? 0 : 1;
}
