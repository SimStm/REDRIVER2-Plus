#ifndef PRESENTATION_INTERPOLATION_H
#define PRESENTATION_INTERPOLATION_H

#include <math.h>

// Visual-only arithmetic. No values from this namespace are fed into physics.
namespace PresentationInterpolation
{
inline int ClampAlpha(int alpha)
{
	return alpha < 0 ? 0 : (alpha > 256 ? 256 : alpha);
}

inline int Lerp(int previous, int current, int alpha)
{
	// World coordinates can be far apart at a discontinuity. Widen before
	// subtracting/multiplying; round negative deltas consistently with positive ones.
	return (int)((long long)previous +
		((long long)current - previous) * ClampAlpha(alpha) / 256);
}

inline short Angle(short previous, short current, int alpha)
{
	const int delta = ((current - previous + 2048) & 4095) - 2048;
	return (short)((previous + delta * ClampAlpha(alpha) / 256) & 4095);
}

struct Rotation
{
	double q[4];
	double scale[3];
};

inline Rotation ReadRotation(const short matrix[3][3])
{
	Rotation result = {};
	double m[3][3];
	for (int row = 0; row < 3; ++row)
	{
		double length = 0;
		for (int col = 0; col < 3; ++col)
			length += (double)matrix[row][col] * matrix[row][col];
		result.scale[row] = sqrt(length);
		for (int col = 0; col < 3; ++col)
			m[row][col] = result.scale[row] > 0 ? matrix[row][col] / result.scale[row] : (row == col ? 1.0 : 0.0);
	}

	const double trace = m[0][0] + m[1][1] + m[2][2];
	if (trace > 0)
	{
		const double s = sqrt(trace + 1) * 2;
		result.q[3] = s / 4;
		result.q[0] = (m[2][1] - m[1][2]) / s;
		result.q[1] = (m[0][2] - m[2][0]) / s;
		result.q[2] = (m[1][0] - m[0][1]) / s;
	}
	else
	{
		int i = m[1][1] > m[0][0] ? 1 : 0;
		if (m[2][2] > m[i][i]) i = 2;
		const int j = (i + 1) % 3, k = (i + 2) % 3;
		const double s = sqrt(1 + m[i][i] - m[j][j] - m[k][k]) * 2;
		result.q[i] = s / 4;
		result.q[j] = (m[i][j] + m[j][i]) / s;
		result.q[k] = (m[i][k] + m[k][i]) / s;
		result.q[3] = (m[k][j] - m[j][k]) / s;
	}
	return result;
}

inline void Matrix(const short previous[3][3], const short current[3][3], int alpha, short out[3][3])
{
	alpha = ClampAlpha(alpha);
	if (alpha == 0 || alpha == 256)
	{
		for (int row = 0; row < 3; ++row)
			for (int col = 0; col < 3; ++col)
				out[row][col] = alpha == 0 ? previous[row][col] : current[row][col];
		return;
	}

	const Rotation a = ReadRotation(previous), b = ReadRotation(current);
	double dot = 0;
	for (int i = 0; i < 4; ++i) dot += a.q[i] * b.q[i];
	const double t = alpha / 256.0;
	double q[4], length = 0;
	for (int i = 0; i < 4; ++i)
	{
		// q and -q encode the same rotation. Choose the short arc across wrap.
		q[i] = a.q[i] * (1 - t) + b.q[i] * (dot < 0 ? -t : t);
		length += q[i] * q[i];
	}
	length = sqrt(length);
	for (int i = 0; i < 4; ++i) q[i] /= length;
	const double x = q[0], y = q[1], z = q[2], w = q[3];
	const double rotation[3][3] = {
		{1 - 2*(y*y + z*z), 2*(x*y - z*w), 2*(x*z + y*w)},
		{2*(x*y + z*w), 1 - 2*(x*x + z*z), 2*(y*z - x*w)},
		{2*(x*z - y*w), 2*(y*z + x*w), 1 - 2*(x*x + y*y)}
	};
	for (int row = 0; row < 3; ++row)
	{
		const double scale = a.scale[row] * (1 - t) + b.scale[row] * t;
		for (int col = 0; col < 3; ++col)
		{
			const double value = rotation[row][col] * scale;
			out[row][col] = (short)(value + (value < 0 ? -0.5 : 0.5));
		}
	}
}
}

#endif
