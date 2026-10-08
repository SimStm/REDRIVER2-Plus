#ifndef NATIVE_TEXTURE_DECODE_H
#define NATIVE_TEXTURE_DECODE_H
#include "NativeAssetDecode.h"
namespace NativeAsset
{
struct Texture
{
	unsigned int width, height;
	unsigned short tpage, clut, u, v;
	bool hasCutout, hasStp;
	std::vector<unsigned char> rgba; // sRGB artwork; alpha carries original STP.
	Texture() : width(0), height(0), tpage(0), clut(0), u(0), v(0), hasCutout(false), hasStp(false) {}
};
// Read-only original 1024x512 word VRAM mirror, explicit CLUT/blend context.
// Opaque STP-black remains opaque; zero always cuts out. No GPU-mapped reads.
Result DecodeTexture(Bytes vram, unsigned short tpage, unsigned short clut,
	unsigned int u, unsigned int v, unsigned int width, unsigned int height,
	bool semiTransparent, Texture& output);
// Named regions do not bound original MODEL UVs. Enlarge the original atlas
// crop to include every source texel; never apply this to an active PNG region.
Result ExpandOriginalTextureRegion(const Part& part, unsigned short& u, unsigned short& v,
	unsigned short& width, unsigned short& height);
}
#endif
