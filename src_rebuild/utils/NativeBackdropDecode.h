#ifndef NATIVE_BACKDROP_DECODE_H
#define NATIVE_BACKDROP_DECODE_H
#include "NativeAssetDecode.h"
namespace NativeAsset
{
struct BackdropFace
{
	unsigned char texture;
	unsigned char uv[8]; // original UV quad; PlotSkyPoly applies its corner permutation
};
// Bounded original horizon MODEL + owned/art-table bindings. No projected GTE
// coordinates or world/painted-sky reconstruction. One part per original quad.
Result DecodeBackdrop(Bytes model, const BackdropFace* faces, size_t faceCount,
	Mesh& output, Failure& failure);
}
#endif
