// Standalone regression tests for the E03 block 2.4 CPU texture decoder.
//
// The decoder turns a raw indexed (or 16-bit direct) PSX page plus its CLUT
// bytes into RGBA8 without touching VRAM, PsyCross or the game. The fixtures
// below are synthetic and analytic: they pack known halfwords, assert the
// nibble/byte order, the BGR555 expansion, the alpha rules, explicit row
// strides and bounded validation with zero partial output. They need no game
// assets, no game/PSX headers and no GPU context. Build and run instructions
// are in tests/README.md.
//
// The optional real-page probe reads one local TIM (not committed) when
// REDRIVER2_REPO_ROOT points at the repository; it reports SKIP otherwise.

#include "../utils/TextureDecode.h"

#include <algorithm>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
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

// --- helpers ----------------------------------------------------------------

static void PushHalfword(std::vector<unsigned char>& bytes, unsigned int value)
{
	bytes.push_back((unsigned char)(value & 0xff));
	bytes.push_back((unsigned char)((value >> 8) & 0xff));
}

static TextureDecodeRequest ZeroRequest()
{
	TextureDecodeRequest request;
	memset(&request, 0, sizeof(request));
	request.format = TEXTURE_DECODE_FORMAT_4BIT_INDEXED;
	return request;
}

static bool PixelIs(const std::vector<unsigned char>& rgba, int width, int x, int y,
	unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
	const unsigned char* p = rgba.data() + ((size_t)y * width + x) * 4;
	return p[0] == r && p[1] == g && p[2] == b && p[3] == a;
}

static unsigned long long Fnv1a64(const unsigned char* data, size_t size)
{
	unsigned long long hash = 1469598103934665603ULL;
	for (size_t i = 0; i < size; ++i)
	{
		hash ^= data[i];
		hash *= 1099511628211ULL;
	}
	return hash;
}

// Runs a decode whose output buffers are pre-filled with a sentinel, so a
// failure that wrote anything is visible.
static int DecodeWithSentinel(const TextureDecodeRequest& request, std::vector<unsigned char>& rgba,
	std::vector<unsigned char>& palette, TextureDecodeResult& result, char* error, int capacity)
{
	std::fill(rgba.begin(), rgba.end(), (unsigned char)0xAB);
	std::fill(palette.begin(), palette.end(), (unsigned char)0xAB);

	TextureDecodeRequest mutableRequest = request;
	mutableRequest.rgbaOut = rgba.empty() ? NULL : rgba.data();
	mutableRequest.rgbaCapacity = (int)rgba.size();
	mutableRequest.paletteOut = palette.empty() ? NULL : palette.data();
	mutableRequest.paletteCapacity = (int)palette.size();
	return TextureDecode_Decode(&mutableRequest, &result, error, capacity);
}

static bool AllBytesAre(const std::vector<unsigned char>& bytes, unsigned char value)
{
	for (size_t i = 0; i < bytes.size(); ++i)
		if (bytes[i] != value)
			return false;
	return true;
}

// --- F-1: 4-bit page, 2-entry CLUT, nibble order ----------------------------

static void Test4BitPageAndPalette()
{
	// A 4x2 4-bit page: one halfword per row, low nibble leftmost.
	std::vector<unsigned char> page;
	PushHalfword(page, 0x1010); // row 0 texels 0,1,0,1
	PushHalfword(page, 0x0011); // row 1 texels 1,1,0,0

	std::vector<unsigned char> clut;
	PushHalfword(clut, 0x001F); // entry 0: pure red
	PushHalfword(clut, 0x7C00); // entry 1: pure blue

	TextureDecodeRequest request = ZeroRequest();
	request.page = page.data();
	request.pageBytes = (int)page.size();
	request.widthTexels = 4;
	request.heightTexels = 2;
	request.rowStrideHalfwords = 1;
	request.clut = clut.data();
	request.clutBytes = (int)clut.size();
	request.paletteEntryCount = 2;
	request.paletteVariantCount = 1;

	std::vector<unsigned char> rgba(4 * 2 * 4);
	std::vector<unsigned char> palette(2 * 4);
	TextureDecodeResult result;
	char error[256] = "";
	CHECK(DecodeWithSentinel(request, rgba, palette, result, error, sizeof(error)) == TEXTURE_DECODE_OK);
	CHECK(error[0] == '\0');
	CHECK(result.widthTexels == 4 && result.heightTexels == 2);
	CHECK(result.rgbaBytes == 32 && result.paletteBytes == 8);
	CHECK(result.paletteVariantCount == 1 && result.paletteEntryCount == 2);

	// Row 0: red, blue, red, blue.
	CHECK(PixelIs(rgba, 4, 0, 0, 255, 0, 0, 255));
	CHECK(PixelIs(rgba, 4, 1, 0, 0, 0, 255, 255));
	CHECK(PixelIs(rgba, 4, 2, 0, 255, 0, 0, 255));
	CHECK(PixelIs(rgba, 4, 3, 0, 0, 0, 255, 255));
	// Row 1: blue, blue, red, red.
	CHECK(PixelIs(rgba, 4, 0, 1, 0, 0, 255, 255));
	CHECK(PixelIs(rgba, 4, 1, 1, 0, 0, 255, 255));
	CHECK(PixelIs(rgba, 4, 2, 1, 255, 0, 0, 255));
	CHECK(PixelIs(rgba, 4, 3, 1, 255, 0, 0, 255));

	// Palette table: one RGBA per CLUT entry.
	CHECK(PixelIs(palette, 2, 0, 0, 255, 0, 0, 255));
	CHECK(PixelIs(palette, 2, 1, 0, 0, 0, 255, 255));

	// Explicit nibble-order fixture: 0x0010 puts entry 0 leftmost.
	std::vector<unsigned char> orderPage;
	PushHalfword(orderPage, 0x0010);
	TextureDecodeRequest order = request;
	order.page = orderPage.data();
	order.pageBytes = (int)orderPage.size();
	order.widthTexels = 2;
	order.heightTexels = 1;
	order.rowStrideHalfwords = 1;
	std::vector<unsigned char> orderRgba(2 * 4);
	std::vector<unsigned char> orderPalette(2 * 4);
	CHECK(DecodeWithSentinel(order, orderRgba, orderPalette, result, error, sizeof(error)) == TEXTURE_DECODE_OK);
	CHECK(PixelIs(orderRgba, 2, 0, 0, 255, 0, 0, 255)); // low nibble = leftmost = entry 0
	CHECK(PixelIs(orderRgba, 2, 1, 0, 0, 0, 255, 255));
}

// --- F-2: 8-bit page, low byte leftmost -------------------------------------

static void Test8BitPage()
{
	// A 2x1 8-bit page: byte 0 = index 2, byte 1 = index 3.
	std::vector<unsigned char> page;
	PushHalfword(page, 0x0302);

	std::vector<unsigned char> clut;
	PushHalfword(clut, 0x0000); // 0: transparent
	PushHalfword(clut, 0x8000); // 1: black STP
	PushHalfword(clut, 0x03E0); // 2: pure green
	PushHalfword(clut, 0x7FFF); // 3: white

	TextureDecodeRequest request = ZeroRequest();
	request.format = TEXTURE_DECODE_FORMAT_8BIT_INDEXED;
	request.page = page.data();
	request.pageBytes = (int)page.size();
	request.widthTexels = 2;
	request.heightTexels = 1;
	request.rowStrideHalfwords = 1;
	request.clut = clut.data();
	request.clutBytes = (int)clut.size();
	request.paletteEntryCount = 4;
	request.paletteVariantCount = 1;

	std::vector<unsigned char> rgba(2 * 4);
	std::vector<unsigned char> palette(4 * 4);
	TextureDecodeResult result;
	char error[256] = "";
	CHECK(DecodeWithSentinel(request, rgba, palette, result, error, sizeof(error)) == TEXTURE_DECODE_OK);
	CHECK(PixelIs(rgba, 2, 0, 0, 0, 255, 0, 255));      // low byte = leftmost = entry 2
	CHECK(PixelIs(rgba, 2, 1, 0, 255, 255, 255, 255));  // entry 3

	// Palette expansion follows the same alpha rules.
	CHECK(PixelIs(palette, 4, 0, 0, 0, 0, 0, 0));
	CHECK(PixelIs(palette, 4, 1, 0, 0, 0, 0, 128));
	CHECK(PixelIs(palette, 4, 2, 0, 0, 255, 0, 255));
	CHECK(PixelIs(palette, 4, 3, 0, 255, 255, 255, 255));
}

// --- F-3: alpha semantics ---------------------------------------------------

static void TestAlphaSemantics()
{
	// 16-bit direct page: transparent, STP black, opaque red.
	std::vector<unsigned char> page;
	PushHalfword(page, 0x0000);
	PushHalfword(page, 0x8000);
	PushHalfword(page, 0x001F);

	TextureDecodeRequest request = ZeroRequest();
	request.format = TEXTURE_DECODE_FORMAT_16BIT_DIRECT;
	request.page = page.data();
	request.pageBytes = (int)page.size();
	request.widthTexels = 3;
	request.heightTexels = 1;
	request.rowStrideHalfwords = 3;

	std::vector<unsigned char> rgba(3 * 4);
	std::vector<unsigned char> palette; // unused for direct
	TextureDecodeResult result;
	char error[256] = "";
	CHECK(DecodeWithSentinel(request, rgba, palette, result, error, sizeof(error)) == TEXTURE_DECODE_OK);
	CHECK(PixelIs(rgba, 3, 0, 0, 0, 0, 0, 0));       // 0x0000 -> fully transparent
	CHECK(PixelIs(rgba, 3, 1, 0, 0, 0, 0, 128));     // 0x8000 -> black STP, alpha 128
	CHECK(PixelIs(rgba, 3, 2, 0, 255, 0, 0, 255));   // 0x001F -> opaque red
	CHECK(result.paletteBytes == 0 && result.paletteVariantCount == 0);

	// The same rules through a CLUT: entries 0x0000 and 0x8000.
	std::vector<unsigned char> clut;
	PushHalfword(clut, 0x0000);
	PushHalfword(clut, 0x8000);
	std::vector<unsigned char> indexedPage;
	PushHalfword(indexedPage, 0x0010); // low nibble = leftmost: texel 0 -> entry 0, texel 1 -> entry 1

	TextureDecodeRequest indexed = ZeroRequest();
	indexed.page = indexedPage.data();
	indexed.pageBytes = (int)indexedPage.size();
	indexed.widthTexels = 2;
	indexed.heightTexels = 1;
	indexed.rowStrideHalfwords = 1;
	indexed.clut = clut.data();
	indexed.clutBytes = (int)clut.size();
	indexed.paletteEntryCount = 2;
	indexed.paletteVariantCount = 1;

	std::vector<unsigned char> indexedRgba(2 * 4);
	std::vector<unsigned char> indexedPalette(2 * 4);
	CHECK(DecodeWithSentinel(indexed, indexedRgba, indexedPalette, result, error, sizeof(error)) == TEXTURE_DECODE_OK);
	CHECK(PixelIs(indexedRgba, 2, 0, 0, 0, 0, 0, 0));
	CHECK(PixelIs(indexedRgba, 2, 1, 0, 0, 0, 0, 128));
}

// --- F-4: explicit row stride -----------------------------------------------

static void TestRowStride()
{
	std::vector<unsigned char> clut;
	PushHalfword(clut, 0x001F); // red
	PushHalfword(clut, 0x03E0); // green

	// Tight 4x2 page, one halfword per row.
	std::vector<unsigned char> tight;
	PushHalfword(tight, 0x0100); // row 0: texels 0,0,0,1
	PushHalfword(tight, 0x0001); // row 1: texels 1,0,0,0

	// The same data with one padding halfword per row (stride 2); the padding
	// decodes to white if the decoder reads past the row.
	std::vector<unsigned char> padded;
	PushHalfword(padded, 0x0100); PushHalfword(padded, 0x7FFF);
	PushHalfword(padded, 0x0001); PushHalfword(padded, 0x7FFF);

	// A VRAM-strided copy of the same data (stride 1024).
	std::vector<unsigned char> vram(((size_t)1024 + 1) * 2, 0);
	{
		const unsigned char row0[2] = { 0x00, 0x01 };
		const unsigned char row1[2] = { 0x01, 0x00 };
		memcpy(&vram[0], row0, 2);
		memcpy(&vram[1024 * 2], row1, 2);
	}

	TextureDecodeRequest request = ZeroRequest();
	request.widthTexels = 4;
	request.heightTexels = 2;
	request.rowStrideHalfwords = 1;
	request.clut = clut.data();
	request.clutBytes = (int)clut.size();
	request.paletteEntryCount = 2;
	request.paletteVariantCount = 1;

	std::vector<unsigned char> tightRgba(4 * 2 * 4);
	std::vector<unsigned char> palette(2 * 4);
	TextureDecodeResult result;
	char error[256] = "";

	request.page = tight.data();
	request.pageBytes = (int)tight.size();
	CHECK(DecodeWithSentinel(request, tightRgba, palette, result, error, sizeof(error)) == TEXTURE_DECODE_OK);

	request.page = padded.data();
	request.pageBytes = (int)padded.size();
	request.rowStrideHalfwords = 2;
	std::vector<unsigned char> paddedRgba(4 * 2 * 4);
	CHECK(DecodeWithSentinel(request, paddedRgba, palette, result, error, sizeof(error)) == TEXTURE_DECODE_OK);
	CHECK(paddedRgba == tightRgba); // padding is never read

	request.page = vram.data();
	request.pageBytes = (int)vram.size();
	request.rowStrideHalfwords = 1024;
	std::vector<unsigned char> vramRgba(4 * 2 * 4);
	CHECK(DecodeWithSentinel(request, vramRgba, palette, result, error, sizeof(error)) == TEXTURE_DECODE_OK);
	CHECK(vramRgba == tightRgba); // a VRAM copy decodes identically

	// A stride smaller than a row is a named error.
	request.rowStrideHalfwords = 0;
	CHECK(DecodeWithSentinel(request, vramRgba, palette, result, error, sizeof(error)) == TEXTURE_DECODE_ERR_STRIDE);
}

// --- F-5: palette variants --------------------------------------------------

static void TestPaletteVariants()
{
	std::vector<unsigned char> page;
	PushHalfword(page, 0x0010); // texels 0,1
	std::vector<unsigned char> clut;
	PushHalfword(clut, 0x001F); // variant 0 entry 0: red
	PushHalfword(clut, 0x7C00); // variant 0 entry 1: blue
	PushHalfword(clut, 0x03E0); // variant 1 entry 0: green
	PushHalfword(clut, 0x7FFF); // variant 1 entry 1: white

	TextureDecodeRequest request = ZeroRequest();
	request.page = page.data();
	request.pageBytes = (int)page.size();
	request.widthTexels = 2;
	request.heightTexels = 1;
	request.rowStrideHalfwords = 1;
	request.clut = clut.data();
	request.clutBytes = (int)clut.size();
	request.paletteEntryCount = 2;
	request.paletteVariantCount = 2;

	std::vector<unsigned char> rgba(2 * 4);
	std::vector<unsigned char> palette(4 * 4);
	TextureDecodeResult result;
	char error[256] = "";
	CHECK(DecodeWithSentinel(request, rgba, palette, result, error, sizeof(error)) == TEXTURE_DECODE_OK);
	CHECK(result.paletteBytes == 16 && result.paletteVariantCount == 2);

	// The page uses variant 0.
	CHECK(PixelIs(rgba, 2, 0, 0, 255, 0, 0, 255));
	CHECK(PixelIs(rgba, 2, 1, 0, 0, 0, 255, 255));

	// Variant 0 then variant 1, one RGBA per entry.
	CHECK(PixelIs(palette, 4, 0, 0, 255, 0, 0, 255));
	CHECK(PixelIs(palette, 4, 1, 0, 0, 0, 255, 255));
	CHECK(PixelIs(palette, 4, 2, 0, 0, 255, 0, 255));
	CHECK(PixelIs(palette, 4, 3, 0, 255, 255, 255, 255));
}

// --- F-6: argument and range errors, zero output ----------------------------

static void TestErrorsWithZeroOutput()
{
	std::vector<unsigned char> page;
	PushHalfword(page, 0x1010);
	std::vector<unsigned char> clut;
	PushHalfword(clut, 0x001F);
	PushHalfword(clut, 0x7C00);

	TextureDecodeRequest base = ZeroRequest();
	base.page = page.data();
	base.pageBytes = (int)page.size();
	base.widthTexels = 4;
	base.heightTexels = 1;
	base.rowStrideHalfwords = 1;
	base.clut = clut.data();
	base.clutBytes = (int)clut.size();
	base.paletteEntryCount = 2;
	base.paletteVariantCount = 1;

	std::vector<unsigned char> rgba(4 * 4);
	std::vector<unsigned char> palette(2 * 4);
	TextureDecodeResult result;
	char error[256] = "";

	// A helper that runs one mutated request and asserts a named error plus an
	// untouched sentinel output.
	struct Case { const char* name; TextureDecodeRequest request; int expected; };

	std::vector<Case> cases;
	{
		Case c = { "bad format", base, TEXTURE_DECODE_ERR_FORMAT };
		c.request.format = (TextureDecodeFormat)7;
		cases.push_back(c);
	}
	{
		Case c = { "null page", base, TEXTURE_DECODE_ERR_ARGUMENT };
		c.request.page = NULL;
		cases.push_back(c);
	}
	{
		Case c = { "zero page bytes", base, TEXTURE_DECODE_ERR_ARGUMENT };
		c.request.pageBytes = 0;
		cases.push_back(c);
	}
	{
		Case c = { "zero width", base, TEXTURE_DECODE_ERR_DIMENSIONS };
		c.request.widthTexels = 0;
		cases.push_back(c);
	}
	{
		Case c = { "width over max", base, TEXTURE_DECODE_ERR_DIMENSIONS };
		c.request.widthTexels = TEXTURE_DECODE_MAX_DIMENSION + 1;
		cases.push_back(c);
	}
	{
		Case c = { "height over max", base, TEXTURE_DECODE_ERR_DIMENSIONS };
		c.request.heightTexels = TEXTURE_DECODE_MAX_DIMENSION + 1;
		cases.push_back(c);
	}
	{
		Case c = { "page too small", base, TEXTURE_DECODE_ERR_PAGE_SIZE };
		c.request.pageBytes = 1;
		cases.push_back(c);
	}
	{
		Case c = { "null clut", base, TEXTURE_DECODE_ERR_ARGUMENT };
		c.request.clut = NULL;
		cases.push_back(c);
	}
	{
		Case c = { "zero entries", base, TEXTURE_DECODE_ERR_CLUT_COUNT };
		c.request.paletteEntryCount = 0;
		cases.push_back(c);
	}
	{
		Case c = { "too many entries", base, TEXTURE_DECODE_ERR_CLUT_COUNT };
		c.request.paletteEntryCount = TEXTURE_DECODE_PALETTE_ENTRIES_4BIT + 1;
		cases.push_back(c);
	}
	{
		Case c = { "zero variants", base, TEXTURE_DECODE_ERR_CLUT_COUNT };
		c.request.paletteVariantCount = 0;
		cases.push_back(c);
	}
	{
		Case c = { "too many variants", base, TEXTURE_DECODE_ERR_CLUT_COUNT };
		c.request.paletteVariantCount = TEXTURE_DECODE_MAX_PALETTE_VARIANTS + 1;
		cases.push_back(c);
	}
	{
		Case c = { "clut too small", base, TEXTURE_DECODE_ERR_CLUT_SIZE };
		c.request.paletteVariantCount = 2; // needs 8 bytes, only 4 provided
		cases.push_back(c);
	}
	{
		Case c = { "rgba too small", base, TEXTURE_DECODE_ERR_OUTPUT_SIZE };
		c.request.rgbaCapacity = -1; // forced below the required size
		cases.push_back(c);
	}
	{
		Case c = { "palette too small", base, TEXTURE_DECODE_ERR_OUTPUT_SIZE };
		c.request.paletteCapacity = -1;
		cases.push_back(c);
	}

	for (size_t i = 0; i < cases.size(); ++i)
	{
		Case& c = cases[i];
		// rgab/palette capacities are supplied by the sentinel helper; a case
		// that wants to under-size them uses a negative capacity here.
		std::vector<unsigned char> outRgba(4 * 4);
		std::vector<unsigned char> outPalette(2 * 4);
		TextureDecodeRequest request = c.request;
		if (request.rgbaCapacity < 0)
		{
			outRgba.assign(4, 0xAB); // 4 bytes is below the 16 the page needs
			request.rgbaCapacity = 0;
		}
		if (request.paletteCapacity < 0)
		{
			outPalette.assign(2, 0xAB); // 2 bytes is below the 8 the palette needs
			request.paletteCapacity = 0;
		}

		const int code = DecodeWithSentinel(request, outRgba, outPalette, result, error, sizeof(error));
		if (code != c.expected)
		{
			g_failures++;
			printf("FAIL %s: expected %s, got %s\n", c.name,
				TextureDecode_ErrorName(c.expected), TextureDecode_ErrorName(code));
		}
		g_checks++;
		CHECK(error[0] != '\0');
		CHECK(result.rgbaBytes == 0 && result.paletteBytes == 0);
		CHECK(AllBytesAre(outRgba, 0xAB));
		CHECK(AllBytesAre(outPalette, 0xAB));
	}

	// A texel that references an index outside the provided palette is a named
	// error, never a clamp: page 0x0010 uses entry 1, but only one entry exists.
	{
		TextureDecodeRequest request = base;
		request.paletteEntryCount = 1;
		request.clutBytes = 2;
		std::vector<unsigned char> outRgba(4 * 4);
		std::vector<unsigned char> outPalette(1 * 4);
		CHECK(DecodeWithSentinel(request, outRgba, outPalette, result, error, sizeof(error)) == TEXTURE_DECODE_ERR_CLUT_INDEX);
		CHECK(AllBytesAre(outRgba, 0xAB));
		CHECK(AllBytesAre(outPalette, 0xAB));
	}

	// A NULL result and a NULL request are argument errors.
	CHECK(TextureDecode_Decode(&base, NULL, error, sizeof(error)) == TEXTURE_DECODE_ERR_ARGUMENT);
	CHECK(TextureDecode_Decode(NULL, &result, error, sizeof(error)) == TEXTURE_DECODE_ERR_ARGUMENT);

	// A NULL error buffer must not crash.
	TextureDecodeRequest valid = base;
	valid.rgbaOut = rgba.data();
	valid.rgbaCapacity = (int)rgba.size();
	valid.paletteOut = palette.data();
	valid.paletteCapacity = (int)palette.size();
	CHECK(TextureDecode_Decode(&valid, &result, NULL, 0) == TEXTURE_DECODE_OK);
}

// --- F-7: determinism -------------------------------------------------------

static void TestDeterminism()
{
	std::vector<unsigned char> page;
	for (int i = 0; i < 8; ++i)
		PushHalfword(page, (unsigned int)(0x1357 + i * 0x0101));
	std::vector<unsigned char> clut;
	for (int i = 0; i < 16; ++i)
		PushHalfword(clut, (unsigned int)(i * 0x0821));

	TextureDecodeRequest request = ZeroRequest();
	request.page = page.data();
	request.pageBytes = (int)page.size();
	request.widthTexels = 8;
	request.heightTexels = 4;
	request.rowStrideHalfwords = 2;
	request.clut = clut.data();
	request.clutBytes = (int)clut.size();
	request.paletteEntryCount = 16;
	request.paletteVariantCount = 1;

	std::vector<unsigned char> first(8 * 4 * 4);
	std::vector<unsigned char> firstPalette(16 * 4);
	std::vector<unsigned char> second(8 * 4 * 4);
	std::vector<unsigned char> secondPalette(16 * 4);
	TextureDecodeResult result;
	char error[256] = "";
	CHECK(DecodeWithSentinel(request, first, firstPalette, result, error, sizeof(error)) == TEXTURE_DECODE_OK);
	CHECK(DecodeWithSentinel(request, second, secondPalette, result, error, sizeof(error)) == TEXTURE_DECODE_OK);
	CHECK(first == second);
	CHECK(firstPalette == secondPalette);
}

// --- optional real-page probe -----------------------------------------------

static unsigned int ReadU16(const unsigned char* bytes)
{
	return (unsigned int)bytes[0] | ((unsigned int)bytes[1] << 8);
}

static unsigned int ReadU32(const unsigned char* bytes)
{
	return ReadU16(bytes) | (ReadU16(bytes + 2) << 16);
}

static bool ReadFileBytes(const char* path, std::vector<unsigned char>& bytes)
{
	FILE* file = fopen(path, "rb");
	if (!file)
		return false;
	fseek(file, 0, SEEK_END);
	const long size = ftell(file);
	fseek(file, 0, SEEK_SET);
	if (size <= 0)
	{
		fclose(file);
		return false;
	}
	bytes.resize((size_t)size);
	const size_t read = fread(bytes.data(), 1, (size_t)size, file);
	fclose(file);
	return read == (size_t)size;
}

// Decodes one local TIM (8bpp indexed) and prints a hash. It is read-only and
// skips cleanly when the repository or the file is not available, so the suite
// stays asset-free on a clean checkout.
//
// Observed 2026-10-09 on data/_DRIVER2/GFX/GAL/IMG0.TIM (8-bit indexed,
// 640x512 texels, 256-entry CLUT, tight stride 320 halfwords):
//   rgba FNV-1a64    = 13049095375962929689
//   palette FNV-1a64 = 18035046086311861693
// The hash is recorded, not pinned: the file is local and not committed.
static void TestRealPageProbe()
{
	const char* root = getenv("REDRIVER2_REPO_ROOT");
	if (!root || root[0] == '\0')
	{
		printf("SKIP real-page probe: REDRIVER2_REPO_ROOT is not set\n");
		return;
	}

	char path[1024];
	snprintf(path, sizeof(path), "%s/data/_DRIVER2/GFX/GAL/IMG0.TIM", root);
	std::vector<unsigned char> tim;
	if (!ReadFileBytes(path, tim) || tim.size() < 20)
	{
		printf("SKIP real-page probe: %s is not available\n", path);
		return;
	}

	if (ReadU32(tim.data()) != 0x10)
	{
		printf("SKIP real-page probe: %s is not a TIM\n", path);
		return;
	}
	const unsigned int flags = ReadU32(tim.data() + 4);
	if (((flags >> 3) & 1) == 0)
	{
		printf("SKIP real-page probe: %s has no CLUT\n", path);
		return;
	}

	const size_t clutHeader = 8;
	const size_t clutData = clutHeader + 12;
	const unsigned int clutWidth = ReadU16(tim.data() + clutHeader + 8);
	const unsigned int clutHeight = ReadU16(tim.data() + clutHeader + 10);
	const size_t imageHeader = clutHeader + ReadU32(tim.data() + clutHeader);
	const size_t imageData = imageHeader + 12;
	const unsigned int imageWidthHalfwords = ReadU16(tim.data() + imageHeader + 8);
	const unsigned int imageHeight = ReadU16(tim.data() + imageHeader + 10);

	const unsigned int format = flags & 7;
	if (format != 1 || imageWidthHalfwords == 0 || imageHeight == 0 ||
		imageData > tim.size())
	{
		printf("SKIP real-page probe: %s is not an 8-bit indexed TIM\n", path);
		return;
	}

	TextureDecodeRequest request;
	memset(&request, 0, sizeof(request));
	request.format = TEXTURE_DECODE_FORMAT_8BIT_INDEXED;
	request.page = tim.data() + imageData;
	request.pageBytes = (int)(tim.size() - imageData);
	request.widthTexels = (int)imageWidthHalfwords * 2;
	request.heightTexels = (int)imageHeight;
	request.rowStrideHalfwords = (int)imageWidthHalfwords;
	request.clut = tim.data() + clutData;
	request.clutBytes = (int)(clutWidth * clutHeight * 2);
	request.paletteEntryCount = (int)clutWidth;
	request.paletteVariantCount = (int)clutHeight;

	const long long rgbaBytes = (long long)request.widthTexels * request.heightTexels * 4;
	const long long paletteBytes = (long long)request.paletteVariantCount * request.paletteEntryCount * 4;
	std::vector<unsigned char> rgba((size_t)rgbaBytes);
	std::vector<unsigned char> palette((size_t)paletteBytes);
	TextureDecodeResult result;
	char error[256] = "";
	const int code = DecodeWithSentinel(request, rgba, palette, result, error, sizeof(error));
	if (code != TEXTURE_DECODE_OK)
	{
		printf("FAIL real-page probe: decode returned %s (%s)\n", TextureDecode_ErrorName(code), error);
		g_failures++;
		g_checks++;
		return;
	}
	g_checks++;

	const unsigned long long rgbaHash = Fnv1a64(rgba.data(), rgba.size());
	const unsigned long long paletteHash = Fnv1a64(palette.data(), palette.size());
	printf("REAL-PAGE %s: %dx%d rgba=%llu palette=%llu\n", path,
		request.widthTexels, request.heightTexels, rgbaHash, paletteHash);

	// Determinism on real data: a second decode must match byte-for-byte.
	std::vector<unsigned char> rgba2((size_t)rgbaBytes);
	std::vector<unsigned char> palette2((size_t)paletteBytes);
	CHECK(DecodeWithSentinel(request, rgba2, palette2, result, error, sizeof(error)) == TEXTURE_DECODE_OK);
	CHECK(rgba == rgba2);
	CHECK(palette == palette2);
}

int main()
{
	Test4BitPageAndPalette();
	Test8BitPage();
	TestAlphaSemantics();
	TestRowStride();
	TestPaletteVariants();
	TestErrorsWithZeroOutput();
	TestDeterminism();
	TestRealPageProbe();

	printf("%d checks, %d failures\n", g_checks, g_failures);
	return g_failures == 0 ? 0 : 1;
}
