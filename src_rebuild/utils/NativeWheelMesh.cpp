#include "NativeWheelMesh.h"
#include <stdio.h>
#include <string.h>

bool NativeWheelMesh_SameSource(const NativeWheelSourceIdentity& a, const NativeWheelSourceIdentity& b)
{
	return NativeCarMesh_SameSource(a.car, b.car) && a.vertices == b.vertices &&
		a.polygons == b.polygons && a.wheelSize == b.wheelSize;
}

bool NativeWheelMesh_ApplyConstantColors(int quads, std::vector<unsigned char>* colors, char* error, int capacity)
{
	if (error && capacity > 0) error[0] = 0;
	if (!colors || quads <= 0 || quads > 16 || colors->size() != (size_t)quads * 6 * 3)
	{
		if (error && capacity > 0) snprintf(error, capacity, "invalid original wheel color inventory");
		return false;
	}
	// DrawWheelObject's descending loop index makes every quad after the
	// first two permanently black, independently of view-dependent lighting.
	for (size_t channel = 2 * 6 * 3; channel < colors->size(); ++channel) (*colors)[channel] = 0;
	return true;
}

bool NativeWheelMesh_Deform(const NativeWheelCapture& capture, int size,
	int sine, int cosine, std::vector<float>* expanded, char* error, int capacity)
{
	if (error && capacity > 0) error[0] = 0;
	if (!expanded || capture.originalPositions.size() < 24 * 3 || capture.originalPositions.size() > 256 * 3 ||
		capture.originalPositions.size() % 3 || capture.cornerVertices.empty() || capture.cornerVertices.size() > 96 ||
		size <= 0 || size > 4096 || sine < -4096 || sine > 4096 || cosine < -4096 || cosine > 4096)
	{
		if (expanded) expanded->clear();
		if (error && capacity > 0) snprintf(error, capacity, "invalid original wheel deformation inventory");
		return false;
	}
	float positions[256 * 3];
	memcpy(positions, capture.originalPositions.data(), capture.originalPositions.size() * sizeof(float));
	const int scale = size * 14142 / 10000;
	// Match FIXEDH, including its asymmetric negative half-rounding.
	const int s = (sine * scale + 2048) >> 12, c = (cosine * scale + 2048) >> 12;
	const int yz[16][2] = { {c,s},{-s,c},{-c,-s},{s,-c},{s,-c},{-c,-s},{-s,c},{c,s},
		{-size,0},{-size,0},{0,-size},{0,-size},{0,size},{0,size},{size,0},{size,0} };
	for (int v = 8; v < 24; ++v) { positions[v * 3 + 1] = (float)yz[v - 8][0]; positions[v * 3 + 2] = (float)yz[v - 8][1]; }
	for (size_t c = 0; c < capture.cornerVertices.size(); ++c)
		if (capture.cornerVertices[c] >= capture.originalPositions.size() / 3)
		{
			expanded->clear();
			if (error && capacity > 0) snprintf(error, capacity, "original wheel corner is out of range");
			return false;
		}
	expanded->resize(capture.cornerVertices.size() * 3);
	for (size_t c = 0; c < capture.cornerVertices.size(); ++c)
		for (int axis = 0; axis < 3; ++axis) (*expanded)[c * 3 + axis] = positions[capture.cornerVertices[c] * 3 + axis];
	return true;
}

void NativeWheelMesh_TriangulateQuad(const unsigned int vertices[4], const unsigned char uv[8],
	unsigned short page, unsigned short clut, NativeCarTriangle out[2])
{
	// DrawWheelObject swaps GPU corners 2/3; PsyX's parser swaps them back.
	// TriangulateQuad then emits 0,1,3 + 1,3,2, preserving diagonal 1--3.
	const int corners[6] = { 0,1,3, 1,3,2 };
	for (int t = 0; t < 2; ++t)
	{
		out[t] = NativeCarTriangle();
		out[t].textured = true; out[t].tpage = page; out[t].clut = clut;
		for (int c = 0; c < 3; ++c)
		{
			const int corner = corners[t * 3 + c];
			out[t].vertices[c] = vertices[corner];
			out[t].uv[c * 2] = uv[corner * 2]; out[t].uv[c * 2 + 1] = uv[corner * 2 + 1];
		}
	}
}

void NativeWheelMesh_ComposeTransform(const float car[16], const short displacement[3],
	int size, int compression, int slot, int steerSin, int steerCos, float out[16])
{
	memcpy(out, car, 16 * sizeof(float));
	const float offset[3] = { (float)((slot & 2 ? -17 : 17) - displacement[0]),
		(float)(-size - displacement[1] - compression + 14), (float)-displacement[2] };
	for (int row = 0; row < 3; ++row)
		out[12 + row] = car[12 + row] + car[row] * offset[0] + car[4 + row] * offset[1] + car[8 + row] * offset[2];
	if (!(slot & 1))
	{
		const float s = (float)steerSin / 4096, c = (float)steerCos / 4096;
		for (int row = 0; row < 3; ++row)
		{
			out[row] = car[row] * c - car[8 + row] * s;
			out[8 + row] = car[row] * s + car[8 + row] * c;
		}
	}
}
