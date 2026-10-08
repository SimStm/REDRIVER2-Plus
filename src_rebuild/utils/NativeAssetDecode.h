#ifndef NATIVE_ASSET_DECODE_H
#define NATIVE_ASSET_DECODE_H

#include "PsyX/PsyX_native.h"
#include <stddef.h>
#include <string>
#include <vector>

// Game-owned original-file adapter. Inputs are bounded byte spans consumed only
// during Decode; outputs own every byte. No GTE, loader or simulation mutation.
namespace NativeAsset
{
const unsigned int DecoderVersion = 2;
enum class Result { Complete, Invalid, Unsupported, OutOfBudget };
enum class Source { OriginalStatic, ModernStatic, OriginalBackdrop };

struct Bytes
{
	const unsigned char* data;
	size_t size;
	Bytes(const void* bytes = NULL, size_t length = 0)
		: data(static_cast<const unsigned char*>(bytes)), size(length) {}
};

struct MaterialReference
{
	unsigned char textureSet, textureIndex, polygonType, normalDirection;
	unsigned char sourceColor[4]; // Original stored bytes, independent of draw tint.
	bool hasSourceColor;
	MaterialReference();
	bool operator==(const MaterialReference& other) const;
};

struct Part
{
	MaterialReference material;
	uint32_t depthLayer = 0; // Coplanar original terrain artwork; vertices stay exact.
	std::vector<PsyXNativeVertex> vertices;
	std::vector<uint32_t> indices;
};

// Shared immutable import result for original and bounded modern adapters.
// Source identity/revisions and material resolution remain caller-owned.
struct Mesh
{
	Source source;
	unsigned int decoderVersion;
	unsigned int sourceVertices, sourcePolygons, triangles;
	short vertexSource;
	unsigned short shapeFlags, secondaryFlags;
	float boundsMin[3], boundsMax[3];
	std::vector<Part> parts;
	Mesh();
};

struct Failure
{
	Result result;
	unsigned int polygon;
	std::string reason;
	Failure() : result(Result::Complete), polygon(0) {}
};

// Original MODEL header and PL_POLYFT3/4 scenery/tile layout, as consumed by
// PlotBuildingModel/Tile1x1. Parent vertices have a separate bounded MODEL span.
// Unsupported complete models return no partial mesh.
Result DecodeStatic(Bytes model, Bytes vertexSource, Mesh& output, Failure& failure);

struct Placement
{
	int modelIndex;
	unsigned int yaw;
	int position[3]; // Authoritative original integer units, Y down.
	float world[16]; // Native Y-up transform; resource vertices converted once.
};

// PACKED_CELL_OBJECT's four little-endian words; cell centre restores wrap.
Result DecodePlacement(Bytes packed, int cellX, int cellZ, Placement& output);
const char* ResultName(Result result);
}

#endif
