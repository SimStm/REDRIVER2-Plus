#include "SourceModelDecoder.h"

#include <stdio.h>
#include <string.h>

namespace
{
// Mirrors Game/engine/mdl.h. Only u_char members, so there is no padding and
// the sizes are exact without packing pragmas.
struct SourceUV
{
	unsigned char u;
	unsigned char v;
};

struct SourceRGB
{
	unsigned char r;
	unsigned char g;
	unsigned char b;
	unsigned char pad;
};

struct LayoutFT3
{
	unsigned char id, textureSet, textureId, spare;
	unsigned char v0, v1, v2, pad;
	SourceUV uv0, uv1, uv2, pad2;
	SourceRGB color;
};

struct LayoutFT4
{
	unsigned char id, textureSet, textureId, spare;
	unsigned char v0, v1, v2, v3;
	SourceUV uv0, uv1, uv2, uv3;
	SourceRGB color;
};

// PL_POLYFT4 (mdl.h:113-121): 16 bytes, no RGB. `th` is a light/normal index.
struct LayoutPLFT4
{
	unsigned char id, textureSet, textureId, th;
	unsigned char v0, v1, v2, v3;
	SourceUV uv0, uv1, uv2, uv3;
};

struct LayoutGT3
{
	unsigned char id, textureSet, textureId, spare;
	unsigned char v0, v1, v2, pad;
	unsigned char n0, n1, n2, pad2;
	SourceUV uv0, uv1, uv2, pad3;
	SourceRGB color;
};

struct LayoutGT4
{
	unsigned char id, textureSet, textureId, spare;
	unsigned char v0, v1, v2, v3;
	unsigned char n0, n1, n2, n3;
	SourceUV uv0, uv1, uv2, uv3;
	SourceRGB color;
};

// The mirrored layout size must equal PolySizes[ptype] (draw.c).
static_assert(sizeof(LayoutPLFT4) == 16, "PL_POLYFT4 must match PolySizes[11]");
static_assert(sizeof(LayoutFT3) == 20, "POLYFT3 must match PolySizes[20]");
static_assert(sizeof(LayoutFT4) == 20, "POLYFT4 must match PolySizes[21]");
static_assert(sizeof(LayoutGT3) == 24, "POLYGT3 must match PolySizes[22]");
static_assert(sizeof(LayoutGT4) == 24, "POLYGT4 must match PolySizes[23]");

void SetError(char* error, int capacity, const char* message)
{
	if (error && capacity > 0)
		snprintf(error, (size_t)capacity, "%s", message);
}

// Allowlisted record stride, mirroring PolySizes; 0 means unsupported.
int StrideForType(unsigned int ptype)
{
	switch (ptype)
	{
	case 11: return 16; // PL_POLYFT4
	case 20: return 20; // POLYFT3
	case 21: return 20; // POLYFT4
	case 22: return 24; // POLYGT3
	case 23: return 24; // POLYGT4
	default: return 0;
	}
}

int LayoutSizeForType(unsigned int ptype)
{
	switch (ptype)
	{
	case 11: return (int)sizeof(LayoutPLFT4);
	case 20: return (int)sizeof(LayoutFT3);
	case 21: return (int)sizeof(LayoutFT4);
	case 22: return (int)sizeof(LayoutGT3);
	case 23: return (int)sizeof(LayoutGT4);
	default: return 0;
	}
}

SourceModelDecoderResult Failure(SourceModelDecoderError code, const char* message,
	char* error, int capacity, int primitiveIndex = -1, unsigned int primitiveType = 0,
	long long offset = -1)
{
	SourceModelDecoderResult result;
	result.error = code;
	result.primitiveIndex = primitiveIndex;
	result.primitiveType = primitiveType;
	result.offset = offset;
	SetError(error, capacity, message);
	return result;
}

SourceModelDecoderResult Success()
{
	SourceModelDecoderResult result;
	result.error = SOURCE_MODEL_ERROR_NONE;
	result.primitiveIndex = -1;
	result.primitiveType = 0;
	result.offset = -1;
	return result;
}

// Reads one signed 16-bit SVECTOR component from a byte offset, little-endian.
short ReadInt16(const unsigned char* begin, long long offset)
{
	const unsigned int raw = (unsigned int)begin[offset] | ((unsigned int)begin[offset + 1] << 8);
	return (short)raw;
}
} // namespace

bool SourceModelDecoder_IsSupportedType(unsigned int ptype)
{
	return StrideForType(ptype & 0x1f) != 0;
}

const char* SourceModelDecoder_ErrorName(SourceModelDecoderError error)
{
	switch (error)
	{
	case SOURCE_MODEL_ERROR_NONE: return "none";
	case SOURCE_MODEL_ERROR_ARGUMENT: return "argument";
	case SOURCE_MODEL_ERROR_RANGE: return "range";
	case SOURCE_MODEL_ERROR_INSTANCE: return "instance";
	case SOURCE_MODEL_ERROR_VERTICES_RANGE: return "vertices-range";
	case SOURCE_MODEL_ERROR_POINT_NORMALS_RANGE: return "point-normals-range";
	case SOURCE_MODEL_ERROR_POLY_BLOCK_RANGE: return "poly-block-range";
	case SOURCE_MODEL_ERROR_POLY_RECORD_RANGE: return "poly-record-range";
	case SOURCE_MODEL_ERROR_VERTEX_INDEX: return "vertex-index";
	case SOURCE_MODEL_ERROR_NORMAL_INDEX: return "normal-index";
	case SOURCE_MODEL_ERROR_UNSUPPORTED_PTYPE: return "unsupported-ptype";
	case SOURCE_MODEL_ERROR_LAYOUT_SIZE: return "layout-size";
	default: return "unknown";
	}
}

SourceModelDecoderResult DecodeSourceModel(const unsigned char* begin, const unsigned char* end,
	const SourceModelView& model, SourceMesh* out, char* error, int capacity)
{
	if (error && capacity > 0)
		error[0] = '\0';

	if (!out)
		return Failure(SOURCE_MODEL_ERROR_ARGUMENT, "a destination mesh is required", error, capacity);

	*out = SourceMesh(); // never leave a partial result behind

	if (!begin || !end || end <= begin)
		return Failure(SOURCE_MODEL_ERROR_RANGE, "the model span is empty or invalid", error, capacity);

	const long long spanSize = (long long)(end - begin);

	if (model.instanceNumber != -1)
		return Failure(SOURCE_MODEL_ERROR_INSTANCE,
			"instanced models cannot be decoded from a single span", error, capacity);

	// The declared vertex array must be wholly inside the span.
	if (model.numVertices > 0)
	{
		const long long needed = (long long)model.numVertices * 8;
		if (model.vertices < 0 || (long long)model.vertices + needed > spanSize)
			return Failure(SOURCE_MODEL_ERROR_VERTICES_RANGE,
				"the vertex array lies outside the model span", error, capacity);
	}

	SourceMesh mesh;
	bool pointNormalsValidated = false;
	bool sawGouraud = false;
	bool sawColor = false;

	long long offset = (long long)model.polyBlock;

	for (unsigned int i = 0; i < model.numPolys; ++i)
	{
		if (offset < 0 || offset + 4 > spanSize)
			return Failure(SOURCE_MODEL_ERROR_POLY_BLOCK_RANGE,
				"the polygon block start lies outside the model span", error, capacity);

		const unsigned int ptype = (unsigned int)(begin[offset] & 0x1f);
		const int stride = StrideForType(ptype);

		if (stride == 0)
		{
			char message[256];
			snprintf(message, sizeof(message),
				"unsupported polygon type %u at polygon %u (offset %lld)", ptype, i, offset);
			return Failure(SOURCE_MODEL_ERROR_UNSUPPORTED_PTYPE, message, error, capacity,
				(int)i, ptype, offset);
		}

		if (LayoutSizeForType(ptype) != stride)
			return Failure(SOURCE_MODEL_ERROR_LAYOUT_SIZE,
				"the mirrored polygon layout does not match its record stride", error, capacity,
				(int)i, ptype, offset);

		if (offset + stride > spanSize)
		{
			char message[256];
			snprintf(message, sizeof(message),
				"polygon %u (type %u) extends beyond the model span", i, ptype);
			return Failure(SOURCE_MODEL_ERROR_POLY_RECORD_RANGE, message, error, capacity,
				(int)i, ptype, offset);
		}

		const bool gouraud = (ptype == 22 || ptype == 23);

		// Validate the point-normal array lazily, at the first gouraud polygon.
		if (gouraud && !pointNormalsValidated)
		{
			pointNormalsValidated = true;
			if (model.numPointNormals > 0)
			{
				const long long needed = (long long)model.numPointNormals * 8;
				if (model.pointNormals < 0 || (long long)model.pointNormals + needed > spanSize)
					return Failure(SOURCE_MODEL_ERROR_POINT_NORMALS_RANGE,
						"the point-normal array lies outside the model span", error, capacity);
			}
		}

		// Collect the corners in world emission order.
		unsigned char vertexIndex[4];
		SourceUV uv[4];
		unsigned char normalIndex[4] = { 0, 0, 0, 0 };
		unsigned int cornerCount = 0;
		SourceRGB color;
		memset(&color, 0, sizeof(color));
		SourcePrimitiveClass primitiveClass = SourcePrimitiveClass::TexturedFlatQuad;
		bool storesColor = true;

		if (ptype == 11)
		{
			// PL_POLYFT4 has no RGB; `th` is a light/normal index, ignored here.
			LayoutPLFT4 record;
			memcpy(&record, begin + offset, sizeof(record));
			vertexIndex[0] = record.v0; vertexIndex[1] = record.v1;
			vertexIndex[2] = record.v3; vertexIndex[3] = record.v2;
			uv[0] = record.uv0; uv[1] = record.uv1; uv[2] = record.uv3; uv[3] = record.uv2;
			cornerCount = 4;
			primitiveClass = SourcePrimitiveClass::TexturedPlQuad;
			storesColor = false;
		}
		else if (ptype == 20)
		{
			LayoutFT3 record;
			memcpy(&record, begin + offset, sizeof(record));
			vertexIndex[0] = record.v0; vertexIndex[1] = record.v1; vertexIndex[2] = record.v2;
			uv[0] = record.uv0; uv[1] = record.uv1; uv[2] = record.uv2;
			color = record.color;
			cornerCount = 3;
			primitiveClass = SourcePrimitiveClass::TexturedFlatTriangle;
		}
		else if (ptype == 21)
		{
			LayoutFT4 record;
			memcpy(&record, begin + offset, sizeof(record));
			vertexIndex[0] = record.v0; vertexIndex[1] = record.v1;
			vertexIndex[2] = record.v3; vertexIndex[3] = record.v2;
			uv[0] = record.uv0; uv[1] = record.uv1; uv[2] = record.uv3; uv[3] = record.uv2;
			color = record.color;
			cornerCount = 4;
			primitiveClass = SourcePrimitiveClass::TexturedFlatQuad;
		}
		else if (ptype == 22)
		{
			LayoutGT3 record;
			memcpy(&record, begin + offset, sizeof(record));
			vertexIndex[0] = record.v0; vertexIndex[1] = record.v1; vertexIndex[2] = record.v2;
			uv[0] = record.uv0; uv[1] = record.uv1; uv[2] = record.uv2;
			normalIndex[0] = record.n0; normalIndex[1] = record.n1; normalIndex[2] = record.n2;
			color = record.color;
			cornerCount = 3;
			primitiveClass = SourcePrimitiveClass::TexturedGouraudTriangle;
			sawGouraud = true;
		}
		else // ptype == 23
		{
			LayoutGT4 record;
			memcpy(&record, begin + offset, sizeof(record));
			vertexIndex[0] = record.v0; vertexIndex[1] = record.v1;
			vertexIndex[2] = record.v3; vertexIndex[3] = record.v2;
			uv[0] = record.uv0; uv[1] = record.uv1; uv[2] = record.uv3; uv[3] = record.uv2;
			normalIndex[0] = record.n0; normalIndex[1] = record.n1;
			normalIndex[2] = record.n3; normalIndex[3] = record.n2;
			color = record.color;
			cornerCount = 4;
			primitiveClass = SourcePrimitiveClass::TexturedGouraudQuad;
			sawGouraud = true;
		}

		// Validate every index before writing anything.
		for (unsigned int c = 0; c < cornerCount; ++c)
		{
			if ((unsigned int)vertexIndex[c] >= model.numVertices)
			{
				char message[256];
				snprintf(message, sizeof(message),
					"polygon %u vertex index %u is outside the %u declared vertices",
					i, (unsigned int)vertexIndex[c], model.numVertices);
				return Failure(SOURCE_MODEL_ERROR_VERTEX_INDEX, message, error, capacity,
					(int)i, ptype, offset);
			}
			if (gouraud && (unsigned int)normalIndex[c] >= model.numPointNormals)
			{
				char message[256];
				snprintf(message, sizeof(message),
					"polygon %u normal index %u is outside the %u declared point normals",
					i, (unsigned int)normalIndex[c], model.numPointNormals);
				return Failure(SOURCE_MODEL_ERROR_NORMAL_INDEX, message, error, capacity,
					(int)i, ptype, offset);
			}
		}

		const unsigned int baseVertex = (unsigned int)(mesh.positions.size() / 3);
		const unsigned int firstIndex = (unsigned int)mesh.indices.size();

		for (unsigned int c = 0; c < cornerCount; ++c)
		{
			const long long vertexOffset = (long long)model.vertices + (long long)vertexIndex[c] * 8;
			mesh.positions.push_back((float)ReadInt16(begin, vertexOffset + 0));
			mesh.positions.push_back((float)ReadInt16(begin, vertexOffset + 2));
			mesh.positions.push_back((float)ReadInt16(begin, vertexOffset + 4));

			mesh.uvs.push_back((float)uv[c].u);
			mesh.uvs.push_back((float)uv[c].v);

			if (gouraud)
			{
				const long long normalOffset = (long long)model.pointNormals + (long long)normalIndex[c] * 8;
				mesh.normals.push_back((float)ReadInt16(begin, normalOffset + 0));
				mesh.normals.push_back((float)ReadInt16(begin, normalOffset + 2));
				mesh.normals.push_back((float)ReadInt16(begin, normalOffset + 4));
			}
			else
			{
				// Flat/PL records store no per-vertex normals. The placeholder
				// keeps index alignment and is invalid for this run, marked by
				// the submesh's hasNormals flag.
				mesh.normals.push_back(0.0f);
				mesh.normals.push_back(0.0f);
				mesh.normals.push_back(0.0f);
			}

			if (storesColor)
			{
				mesh.colors.push_back(color.r);
				mesh.colors.push_back(color.g);
				mesh.colors.push_back(color.b);
			}
			else
			{
				// PL records store no RGB. The placeholder is invalid for this
				// run and is marked by the submesh's hasColors flag.
				mesh.colors.push_back(0);
				mesh.colors.push_back(0);
				mesh.colors.push_back(0);
			}
		}

		// Triangle list: the world quad (v0,v1,v3,v2) as (0,1,2)+(0,2,3).
		mesh.indices.push_back(baseVertex + 0);
		mesh.indices.push_back(baseVertex + 1);
		mesh.indices.push_back(baseVertex + 2);
		if (cornerCount == 4)
		{
			mesh.indices.push_back(baseVertex + 0);
			mesh.indices.push_back(baseVertex + 2);
			mesh.indices.push_back(baseVertex + 3);
		}

		const unsigned int addedIndices = (unsigned int)mesh.indices.size() - firstIndex;
		const unsigned char textureSet = (unsigned char)begin[offset + 1];
		const unsigned char textureId = (unsigned char)begin[offset + 2];

		bool merged = false;
		if (!mesh.submeshes.empty())
		{
			SourceSubmesh& last = mesh.submeshes.back();
			if (last.primitiveClass == primitiveClass &&
				last.textureSet == textureSet && last.textureId == textureId)
			{
				last.indexCount += addedIndices;
				merged = true;
			}
		}
		if (!merged)
		{
			SourceSubmesh submesh;
			submesh.primitiveClass = primitiveClass;
			submesh.textureSet = textureSet;
			submesh.textureId = textureId;
			submesh.firstIndex = firstIndex;
			submesh.indexCount = addedIndices;
			submesh.hasColors = storesColor;
			submesh.hasNormals = gouraud;
			mesh.submeshes.push_back(submesh);
		}

		if (storesColor)
			sawColor = true;

		offset += stride;
	}

	if (!sawColor)
		mesh.colors.clear(); // no submesh stored an RGB face color
	if (!sawGouraud)
		mesh.normals.clear(); // no submesh stored per-vertex normals

	*out = mesh;
	return Success();
}
