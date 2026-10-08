#include "NativeAssetDecode.h"
#include "NativeTerrainLayers.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <map>
#include <new>
#include <utility>

namespace NativeAsset
{
namespace
{
const size_t HeaderBytes = 36, MaxOutputBytes = 8 * 1024 * 1024;
const unsigned int PolygonSizes[32] = {
	8,12,16,24,20,20,28,32,8,12,16,16,0,0,0,0,
	12,12,12,16,20,20,24,24,0,0,0,0,0,0,0,0
};

bool Contains(Bytes span, size_t offset, size_t count)
{
	return span.data && offset <= span.size && count <= span.size - offset;
}
unsigned int Word(const unsigned char* p) { return p[0] | (unsigned int)p[1] << 8; }
int SignedWord(const unsigned char* p)
{
	const unsigned int word = Word(p);
	return word < 32768 ? (int)word : (int)word - 65536;
}
uint32_t Dword(const unsigned char* p)
{
	return Word(p) | (uint32_t)Word(p + 2) << 16;
}
Result Fail(Failure& failure, Result result, unsigned int polygon, const char* reason)
{
	failure.result = result; failure.polygon = polygon; failure.reason = reason;
	return result;
}

struct MaterialLess
{
	bool operator()(const MaterialReference& a, const MaterialReference& b) const
	{
		if (a.textureSet != b.textureSet) return a.textureSet < b.textureSet;
		if (a.textureIndex != b.textureIndex) return a.textureIndex < b.textureIndex;
		if (a.polygonType != b.polygonType) return a.polygonType < b.polygonType;
		if (a.normalDirection != b.normalDirection) return a.normalDirection < b.normalDirection;
		if (a.hasSourceColor != b.hasSourceColor) return a.hasSourceColor < b.hasSourceColor;
		return std::memcmp(a.sourceColor, b.sourceColor, sizeof(a.sourceColor)) < 0;
	}
};
typedef std::pair<MaterialReference, uint32_t> PartKey;
struct PartLess
{
	bool operator()(const PartKey& a, const PartKey& b) const
	{
		const MaterialLess materialLess;
		if (materialLess(a.first,b.first)) return true;
		if (materialLess(b.first,a.first)) return false;
		return a.second < b.second;
	}
};
typedef std::map<PartKey, size_t, PartLess> PartLookup;
Part& PartFor(Mesh& mesh, const MaterialReference& reference, uint32_t depthLayer, PartLookup& lookup)
{
	const PartKey key(reference,depthLayer);
	const PartLookup::const_iterator found = lookup.find(key);
	if (found != lookup.end()) return mesh.parts[found->second];
	lookup.insert(std::make_pair(key, mesh.parts.size()));
	mesh.parts.push_back(Part());
	mesh.parts.back().material = reference;
	mesh.parts.back().depthLayer = depthLayer;
	return mesh.parts.back();
}

void AppendTriangle(Part& part, const PsyXNativeVertex* corners, int a, int b, int c)
{
	// Reflection (x,-y,z) changes handedness. Reverse winding exactly once.
	const int order[3] = { a, c, b };
	float edgeA[3], edgeB[3], normal[3];
	for (int axis = 0; axis < 3; ++axis)
	{
		edgeA[axis] = corners[c].position[axis] - corners[a].position[axis];
		edgeB[axis] = corners[b].position[axis] - corners[a].position[axis];
	}
	normal[0] = edgeA[1]*edgeB[2] - edgeA[2]*edgeB[1];
	normal[1] = edgeA[2]*edgeB[0] - edgeA[0]*edgeB[2];
	normal[2] = edgeA[0]*edgeB[1] - edgeA[1]*edgeB[0];
	const float length = std::sqrt(normal[0]*normal[0]+normal[1]*normal[1]+normal[2]*normal[2]);
	// Keep zero-area source triangles/counts; no invented normal or missing face.
	for (int i = 0; i < 3; ++i)
	{
		PsyXNativeVertex vertex = corners[order[i]];
		for (int axis = 0; axis < 3; ++axis) vertex.normal[axis] = length > 0 ? normal[axis]/length : 0;
		part.indices.push_back((uint32_t)part.vertices.size());
		part.vertices.push_back(vertex);
	}
}

Result Decode(Bytes model, Bytes source, Mesh& mesh, Failure& failure)
{
	if (!Contains(model, 0, HeaderBytes) || !Contains(source, 0, HeaderBytes))
		return Fail(failure, Result::Invalid, 0, "Truncated MODEL header");
	mesh.shapeFlags = (unsigned short)Word(model.data);
	mesh.secondaryFlags = (unsigned short)Word(model.data + 2);
	mesh.vertexSource = (short)SignedWord(model.data + 4);
	if (mesh.vertexSource < -1)
		return Fail(failure, Result::Invalid, 0, "Invalid negative instance vertex source");
	if (SignedWord(source.data + 4) != -1)
		return Fail(failure, Result::Unsupported, 0, "Nested instance vertex source");
	mesh.sourceVertices = Word(source.data + 12);
	mesh.sourcePolygons = Word(model.data + 14);
	const size_t vertexOffset = Dword(source.data + 16), polygonOffset = Dword(model.data + 20);
	if (!mesh.sourceVertices || !mesh.sourcePolygons || vertexOffset < HeaderBytes || polygonOffset < HeaderBytes ||
		!Contains(source, vertexOffset, (size_t)mesh.sourceVertices * 8))
		return Fail(failure, Result::Invalid, 0, "Invalid vertex or polygon span");
	for (int axis = 0; axis < 3; ++axis)
	{
		mesh.boundsMin[axis] = std::numeric_limits<float>::max();
		mesh.boundsMax[axis] = -std::numeric_limits<float>::max();
	}

	size_t offset = polygonOffset, outputBytes = 0;
	PartLookup partLookup;
	TerrainLayers terrainLayers;
	for (unsigned int face = 0; face < mesh.sourcePolygons; ++face)
	{
		if (!Contains(model, offset, 1)) return Fail(failure, Result::Invalid, face, "Truncated polygon id");
		const unsigned int type = model.data[offset] & 31, stride = PolygonSizes[type];
		if (type != 10 && type != 11 && type != 20 && type != 21 && type != 22 && type != 23)
			return Fail(failure, Result::Unsupported, face, "Unsupported complete static polygon family");
		if (!Contains(model, offset, stride)) return Fail(failure, Result::Invalid, face, "Truncated polygon record");
		const unsigned char* polygon = model.data + offset;
		const bool quad = (type & 1) != 0;
		const unsigned int cornerCount = quad ? 4 : 3;
		MaterialReference reference;
		reference.textureSet = polygon[1]; reference.textureIndex = polygon[2];
		reference.polygonType = (unsigned char)type; reference.normalDirection = polygon[3];
		reference.hasSourceColor = type >= 20;
		if (reference.hasSourceColor) std::memcpy(reference.sourceColor, polygon + stride - 4, 4);
		PsyXNativeVertex corners[4] = {};
		for (unsigned int corner = 0; corner < cornerCount; ++corner)
		{
			if (polygon[4+corner] >= mesh.sourceVertices)
				return Fail(failure, Result::Invalid, face, "Polygon index outside vertex source");
			const unsigned char* point = source.data + vertexOffset + polygon[4+corner] * 8;
			for (int axis = 0; axis < 3; ++axis)
			{
				corners[corner].position[axis] = (float)SignedWord(point + axis * 2) * (axis == 1 ? -1 : 1);
				mesh.boundsMin[axis] = std::min(mesh.boundsMin[axis], corners[corner].position[axis]);
				mesh.boundsMax[axis] = std::max(mesh.boundsMax[axis], corners[corner].position[axis]);
			}
			// Texel centres; per-resource material adapter later remaps the region.
			corners[corner].uv[0] = (polygon[8+corner*2]+0.5f)/256.0f;
			corners[corner].uv[1] = (polygon[9+corner*2]+0.5f)/256.0f;
			for (int channel = 0; channel < 4; ++channel) corners[corner].color[channel] = 1;
		}
		const unsigned int triangles = quad && polygon[7] != polygon[6] ? 2 : 1;
		const size_t bytes = triangles * 3 * (sizeof(PsyXNativeVertex) + sizeof(uint32_t));
		if (bytes > MaxOutputBytes - outputBytes)
			return Fail(failure, Result::OutOfBudget, face, "Decoded static resource exceeds CPU budget");
		outputBytes += bytes;
		uint32_t depthLayer=0;
		if (mesh.shapeFlags & 0x400) // SHAPE_FLAG_TILE: original horizontal terrain/paint.
		{
			const Result classified=terrainLayers.Classify(corners,triangles==1 ? 3 : cornerCount,depthLayer);
			if (classified != Result::Complete)
				return Fail(failure,classified,face,"Coplanar terrain layer budget exceeded");
		}
		Part& part = PartFor(mesh, reference, depthLayer, partLookup);
		if (quad)
		{
			AppendTriangle(part, corners, 0, 1, 3);
			if (triangles == 2) AppendTriangle(part, corners, 3, 1, 2);
		}
		else AppendTriangle(part, corners, 0, 1, 2);
		mesh.triangles += triangles;
		offset += stride;
	}
	return Result::Complete;
}
}

MaterialReference::MaterialReference() : textureSet(0), textureIndex(0), polygonType(0), normalDirection(0), hasSourceColor(false)
{
	std::memset(sourceColor, 0, sizeof(sourceColor));
}
bool MaterialReference::operator==(const MaterialReference& other) const
{
	return textureSet == other.textureSet && textureIndex == other.textureIndex && polygonType == other.polygonType &&
		normalDirection == other.normalDirection && hasSourceColor == other.hasSourceColor &&
		std::memcmp(sourceColor, other.sourceColor, 4) == 0;
}
Mesh::Mesh() : source(Source::OriginalStatic), decoderVersion(DecoderVersion), sourceVertices(0), sourcePolygons(0),
	triangles(0), vertexSource(-1), shapeFlags(0), secondaryFlags(0)
{
	std::memset(boundsMin, 0, sizeof(boundsMin)); std::memset(boundsMax, 0, sizeof(boundsMax));
}
Result DecodeStatic(Bytes model, Bytes vertexSource, Mesh& output, Failure& failure)
{
	output = Mesh(); failure = Failure();
	try
	{
		Mesh decoded;
		const Result result = Decode(model, vertexSource, decoded, failure);
		if (result == Result::Complete) output = std::move(decoded);
		return result;
	}
	catch (const std::bad_alloc&)
	{
		// Reporting allocation failure must not allocate another error string.
		failure.result = Result::OutOfBudget; failure.reason.clear();
		return Result::OutOfBudget;
	}
}

Result DecodePlacement(Bytes packed, int cellX, int cellZ, Placement& output)
{
	output = Placement();
	if (!Contains(packed, 0, 8)) return Result::Invalid;
	const unsigned int y = Word(packed.data + 2), value = Word(packed.data + 6);
	if (value == 65535 && (y & 1)) return Result::Unsupported; // Cell sentinel.
	output.modelIndex = (value >> 6) | ((y & 1) << 10); output.yaw = value & 63;
	// Explicit signed 16-bit wrap, matching QuickUnpackCellObject without relying
	// on implementation-defined narrowing or overflowing cell-centre arithmetic.
	const int wrappedX = (int)((Word(packed.data) - (uint32_t)cellX) & 65535);
	const int wrappedZ = (int)((Word(packed.data+4) - (uint32_t)cellZ) & 65535);
	const int64_t x = (int64_t)cellX + (wrappedX < 32768 ? wrappedX : wrappedX - 65536);
	const int64_t z = (int64_t)cellZ + (wrappedZ < 32768 ? wrappedZ : wrappedZ - 65536);
	if (x < INT32_MIN || x > INT32_MAX || z < INT32_MIN || z > INT32_MAX) return Result::Invalid;
	const int signedY = y < 32768 ? (int)y : (int)y - 65536;
	output.position[0] = (int)x; output.position[1] = (int)std::floor(signedY/2.0); output.position[2] = (int)z;
	const float angle = output.yaw * (6.283185307179586f/64);
	const float cosine = std::cos(angle), sine = std::sin(angle);
	output.world[0] = cosine; output.world[2] = -sine;
	output.world[5] = 1; output.world[8] = sine; output.world[10] = cosine; output.world[15] = 1;
	output.world[12] = (float)x; output.world[13] = (float)-output.position[1]; output.world[14] = (float)z;
	return Result::Complete;
}
const char* ResultName(Result result)
{
	switch (result)
	{
	case Result::Complete: return "complete";
	case Result::Invalid: return "invalid";
	case Result::Unsupported: return "unsupported";
	case Result::OutOfBudget: return "out of budget";
	}
	return "unknown";
}
}
