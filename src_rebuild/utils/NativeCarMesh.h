#ifndef NATIVE_CAR_MESH_H
#define NATIVE_CAR_MESH_H

#include "WorldMeshConversion.h"

struct MODEL;
struct NativeCarSourceIdentity
{
	int model, palette;
	unsigned int revision;
	const MODEL* cleanModel; // identity only; workers never dereference it
	const MODEL* lowModel;
};

bool NativeCarMesh_SameSource(const NativeCarSourceIdentity& a, const NativeCarSourceIdentity& b);
bool NativeCarMesh_IsCurrentPublication(const NativeCarSourceIdentity& current,
	const NativeCarSourceIdentity& captured, unsigned int currentGeneration, unsigned int capturedGeneration);

// Owned original-car triangles, independent of CAR_MODEL/VRAM lifetimes.
struct NativeCarTriangle
{
	unsigned int vertices[3];
	unsigned char uv[6];
	unsigned short tpage, clut;
	bool textured;
	unsigned originalIndex;
	bool paletteClut; // GT3 uses palette CLUT OR carry; FT3 adds to packed CLUT.
};

// Immutable expanded-corner bindings follow the converted primitive order.
struct NativeCarCornerBinding
{
	unsigned vertex, originalIndex;
	unsigned short uv;
	bool textured;
	unsigned short uv0, uv1, clut;
	bool paletteClut, pageAtlas, clutAtlas;
};
struct NativeCarBodyCapture
{
	WorldMeshConversionInput conversion;
	std::vector<NativeCarCornerBinding> bindings;
	bool lowDetail = false;
};
bool NativeCarMesh_ExpandDamage(const NativeCarBodyCapture& capture,
	const std::vector<float>& positions, const std::vector<unsigned char>& damage,
	std::vector<float>* xyz, std::vector<float>* uvs, char* error, int capacity);
inline bool NativeCarMesh_SelectLowDetail(int cameraDepth, int switchDistance, bool forceLow, bool player)
{
	return !((cameraDepth <= switchDistance && !forceLow) || player);
}

// Captures all triangles and original page/CLUT pixels; failure clears output.
// Limits: 256 original vertices, 1024 triangles, 64 materials, 16 MiB pixels.
bool NativeCarMesh_Capture(const std::vector<float>& positions,
	const std::vector<NativeCarTriangle>& triangles, const std::vector<unsigned short>& vram,
	WorldMeshConversionInput* out, char* error, int capacity,
	std::vector<NativeCarCornerBinding>* bindings = nullptr);

// Full original drawCarMat rotation and cog, not static-world yaw. Translation
// comes from the same visual snapshot interval; game/camera axes remain unchanged.
void NativeCarMesh_ComposeTransform(const int position[3], const short rotation[3][3],
	const short cog[3], bool miniature, float out[16]);

#endif
