#ifndef NATIVE_SKY_MESH_H
#define NATIVE_SKY_MESH_H
#include "WorldMeshConversion.h"
#include <memory>

// Owned original draw inputs. No MODEL, VRAM or primitive-buffer pointers.
struct NativeSkyTile { unsigned char uv[8]; unsigned short page, clut; };
struct NativeSkyQuad
{
	float positions[12]; // source v0,v1,v2,v3
	unsigned char tile, model;
	bool reflection;
};
struct NativeSkyModelInput
{
	const unsigned char* vertices;
	size_t vertexBytes;
	const unsigned char* polygons;
	size_t polygonBytes;
	unsigned vertexCount, polygonCount;
	const unsigned char* textures;
	unsigned textureCount, textureOffset;
	unsigned char model;
};
// Presentation copy of the specific original horizon type 5/11/21 lists.
// Spans must come from real resident MODEL allocation bounds, not guessed ends.
// Original PlotSkyPoly reads only the shared vertex prefix, even for type 5.
unsigned NativeSkyMesh_ModelStride(unsigned type);
bool NativeSkyMesh_CopyModel(const NativeSkyModelInput& input, std::vector<NativeSkyQuad>* quads,
	short gate[3], char* error, int capacity);
struct NativeSkyAssets
{
	unsigned generation = 0, revision = 0;
	int level = 0, segment = 0;
	std::vector<unsigned char> raw;
	std::vector<NativeSkyQuad> quads;
	NativeSkyTile tiles[28] = {};
	short gateVertices[4][3] = {};
};
struct NativeSkyFrame
{
	unsigned epoch = 0, generation = 0, revision = 0;
	int view = -1, rgb[3] = {};
	float rotation[16] = {};
	unsigned char enabled[4] = {};
	bool selected = false;
};
struct NativeSkyEpoch
{
	unsigned frame = 0;
	int view = -1;
	NativeSkyFrame captured;
	void Begin();
	void Select(int selectedView);
	// Repeated screenshot/present reads are allowed only in the current epoch.
	bool Consume(int selectedView, unsigned generation, unsigned revision, NativeSkyFrame* out);
};
bool NativeSkyMesh_Build(const NativeSkyAssets& assets, WorldMeshConversionInput* out, char* error, int capacity);
void NativeSkyMesh_Pose(const short rotation[3][3], const short gates[4][3], int yOffset, NativeSkyFrame* out);
void NativeSkyMesh_Color(const int rgb[3], bool reflection, float out[3]);
#endif
