#include "TextureDecode.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <vector>

namespace
{
void SetError(char* error, int capacity, const char* message)
{
	if (error && capacity > 0)
		snprintf(error, capacity, "%s", message);
}

// The loader hands over raw PSX bytes; halfwords are little-endian on the
// source and must decode the same on any host.
unsigned int ReadHalfwordLE(const unsigned char* bytes)
{
	return (unsigned int)bytes[0] | ((unsigned int)bytes[1] << 8);
}

// BGR555 -> 8-bit channel, matching DuckStation's E5TO8 and the exported PNGs.
unsigned char Expand5To8(unsigned int channel5)
{
	return (unsigned char)(((channel5 * 527u) + 23u) >> 6);
}

// Expands one BGR555 word into RGBA8. 0x0000 is always transparent; with no
// known blend context a STP word keeps alpha 128; everything else is opaque.
void ExpandColour(unsigned int colour, unsigned char* rgba)
{
	rgba[0] = Expand5To8(colour & 31u);
	rgba[1] = Expand5To8((colour >> 5) & 31u);
	rgba[2] = Expand5To8((colour >> 10) & 31u);
	rgba[3] = colour == 0u ? 0u : (((colour & 0x8000u) != 0u) ? 128u : 255u);
}

// Halfwords a row needs for `widthTexels` at the given format.
int RowHalfwords(enum TextureDecodeFormat format, int widthTexels)
{
	switch (format)
	{
		case TEXTURE_DECODE_FORMAT_4BIT_INDEXED: return (widthTexels + 3) / 4;
		case TEXTURE_DECODE_FORMAT_8BIT_INDEXED: return (widthTexels + 1) / 2;
		case TEXTURE_DECODE_FORMAT_16BIT_DIRECT: return widthTexels;
	}
	return 0;
}

int MaxEntries(enum TextureDecodeFormat format)
{
	switch (format)
	{
		case TEXTURE_DECODE_FORMAT_4BIT_INDEXED: return TEXTURE_DECODE_PALETTE_ENTRIES_4BIT;
		case TEXTURE_DECODE_FORMAT_8BIT_INDEXED: return TEXTURE_DECODE_PALETTE_ENTRIES_8BIT;
		case TEXTURE_DECODE_FORMAT_16BIT_DIRECT: return 0;
	}
	return 0;
}

// Writes the page using the first palette variant. Returns false when a texel
// index is outside the provided palette, which is a malformed page, not a clamp.
bool DecodePage(const struct TextureDecodeRequest& request, std::vector<unsigned char>& rgba)
{
	const int width = request.widthTexels;
	const int height = request.heightTexels;
	const int strideBytes = request.rowStrideHalfwords * 2;
	const int entryCount = request.paletteEntryCount;

	for (int y = 0; y < height; ++y)
	{
		const unsigned char* row = request.page + (size_t)y * (size_t)strideBytes;
		unsigned char* outRow = rgba.data() + (size_t)y * (size_t)width * 4;

		for (int x = 0; x < width; ++x)
		{
			unsigned int colour;
			if (request.format == TEXTURE_DECODE_FORMAT_16BIT_DIRECT)
			{
				colour = ReadHalfwordLE(row + (size_t)x * 2);
			}
			else
			{
				const unsigned int packed = ReadHalfwordLE(row + (size_t)(x >> (request.format == TEXTURE_DECODE_FORMAT_4BIT_INDEXED ? 2 : 1)) * 2);
				const int shift = request.format == TEXTURE_DECODE_FORMAT_4BIT_INDEXED ? (x & 3) * 4 : (x & 1) * 8;
				const int index = (int)((packed >> shift) & (request.format == TEXTURE_DECODE_FORMAT_4BIT_INDEXED ? 0xFu : 0xFFu));
				if (index >= entryCount)
					return false;
				colour = ReadHalfwordLE(request.clut + (size_t)index * 2);
			}
			ExpandColour(colour, outRow + (size_t)x * 4);
		}
	}
	return true;
}
} // namespace

const char* TextureDecode_ErrorName(int error)
{
	switch (error)
	{
		case TEXTURE_DECODE_OK: return "ok";
		case TEXTURE_DECODE_ERR_ARGUMENT: return "argument";
		case TEXTURE_DECODE_ERR_FORMAT: return "format";
		case TEXTURE_DECODE_ERR_DIMENSIONS: return "dimensions";
		case TEXTURE_DECODE_ERR_STRIDE: return "stride";
		case TEXTURE_DECODE_ERR_PAGE_SIZE: return "page-size";
		case TEXTURE_DECODE_ERR_CLUT_COUNT: return "clut-count";
		case TEXTURE_DECODE_ERR_CLUT_SIZE: return "clut-size";
		case TEXTURE_DECODE_ERR_CLUT_INDEX: return "clut-index";
		case TEXTURE_DECODE_ERR_OUTPUT_SIZE: return "output-size";
	}
	return "unknown";
}

int TextureDecode_Decode(const struct TextureDecodeRequest* request,
	struct TextureDecodeResult* out, char* error, int errorCapacity)
{
	if (error && errorCapacity > 0)
		error[0] = '\0';

	if (!out)
		return TEXTURE_DECODE_ERR_ARGUMENT;
	memset(out, 0, sizeof(*out));

	if (!request)
	{
		SetError(error, errorCapacity, "a decode request is required");
		return TEXTURE_DECODE_ERR_ARGUMENT;
	}
	if (request->format != TEXTURE_DECODE_FORMAT_4BIT_INDEXED &&
		request->format != TEXTURE_DECODE_FORMAT_8BIT_INDEXED &&
		request->format != TEXTURE_DECODE_FORMAT_16BIT_DIRECT)
	{
		SetError(error, errorCapacity, "unknown texture format");
		return TEXTURE_DECODE_ERR_FORMAT;
	}
	if (!request->page || request->pageBytes <= 0)
	{
		SetError(error, errorCapacity, "the page bytes are required");
		return TEXTURE_DECODE_ERR_ARGUMENT;
	}
	if (request->widthTexels < 1 || request->heightTexels < 1 ||
		request->widthTexels > TEXTURE_DECODE_MAX_DIMENSION ||
		request->heightTexels > TEXTURE_DECODE_MAX_DIMENSION)
	{
		SetError(error, errorCapacity, "the page dimensions are outside the supported range");
		return TEXTURE_DECODE_ERR_DIMENSIONS;
	}

	const int rowHalfwords = RowHalfwords(request->format, request->widthTexels);
	if (request->rowStrideHalfwords < rowHalfwords)
	{
		SetError(error, errorCapacity, "the row stride is smaller than a page row");
		return TEXTURE_DECODE_ERR_STRIDE;
	}

	// The last row only needs its data halfwords, not the full stride.
	const long long requiredPageBytes =
		((long long)(request->heightTexels - 1) * request->rowStrideHalfwords + rowHalfwords) * 2;
	if (requiredPageBytes > (long long)request->pageBytes)
	{
		SetError(error, errorCapacity, "the page buffer is too small for the requested page");
		return TEXTURE_DECODE_ERR_PAGE_SIZE;
	}

	const bool indexed = request->format != TEXTURE_DECODE_FORMAT_16BIT_DIRECT;
	int entryCount = 0;
	int variantCount = 0;
	if (indexed)
	{
		if (request->paletteEntryCount < 1 || request->paletteEntryCount > MaxEntries(request->format) ||
			request->paletteVariantCount < 1 || request->paletteVariantCount > TEXTURE_DECODE_MAX_PALETTE_VARIANTS)
		{
			SetError(error, errorCapacity, "the palette entry or variant count is invalid for the format");
			return TEXTURE_DECODE_ERR_CLUT_COUNT;
		}
		if (!request->clut)
		{
			SetError(error, errorCapacity, "the indexed page has no CLUT");
			return TEXTURE_DECODE_ERR_ARGUMENT;
		}
		entryCount = request->paletteEntryCount;
		variantCount = request->paletteVariantCount;
		const long long requiredClutBytes = (long long)variantCount * entryCount * 2;
		if (requiredClutBytes > (long long)request->clutBytes)
		{
			SetError(error, errorCapacity, "the CLUT buffer is too small for the requested entries");
			return TEXTURE_DECODE_ERR_CLUT_SIZE;
		}
	}
	else if (request->paletteEntryCount != 0 || request->paletteVariantCount != 0)
	{
		SetError(error, errorCapacity, "a 16-bit direct page has no palette");
		return TEXTURE_DECODE_ERR_CLUT_COUNT;
	}

	const long long rgbaBytes = (long long)request->widthTexels * request->heightTexels * 4;
	const long long paletteBytes = indexed ? (long long)variantCount * entryCount * 4 : 0;

	if (!request->rgbaOut || rgbaBytes > (long long)request->rgbaCapacity)
	{
		SetError(error, errorCapacity, "the RGBA output buffer is too small");
		return TEXTURE_DECODE_ERR_OUTPUT_SIZE;
	}
	if (indexed && (!request->paletteOut || paletteBytes > (long long)request->paletteCapacity))
	{
		SetError(error, errorCapacity, "the palette output buffer is too small");
		return TEXTURE_DECODE_ERR_OUTPUT_SIZE;
	}

	// Decode into local buffers first so a failure never leaves a partial page
	// in the caller's outputs.
	std::vector<unsigned char> rgba((size_t)rgbaBytes);
	if (!DecodePage(*request, rgba))
	{
		SetError(error, errorCapacity, "a texel references an index outside the CLUT");
		return TEXTURE_DECODE_ERR_CLUT_INDEX;
	}

	std::vector<unsigned char> palette;
	if (indexed)
	{
		palette.resize((size_t)paletteBytes);
		for (int variant = 0; variant < variantCount; ++variant)
		{
			for (int entry = 0; entry < entryCount; ++entry)
			{
				const unsigned int colour = ReadHalfwordLE(request->clut + (size_t)(variant * entryCount + entry) * 2);
				ExpandColour(colour, palette.data() + (size_t)(variant * entryCount + entry) * 4);
			}
		}
	}

	memcpy(request->rgbaOut, rgba.data(), (size_t)rgbaBytes);
	if (indexed)
		memcpy(request->paletteOut, palette.data(), (size_t)paletteBytes);

	out->widthTexels = request->widthTexels;
	out->heightTexels = request->heightTexels;
	out->rgbaBytes = (int)rgbaBytes;
	out->paletteBytes = (int)paletteBytes;
	out->paletteVariantCount = variantCount;
	out->paletteEntryCount = entryCount;
	return TEXTURE_DECODE_OK;
}
