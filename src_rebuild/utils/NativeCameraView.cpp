#include "NativeCameraView.h"
#include <math.h>

void NativeCameraView_Compose(const int rotation[9], const int cameraPosition[3], float out[16])
{
	// inv_camera_matrix stores the rotation in fixed point (4096 = 1.0); the
	// view matrix and the vertices share raw game units, so the rotation is
	// scaled to a unit basis while the translation stays raw.
	const float scale = 1.0f / 4096.0f;
	float r[3][3];
	for (int row = 0; row < 3; ++row)
		for (int col = 0; col < 3; ++col)
			r[row][col] = (float)rotation[row * 3 + col] * scale;

	const float px = (float)cameraPosition[0];
	const float py = (float)cameraPosition[1];
	const float pz = (float)cameraPosition[2];

	// Column-major: out[col * 4 + row].
	out[0] = r[0][0]; out[4] = r[0][1]; out[8] = r[0][2];
	out[1] = r[1][0]; out[5] = r[1][1]; out[9] = r[1][2];
	out[2] = r[2][0]; out[6] = r[2][1]; out[10] = r[2][2];

	// Translation is -R * camera_position, raw units.
	out[12] = -(r[0][0] * px + r[0][1] * py + r[0][2] * pz);
	out[13] = -(r[1][0] * px + r[1][1] * py + r[1][2] * pz);
	out[14] = -(r[2][0] * px + r[2][1] * py + r[2][2] * pz);

	out[3] = 0.0f; out[7] = 0.0f; out[11] = 0.0f; out[15] = 1.0f;
}

int NativeCameraView_BoundsVisible(const float mvp[16], const float minimum[3],
	const float maximum[3])
{
	// Only reject when all eight corners lie strictly outside the same plane.
	// No perspective division: negative w and boxes crossing the near plane
	// are handled by the homogeneous half-spaces themselves.
	unsigned int sharedOutside = 63;
	for (int corner = 0; corner < 8; ++corner)
	{
		const float x = (corner & 1) ? maximum[0] : minimum[0];
		const float y = (corner & 2) ? maximum[1] : minimum[1];
		const float z = (corner & 4) ? maximum[2] : minimum[2];
		const float cx = mvp[0]*x + mvp[4]*y + mvp[8]*z + mvp[12];
		const float cy = mvp[1]*x + mvp[5]*y + mvp[9]*z + mvp[13];
		const float cz = mvp[2]*x + mvp[6]*y + mvp[10]*z + mvp[14];
		const float cw = mvp[3]*x + mvp[7]*y + mvp[11]*z + mvp[15];
		// Float roundoff at a plane must not remove a touching object. Scale the
		// margin to the homogeneous coordinate magnitude (raw units can be large).
		const float epsilon = 0.00001f * (fabsf(cx) + fabsf(cy) + fabsf(cz) + fabsf(cw) + 1.0f);
		unsigned int outside = 0;
		if (cx < -cw - epsilon) outside |= 1;
		if (cx >  cw + epsilon) outside |= 2;
		if (cy < -cw - epsilon) outside |= 4;
		if (cy >  cw + epsilon) outside |= 8;
		if (cz < -epsilon) outside |= 16;
		if (cz > cw + epsilon) outside |= 32;
		sharedOutside &= outside;
	}
	return sharedOutside == 0;
}
