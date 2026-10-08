#ifndef DEVELOPER_NATIVE_CAMERA_H
#define DEVELOPER_NATIVE_CAMERA_H
#include "PsyX/PsyX_native.h"

/* Pure adapter; gameRotation is the final world-to-camera basis, converted
 * from fixed point by the caller. No GTE cache or PGXP screen encoding. */
bool DeveloperNativeCamera_Build(PsyXNativeView* view, const float gameRotation[9],
	const float gameCamera[3], float focal, float logicalWidth, float logicalHeight,
	float windowWidth, float windowHeight);
// Sky geometry rotates with the camera but ignores its world translation.
// Preserve the original view-space Y-down offset without assuming an exactly
// orthonormal (fixed-point quantized) basis.
bool DeveloperNativeCamera_BuildBackdropTransform(float world[16], const PsyXNativeView& view, float verticalOffset);
#endif
