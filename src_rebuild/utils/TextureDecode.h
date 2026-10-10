#ifndef TEXTURE_DECODE_H
#define TEXTURE_DECODE_H

/*
 * E03 block 2.4 CPU texture decode (asset-free, no GPU, no PsyCross, no FMOD).
 *
 * Decodes one indexed PSX texture page (or a 16-bit direct page) plus its CLUT
 * entries into RGBA8, using exactly the bytes the loader hands over before
 * LoadImage. The module is pure data: it takes caller-owned input bytes and
 * writes caller-owned output buffers. It never touches VRAM, PsyCross or the
 * game.
 *
 * Loader layout (derived from Game/C/texture.c; both desktop loader paths):
 *  - The TIM path (LoadTPageFromTIMs, texture.c:308-351) uploads the pixel
 *    section directly: `tmptpage.w = timData->width` halfwords per row with no
 *    VRAM padding, so the page is TIGHT: rowStrideHalfwords == the section
 *    width in halfwords (4-bit: widthTexels/4, 8-bit: widthTexels/2,
 *    16-bit: widthTexels). Its CLUT section is `timClut->height` blocks of
 *    32 bytes (16 halfwords) each.
 *  - The decompressed path (LoadTPageAndCluts, texture.c:362-389) uploads
 *    `_other_buffer` with `temptpage.w = tpage->w` (64 halfwords for a 4-bit
 *    permanent page) and a 32-byte CLUT block per palette (texture.c:375-376).
 *    It is also TIGHT.
 *  - A page copied straight out of VRAM instead uses the VRAM row stride of
 *    1024 halfwords (PsyX_render.h:113); pass that as rowStrideHalfwords.
 *  The decoder reads halfwords little-endian, so host endianness does not
 *  change the result.
 *
 * Pixel rules (knowledge/rules/texture-alpha-semantics.md):
 *  - 4-bit: low nibble is the leftmost texel (word = x >> 2, shift (x & 3) * 4).
 *  - 8-bit: low byte is the leftmost texel (word = x >> 1, shift (x & 1) * 8).
 *  - BGR555 expansion: E5TO8(c) = ((c * 527) + 23) >> 6; bits 0-4 red,
 *    5-9 green, 10-14 blue, bit 15 STP.
 *  - A 0x0000 word is always fully transparent -> (0,0,0,0). Opaque black is
 *    0x8000.
 *  - STP=1 has no known blend context in a whole-page decode, so it keeps the
 *    conservative alpha 128; every other non-zero word gets alpha 255.
 *
 * The texture window (GP0 E2) is a SAMPLE-TIME concern and is NOT applied
 * here: a whole-page decode returns every texel as stored.
 *
 * The API is C-compatible (plain structs, a named error enum and an int return
 * code), so the C loader can adopt it later; the implementation is C++11. On
 * any error no byte is written to an output buffer (no partial output).
 */

#ifdef __cplusplus
extern "C" {
#endif

/* Source layout of the page. */
enum TextureDecodeFormat
{
	TEXTURE_DECODE_FORMAT_4BIT_INDEXED = 0, /* 16-colour CLUT, 4 texels/halfword */
	TEXTURE_DECODE_FORMAT_8BIT_INDEXED = 1, /* 256-colour CLUT, 2 texels/halfword */
	TEXTURE_DECODE_FORMAT_16BIT_DIRECT = 2, /* BGR555 direct, no CLUT */
};

/* Named failure codes. */
enum TextureDecodeError
{
	TEXTURE_DECODE_OK = 0,
	TEXTURE_DECODE_ERR_ARGUMENT,     /* NULL request/result/page/clut/output */
	TEXTURE_DECODE_ERR_FORMAT,       /* unknown format */
	TEXTURE_DECODE_ERR_DIMENSIONS,   /* width/height outside 1..MAX_DIMENSION */
	TEXTURE_DECODE_ERR_STRIDE,       /* row stride smaller than the row needs */
	TEXTURE_DECODE_ERR_PAGE_SIZE,    /* page byte count cannot hold the page */
	TEXTURE_DECODE_ERR_CLUT_COUNT,   /* entry/variant count invalid for the format */
	TEXTURE_DECODE_ERR_CLUT_SIZE,    /* CLUT byte count cannot hold the entries */
	TEXTURE_DECODE_ERR_CLUT_INDEX,   /* a texel references an index >= entry count */
	TEXTURE_DECODE_ERR_OUTPUT_SIZE,  /* an output buffer is too small */
};

/* Bounds. A PSX texture page is at most 256x256, but a standalone TIM image
   section (for example a 640x512 frontend image) can be larger, so the decode
   is bounded to the VRAM extent instead of the page size. */
#define TEXTURE_DECODE_MAX_DIMENSION 1024
/* texture_cluts[128][32] (texture.c:100): at most 32 palettes per page. */
#define TEXTURE_DECODE_MAX_PALETTE_VARIANTS 32
#define TEXTURE_DECODE_PALETTE_ENTRIES_4BIT 16
#define TEXTURE_DECODE_PALETTE_ENTRIES_8BIT 256

/* Input plus the caller-owned output buffers.
   - `page` is raw little-endian halfwords; `pageBytes` is its byte count.
   - `rowStrideHalfwords` is the distance between consecutive rows, in
     halfwords. Use the tight section width for a loader page (see above) or
     1024 for a VRAM copy.
   - `clut` points at `paletteVariantCount` contiguous palettes of
     `paletteEntryCount` 16-bit entries each (32 bytes per 16-entry block);
     `clutBytes` is its byte count. It is NULL for 16-bit direct.
   - `rgbaOut` receives widthTexels * heightTexels * 4 bytes decoded with the
     first palette variant. `paletteOut` receives one RGBA8 per CLUT entry for
     every palette variant, in variant order (variantCount * entryCount * 4
     bytes); it may be NULL only for 16-bit direct. */
struct TextureDecodeRequest
{
	enum TextureDecodeFormat format;
	const unsigned char* page;
	int pageBytes;
	int widthTexels;
	int heightTexels;
	int rowStrideHalfwords;

	const unsigned char* clut;
	int clutBytes;
	int paletteEntryCount;    /* 1..16 (4-bit) or 1..256 (8-bit); 0 for 16-bit */
	int paletteVariantCount;  /* 1..MAX_PALETTE_VARIANTS; 0 for 16-bit */

	unsigned char* rgbaOut;
	int rgbaCapacity;
	unsigned char* paletteOut;
	int paletteCapacity;
};

/* Sizes actually written on success. Zeroed on failure. */
struct TextureDecodeResult
{
	int widthTexels;
	int heightTexels;
	int rgbaBytes;            /* widthTexels * heightTexels * 4 */
	int paletteBytes;         /* paletteVariantCount * paletteEntryCount * 4 */
	int paletteVariantCount;
	int paletteEntryCount;
};

/* Decodes the page in `request` into `out`. On success returns
   TEXTURE_DECODE_OK and fills `out`; the caller's output buffers hold the
   complete page and palette table. On any failure returns a named error and
   writes nothing to `out` or to the output buffers, so no partial page or
   palette is ever reported. `error` receives a human-readable message when it
   is non-NULL and `errorCapacity` is positive; a NULL error buffer is fine. */
int TextureDecode_Decode(const struct TextureDecodeRequest* request,
	struct TextureDecodeResult* out, char* error, int errorCapacity);

/* Human-readable name of a decode error code. */
const char* TextureDecode_ErrorName(int error);

#ifdef __cplusplus
}
#endif

#endif // TEXTURE_DECODE_H
