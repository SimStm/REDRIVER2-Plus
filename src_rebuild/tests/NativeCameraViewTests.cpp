// Pure tests for the E03 native camera view helper (utils/NativeCameraView.*).
// No GPU, no game assets, no PSX headers.
//
// The helper composes the raw-unit world->camera view matrix
// view = [ R | -R*cam ] from the game's inverse camera rotation (fixed point,
// 4096 = 1.0) and the camera position in raw units, so camera space is
// R * (world - cam) and +Z is forward (Game/ASM/rndrasm.c; the native
// projection is +Z-forward). A point ahead must have positive camera z, one
// behind negative.

#include "../utils/NativeCameraView.h"

#include <math.h>
#include <stdio.h>

static int g_checks = 0;
static int g_failures = 0;

static void CHECK(bool passed, const char* description)
{
	g_checks++;
	printf("%s: %s\n", passed ? "PASS" : "FAIL", description);
	if (!passed)
		g_failures++;
}

static bool Near(float a, float b, float tolerance)
{
	return fabsf(a - b) <= tolerance;
}

// Column-major matrix * (x,y,z,1), matching the native shader.
static void TransformPoint(const float m[16], const float p[3], float out[3])
{
	out[0] = m[0] * p[0] + m[4] * p[1] + m[8] * p[2] + m[12];
	out[1] = m[1] * p[0] + m[5] * p[1] + m[9] * p[2] + m[13];
	out[2] = m[2] * p[0] + m[6] * p[1] + m[10] * p[2] + m[14];
}

// Identity inverse-camera rotation in fixed point (4096 = 1.0).
static void IdentityRotation(int rotation[9])
{
	for (int i = 0; i < 9; ++i)
		rotation[i] = 0;
	rotation[0] = 4096;
	rotation[4] = 4096;
	rotation[8] = 4096;
}

// Independent analytic PSX RotMatrixY at `angle` (4096-unit turn), row-major
// m[row][col] in fixed point (matches PsyCross RotMatrixY).
static void RotMatrixYFixed(int angle, int m[9])
{
	const float a = (float)(angle & 4095) * (2.0f * 3.14159265358979f / 4096.0f);
	const float c = cosf(a);
	const float s = sinf(a);
	m[0] = (int)lroundf(c * 4096.0f); m[1] = 0; m[2] = (int)lroundf(s * 4096.0f);
	m[3] = 0; m[4] = 4096; m[5] = 0;
	m[6] = (int)lroundf(-s * 4096.0f); m[7] = 0; m[8] = (int)lroundf(c * 4096.0f);
}

// The camera at the origin with an identity rotation is a pass-through: a world
// point maps to the same raw camera-space point, and +Z is ahead.
static void TestIdentityAtOrigin()
{
	int rotation[9];
	IdentityRotation(rotation);
	const int camera[3] = { 0, 0, 0 };

	float view[16];
	NativeCameraView_Compose(rotation, camera, view);

	const float p[3] = { 0.0f, 0.0f, 4096.0f };
	float cam[3];
	TransformPoint(view, p, cam);
	CHECK(Near(cam[0], 0.0f, 1e-3f) && Near(cam[1], 0.0f, 1e-3f) && Near(cam[2], 4096.0f, 1e-3f),
		"identity camera at origin passes a world point through unchanged");
	CHECK(cam[2] > 0.0f, "a point at +Z ahead has positive camera z");

	const float behind[3] = { 0.0f, 0.0f, -4096.0f };
	TransformPoint(view, behind, cam);
	CHECK(cam[2] < 0.0f, "a point at -Z behind has negative camera z (clipped)");
}

// A translated camera subtracts its world position, raw units.
static void TestTranslation()
{
	int rotation[9];
	IdentityRotation(rotation);
	const int camera[3] = { 4096, -2048, 8192 };

	float view[16];
	NativeCameraView_Compose(rotation, camera, view);

	const float p[3] = { 4096.0f + 1000.0f, -2048.0f + 500.0f, 8192.0f + 4096.0f };
	float cam[3];
	TransformPoint(view, p, cam);
	CHECK(Near(cam[0], 1000.0f, 1e-3f) && Near(cam[1], 500.0f, 1e-3f) && Near(cam[2], 4096.0f, 1e-3f),
		"camera translation is subtracted in raw units");
}

// A rotated camera maps the world axes as the analytic rotation dictates.
static void TestYawRotation()
{
	int rotation[9];
	RotMatrixYFixed(1024, rotation); // +90 degrees
	const int camera[3] = { 0, 0, 0 };

	float view[16];
	NativeCameraView_Compose(rotation, camera, view);

	// RotMatrixY(90) maps +Z world to +X camera.
	const float p[3] = { 0.0f, 0.0f, 4096.0f };
	float cam[3];
	TransformPoint(view, p, cam);
	CHECK(Near(cam[0], 4096.0f, 1.0f) && Near(cam[1], 0.0f, 1e-3f) && Near(cam[2], 0.0f, 1.0f),
		"a +90 degree yaw maps world +Z to camera +X");
}

// The composition equals the independent R * (world - cam) for a rotated,
// translated camera.
static void TestAgainstAnalytic()
{
	int rotation[9];
	RotMatrixYFixed(700, rotation);
	const int camera[3] = { 1234, -56, 7890 };

	float view[16];
	NativeCameraView_Compose(rotation, camera, view);

	const float p[3] = { 5000.0f, 200.0f, -300.0f };
	float cam[3];
	TransformPoint(view, p, cam);

	const float scale = 1.0f / 4096.0f;
	const float dx = p[0] - (float)camera[0];
	const float dy = p[1] - (float)camera[1];
	const float dz = p[2] - (float)camera[2];
	const float expect[3] =
	{
		((float)rotation[0] * dx + (float)rotation[1] * dy + (float)rotation[2] * dz) * scale,
		((float)rotation[3] * dx + (float)rotation[4] * dy + (float)rotation[5] * dz) * scale,
		((float)rotation[6] * dx + (float)rotation[7] * dy + (float)rotation[8] * dz) * scale,
	};
	CHECK(Near(cam[0], expect[0], 1e-2f) && Near(cam[1], expect[1], 1e-2f) && Near(cam[2], expect[2], 1e-2f),
		"view equals the independent R * (world - cam)");
}

// The rotation part is a proper unit rotation in raw units.
static void TestRotationIsUnit()
{
	int rotation[9];
	RotMatrixYFixed(1024, rotation);
	const int camera[3] = { 0, 0, 0 };

	float view[16];
	NativeCameraView_Compose(rotation, camera, view);

	const float col0[3] = { view[0], view[1], view[2] };
	const float col2[3] = { view[8], view[9], view[10] };
	const float len0 = col0[0] * col0[0] + col0[1] * col0[1] + col0[2] * col0[2];
	const float len2 = col2[0] * col2[0] + col2[1] * col2[1] + col2[2] * col2[2];
	const float dot02 = col0[0] * col2[0] + col0[1] * col2[1] + col0[2] * col2[2];
	CHECK(Near(len0, 1.0f, 1e-4f) && Near(len2, 1.0f, 1e-4f), "view rotation columns are unit length");
	CHECK(Near(dot02, 0.0f, 1e-4f), "view rotation columns are orthogonal");
	CHECK(Near(view[3], 0.0f, 1e-6f) && Near(view[7], 0.0f, 1e-6f) &&
		Near(view[11], 0.0f, 1e-6f) && Near(view[15], 1.0f, 1e-6f), "affine bottom row is 0,0,0,1");
}

int main()
{
	TestIdentityAtOrigin();
	TestTranslation();
	TestYawRotation();
	TestAgainstAnalytic();
	TestRotationIsUnit();

	printf("%d checks, %d failures\n", g_checks, g_failures);
	return g_failures == 0 ? 0 : 1;
}
