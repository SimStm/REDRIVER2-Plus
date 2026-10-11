#include "NativeCarMesh.h"
#include "TextureDecode.h"

#include <stdio.h>
#include <string.h>
#include <utility>
#include <cmath>

namespace
{
bool Fail(WorldMeshConversionInput* out, char* error, int capacity, const char* detail)
{
	if (out) *out = WorldMeshConversionInput();
	if (error && capacity > 0) snprintf(error, capacity, "%s", detail);
	return false;
}

bool DecodeCarPage(const std::vector<unsigned short>& vram, unsigned short page,
	unsigned short clut, std::vector<unsigned char>& pixels, char* error, int capacity)
{
	const int format = (page >> 7) & 3;
	const int x = (page & 15) * 64, y = (page & 16) ? 256 : 0;
	const int rowWords = format == 0 ? 64 : format == 1 ? 128 : 256;
	const int entries = format == 0 ? 16 : format == 1 ? 256 : 0;
	const int cx = (clut & 63) * 16, cy = clut >> 6;
	if (format == 3 || x + rowWords > 1024 ||
		(entries && (cx + entries > 1024 || cy >= 512)))
	{
		if (error && capacity > 0) snprintf(error, capacity, "car page/CLUT is outside original VRAM");
		return false;
	}
	const size_t offset = (size_t)y * 1024 + x;
	unsigned char palette[1024];
	pixels.resize(256u * 256u * 4u);
	TextureDecodeRequest request = {};
	request.format = (TextureDecodeFormat)format;
	request.page = (const unsigned char*)vram.data() + offset * 2;
	request.pageBytes = (int)((vram.size() - offset) * 2);
	request.widthTexels = request.heightTexels = 256;
	request.rowStrideHalfwords = 1024;
	if (entries)
	{
		request.clut = (const unsigned char*)vram.data() + ((size_t)cy * 1024 + cx) * 2;
		request.clutBytes = entries * 2;
		request.paletteEntryCount = entries;
		request.paletteVariantCount = 1;
		request.paletteOut = palette;
		request.paletteCapacity = entries * 4;
	}
	request.rgbaOut = pixels.data();
	request.rgbaCapacity = (int)pixels.size();
	TextureDecodeResult result = {};
	return TextureDecode_Decode(&request, &result, error, capacity) == TEXTURE_DECODE_OK;
}

float AtlasUv(unsigned texel, bool expandedAxis, bool carry)
{
	// Raw renderer UVs are always divided by256. Replicated one-pixel cell
	// gutters preserve clamp-edge behavior with either nearest or bilinear.
	return expandedAxis ? (float)(1 + texel + (carry ? 258 : 0)) * (256.0f / 516.0f) : (float)texel;
}
}

bool NativeCarMesh_SameSource(const NativeCarSourceIdentity& a, const NativeCarSourceIdentity& b)
{
	return a.model == b.model && a.palette == b.palette && a.revision == b.revision &&
		a.cleanModel == b.cleanModel && a.lowModel == b.lowModel;
}

bool NativeCarMesh_IsCurrentPublication(const NativeCarSourceIdentity& current,
	const NativeCarSourceIdentity& captured, unsigned int currentGeneration, unsigned int capturedGeneration)
{
	return currentGeneration == capturedGeneration && NativeCarMesh_SameSource(current, captured);
}

bool NativeCarMesh_Capture(const std::vector<float>& positions,
	const std::vector<NativeCarTriangle>& triangles, const std::vector<unsigned short>& vram,
	WorldMeshConversionInput* out, char* error, int capacity,
	std::vector<NativeCarCornerBinding>* bindings)
{
	if (bindings) bindings->clear();
	if (error && capacity > 0) error[0] = 0;
	if (!out || positions.empty() || positions.size() % 3 || positions.size() > 256u * 3u ||
		triangles.empty() || triangles.size() > 1024 || vram.size() != 1024u * 512u)
		return Fail(out, error, capacity, "invalid original car capture inventory");
	*out = WorldMeshConversionInput();
	std::vector<NativeCarCornerBinding> corners;
	struct Material { unsigned short page, clut; bool textured, pageAtlas, clutAtlas; };
	std::vector<Material> materials;
	for (size_t t = 0; t < triangles.size(); ++t)
	{
		const NativeCarTriangle& triangle = triangles[t];
		for (int c = 0; c < 3; ++c)
			if (triangle.vertices[c] >= positions.size() / 3)
				return Fail(out, error, capacity, "original car vertex index is out of range");
		const unsigned uv0 = triangle.uv[0] | (triangle.uv[1] << 8);
		const unsigned uv1 = triangle.uv[2] | (triangle.uv[3] << 8);
		const bool pageAtlas = bindings && triangle.textured && uv1 + 255u > 65535u;
		const bool clutAtlas = bindings && triangle.textured && uv0 + 255u > 65535u &&
			(!triangle.paletteClut || !(triangle.clut & 1));
		bool found = false;
		for (size_t m = 0; m < materials.size(); ++m)
			if (materials[m].textured == triangle.textured && (!triangle.textured ||
				(materials[m].page == triangle.tpage && materials[m].clut == triangle.clut)))
			{
				found = true;
				materials[m].pageAtlas |= pageAtlas;
				materials[m].clutAtlas |= clutAtlas;
			}
		if (!found)
		{
			if (materials.size() == 64) return Fail(out, error, capacity, "original car material capacity reached");
			Material material = { triangle.tpage, triangle.clut, triangle.textured, pageAtlas, clutAtlas };
			materials.push_back(material);
		}
	}
	size_t pixelBytes = 0;
	for (size_t m = 0; m < materials.size(); ++m)
		if (materials[m].textured) pixelBytes += (materials[m].pageAtlas ? 516u : 256u) *
			(materials[m].clutAtlas ? 516u : 256u) * 4u;
	if (pixelBytes > 16u * 1024u * 1024u)
		return Fail(out, error, capacity, "original car decoded pixel capacity reached");
	for (size_t m = 0; m < materials.size(); ++m)
	{
		const Material& material = materials[m];
		SourceSubmesh submesh = {};
		submesh.primitiveClass = SourcePrimitiveClass::TexturedFlatTriangle;
		submesh.textureId = (unsigned char)m;
		submesh.firstIndex = (unsigned int)out->source.indices.size();
		for (size_t t = 0; t < triangles.size(); ++t)
		{
			const NativeCarTriangle& triangle = triangles[t];
			if (triangle.textured != material.textured || (material.textured &&
				(triangle.tpage != material.page || triangle.clut != material.clut))) continue;
			for (int c = 0; c < 3; ++c)
			{
				NativeCarCornerBinding binding = { triangle.vertices[c], triangle.originalIndex,
					(unsigned short)(triangle.uv[c * 2] | (triangle.uv[c * 2 + 1] << 8)), triangle.textured,
					(unsigned short)(triangle.uv[0] | (triangle.uv[1] << 8)),
					(unsigned short)(triangle.uv[2] | (triangle.uv[3] << 8)), triangle.clut,
					triangle.paletteClut, material.pageAtlas, material.clutAtlas };
				corners.push_back(binding);
				out->source.indices.push_back((unsigned int)out->source.positions.size() / 3);
				for (int axis = 0; axis < 3; ++axis)
					out->source.positions.push_back(positions[triangle.vertices[c] * 3 + axis]);
				out->source.uvs.push_back(AtlasUv(triangle.uv[c * 2], material.pageAtlas, false));
				out->source.uvs.push_back(AtlasUv(triangle.uv[c * 2 + 1], material.clutAtlas, false));
				// Explicit neutral prelit body baseline. Original per-normal/day/night
				// lighting is a separate actor-material follow-up, not world shading.
				out->legacyColors.insert(out->legacyColors.end(), 3, material.textured ? 128 : 40);
			}
		}
		submesh.indexCount = (unsigned int)out->source.indices.size() - submesh.firstIndex;
		out->source.submeshes.push_back(submesh);
		WorldConversionImage image = { -1, 0, 0 };
		if (material.textured)
		{
			image.width = material.pageAtlas ? 516 : 256;
			image.height = material.clutAtlas ? 516 : 256;
			std::vector<unsigned char> pixels((size_t)image.width * image.height * 4);
			const int cellWidth = material.pageAtlas ? 258 : 256;
			const int cellHeight = material.clutAtlas ? 258 : 256;
			for (int cy = 0; cy < (material.clutAtlas ? 2 : 1); ++cy)
			for (int px = 0; px < (material.pageAtlas ? 2 : 1); ++px)
			{
				std::vector<unsigned char> page;
				if (!DecodeCarPage(vram, (unsigned short)(material.page + px), (unsigned short)(material.clut + cy), page, error, capacity))
				{
					*out = WorldMeshConversionInput();
					return false;
				}
				for (int row = 0; row < cellHeight; ++row)
				for (int col = 0; col < cellWidth; ++col)
				{
					int y = row - (material.clutAtlas ? 1 : 0), x = col - (material.pageAtlas ? 1 : 0);
					if (y < 0) y = 0; if (y > 255) y = 255;
					if (x < 0) x = 0; if (x > 255) x = 255;
					memcpy(pixels.data() + ((size_t)(cy * cellHeight + row) * image.width + px * cellWidth + col) * 4,
						page.data() + ((size_t)y * 256 + x) * 4, 4);
				}
			}
			image.pixelIndex = (int)out->pixels.size();
			out->pixels.push_back(std::move(pixels));
		}
		out->images.push_back(image);
	}
	if (bindings) *bindings = std::move(corners);
	return true;
}

bool NativeCarMesh_ExpandDamage(const NativeCarBodyCapture& capture,
	const std::vector<float>& positions, const std::vector<unsigned char>& damage,
	std::vector<float>* xyz, std::vector<float>* uvs, char* error, int capacity)
{
	if (error && capacity > 0) error[0] = 0;
	if (xyz) xyz->clear();
	if (uvs) uvs->clear();
	const std::vector<NativeCarCornerBinding>& bindings = capture.bindings;
	if (!xyz || !uvs || positions.empty() || positions.size() % 3 || positions.size() > 256u * 3u ||
		bindings.empty() || bindings.size() > 3072 || bindings.size() * 3 != capture.conversion.source.positions.size())
	{
		if (error && capacity > 0) snprintf(error, capacity, "invalid original car damage inventory");
		return false;
	}
	for (size_t v = 0; v < positions.size(); ++v)
		if (!std::isfinite(positions[v]))
		{
			if (error && capacity > 0) snprintf(error, capacity, "non-finite original car damage position");
			return false;
		}
	for (size_t c = 0; c < bindings.size(); ++c)
		if (bindings[c].vertex >= positions.size() / 3 ||
			(bindings[c].textured && bindings[c].originalIndex >= damage.size()))
		{
			if (error && capacity > 0) snprintf(error, capacity, "original car damage binding is out of range");
			return false;
		}
	xyz->reserve(bindings.size() * 3); uvs->reserve(bindings.size() * 2);
	for (size_t c = 0; c < bindings.size(); ++c)
	{
		const NativeCarCornerBinding& binding = bindings[c];
		for (int axis = 0; axis < 3; ++axis) xyz->push_back(positions[binding.vertex * 3 + axis]);
		// The original plotters add the unsigned per-polygon byte to packed U/V.
		const unsigned uv = binding.uv + (binding.textured ? damage[binding.originalIndex] : 0);
		const unsigned offset = binding.textured ? damage[binding.originalIndex] : 0;
		const bool pageCarry = (binding.uv1 + offset) > 65535u;
		const bool clutCarry = (binding.uv0 + offset) > 65535u && (!binding.paletteClut || !(binding.clut & 1));
		uvs->push_back(AtlasUv(uv & 255, binding.pageAtlas, pageCarry));
		uvs->push_back(AtlasUv((uv >> 8) & 255, binding.clutAtlas, clutCarry));
	}
	return true;
}

void NativeCarMesh_ComposeTransform(const int position[3], const short rotation[3][3],
	const short cog[3], bool miniature, float out[16])
{
	memset(out, 0, 16 * sizeof(float));
	for (int row = 0; row < 3; ++row)
	{
		float translation = (float)(row == 1 ? -position[row] : position[row]);
		for (int col = 0; col < 3; ++col)
		{
			// Match the original arithmetic right shift for miniature cars.
			const int value = (row == 1 ? rotation[row][col] : -rotation[row][col]);
			const float draw = (float)(miniature ? value >> 1 : value) / 4096.0f;
			out[col * 4 + row] = draw;
			translation += draw * (float)(miniature ? cog[col] * 2 : cog[col]);
		}
		out[12 + row] = translation;
	}
	out[15] = 1.0f;
}
