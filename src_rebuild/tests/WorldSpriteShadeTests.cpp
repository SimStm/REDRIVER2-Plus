#include "../utils/WorldSpriteShade.h"
#include "../utils/WorldMeshBuilder.h"
#include <stdio.h>
static int failures = 0;
#define CHECK(x) do { if (!(x)) { ++failures; printf("FAIL line %d: %s\n", __LINE__, #x); } } while (0)

static void OriginalLightArithmetic()
{
	const short zero[3] = {};
	CHECK(WorldSpriteShade_LightLevel(zero,zero,0,true,false,false,0,128) == 80);
	CHECK(WorldSpriteShade_LightLevel(zero,zero,0,false,true,false,0,128) == 160);
	CHECK(WorldSpriteShade_LightLevel(zero,zero,3,false,true,false,0,128) == 26);
	CHECK(WorldSpriteShade_LightLevel(zero,zero,1,true,false,false,1,78) == 48);
	CHECK(WorldSpriteShade_LightLevel(zero,zero,1,false,false,true,0,26) == 16);
	CHECK(WorldSpriteShade_LightLevel(zero,zero,1,false,false,true,0,511) == 63); // byte wrap after weather stage
	// Independent spelling of original DrawSprites; asymmetric signed inputs
	// exercise per-term FIXEDH rather than rounding the accumulated dot product.
	const short forward[3] = {-2317,1231,3907}, light[3] = {31999,-17431,29007};
	const unsigned dot = ((forward[0]*light[0]+2048)>>12) + ((forward[1]*light[1]+2048)>>12) + ((forward[2]*light[2]+2048)>>12) + 4096*3072;
	const unsigned char expected = (unsigned char)((dot >> 18) + 32);
	CHECK(WorldSpriteShade_LightLevel(forward,light,2,true,false,false,0,128) == expected);
	CHECK(WorldSpriteShade_Mode(false,63,true) == WORLD_SPRITE_SHADE_NONE);
	CHECK(WorldSpriteShade_Mode(true,0,false) == WORLD_SPRITE_SHADE_CAMERA);
	CHECK(WorldSpriteShade_Mode(true,63,false) == WORLD_SPRITE_SHADE_NEUTRAL);
	CHECK(WorldSpriteShade_Mode(true,5,true) == WORLD_SPRITE_SHADE_NEUTRAL);
}

static void SpriteNeutralOverridesStoredFaceTint()
{
	// Real original POLYFT4 layout, deliberately containing a red stored RGB.
	// DrawSprites ignores that field and supplies its own grayscale draw colour.
	std::vector<unsigned char> bytes(52,0);
	for (int v = 0; v < 4; ++v)
	{
		bytes[v*8] = (unsigned char)(v*10); bytes[v*8+2] = (unsigned char)(v*v*10);
		bytes[36+v] = (unsigned char)v; bytes[40+v*2] = bytes[41+v*2] = 64;
	}
	bytes[32] = 21; bytes[48] = 255;
	SourceModelView view = {}; view.instanceNumber = -1; view.numVertices = 4; view.numPolys = 1; view.polyBlock = 32;
	SourceMesh source; char error[192] = {};
	CHECK(DecodeSourceModel(bytes.data(),bytes.data()+bytes.size(),view,&source,error,sizeof(error)).error == SOURCE_MODEL_ERROR_NONE);
	CHECK(source.colors[0] == 255 && source.colors[1] == 0 && source.shadeIndices.size() == 4);
	source.shadeIndices.clear(); // sprite neutral capture must not require normal metadata
	std::vector<unsigned char> rgb = {3,4,5};
	CHECK(!WorldSpriteShade_CaptureNeutral(false,4,rgb) && rgb == std::vector<unsigned char>({3,4,5}));
	CHECK(WorldSpriteShade_CaptureNeutral(true,source.positions.size()/3,rgb) && rgb == std::vector<unsigned char>(12,128));
	const unsigned char texel[4] = {0,80,0,128}; const WorldSubmeshImage image = {texel,1,1,sizeof(texel)};
	GltfMultiMeshData mesh; WorldMeshBuilderResult result = {};
	CHECK(WorldMeshBuilder_BuildMulti(source,&mesh,&result,error,sizeof(error),&image,1,rgb.data(),rgb.size()) == WORLD_MESH_ERROR_NONE);
	CHECK(mesh.primitives.size() == 1);
	CHECK(mesh.primitives[0].baseColorFactor[0] == 1 && mesh.primitives[0].baseColorFactor[1] == 1);
	for (size_t v = 0; v < mesh.primitives[0].colors.size()/4; ++v)
		CHECK(mesh.primitives[0].colors[v*4] == 128 && mesh.primitives[0].colors[v*4+1] == 128 && mesh.primitives[0].colors[v*4+2] == 128);
	CHECK(mesh.primitives[0].imageRgba[3] == 128); // original opaque STP/cutout contract unchanged
}
int main()
{
	OriginalLightArithmetic(); SpriteNeutralOverridesStoredFaceTint();
	printf("WorldSpriteShadeTests: %d failures\n",failures); return failures ? 1 : 0;
}
