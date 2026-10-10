// Pure tests for the E03 native instance placement helper
// (utils/NativeInstanceTransform.*). No GPU, no game assets, no PSX headers.
//
// The helper composes T = Translate(pos) * RotY(yang * 64) in raw game units,
// column-major. It must agree with the game's object rotation table
// matrixtable[i] = RotMatrixY(i * 64) over a 4096-unit turn
// (Game/C/draw.c:507-525, 1508-1517) and with utils/DeveloperModernMesh.cpp
// BuildInstanceWorld (484-492), and yang must wrap at 64.
//
// TestGamePipelineParity additionally pins the full game draw pipeline: the
// game builds CompoundMatrix[yang] = MulMatrix0(inv_camera_matrix,
// matrixtable[yang]) and trans = inv_camera_matrix * (pos - camera_position)
// (draw.c:1505-1545, rndrasm.c:63-93), then transforms a vertex as
// CompoundMatrix * v + trans. The native path applies the same view
// (utils/NativeCameraView.h) to the instance world matrix, so
// view * NativeInstance_ComposeTransform(pos, yang) must equal that game camera
// matrix for every yang and point.

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

// --- Game draw-pipeline parity ---------------------------------------------
//
// These helpers reimplement, locally and in float, the exact game math the
// native placement must replace:
//   * matrixtable[i] = RotMatrixY(i * 64)              (draw.c:507-525)
//   * CompoundMatrix = MulMatrix0(inv_camera_matrix, matrixtable[yang])
//                                                       (draw.c:1508-1517)
//   * trans = inv_camera_matrix * (pos - camera_position) (rndrasm.c:63-93)
//   * camera = CompoundMatrix * v + trans               (GTE, PsyX_GTE.cpp:382)
// No Game/PSX headers are used.

// Row-major 3x3 product m0 * m1, matching the game's MulMatrix0 (the "correct
// Psy-Q" branch computes m2[i][j] = sum_k m0[i][k] * m1[k][j];
// PsyCross/src/psx/LIBGTE.C:263-337).
static void MulMatrix0(const float m0[3][3], const float m1[3][3], float out[3][3])
{
	for (int i = 0; i < 3; ++i)
		for (int j = 0; j < 3; ++j)
			out[i][j] = m0[i][0] * m1[0][j] + m0[i][1] * m1[1][j] + m0[i][2] * m1[2][j];
}

// Column-major world->camera view [R | -R*cam], matching the native view
// (utils/NativeCameraView.cpp) and the game's inv_camera_matrix/camera_position.
static void BuildViewColumnMajor(const float r[3][3], const float cam[3], float out[16])
{
	out[0] = r[0][0]; out[4] = r[0][1]; out[8] = r[0][2];
	out[1] = r[1][0]; out[5] = r[1][1]; out[9] = r[1][2];
	out[2] = r[2][0]; out[6] = r[2][1]; out[10] = r[2][2];
	out[12] = -(r[0][0] * cam[0] + r[0][1] * cam[1] + r[0][2] * cam[2]);
	out[13] = -(r[1][0] * cam[0] + r[1][1] * cam[1] + r[1][2] * cam[2]);
	out[14] = -(r[2][0] * cam[0] + r[2][1] * cam[1] + r[2][2] * cam[2]);
	out[3] = 0.0f; out[7] = 0.0f; out[11] = 0.0f; out[15] = 1.0f;
}

// Column-major 4x4 product, matching NativeRenderer.cpp MultiplyColumnMajor and
// the shader's mul(MVP, float4(pos, 1)).
static void MultiplyColumnMajor(const float a[16], const float b[16], float out[16])
{
	for (int col = 0; col < 4; ++col)
		for (int row = 0; row < 4; ++row)
		{
			float sum = 0.0f;
			for (int k = 0; k < 4; ++k)
				sum += a[k * 4 + row] * b[col * 4 + k];
			out[col * 4 + row] = sum;
		}
}

// The game's per-object camera matrix as a column-major 4x4:
//   [ CompoundMatrix[yang] | inv_camera_matrix * (pos - camera_position) ].
static void BuildGameCameraMatrix(const float rCam[3][3], const float cam[3],
	const float pos[3], unsigned int yang, float out[16])
{
	float obj[3][3];
	RotMatrixYAnalytic((int)(yang & 63u) * 64, obj);

	float compound[3][3];
	MulMatrix0(rCam, obj, compound);

	const float dx = pos[0] - cam[0];
	const float dy = pos[1] - cam[1];
	const float dz = pos[2] - cam[2];
	const float tx = rCam[0][0] * dx + rCam[0][1] * dy + rCam[0][2] * dz;
	const float ty = rCam[1][0] * dx + rCam[1][1] * dy + rCam[1][2] * dz;
	const float tz = rCam[2][0] * dx + rCam[2][1] * dy + rCam[2][2] * dz;

	out[0] = compound[0][0]; out[4] = compound[0][1]; out[8] = compound[0][2]; out[12] = tx;
	out[1] = compound[1][0]; out[5] = compound[1][1]; out[9] = compound[1][2]; out[13] = ty;
	out[2] = compound[2][0]; out[6] = compound[2][1]; out[10] = compound[2][2]; out[14] = tz;
	out[3] = 0.0f; out[7] = 0.0f; out[11] = 0.0f; out[15] = 1.0f;
}

static void TestGamePipelineParity()
{
	// A non-trivial camera and a non-zero translation: a wrong handedness,
	// transpose, angle unit or translation would change the result.
	float rCam[3][3];
	RotMatrixYAnalytic(700, rCam); // camera yawed 700/4096 of a turn
	const float cam[3] = { 12.0f, 34.0f, -56.0f };
	const int pos[3] = { 1234, -56, 7890 };
	const float fpos[3] = { (float)pos[0], (float)pos[1], (float)pos[2] };

	// The requested representative *angles* 0,512,1024,2048,3072,4095 map to
	// matrixtable entries angle/64: 0,8,16,32,48,63 (4095 quantises down to
	// entry 63 = RotMatrixY(4032), the closest table entry). yang is the index.
	CHECK(((0 / 64) & 63) == 0 && ((512 / 64) & 63) == 8 && ((1024 / 64) & 63) == 16 &&
		((2048 / 64) & 63) == 32 && ((3072 / 64) & 63) == 48 && ((4095 / 64) & 63) == 63,
		"representative angles map to yang indices 0,8,16,32,48,63");

	const unsigned int yangs[] = { 0, 8, 16, 32, 48, 63 };
	const float points[3][3] = { { 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } };

	float view[16];
	BuildViewColumnMajor(rCam, cam, view);

	for (int c = 0; c < (int)(sizeof(yangs) / sizeof(yangs[0])); ++c)
	{
		const unsigned int yang = yangs[c];

		float gameCamera[16];
		BuildGameCameraMatrix(rCam, cam, fpos, yang, gameCamera);

		float world[16];
		NativeInstance_ComposeTransform(pos, yang, world);

		float nativeCamera[16];
		MultiplyColumnMajor(view, world, nativeCamera);

		CHECK(MatrixNear(gameCamera, nativeCamera, 1e-3f),
			"native view*world equals the game MulMatrix0(inv_cam, matrixtable) matrix");
	}

	// Exhaustive: every yang index must agree on all three basis points.
	for (unsigned int yang = 0; yang < 64; ++yang)
	{
		float gameCamera[16];
		BuildGameCameraMatrix(rCam, cam, fpos, yang, gameCamera);

		float world[16];
		NativeInstance_ComposeTransform(pos, yang, world);

		float nativeCamera[16];
		MultiplyColumnMajor(view, world, nativeCamera);

		bool agree = true;
		for (int p = 0; p < 3; ++p)
		{
			float g[3];
			float n[3];
			TransformPoint(gameCamera, points[p], g);
			TransformPoint(nativeCamera, points[p], n);
			if (!Near(g[0], n[0], 1e-3f) || !Near(g[1], n[1], 1e-3f) || !Near(g[2], n[2], 1e-3f))
				agree = false;
		}
		CHECK(agree, "game and native map the same world points to the same camera-space points");
	}
}

static void TestYDecodeConvention()
{
	// The packed cell object stores y pre-doubled (ppco->pos.vy = y<<1 | flag)
	// and QuickUnpackCellObject decodes it as (short)vy >> 1 (cell.h:17). The
	// region-draw bridge submits that decoded value (assetcatalog_game.c:407-410),
	// so this helper must copy y verbatim and must not shift it again.
	const int decodedY = 1234;
	const int modelHighBit = 1; // (ppco->pos.vy & 1) selects the model-type high bit
	const int packedVy = (decodedY << 1) | modelHighBit;
	const int unpackedY = (int)((short)packedVy >> 1);

	CHECK(unpackedY == decodedY, "the packed vy decodes to y>>1");

	const int pos[3] = { 10, unpackedY, 20 };
	float m[16];
	NativeInstance_ComposeTransform(pos, 0, m);
	CHECK(Near(m[13], (float)decodedY, 1e-6f),
		"compose copies the decoded y verbatim (no second halving)");
	CHECK(!Near(m[13], (float)packedVy, 1e-6f),
		"compose does not use the packed (doubled) vy");
}

int main()
{
	TestCardinalAngles();
	TestTranslationAndLayout();
	TestYangWrap();
	TestAgainstAnalytic();
	TestGamePipelineParity();
	TestYDecodeConvention();

	printf("%d checks, %d failures\n", g_checks, g_failures);
	return g_failures == 0 ? 0 : 1;
}
