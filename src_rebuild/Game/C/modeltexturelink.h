#ifndef MODEL_TEXTURE_LINK_H
#define MODEL_TEXTURE_LINK_H

// Extracts the catalog texture key from a source polygon record when its
// declared type carries a texture set/index in bytes 1 and 2.
static inline int ModelTextureLink_GetTextureKey(const unsigned char* polygon,
	int* textureSet, int* textureId)
{
	if (polygon == 0 || textureSet == 0 || textureId == 0)
		return 0;

	switch (polygon[0] & 0x1F)
	{
	case 4:
	case 5:
	case 6:
	case 7:
	case 10:
	case 11:
	case 20:
	case 21:
	case 22:
	case 23:
		*textureSet = polygon[1];
		*textureId = polygon[2];
		return 1;
	default:
		return 0;
	}
}

#endif
