#ifndef REDRIVER2_WORLD_SHADE_PALETTE_H
#define REDRIVER2_WORLD_SHADE_PALETTE_H

// Copies the legacy f4colourTable plane pattern into tightly packed RGB bytes.
// Each 4-entry group is {plane[i], plane[0], plane[5], plane[0]}.
static inline void WorldShadePalette_WriteRgb(const unsigned int planeColours[8],
	unsigned char rgb[32 * 3])
{
	for (int i = 0; i < 32; ++i)
	{
		const int component = i & 3;
		const unsigned int colour = planeColours[component == 0 ? i / 4 : (component == 2 ? 5 : 0)];
		rgb[i * 3 + 0] = (unsigned char)(colour >> 16);
		rgb[i * 3 + 1] = (unsigned char)(colour >> 8);
		rgb[i * 3 + 2] = (unsigned char)colour;
	}
}

#endif
