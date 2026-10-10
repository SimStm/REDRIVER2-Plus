#include "NativeCameraView.h"

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
