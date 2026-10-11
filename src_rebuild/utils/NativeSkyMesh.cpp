#include "NativeSkyMesh.h"
#include "TextureDecode.h"
#include <stdint.h>
#include <stdio.h>
#include <math.h>
#include <cmath>

unsigned NativeSkyMesh_ModelStride(unsigned type)
{
	// draw.c PolySizes; sky.c supplies UVs/colors separately from the records.
	return type == 11 ? 16 : (type == 5 || type == 21) ? 20 : 0;
}

bool NativeSkyMesh_CopyModel(const NativeSkyModelInput& input, std::vector<NativeSkyQuad>* quads,
	short gate[3], char* error, int capacity)
{
	if (quads) quads->clear();
	if (gate) for (int c = 0; c < 3; ++c) gate[c] = 0;
	const auto fail = [&](const char* message) {
		if (quads) quads->clear();
		if (error && capacity > 0) snprintf(error, capacity, "%s", message);
		return false;
	};
	if (!quads || !gate || !input.vertices || !input.polygons || !input.textures ||
		input.model >= 4 || input.vertexCount < 15 || input.vertexCount > 256 || input.vertexCount % 3 ||
		input.polygonCount <= 8 || input.polygonCount > 40 || input.textureOffset > input.textureCount ||
		input.polygonCount > input.textureCount - input.textureOffset || input.vertexBytes < input.vertexCount * 8u || !input.polygonBytes)
		return fail("invalid original horizon spans/counts/selectors");
	const unsigned type = input.polygons[0], stride = NativeSkyMesh_ModelStride(type);
	if (!stride || input.polygonBytes < input.polygonCount * stride) return fail("original horizon polygon span/type is invalid");
	const auto read = [&](unsigned vertex, int axis) {
		const unsigned char* bytes = input.vertices + vertex * 8 + axis * 2;
		const int value = bytes[0] | (bytes[1] << 8);
		return (short)(value >= 32768 ? value - 65536 : value);
	};
	for (unsigned q = 0; q < input.polygonCount; ++q)
	{
		const unsigned char* record = input.polygons + q * stride;
		if (record[0] != type || input.textures[input.textureOffset + q] >= 28) return fail("original horizon record/tile is invalid");
		NativeSkyQuad quad = {}; quad.model = input.model; quad.tile = input.textures[input.textureOffset + q];
		quad.reflection = q >= input.polygonCount - 8;
		for (int c = 0; c < 4; ++c)
		{
			const unsigned vertex = record[4 + c];
			if (vertex >= input.vertexCount) return fail("original horizon vertex index is invalid");
			for (int axis = 0; axis < 3; ++axis) quad.positions[c * 3 + axis] = (float)read(vertex, axis);
		}
		quads->push_back(quad);
	}
	for (int c = 0; c < 3; ++c) gate[c] = read(input.vertexCount - 13, c);
	return true;
}

void NativeSkyEpoch::Begin() { ++frame; view = -1; captured = NativeSkyFrame(); }
void NativeSkyEpoch::Select(int selectedView) { view = selectedView; captured = NativeSkyFrame(); }
bool NativeSkyEpoch::Consume(int selectedView, unsigned generation, unsigned revision, NativeSkyFrame* out)
{
	if (!out) return false;
	*out = captured; // screenshot and present may read the same current frame
	return out->selected && out->epoch == frame && out->view == view && view == selectedView &&
		out->generation == generation && out->revision == revision;
}
void NativeSkyMesh_Color(const int rgb[3], bool reflection, float out[3])
{
	for (int c = 0; c < 3; ++c) out[c] = (float)(reflection ? rgb[c] / 2 : rgb[c]) * (2.0f / 255.0f);
}
void NativeSkyMesh_Pose(const short rotation[3][3], const short gates[4][3], int yOffset, NativeSkyFrame* out)
{
	for (int i = 0; i < 16; ++i) out->rotation[i] = 0;
	for (int row = 0; row < 3; ++row) for (int col = 0; col < 3; ++col)
		out->rotation[col * 4 + row] = rotation[row][col] / 4096.0f;
	out->rotation[13] = (float)yOffset; out->rotation[15] = 1;
	for (int m = 0; m < 4; ++m)
	{
		int64_t z = 0;
		for (int c = 0; c < 3; ++c) z += (int64_t)rotation[2][c] * gates[m][c];
		// gte_stszotz stores saturated SZ3 >> 2. The original gate is > 0.
		out->enabled[m] = (z >> 12) >= 4 ? 1 : 0;
	}
}
namespace {
bool Fail(WorldMeshConversionInput* out, char* error, int capacity, const char* message)
{
	if (out) *out = WorldMeshConversionInput();
	if (error && capacity > 0) snprintf(error, capacity, "%s", message);
	return false;
}
bool Decode(const NativeSkyAssets& assets, const NativeSkyTile& tile, std::vector<unsigned char>* pixels, char* error, int capacity)
{
	const int x = (tile.page & 15) * 64 - 320, y = (tile.page & 16) ? 256 : 0;
	const int cx = (tile.clut & 63) * 16 - 320, cy = tile.clut >> 6;
	if ((tile.page >> 7 & 3) != 0 || (x != 0 && x != 64) || y != 0 || cx < 0 || cx > 112 || cy < 252 || cy > 254)
		return Fail(nullptr, error, capacity, "original sky page/CLUT is outside the loaded sky block");
	pixels->resize(256u * 256u * 4u);
	unsigned char palette[64]; TextureDecodeRequest request = {};
	request.format = TEXTURE_DECODE_FORMAT_4BIT_INDEXED;
	request.page = assets.raw.data() + x * 2; request.pageBytes = (int)assets.raw.size() - x * 2;
	request.widthTexels = request.heightTexels = 256; request.rowStrideHalfwords = 128;
	request.clut = assets.raw.data() + (cy * 128 + cx) * 2; request.clutBytes = 32;
	request.paletteEntryCount = 16; request.paletteVariantCount = 1;
	request.paletteOut = palette; request.paletteCapacity = sizeof(palette);
	request.rgbaOut = pixels->data(); request.rgbaCapacity = (int)pixels->size();
	TextureDecodeResult result = {};
	return TextureDecode_Decode(&request, &result, error, capacity) == TEXTURE_DECODE_OK;
}
}
bool NativeSkyMesh_Build(const NativeSkyAssets& assets, WorldMeshConversionInput* out, char* error, int capacity)
{
	if (error && capacity > 0) error[0] = 0;
	if (!out || assets.raw.size() != 65536 || assets.quads.empty() || assets.quads.size() > 160)
		return Fail(out, error, capacity, "invalid original sky inventory (64 KiB, 1..160 quads required)");
	*out = WorldMeshConversionInput();
	std::vector<int> materials;
	for (size_t q = 0; q < assets.quads.size(); ++q)
	{
		const NativeSkyQuad& quad = assets.quads[q];
		if (quad.tile >= 28 || quad.model >= 4) return Fail(out, error, capacity, "original sky selector is out of range");
		for (int c = 0; c < 12; ++c) if (!std::isfinite(quad.positions[c])) return Fail(out, error, capacity, "non-finite original sky position");
		bool found = false;
		for (size_t m = 0; m < materials.size(); ++m)
			if (assets.tiles[materials[m]].page == assets.tiles[quad.tile].page && assets.tiles[materials[m]].clut == assets.tiles[quad.tile].clut) found = true;
		if (!found) materials.push_back(quad.tile);
	}
	if (materials.size() > 12) return Fail(out, error, capacity, "original sky decoded page capacity exceeds 3 MiB");
	// GPU quad order is source 0,1,3,2; its two triangles are 012/132.
	const int corners[6] = { 0, 1, 3, 1, 2, 3 }, texCorners[6] = { 2, 3, 0, 3, 1, 0 };
	for (size_t m = 0; m < materials.size(); ++m)
	{
		const NativeSkyTile& material = assets.tiles[materials[m]];
		SourceSubmesh submesh = {}; submesh.primitiveClass = SourcePrimitiveClass::TexturedFlatTriangle;
		submesh.textureId = (unsigned char)m; submesh.firstIndex = (unsigned)out->source.indices.size();
		for (size_t q = 0; q < assets.quads.size(); ++q)
		{
			const NativeSkyQuad& quad = assets.quads[q]; const NativeSkyTile& tile = assets.tiles[quad.tile];
			if (tile.page != material.page || tile.clut != material.clut) continue;
			for (int c = 0; c < 6; ++c)
			{
				out->source.indices.push_back((unsigned)out->source.positions.size() / 3);
				for (int axis = 0; axis < 3; ++axis) out->source.positions.push_back(quad.positions[corners[c] * 3 + axis]);
				for (int axis = 0; axis < 2; ++axis) out->source.uvs.push_back(tile.uv[texCorners[c] * 2 + axis]);
				out->legacyColors.insert(out->legacyColors.end(), 3, quad.reflection ? 64 : 128);
				// Sky-only shader uses this immutable tag for original per-model eligibility.
				out->worldShadeIndices.push_back((float)quad.model);
			}
		}
		submesh.indexCount = (unsigned)out->source.indices.size() - submesh.firstIndex;
		out->source.submeshes.push_back(submesh);
		std::vector<unsigned char> pixels;
		if (!Decode(assets, material, &pixels, error, capacity)) { *out = WorldMeshConversionInput(); return false; }
		WorldConversionImage image = { (int)out->pixels.size(), 256, 256 };
		out->images.push_back(image); out->pixels.push_back(std::move(pixels));
	}
	return true;
}
