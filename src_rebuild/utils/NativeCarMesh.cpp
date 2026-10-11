#include "NativeCarMesh.h"
#include "TextureDecode.h"

#include <stdio.h>
#include <string.h>
#include <utility>

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
}

bool NativeCarMesh_SameSource(const NativeCarSourceIdentity& a, const NativeCarSourceIdentity& b)
{
	return a.model == b.model && a.palette == b.palette && a.revision == b.revision && a.cleanModel == b.cleanModel;
}

bool NativeCarMesh_IsCurrentPublication(const NativeCarSourceIdentity& current,
	const NativeCarSourceIdentity& captured, unsigned int currentGeneration, unsigned int capturedGeneration)
{
	return currentGeneration == capturedGeneration && NativeCarMesh_SameSource(current, captured);
}

bool NativeCarMesh_Capture(const std::vector<float>& positions,
	const std::vector<NativeCarTriangle>& triangles, const std::vector<unsigned short>& vram,
	WorldMeshConversionInput* out, char* error, int capacity)
{
	if (error && capacity > 0) error[0] = 0;
	if (!out || positions.empty() || positions.size() % 3 || positions.size() > 256u * 3u ||
		triangles.empty() || triangles.size() > 1024 || vram.size() != 1024u * 512u)
		return Fail(out, error, capacity, "invalid original car capture inventory");
	*out = WorldMeshConversionInput();
	struct Material { unsigned short page, clut; bool textured; };
	std::vector<Material> materials;
	for (size_t t = 0; t < triangles.size(); ++t)
	{
		const NativeCarTriangle& triangle = triangles[t];
		for (int c = 0; c < 3; ++c)
			if (triangle.vertices[c] >= positions.size() / 3)
				return Fail(out, error, capacity, "original car vertex index is out of range");
		bool found = false;
		for (size_t m = 0; m < materials.size(); ++m)
			if (materials[m].textured == triangle.textured && (!triangle.textured ||
				(materials[m].page == triangle.tpage && materials[m].clut == triangle.clut))) found = true;
		if (!found)
		{
			if (materials.size() == 64) return Fail(out, error, capacity, "original car material capacity reached");
			Material material = { triangle.tpage, triangle.clut, triangle.textured };
			materials.push_back(material);
		}
	}
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
				out->source.indices.push_back((unsigned int)out->source.positions.size() / 3);
				for (int axis = 0; axis < 3; ++axis)
					out->source.positions.push_back(positions[triangle.vertices[c] * 3 + axis]);
				out->source.uvs.push_back((float)triangle.uv[c * 2]);
				out->source.uvs.push_back((float)triangle.uv[c * 2 + 1]);
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
			std::vector<unsigned char> pixels;
			if (!DecodeCarPage(vram, material.page, material.clut, pixels, error, capacity))
			{
				*out = WorldMeshConversionInput();
				return false;
			}
			image.pixelIndex = (int)out->pixels.size();
			image.width = image.height = 256;
			out->pixels.push_back(std::move(pixels));
		}
		out->images.push_back(image);
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
