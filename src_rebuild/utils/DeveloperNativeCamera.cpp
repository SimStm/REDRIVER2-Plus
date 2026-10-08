#include "DeveloperNativeCamera.h"
#include <cmath>
#include <cstring>

bool DeveloperNativeCamera_BuildBackdropTransform(float out[16], const PsyXNativeView& view, float offset)
{
	if (!out || !std::isfinite(offset)) return false;
	const float* m = view.view;
	for (unsigned int column = 0; column < 3; ++column)
		for (unsigned int row = 0; row < 3; ++row) if (!std::isfinite(m[column*4+row])) return false;
	const float a=m[0], b=m[4], c=m[8], d=m[1], e=m[5], f=m[9], g=m[2], h=m[6], i=m[10];
	const float determinant = a*(e*i-f*h)-b*(d*i-f*g)+c*(d*h-e*g);
	if (!std::isfinite(determinant) || std::fabs(determinant)<0.001f) return false;
	float world[16] = {};
	world[0]=world[5]=world[10]=world[15]=1;
	const float y = -offset;
	world[12]=(c*h-b*i)*y/determinant;
	world[13]=(a*i-c*g)*y/determinant;
	world[14]=(b*g-a*h)*y/determinant;
	for (unsigned int axis=12; axis<15; ++axis) if (!std::isfinite(world[axis])) return false;
	memcpy(out,world,sizeof(world));
	return true;
}

bool DeveloperNativeCamera_Build(PsyXNativeView* out, const float rotation[9], const float camera[3],
	float focal, float logicalWidth, float logicalHeight, float width, float height)
{
	if (!out || !rotation || !camera || !std::isfinite(focal) || focal <= 0 ||
		!std::isfinite(logicalWidth) || logicalWidth <= 0 || !std::isfinite(logicalHeight) || logicalHeight <= 0 ||
		!std::isfinite(width) || width <= 0 || !std::isfinite(height) || height <= 0) return false;
	for (unsigned int i = 0; i < 9; ++i) if (!std::isfinite(rotation[i])) return false;
	for (unsigned int i = 0; i < 3; ++i) if (!std::isfinite(camera[i])) return false;
	// A missing or corrupt basis cannot turn into a "ready" native view.
	// Allow fixed-point quantization, not scale/shear or a zero camera matrix.
	for (unsigned int row = 0; row < 3; ++row)
		for (unsigned int other = row; other < 3; ++other)
		{
			float dot = 0;
			for (unsigned int axis = 0; axis < 3; ++axis) dot += rotation[row * 3 + axis] * rotation[other * 3 + axis];
			if (std::fabs(dot - (row == other ? 1.0f : 0.0f)) > 0.01f) return false;
		}
	PsyXNativeView view = {};
	const float worldSigns[] = { 1, -1, 1 }; // original world -> Y-up
	const float cameraSigns[] = { 1, -1, -1 }; // original camera -> Y-up, -Z
	for (unsigned int row = 0; row < 3; ++row)
	{
		view.cameraPosition[row] = camera[row] * worldSigns[row];
		for (unsigned int column = 0; column < 3; ++column)
			view.view[column * 4 + row] = cameraSigns[row] * rotation[row * 3 + column] * worldSigns[column];
		view.view[12 + row] = -cameraSigns[row] *
			(rotation[row * 3] * camera[0] + rotation[row * 3 + 1] * camera[1] + rotation[row * 3 + 2] * camera[2]);
	}
	view.view[15] = 1;
	view.nearPlane = 10;
	view.farPlane = 125000;
	const float aspect = width / height;
	// PsyCross maps source X through windowAspect * (240/320), independently
	// of PGXP. Source Y uses the actual display height (NTSC/PAL).
	view.projection[0] = (2 * focal) / (logicalWidth * aspect * (240.0f / 320.0f));
	view.projection[5] = -(2 * focal) / logicalHeight; // exactly one Vulkan Y flip
	view.projection[10] = view.farPlane / (view.nearPlane - view.farPlane);
	view.projection[11] = -1;
	view.projection[14] = view.nearPlane * view.farPlane / (view.nearPlane - view.farPlane);
	for (unsigned int i = 0; i < 16; ++i)
		if (!std::isfinite(view.view[i]) || !std::isfinite(view.projection[i])) return false;
	*out = view;
	return true;
}
