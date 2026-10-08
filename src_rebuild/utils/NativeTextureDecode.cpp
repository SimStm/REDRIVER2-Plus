#include "NativeTextureDecode.h"
#include <new>
#include <utility>
#include <cmath>
#include <algorithm>
namespace NativeAsset
{
namespace
{
const unsigned int VramWidth = 1024, VramHeight = 512;
unsigned short ReadWord(Bytes vram, unsigned int x, unsigned int y)
{
	const size_t offset = ((size_t)y * VramWidth + x) * 2;
	return (unsigned short)(vram.data[offset] | (unsigned short)vram.data[offset+1] << 8);
}
unsigned char Expand5(unsigned int value) { return (unsigned char)((value * 255 + 15) / 31); }
}
Result ExpandOriginalTextureRegion(const Part& part, unsigned short& u, unsigned short& v,
	unsigned short& width, unsigned short& height)
{
	if (!width || !height || width > 256 || height > 256 || u > 256-width || v > 256-height || part.vertices.empty()) return Result::Invalid;
	unsigned int left = u, top = v, right = unsigned(u)+width, bottom = unsigned(v)+height;
	for (size_t i = 0; i < part.vertices.size(); ++i)
	{
		const float sourceU = part.vertices[i].uv[0], sourceV = part.vertices[i].uv[1];
		if (!std::isfinite(sourceU) || !std::isfinite(sourceV) || sourceU < 0 || sourceV < 0 || sourceU >= 1 || sourceV >= 1) return Result::Invalid;
		const unsigned int x = unsigned(sourceU*256), y = unsigned(sourceV*256);
		left = (std::min)(left, x); top = (std::min)(top, y);
		right = (std::max)(right, x+1); bottom = (std::max)(bottom, y+1);
	}
	u = (unsigned short)left; v = (unsigned short)top;
	width = (unsigned short)(right-left); height = (unsigned short)(bottom-top);
	return Result::Complete;
}

Result DecodeTexture(Bytes vram, unsigned short tpage, unsigned short clut,
	unsigned int u, unsigned int v, unsigned int width, unsigned int height,
	bool semiTransparent, Texture& output)
{
	output = Texture();
	const unsigned int format = (tpage >> 7) & 3;
	if (format == 3) return Result::Unsupported;
	if (!vram.data || vram.size != (size_t)VramWidth * VramHeight * 2 ||
		!width || !height || width > 256 || height > 256 || u > 256-width || v > 256-height)
		return Result::Invalid;
	const unsigned int pageX = (tpage & 15) * 64;
	const unsigned int pageY = ((tpage >> 4) & 1) * 256 + ((tpage >> 2) & 0x200);
	const unsigned int pixelsPerWord = 4 >> format;
	const unsigned int paletteX = (clut & 63) * 16, paletteY = clut >> 6;
	const unsigned int paletteSize = format == 0 ? 16 : 256;
	if (pageX + (u+width-1)/pixelsPerWord >= VramWidth || pageY + v + height > VramHeight ||
		(format != 2 && (paletteY >= VramHeight || paletteX + paletteSize > VramWidth))) return Result::Invalid;
	try
	{
		Texture texture;
		texture.width = width; texture.height = height; texture.tpage = tpage;
		texture.clut = clut; texture.u = (unsigned short)u; texture.v = (unsigned short)v;
		texture.rgba.resize((size_t)width * height * 4);
		for (unsigned int y = 0; y < height; ++y) for (unsigned int x = 0; x < width; ++x)
		{
			const unsigned int texel = u + x;
			const unsigned short word = ReadWord(vram, pageX + texel/pixelsPerWord, pageY + v + y);
			unsigned short color = word;
			if (format != 2)
			{
				const unsigned int bits = format == 0 ? 4 : 8;
				const unsigned int index = (word >> ((texel % pixelsPerWord)*bits)) & ((1u<<bits)-1);
				color = ReadWord(vram, paletteX+index, paletteY);
			}
			const size_t offset = ((size_t)y * width + x) * 4;
			texture.rgba[offset] = Expand5(color & 31);
			texture.rgba[offset+1] = Expand5((color >> 5) & 31);
			texture.rgba[offset+2] = Expand5((color >> 10) & 31);
			texture.rgba[offset+3] = color == 0 ? 0 : semiTransparent && (color & 0x8000) ? 128 : 255;
			texture.hasCutout |= color == 0;
			texture.hasStp |= (color & 0x8000) != 0;
		}
		output = std::move(texture);
	}
	catch (const std::bad_alloc&) { return Result::OutOfBudget; }
	return Result::Complete;
}
}
