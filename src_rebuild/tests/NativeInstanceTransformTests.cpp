// Pure tests for the E03 native instance placement helper
// (utils/NativeInstanceTransform.*). No GPU, no game assets, no PSX headers.
//
// The helper composes T = Translate(pos) * RotY(yang * 64) in raw game units,
// column-major. It must agree with the game's object rotation table
// matrixtable[i] = RotMatrixY(i * 64) over a 4096-unit turn
// (Game/C/draw.c:507-525, 1508-1517) and with utils/DeveloperModernMesh.cpp
// BuildInstanceWorld (484-492), and yang must wrap at 64.

#include "../utils/NativeInstanceTransform.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

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

// Independent analytic PSX RotMatrixY at `angle` (4096-unit turn), row-major
// m[row][col] (matches PsyCross RotMatrixY applied to identity).
static void RotMatrixYAnalytic(int angle, float m[3][3])
{
	const float a = (float)(angle & 4095) * (2.0f * 3.14159265358979f / 4096.0f);
	const float c = cosf(a);
	const float s = sinf(a);
	m[0][0] = c;    m[0][1] = 0.0f; m[0][2] = s;
	m[1][0] = 0.0f; m[1][1] = 1.0f; m[1][2] = 0.0f;
	m[2][0] = -s;   m[2][1] = 0.0f; m[2][2] = c;
}

// Column-major matrix * (x,y,z,1), matching the native shader.
static void TransformPoint(const float m[16], const float p[3], float out[3])
{
	out[0] = m[0] * p[0] + m[4] * p[1] + m[8] * p[2] + m[12];
	out[1] = m[1] * p[0] + m[5] * p[1] + m[9] * p[2] + m[13];
	out[2] = m[2] * p[0] + m[6] * p[1] + m[10] * p[2] + m[14];
}

static bool MatrixNear(const float a[16], const float b[16], float tolerance)
{
	for (int i = 0; i < 16; ++i)
		if (!Near(a[i], b[i], tolerance))
			return false;
	return true;
}

static void TestCardinalAngles()
{
	const int pos[3] = { 0, 0, 0 };
	float m[16];

	// yang = 0: identity rotation.
	NativeInstance_ComposeTransform(pos, 0, m);
	CHECK(Near(m[0], 1.0f, 1e-5f) && Near(m[2], 0.0f, 1e-5f) &&
		Near(m[8], 0.0f, 1e-5f) && Near(m[10], 1.0f, 1e-5f), "yang 0 is the identity rotation");

	// yang = 16: a quarter turn. RotMatrixY(1024) = [[0,0,1],[0,1,0],[-1,0,0]],
	// so column-major out[2] = -1 and out[8] = +1.
	NativeInstance_ComposeTransform(pos, 16, m);
	CHECK(Near(m[0], 0.0f, 1e-5f) && Near(m[2], -1.0f, 1e-5f) &&
		Near(m[8], 1.0f, 1e-5f) && Near(m[10], 0.0f, 1e-5f), "yang 16 is a +90 degree Y rotation");

	// yang = 32: a half turn.
	NativeInstance_ComposeTransform(pos, 32, m);
	CHECK(Near(m[0], -1.0f, 1e-5f) && Near(m[2], 0.0f, 1e-5f) &&
		Near(m[8], 0.0f, 1e-5f) && Near(m[10], -1.0f, 1e-5f), "yang 32 is a 180 degree Y rotation");

	// yang = 48: three quarter turns.
	NativeInstance_ComposeTransform(pos, 48, m);
	CHECK(Near(m[0], 0.0f, 1e-5f) && Near(m[2], 1.0f, 1e-5f) &&
		Near(m[8], -1.0f, 1e-5f) && Near(m[10], 0.0f, 1e-5f), "yang 48 is a -90 degree Y rotation");
}

static void TestTranslationAndLayout()
{
	const int pos[3] = { 1234, -56, 7890 };
	float m[16];
	NativeInstance_ComposeTransform(pos, 0, m);

	CHECK(Near(m[12], 1234.0f, 1e-3f) && Near(m[13], -56.0f, 1e-3f) &&
		Near(m[14], 7890.0f, 1e-3f), "translation is copied in raw units");
	CHECK(Near(m[3], 0.0f, 1e-6f) && Near(m[7], 0.0f, 1e-6f) &&
		Near(m[11], 0.0f, 1e-6f) && Near(m[15], 1.0f, 1e-6f), "affine bottom row is 0,0,0,1");
	CHECK(Near(m[1], 0.0f, 1e-6f) && Near(m[4], 0.0f, 1e-6f) &&
		Near(m[6], 0.0f, 1e-6f) && Near(m[9], 0.0f, 1e-6f), "Y rotation keeps off-axis entries zero");

	// A local point maps to pos + RotY * p: (1,0,0) -> (c + px, py, -s + pz).
	const float p[3] = { 1, 0, 0 };
	float world[3];
	TransformPoint(m, p, world);
	CHECK(Near(world[0], 1235.0f, 1e-3f) && Near(world[1], -56.0f, 1e-3f) &&
		Near(world[2], 7890.0f, 1e-3f), "identity placement maps a local point by translation");
}

static void TestYangWrap()
{
	// yang is a 6-bit index; yang and yang + 64 select the same matrix entry.
	for (unsigned int yang = 0; yang < 64; ++yang)
	{
		const int pos[3] = { 100, 200, 300 };
		float a[16];
		float b[16];
		float c[16];
		NativeInstance_ComposeTransform(pos, yang, a);
		NativeInstance_ComposeTransform(pos, yang + 64, b);
		NativeInstance_ComposeTransform(pos, yang + 128, c);
		CHECK(MatrixNear(a, b, 1e-5f), "yang and yang + 64 select the same matrix");
		CHECK(MatrixNear(a, c, 1e-5f), "yang and yang + 128 select the same matrix");
	}
}

static void TestAgainstAnalytic()
{
	// The composed rotation must equal the independent analytic RotMatrixY at
	// yang * 64 for every entry, and be a proper rotation (orthonormal,
	// right-handed).
	const int pos[3] = { 0, 0, 0 };
	for (unsigned int yang = 0; yang < 64; ++yang)
	{
		float m[16];
		NativeInstance_ComposeTransform(pos, yang, m);

		float analytic[3][3];
		RotMatrixYAnalytic((int)yang * 64, analytic);

		CHECK(Near(m[0], analytic[0][0], 1e-5f) && Near(m[2], analytic[2][0], 1e-5f) &&
			Near(m[8], analytic[0][2], 1e-5f) && Near(m[10], analytic[2][2], 1e-5f) &&
			Near(m[5], 1.0f, 1e-5f), "rotation matches analytic RotMatrixY(yang * 64)");

		// Column vectors are unit length and orthogonal (proper rotation).
		const float col0[3] = { m[0], m[1], m[2] };
		const float col1[3] = { m[4], m[5], m[6] };
		const float col2[3] = { m[8], m[9], m[10] };
		const float len0 = col0[0] * col0[0] + col0[1] * col0[1] + col0[2] * col0[2];
		const float len2 = col2[0] * col2[0] + col2[1] * col2[1] + col2[2] * col2[2];
		const float dot02 = col0[0] * col2[0] + col0[1] * col2[1] + col0[2] * col2[2];
		CHECK(Near(len0, 1.0f, 1e-4f) && Near(len2, 1.0f, 1e-4f), "rotation columns are unit length");
		CHECK(Near(dot02, 0.0f, 1e-4f), "rotation columns are orthogonal");
		CHECK(Near(col1[0], 0.0f, 1e-6f) && Near(col1[1], 1.0f, 1e-6f) && Near(col1[2], 0.0f, 1e-6f),
			"rotation keeps the Y axis fixed");
	}
}

int main()
{
	TestCardinalAngles();
	TestTranslationAndLayout();
	TestYangWrap();
	TestAgainstAnalytic();

	printf("%d checks, %d failures\n", g_checks, g_failures);
	return g_failures == 0 ? 0 : 1;
}
