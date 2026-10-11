#ifndef NATIVE_WHEEL_MESH_H
#define NATIVE_WHEEL_MESH_H
#include "NativeCarMesh.h"

struct NativeWheelSourceIdentity
{
	NativeCarSourceIdentity car;
	const MODEL* vertices;
	const MODEL* polygons;
	int wheelSize;
};
struct NativeWheelCapture
{
	WorldMeshConversionInput conversion;
	std::vector<float> originalPositions;
	std::vector<unsigned int> cornerVertices;
};
bool NativeWheelMesh_SameSource(const NativeWheelSourceIdentity& a, const NativeWheelSourceIdentity& b);
bool NativeWheelMesh_ApplyConstantColors(int quads, std::vector<unsigned char>* colors, char* error, int capacity);
void NativeWheelMesh_TriangulateQuad(const unsigned int vertices[4], const unsigned char uv[8],
	unsigned short page, unsigned short clut, NativeCarTriangle out[2]);
bool NativeWheelMesh_Deform(const NativeWheelCapture& capture, int wheelSize,
	int spinSin, int spinCos, std::vector<float>* expanded, char* error, int capacity);
void NativeWheelMesh_ComposeTransform(const float car[16], const short displacement[3],
	int wheelSize, int compression, int wheelSlot, int steerSin, int steerCos, float out[16]);
#endif
