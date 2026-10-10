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

// Projects a raw camera-space point through the projection to NDC, matching the
// native shader: column-major mul, then the perspective divide.
static void ProjectNdc(const float projection[16], const float cam[3], float ndc[2])
{
	const float x = projection[0] * cam[0] + projection[4] * cam[1] + projection[8] * cam[2] + projection[12];
	const float y = projection[1] * cam[0] + projection[5] * cam[1] + projection[9] * cam[2] + projection[13];
	const float w = projection[3] * cam[0] + projection[7] * cam[1] + projection[11] * cam[2] + projection[15];
	ndc[0] = x / w;
	ndc[1] = y / w;
}

// The native projection must reproduce the legacy GTE screen mapping from the
// live screen distance h: x_screen = 160 + x*h/z, y_screen = 120 + y*h/z, then
// the PSX display maps to NDC. The shader negates camera-space y, so the native
// NDC.y is the negative of the GTE y-down NDC. At the PSX display aspect the two
// agree exactly.
static void TestGteProjectionParity()
{
	const float dispW = NATIVE_PSX_DISPLAY_WIDTH;
	const float dispH = NATIVE_PSX_DISPLAY_HEIGHT;
	const float aspect = dispW / dispH;
	const float nearPlane = 100.0f;
	const float farPlane = 125000.0f;

	const float h = 320.0f; // representative live scr_z
	const float points[3][3] =
	{
		{ 4096.0f, 0.0f, 16384.0f },
		{ 0.0f, 4096.0f, 16384.0f },
		{ -2048.0f, -3072.0f, 65536.0f },
	};
	const char* names[3] =
	{
		"a camera-space +x point",
		"a camera-space +y point (game down)",
		"an off-axis point",
	};

	float projection[16];
	NativeCameraView_BuildProjection(h, dispW, dispH, dispW * 0.5f, dispH * 0.5f,
		aspect, nearPlane, farPlane, projection);

	for (int i = 0; i < 3; ++i)
	{
		float ndc[2];
		ProjectNdc(projection, points[i], ndc);

		const float xScreen = dispW * 0.5f + points[i][0] * h / points[i][2];
		const float yScreen = dispH * 0.5f + points[i][1] * h / points[i][2];
		const float expectX = 2.0f * xScreen / dispW - 1.0f;
		const float expectY = -(2.0f * yScreen / dispH - 1.0f);

		char description[96];
		snprintf(description, sizeof(description), "h=320: %s projects to the GTE NDC", names[i]);
		CHECK(Near(ndc[0], expectX, 1e-3f) && Near(ndc[1], expectY, 1e-3f), description);
	}

	// The vertical focal is 2h/dispH, independent of the target aspect: a point
	// at camera y == z lands at NDC.y = -(2h/dispH) for any aspect.
	const float verticalPoint[3] = { 0.0f, 1000.0f, 1000.0f };
	float projSquare[16], projWide[16];
	NativeCameraView_BuildProjection(h, dispW, dispH, dispW * 0.5f, dispH * 0.5f,
		1.0f, nearPlane, farPlane, projSquare);
	NativeCameraView_BuildProjection(h, dispW, dispH, dispW * 0.5f, dispH * 0.5f,
		16.0f / 9.0f, nearPlane, farPlane, projWide);
	float ndcSquare[2], ndcWide[2];
	ProjectNdc(projSquare, verticalPoint, ndcSquare);
	ProjectNdc(projWide, verticalPoint, ndcWide);
	CHECK(Near(ndcSquare[1], -(2.0f * h / dispH), 1e-3f) &&
		Near(ndcWide[1], ndcSquare[1], 1e-6f),
		"the vertical focal is 2h/dispH and does not depend on the target aspect");

	// The horizontal scale is f/aspect: widening the target shrinks ndc.x for a
	// fixed point, so the legacy window mapping keeps square pixels.
	const float horizontalPoint[3] = { 1000.0f, 0.0f, 1000.0f };
	ProjectNdc(projSquare, horizontalPoint, ndcSquare);
	ProjectNdc(projWide, horizontalPoint, ndcWide);
	CHECK(Near(ndcWide[0], ndcSquare[0] / (16.0f / 9.0f), 1e-4f),
		"the horizontal scale follows the target aspect (square pixels)");

	// +Z-forward, [0,1] depth: out[11] = +1 keeps clip.w = +z and out[5] is
	// negative so camera-space +y maps to the lower half (game +Y-down).
	CHECK(projection[11] == 1.0f && projection[5] < 0.0f,
		"the projection is +Z-forward and negates camera-space y");

	// The field of view follows the live h: doubling h doubles the projected NDC
	// of a fixed camera-space point.
	float projDouble[16];
	NativeCameraView_BuildProjection(2.0f * h, dispW, dispH, dispW * 0.5f, dispH * 0.5f,
		aspect, nearPlane, farPlane, projDouble);
	ProjectNdc(projDouble, points[0], ndcWide);
	ProjectNdc(projection, points[0], ndcSquare);
	CHECK(Near(ndcWide[0], 2.0f * ndcSquare[0], 1e-4f) &&
		Near(ndcWide[1], 2.0f * ndcSquare[1], 1e-4f),
		"doubling h doubles the projected NDC (the live FOV follows scr_z)");
}

// The GTE screen offset (SetGeomOffset) is a constant NDC translation. The
// legacy maps x_screen = ofx + x*h/z and y_screen = ofy + y*h/z and adds the
// projection offset in clip space (psx.vert:43-47, ofsMat), so after the
// perspective divide it is a constant added to NDC, independent of the point:
//
//   ndc.x += 2*ofx/dispW - 1
//   ndc.y += 1 - 2*ofy/dispH
//
// The PSX display centre (ofx=160, ofy=120) is neutral; the game's live
// ofy=128 moves the image down by 8 PSX lines (24px at 720p).
static void TestGteScreenOffsetParity()
{
	const float dispW = NATIVE_PSX_DISPLAY_WIDTH;
	const float dispH = NATIVE_PSX_DISPLAY_HEIGHT;
	const float aspect = dispW / dispH;
	const float nearPlane = 100.0f;
	const float farPlane = 125000.0f;
	const float h = 320.0f;

	float centred[16], live[16];
	NativeCameraView_BuildProjection(h, dispW, dispH, dispW * 0.5f, dispH * 0.5f,
		aspect, nearPlane, farPlane, centred);
	NativeCameraView_BuildProjection(h, dispW, dispH, 160.0f, 128.0f,
		aspect, nearPlane, farPlane, live);

	// ofx=160 is neutral horizontally; ofy=128 gives 1 - 2*128/240 = -0.0666667.
	const float expectedY = 1.0f - 2.0f * 128.0f / dispH;
	CHECK(Near(live[8], 0.0f, 1e-6f), "ofx=160 gives a zero horizontal translation");
	CHECK(Near(live[9], expectedY, 1e-6f), "ofy=128 gives 1 - 2*128/dispH vertical translation");
	CHECK(Near(expectedY, -0.0666667f, 1e-6f), "ofy=128 vs the centred 120 is -0.0666667 NDC (24px at 720p)");

	// The translation is constant: the live projection is the centred one plus
	// (0, expectedY) for every camera-space point.
	const float points[2][3] =
	{
		{ 0.0f, 0.0f, 16384.0f },
		{ -2048.0f, -3072.0f, 65536.0f },
	};
	for (int i = 0; i < 2; ++i)
	{
		float nc[2], nl[2];
		ProjectNdc(centred, points[i], nc);
		ProjectNdc(live, points[i], nl);
		CHECK(Near(nl[0], nc[0], 1e-4f) && Near(nl[1], nc[1] + expectedY, 1e-4f),
			"the screen offset is a constant NDC.y translation for any camera point");
	}

	// A non-centred ofx shifts horizontally by 2*dOfx/dispW (aspect-independent,
	// like the legacy ofsMat) and ofy by -2*dOfy/dispH.
	float shifted[16];
	NativeCameraView_BuildProjection(h, dispW, dispH, 200.0f, 100.0f,
		aspect, nearPlane, farPlane, shifted);
	CHECK(Near(shifted[8], 2.0f * 200.0f / dispW - 1.0f, 1e-6f) &&
		Near(shifted[9], 1.0f - 2.0f * 100.0f / dispH, 1e-6f),
		"a non-centred offset gives the expected NDC translation");

	// The horizontal translation does not depend on the target aspect, matching
	// the legacy constant clip-space offset; the point-dependent term still
	// follows the aspect.
	float shiftedWide[16];
	NativeCameraView_BuildProjection(h, dispW, dispH, 200.0f, 100.0f,
		16.0f / 9.0f, nearPlane, farPlane, shiftedWide);
	CHECK(Near(shiftedWide[8], shifted[8], 1e-6f),
		"the horizontal screen offset is aspect-independent (legacy ofsMat)");
}

int main()
{
	TestIdentityAtOrigin();
	TestTranslation();
	TestYawRotation();
	TestAgainstAnalytic();
	TestRotationIsUnit();
	TestGteProjectionParity();
	TestGteScreenOffsetParity();

	printf("%d checks, %d failures\n", g_checks, g_failures);
	return g_failures == 0 ? 0 : 1;
}
