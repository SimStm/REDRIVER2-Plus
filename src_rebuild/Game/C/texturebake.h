#ifndef TEXTURE_BAKE_H
#define TEXTURE_BAKE_H

/*
 * E03 texture phase 2: game-side VRAM/CLUT bake for the model conversion.
 *
 * `ConvertModelSlot` decodes a resident MODEL into a project-owned `SourceMesh`
 * (utils/SourceModelDecoder), but the decoded mesh only carries the per-submesh
 * texture SET index and texture ID (SourceSubmesh::textureSet/textureId), not
 * the pixels. The source page and CLUT bytes are not retained by the loader
 * (LoadTPageAndCluts/LoadTPageFromTIMs upload straight into VRAM through the
 * `_other_buffer` scratch), so the only surviving copy of a page at conversion
 * time is the live VRAM. This module reads VRAM once per conversion and turns
 * every referenced (page word, CLUT word) pair into one RGBA8 image, so the
 * WorldMeshBuilder multi path can embed a base-colour texture per submesh.
 *
 * What is baked
 * -------------
 * For each submesh, `texture_pages[set]` is the packed PSX tpage word and
 * `texture_cluts[set][id]` the packed CLUT word. They decode to VRAM regions
 * with the same rules the texture override exporter already uses
 * (utils/HdTextureOverrides.cpp):
 *   format = (tpage >> 7) & 3        0 = 4-bit, 1 = 8-bit, 2 = 16-bit, 3 reserved
 *   pageX  = (tpage & 0x0f) << 6     VRAM x of the page, in halfwords
 *   pageY  = (tpage & 0x10) ? 256 : 0
 *   clutX  = (clut & 0x3f) << 4
 *   clutY  = clut >> 6
 * The whole 256x256 texel page is decoded (utils/TextureDecode) with the VRAM
 * row stride of 1024 halfwords, using 16 CLUT entries for 4-bit and 256 for
 * 8-bit; a 16-bit page is direct and needs no CLUT. The TEXINF x/y/width/height
 * region is metadata and is deliberately NOT cropped: a full page keeps the
 * raw-texel UV convention, and the renderer scales the stored 8-bit texel
 * coordinates by 1/256 when it samples. Cropping to the TEXINF sub-rect would
 * change the UV scale and is a recorded follow-up, not part of this block.
 *
 * Residency and honesty
 * ---------------------
 * A page whose set/id is not declared for the loaded level, or whose VRAM slot
 * is no longer resident (a streamed page that was evicted), cannot be baked
 * honestly. Those submeshes are left untextured (a null image) and the model
 * conversion continues; the caller reports a named diagnostic. The residency
 * test uses the loader's own bookkeeping: a page is resident when
 * `tpageloaded[set] != 0`, or when it still occupies a slot in `tpageslots`
 * (which also covers the permanent page loaded into VRAM slot 0, whose
 * `tpageloaded` entry is 0 by construction). An unsupported format, a page or
 * CLUT region outside VRAM, and a decode failure are likewise named and leave
 * only that submesh untextured. Nothing is ever partially written: a failed
 * decode produces no image.
 *
 * Bounds
 * ------
 * Identical (page, CLUT) pairs are decoded once and shared by every submesh
 * that references them, so a model with many faces over a few pages stays
 * small. At most TEXTURE_BAKE_MAX_IMAGES_PER_MODEL unique images are baked per
 * model (each 256x256x4 = 256 KiB, so the bake is capped at 16 MiB); a further
 * unique pair is a named truncation and leaves its submeshes untextured. The
 * module never allocates per submesh beyond the shared images.
 *
 * The module is game-side (it reads VRAM and the loader globals) and is only
 * compiled for the desktop builds, like the rest of assetcatalog_game.c. It
 * never runs unless a developer conversion flag is active, so it cannot change
 * gameplay behaviour.
 */

#include "../../utils/SourceModelDecoder.h"
#include "../../utils/WorldMeshBuilder.h"

#include <vector>

/* At most this many distinct (page, CLUT) images per model. 64 * 256 KiB. */
#define TEXTURE_BAKE_MAX_IMAGES_PER_MODEL 64

/* Named reason a submesh was left untextured. NONE means the bake succeeded. */
enum TextureBakeReason
{
	TEXTURE_BAKE_REASON_NONE = 0,
	TEXTURE_BAKE_REASON_NOT_DECLARED,       /* set/id outside the level texture set */
	TEXTURE_BAKE_REASON_PAGE_NOT_RESIDENT,  /* streamed page not loaded or evicted */
	TEXTURE_BAKE_REASON_UNSUPPORTED_FORMAT, /* tpage format 3 (reserved) */
	TEXTURE_BAKE_REASON_REGION_OUT_OF_VRAM, /* page or CLUT region outside VRAM */
	TEXTURE_BAKE_REASON_DECODE_ERROR,       /* TextureDecode rejected the page */
	TEXTURE_BAKE_REASON_IMAGE_CAP,          /* unique-image cap reached */
};

/* Bake outcome for one model. `images` is always exactly parallel to
   `source.submeshes` (a null `rgba` means that submesh is untextured); `pixels`
   owns every decoded RGBA buffer the non-null images point into and must
   outlive the WorldMeshBuilder_BuildMulti call. */
struct TextureBakeResult
{
	std::vector<WorldSubmeshImage> images;           /* one entry per submesh */
	std::vector<std::vector<unsigned char> > pixels; /* owned RGBA8 buffers */

	int submeshCount;
	int bakedCount;        /* submeshes with a non-null image */
	int dedupedCount;      /* submeshes that reused an already-decoded image */
	int untexturedCount;   /* submeshes left null for a named reason */
	int truncatedCount;    /* submeshes left null because the image cap was hit */
	bool vramRead;         /* true when GR_ReadVRAM ran for this conversion */

	TextureBakeReason firstReason; /* reason of the first untextured submesh */
	char firstDetail[96];          /* human-readable detail of that first reason */

	TextureBakeResult()
		: submeshCount(0), bakedCount(0), dedupedCount(0), untexturedCount(0),
		  truncatedCount(0), vramRead(false), firstReason(TEXTURE_BAKE_REASON_NONE)
	{
		firstDetail[0] = '\0';
	}
};

/* Bakes the submesh images for one decoded model from the resident VRAM. Always
   fills `out`; never throws and never aborts the conversion. Warn-only callers
   report the named reasons. */
void TextureBake_BakeModel(const SourceMesh& source, TextureBakeResult* out);

/* Human-readable name of a bake reason. */
const char* TextureBake_ReasonName(TextureBakeReason reason);

#endif // TEXTURE_BAKE_H
