#ifndef WORLD_SPRITE_SHADE_H
#define WORLD_SPRITE_SHADE_H

enum WorldSpriteShadeMode
{
	WORLD_SPRITE_SHADE_NONE = 0,
	WORLD_SPRITE_SHADE_CAMERA = 1,
	WORLD_SPRITE_SHADE_NEUTRAL = 2
};

#ifdef __cplusplus
#include <vector>
#include <stddef.h>

inline int WorldSpriteShade_Mode(bool sprite, unsigned rotation, bool lit)
{
	return !sprite ? WORLD_SPRITE_SHADE_NONE :
		((rotation & 63) == 63 || lit ? WORLD_SPRITE_SHADE_NEUTRAL : WORLD_SPRITE_SHADE_CAMERA);
}

// Exact DrawSprites arithmetic: FIXEDH on each dot-product term and unsigned
// byte assignment after each stage, including the original night wrap.
inline unsigned char WorldSpriteShade_LightLevel(const short cameraForward[3], const short light[3],
	int city, bool day, bool night, bool dawnOrDusk, int weather, int nightAmbient)
{
	int dot = 4096 * 3072;
	for (int axis = 0; axis < 3; ++axis) dot += (cameraForward[axis] * light[axis] + 2048) >> 12;
	unsigned char level = (unsigned char)((dot >> 18) + 32);
	if ((weather > 0 && day) || dawnOrDusk) level = (unsigned char)((level * 2 * nightAmbient) >> 8);
	if (night) level = (unsigned char)(city == 0 ? level * 2 : level / 3);
	return level;
}

inline bool WorldSpriteShade_CaptureNeutral(bool sprite, size_t vertices, std::vector<unsigned char>& colors)
{
	if (!sprite) return false;
	colors.assign(vertices * 3, 128);
	return true;
}
#endif
#endif
