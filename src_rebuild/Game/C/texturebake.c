#ifndef PSX

#include "driver2.h"
#include "texture.h"
#include "texturebake.h"

#include "../../utils/TextureDecode.h"

#include "PsyX/PsyX_render.h"

#include <stdio.h>
#include <string.h>
#include <utility>

namespace
{
// A PSX texture page is 256x256 texels in every format; the 16-bit page is a
// direct 256-halfword-wide region.
const int kPageTexels = 256;

void SetDetail(char* detail, int capacity, const char* message)
{
	if (detail && capacity > 0)
		snprintf(detail, capacity, "%s", message ? message : "");
}

void MarkUntextured(TextureBakeResult* out, TextureBakeReason reason, const char* detail)
{
	out->untexturedCount++;
	if (out->firstReason == TEXTURE_BAKE_REASON_NONE)
	{
		out->firstReason = reason;
		SetDetail(out->firstDetail, (int)sizeof(out->firstDetail), detail);
	}
}

// True when the set/id pair is declared for the loaded level and its page still
// occupies VRAM. A page loaded into permanent slot 0 stores a 0-based slot in
// `tpageloaded`, so it is indistinguishable from "not loaded" by the flag alone;
// `tpageslots` disambiguates it. A streamed page that was evicted has its flag
// cleared and its slot reused, so it is correctly reported not resident.
bool PageResident(int set, int id)
{
	if (set < 0 || set >= 128 || id < 0 || id >= 32)
		return false;
	if (tpage_texamts[set] <= 0 || id >= tpage_texamts[set])
		return false;

	if (tpageloaded[set] != 0)
		return true;

	for (int slot = 0; slot < slotsused && slot < 19; slot++)
	{
		if (tpageslots[slot] == (unsigned char)set)
			return true;
	}
	return false;
}

// Decodes one full page (and, for an indexed page, its CLUT) out of the VRAM
// copy. Returns false with a named reason and a detail string on failure, and
// never leaves a partial `rgba`.
bool DecodePageRegion(const unsigned short* vram, unsigned short page, unsigned short clut,
	std::vector<unsigned char>& rgba, TextureBakeReason* reason, char* detail, int detailCapacity)
{
	const int format = (page >> 7) & 3;
	if (format == 3)
	{
		*reason = TEXTURE_BAKE_REASON_UNSUPPORTED_FORMAT;
		SetDetail(detail, detailCapacity, "tpage format 3 is reserved");
		return false;
	}

	const int pageX = (page & 0x0f) << 6;
	const int pageY = (page & 0x10) ? 256 : 0;
	const int rowHalfwords = (format == 0) ? 64 : (format == 1) ? 128 : 256;
	const int entryCount = (format == 0) ? 16 : (format == 1) ? 256 : 0;

	if (pageX + rowHalfwords > VRAM_WIDTH || pageY + kPageTexels > VRAM_HEIGHT)
	{
		*reason = TEXTURE_BAKE_REASON_REGION_OUT_OF_VRAM;
		SetDetail(detail, detailCapacity, "the page region is outside VRAM");
		return false;
	}

	const size_t vramHalfwords = (size_t)VRAM_WIDTH * VRAM_HEIGHT;
	const size_t pageOffset = (size_t)pageY * VRAM_WIDTH + pageX;

	size_t clutOffset = 0;
	if (format != 2)
	{
		const int clutX = (clut & 0x3f) << 4;
		const int clutY = clut >> 6;

		if (clutY >= VRAM_HEIGHT || clutX + entryCount > VRAM_WIDTH)
		{
			*reason = TEXTURE_BAKE_REASON_REGION_OUT_OF_VRAM;
			SetDetail(detail, detailCapacity, "the CLUT region is outside VRAM");
			return false;
		}
		clutOffset = (size_t)clutY * VRAM_WIDTH + clutX;
		if (clutOffset + (size_t)entryCount > vramHalfwords)
		{
			*reason = TEXTURE_BAKE_REASON_REGION_OUT_OF_VRAM;
			SetDetail(detail, detailCapacity, "the CLUT region is outside VRAM");
			return false;
		}
	}

	rgba.assign((size_t)kPageTexels * kPageTexels * 4, 0);
	unsigned char palette[TEXTURE_DECODE_PALETTE_ENTRIES_8BIT * 4];

	TextureDecodeRequest request;
	memset(&request, 0, sizeof(request));
	request.format = (TextureDecodeFormat)format;
	request.page = (const unsigned char*)vram + pageOffset * 2;
	request.pageBytes = (int)((vramHalfwords - pageOffset) * 2);
	request.widthTexels = kPageTexels;
	request.heightTexels = kPageTexels;
	request.rowStrideHalfwords = VRAM_WIDTH;

	if (format != 2)
	{
		request.clut = (const unsigned char*)vram + clutOffset * 2;
		request.clutBytes = entryCount * 2;
		request.paletteEntryCount = entryCount;
		request.paletteVariantCount = 1;
		request.paletteOut = palette;
		request.paletteCapacity = entryCount * 4;
	}

	request.rgbaOut = rgba.data();
	request.rgbaCapacity = (int)rgba.size();

	TextureDecodeResult result;
	char decodeError[96];
	const int code = TextureDecode_Decode(&request, &result, decodeError, sizeof(decodeError));
	if (code != TEXTURE_DECODE_OK)
	{
		*reason = TEXTURE_BAKE_REASON_DECODE_ERROR;
		snprintf(detail, detailCapacity, "%s (%s)", TextureDecode_ErrorName(code), decodeError);
		rgba.clear();
		return false;
	}

	return true;
}

int FindImage(const unsigned int* keys, int count, unsigned int key)
{
	for (int i = 0; i < count; i++)
	{
		if (keys[i] == key)
			return i;
	}
	return -1;
}
} // namespace

const char* TextureBake_ReasonName(TextureBakeReason reason)
{
	switch (reason)
	{
		case TEXTURE_BAKE_REASON_NONE: return "none";
		case TEXTURE_BAKE_REASON_NOT_DECLARED: return "texture-not-declared";
		case TEXTURE_BAKE_REASON_PAGE_NOT_RESIDENT: return "page-not-resident";
		case TEXTURE_BAKE_REASON_UNSUPPORTED_FORMAT: return "unsupported-format";
		case TEXTURE_BAKE_REASON_REGION_OUT_OF_VRAM: return "region-out-of-vram";
		case TEXTURE_BAKE_REASON_DECODE_ERROR: return "decode-error";
		case TEXTURE_BAKE_REASON_IMAGE_CAP: return "image-cap";
	}
	return "unknown";
}

void TextureBake_BakeModel(const SourceMesh& source, TextureBakeResult* out)
{
	if (out == NULL)
		return;

	*out = TextureBakeResult();
	out->submeshCount = (int)source.submeshes.size();
	out->images.resize(source.submeshes.size()); // every entry null (untextured)

	if (source.submeshes.empty())
		return;

	// One VRAM read per conversion; VRAM is 1024x512 halfwords.
	std::vector<unsigned short> vram((size_t)VRAM_WIDTH * VRAM_HEIGHT);
	GR_ReadVRAM(vram.data(), 0, 0, VRAM_WIDTH, VRAM_HEIGHT);
	out->vramRead = true;

	out->pixels.reserve(TEXTURE_BAKE_MAX_IMAGES_PER_MODEL);
	unsigned int keys[TEXTURE_BAKE_MAX_IMAGES_PER_MODEL];
	int uniqueCount = 0;

	for (size_t i = 0; i < source.submeshes.size(); i++)
	{
		const SourceSubmesh& submesh = source.submeshes[i];
		const int set = submesh.textureSet;
		const int id = submesh.textureId;
		char detail[96];

		if (!PageResident(set, id))
		{
			const bool declared = set >= 0 && set < 128 && id >= 0 && id < 32 &&
				tpage_texamts[set] > 0 && id < tpage_texamts[set];
			if (declared)
			{
				snprintf(detail, sizeof(detail), "texture set %d id %d page is not resident", set, id);
				MarkUntextured(out, TEXTURE_BAKE_REASON_PAGE_NOT_RESIDENT, detail);
			}
			else
			{
				snprintf(detail, sizeof(detail), "texture set %d id %d is not declared for this level", set, id);
				MarkUntextured(out, TEXTURE_BAKE_REASON_NOT_DECLARED, detail);
			}
			continue;
		}

		const unsigned short page = texture_pages[set];
		const unsigned short clut = texture_cluts[set][id];
		const unsigned int key = ((unsigned int)page << 16) | (unsigned int)clut;

		int pixelIndex = FindImage(keys, uniqueCount, key);
		if (pixelIndex < 0)
		{
			if (uniqueCount >= TEXTURE_BAKE_MAX_IMAGES_PER_MODEL)
			{
				snprintf(detail, sizeof(detail), "the %d-image cap was reached", TEXTURE_BAKE_MAX_IMAGES_PER_MODEL);
				MarkUntextured(out, TEXTURE_BAKE_REASON_IMAGE_CAP, detail);
				out->truncatedCount++;
				continue;
			}

			std::vector<unsigned char> rgba;
			TextureBakeReason reason = TEXTURE_BAKE_REASON_NONE;
			if (!DecodePageRegion(vram.data(), page, clut, rgba, &reason, detail, sizeof(detail)))
			{
				MarkUntextured(out, reason, detail);
				continue;
			}

			pixelIndex = uniqueCount;
			keys[uniqueCount] = key;
			uniqueCount++;
			out->pixels.push_back(std::move(rgba));
		}
		else
		{
			out->dedupedCount++;
		}

		// The inner buffers are stable across later push_backs (a vector move
		// keeps its heap allocation), and the outer vector is reserved, so the
		// pointer stays valid until BuildMulti has copied the bytes.
		WorldSubmeshImage& image = out->images[i];
		image.rgba = out->pixels[pixelIndex].data();
		image.width = kPageTexels;
		image.height = kPageTexels;
		image.byteCount = out->pixels[pixelIndex].size();
		out->bakedCount++;
	}
}

#endif // PSX
