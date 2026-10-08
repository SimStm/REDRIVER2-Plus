#ifndef NATIVE_MODERN_DECODE_H
#define NATIVE_MODERN_DECODE_H
#include "NativeTextureDecode.h"
#include "GltfLoader.h"
namespace NativeAsset
{
struct ModernImport
{
	Mesh mesh;
	Texture baseColor, normal, metallicRoughness, emissive;
	float baseColorFactor[4], emissiveFactor[3], metallicFactor, roughnessFactor;
	ModernImport();
};
// Bounded bridge for ONE decoded, untransformed, opaque triangle primitive.
// Coordinates retain declared glTF Y-up units; scene/game scale is caller-owned.
// This does not validate a GLB container or establish production R09 support.
Result DecodeModernPrimitive(const GltfMeshData& source, ModernImport& output, Failure& failure);
}
#endif
