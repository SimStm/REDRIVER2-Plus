#include "NativeInstanceTransform.h"

#include <math.h>

void NativeInstance_ComposeTransform(const int pos[3], unsigned int yang, float out[16])
{
	// yang is a 6-bit yaw index; entry i is RotMatrixY(i * 64) over a
	// 4096-unit turn, so masking wraps yang + 64 back to yang.
	const int angle = (int)(yang & 63u) * 64;
	const float a = (float)angle * (2.0f * 3.14159265358979f / 4096.0f);
	const float c = cosf(a);
	const float s = sinf(a);

	out[0] = c;    out[4] = 0.0f; out[8] = s;    out[12] = (float)pos[0];
	out[1] = 0.0f; out[5] = 1.0f; out[9] = 0.0f; out[13] = (float)pos[1];
	out[2] = -s;   out[6] = 0.0f; out[10] = c;   out[14] = (float)pos[2];
	out[3] = 0.0f; out[7] = 0.0f; out[11] = 0.0f; out[15] = 1.0f;
}
