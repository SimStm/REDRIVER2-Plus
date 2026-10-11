#include "../utils/WorldMeshConversion.h"
#include <direct.h>
#include <future>
#include <memory>
#include <stdio.h>
#include <string.h>

static int checks = 0, failures = 0;
static void Check(bool passed, const char* description)
{
	++checks;
	if (!passed)
	{
		++failures;
		printf("FAIL: %s\n", description);
	}
}

static WorldMeshConversionInput MakeInput()
{
	WorldMeshConversionInput input;
	input.source.positions = {0, 0, 0, 100, 0, 0, 0, 100, 0};
	input.source.uvs = {0, 0, 1, 0, 0, 1};
	input.source.shadeIndices = {0, 0, 0};
	input.source.indices = {0, 1, 2};
	SourceSubmesh submesh = {};
	submesh.primitiveClass = SourcePrimitiveClass::TexturedFlatTriangle;
	submesh.indexCount = 3;
	input.source.submeshes.push_back(submesh);
	input.pixels.push_back({255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 255, 255});
	input.images.push_back({0, 2, 2});
	input.legacyColors = {128, 128, 128, 128, 128, 128, 128, 128, 128};
	return input;
}

static void AppendNumber(std::vector<unsigned char>& bytes, unsigned long long value, int size)
{
	for (int i = 0; i < size; ++i)
		bytes.push_back((unsigned char)(value >> (8 * i)));
}

int main()
{
	_mkdir("world-cache");
	WorldMeshConversionInput input = MakeInput();
	WorldMeshConversionResult first = {};
	char error[256];
	Check(WorldMeshConversion_Publish("world-cache", input, &first, error, sizeof(error)) == ASSET_CONVERTER_OK,
		  "owned textured snapshot is published");
	GltfMultiMeshData loaded;
	Check(Gltf_LoadFileMulti(first.asset.glbPath, &loaded, error, sizeof(error)) == 0, "published artifact loads");
	Check(loaded.primitives.size() == 1 && loaded.primitives[0].imageRgba == input.pixels[0],
		  "snapshot pixel ownership survives building and publication");
	Check(first.built.vertexCount == 3 && first.built.triangleCount == 1, "geometry survives conversion");
	// Compatibility with the previous concatenated image/legacy-colour key.
	std::vector<unsigned char> imageBytes(1, 1);
	AppendNumber(imageBytes, 2, 4);
	AppendNumber(imageBytes, 2, 4);
	AppendNumber(imageBytes, 16, 4);
	imageBytes.insert(imageBytes.end(), input.pixels[0].begin(), input.pixels[0].end());
	const unsigned long long imageHash = ConversionCache_HashBytes(imageBytes.data(), (int)imageBytes.size());
	std::vector<unsigned char> options(1, 0xA5);
	AppendNumber(options, imageHash, 8);
	options.insert(options.end(), input.legacyColors.begin(), input.legacyColors.end());
	ConversionCacheKey expected;
	ConversionCache_MakeKey(&expected, "world-mesh", first.sourceHash,
							ConversionCache_HashBytes(options.data(), (int)options.size()), 0,
							CONVERSION_CACHE_SCHEMA_VERSION);
	char expectedKey[CONVERSION_CACHE_KEY_CAPACITY];
	ConversionCache_FormatKey(&expected, expectedKey, sizeof(expectedKey));
	Check(!strcmp(expectedKey, first.resourceKey), "existing image/colour cache keys remain compatible");

	// Both workers attempt the same missing artifact. One writes and one reuses;
	// serial publication prevents sharing a live .tmp file or removing a peer's output.
	remove(first.asset.glbPath);
	std::promise<void> release;
	std::shared_future<void> gate = release.get_future().share();
	std::shared_ptr<const WorldMeshConversionInput> owned(new WorldMeshConversionInput(input));
	auto run = [owned, gate]() {
		gate.wait();
		WorldMeshConversionResult result = {};
		char error[256];
		if (WorldMeshConversion_Publish("world-cache", *owned, &result, error, sizeof(error)) != ASSET_CONVERTER_OK)
			result.asset.converted = -100;
		return result;
	};
	std::future<WorldMeshConversionResult> a = std::async(std::launch::async, run);
	std::future<WorldMeshConversionResult> b = std::async(std::launch::async, run);
	input = WorldMeshConversionInput(); // original caller state is gone before conversion
	Check(a.wait_for(std::chrono::seconds(0)) == std::future_status::timeout,
		  "a pending task can be polled without waiting");
	release.set_value();
	const WorldMeshConversionResult resultA = a.get(), resultB = b.get();
	Check(resultA.asset.converted + resultB.asset.converted == 1 && resultA.asset.reused + resultB.asset.reused == 1,
		  "concurrent publication writes once and reuses once");
	Check(!strcmp(resultA.resourceKey, first.resourceKey) &&
			  !strcmp(resultB.asset.contentHash, first.asset.contentHash),
		  "worker ownership preserves identity after caller destruction");

	WorldMeshConversionInput invalid = MakeInput();
	WorldMeshConversionInput shaded = MakeInput();
	shaded.worldShadeIndices = { 0, 4, -1 };
	WorldMeshConversionResult shadeA = {}, shadeB = {};
	Check(WorldMeshConversion_Publish("world-cache", shaded, &shadeA, error, sizeof(error)) == ASSET_CONVERTER_OK,
		"world shade metadata publishes");
	Check(strcmp(shadeA.resourceKey, resultA.resourceKey) != 0,
		"adding orientation metadata invalidates the old baked resource key");
	Check(Gltf_LoadFileMulti(shadeA.asset.glbPath, &loaded, error, sizeof(error)) == 0 &&
		loaded.primitives[0].worldShadeIndices == shaded.worldShadeIndices,
		"owned metadata survives cache readback");
	shaded.worldShadeIndices[0] = 31;
	Check(WorldMeshConversion_Publish("world-cache", shaded, &shadeB, error, sizeof(error)) == ASSET_CONVERTER_OK &&
		strcmp(shadeA.resourceKey, shadeB.resourceKey) != 0, "changed shade indices rebuild the resource");
	shaded.worldShadeIndices.pop_back();
	Check(WorldMeshConversion_Publish("world-cache", shaded, &shadeB, error, sizeof(error)) != ASSET_CONVERTER_OK,
		"incomplete shade metadata rejects publication");
	invalid.images[0].pixelIndex = 9;
	Check(WorldMeshConversion_Publish("world-cache", invalid, &first, error, sizeof(error)) != ASSET_CONVERTER_OK &&
			  error[0],
		  "unowned pixel reference is rejected with a diagnostic");
	invalid = MakeInput();
	invalid.pixels[0].resize(15);
	Check(WorldMeshConversion_Publish("world-cache", invalid, &first, error, sizeof(error)) != ASSET_CONVERTER_OK,
		  "wrong image byte count is rejected");
	invalid = MakeInput();
	invalid.pixels[0].resize(16u * 1024u * 1024u + 1);
	Check(WorldMeshConversion_Publish("world-cache", invalid, &first, error, sizeof(error)) != ASSET_CONVERTER_OK &&
			  error[0],
		  "snapshot pixel capacity is bounded");
	invalid = MakeInput();
	invalid.pixels.resize(65);
	Check(WorldMeshConversion_Publish("world-cache", invalid, &first, error, sizeof(error)) != ASSET_CONVERTER_OK,
		  "snapshot image count is bounded");
	invalid = MakeInput();
	invalid.pixels[0].clear();
	Check(WorldMeshConversion_Publish("world-cache", invalid, &first, error, sizeof(error)) != ASSET_CONVERTER_OK,
		"a declared textured image cannot silently become untextured when pixels are empty");
	invalid = MakeInput();
	invalid.pixels[0].resize(512 * 512 * 4);
	invalid.images.assign(65, { 0, 512, 512 });
	invalid.source.submeshes.assign(65, invalid.source.submeshes[0]);
	Check(WorldMeshConversion_Publish("world-cache", invalid, &first, error, sizeof(error)) != ASSET_CONVERTER_OK &&
		strstr(error, "expanded"), "shared images cannot expand into an unbounded mesh allocation");
	printf("%d checks, %d failures\n", checks, failures);
	return failures ? 1 : 0;
}
