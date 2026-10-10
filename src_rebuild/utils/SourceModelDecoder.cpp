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

// Mirror of draw.c PolySizes[ptype & 0x1f] for the masked range 0..31. It is the
// record stride the runtime advances by; 0 means no advance is possible.
const int kPolySizes[32] = {
	8,  12, 16, 24,
	20, 20, 28, 32,
	8,  12, 16, 16,
	0,  0,  0,  0,
	12, 12, 12, 16,
	20, 20, 24, 24,
	0,  0,  0,  0,
	0,  0,  0,  0,
};

// Record stride for any masked polygon type, mirroring PolySizes; 0 means the
// record cannot be advanced (and therefore cannot be skipped).
int StrideForType(unsigned int ptype)
{
	return kPolySizes[ptype & 0x1f];
}

// Decode allowlist: only these declared types have a mirrored layout.
bool IsSupportedType(unsigned int ptype)
{
	switch (ptype & 0x1f)
	{
	case 10:
	case 11:
	case 20:
	case 21:
	case 22:
	case 23:
		return true;
	default:
		return false;
	}
}

int LayoutSizeForType(unsigned int ptype)
{
	switch (ptype)
	{
	case 10: return (int)sizeof(LayoutPLFT4); // PL 3-vertex in the 16-byte record
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
	result.skippedCount = 0;
	result.firstSkippedIndex = -1;
	result.firstSkippedType = 0;
	result.firstSkippedOffset = -1;
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
	result.skippedCount = 0;
	result.firstSkippedIndex = -1;
	result.firstSkippedType = 0;
	result.firstSkippedOffset = -1;
	return result;
}

// Reads one signed 16-bit SVECTOR component from a byte offset, little-endian.
short ReadInt16(const unsigned char* begin, long long offset)
{
	const unsigned int raw = (unsigned int)begin[offset] | ((unsigned int)begin[offset + 1] << 8);
	return (short)raw;
}

// Reads one signed 16-bit component from an SVECTOR at `index` in the vertex
// array whose first byte is at `vertexArrayOffset`.
int ReadVertexComponent(const unsigned char* begin, long long vertexArrayOffset,
	unsigned int index, long long componentOffset)
{
	return (int)ReadInt16(begin, vertexArrayOffset + (long long)index * 8 + componentOffset);
}

// Exact mirror of draw.c `normalIndex` (draw.c:631-693): the legacy shading
// bucket of a polygon's first three stored vertices, used to index
// f4colourTable. The runtime passes `*(u_int*)&polys->v0`, i.e. the record's
// v0/v1/v2 bytes (draw.c:742), and returns `th23 | 0x80`; the table index uses
// `th & 31`. This returns that bucket. All arithmetic is signed int, matching
// the original.
unsigned char NormalIndexBucket(const unsigned char* begin, long long vertexArrayOffset,
	unsigned int v0, unsigned int v1, unsigned int v2)
{
	const int pvx = ReadVertexComponent(begin, vertexArrayOffset, v1, 0) - ReadVertexComponent(begin, vertexArrayOffset, v0, 0);
	const int pvy = ReadVertexComponent(begin, vertexArrayOffset, v1, 2) - ReadVertexComponent(begin, vertexArrayOffset, v0, 2);
	const int pvz = ReadVertexComponent(begin, vertexArrayOffset, v1, 4) - ReadVertexComponent(begin, vertexArrayOffset, v0, 4);

	const int qvx = ReadVertexComponent(begin, vertexArrayOffset, v2, 0) - ReadVertexComponent(begin, vertexArrayOffset, v0, 0);
	const int qvy = ReadVertexComponent(begin, vertexArrayOffset, v2, 2) - ReadVertexComponent(begin, vertexArrayOffset, v0, 2);
	const int qvz = ReadVertexComponent(begin, vertexArrayOffset, v2, 4) - ReadVertexComponent(begin, vertexArrayOffset, v0, 4);

	int nx = pvy * qvz - pvz * qvy;
	int nz = pvx * qvy - pvy * qvx;

	int x = nx + (nz >> 3);
	int y = nz - (x >> 2);
	x += (y >> 3);

	int th23;
	if (x < y)
	{
		if (x + y < 1)
			th23 = y < 0 ? 5 : 4;
		else
			th23 = x < 0 ? 3 : 2;
	}
	else
	{
		if (x + y < 1)
			th23 = x < 0 ? 6 : 7;
		else
			th23 = y > 0 ? 1 : 0;
	}

	th23 *= 4;

	const int ny = pvz * qvx - pvx * qvz;

	nx = nx < 0 ? -nx : nx;
	nz = nz < 0 ? -nz : nz;

	if (nx + nz < ny)
		th23 += 1;

	if (nx + nz < -ny)
		th23 = (th23 & 31) | 2;
	else
		th23 = th23 & 31;

	return (unsigned char)(th23 & 31);
}
} // namespace

bool SourceModelDecoder_IsSupportedType(unsigned int ptype)
{
	return IsSupportedType(ptype & 0x1f);
}

const char* SourceModelDecoder_ErrorName(SourceModelDecoderError error)
{
	switch (error)
	{
	case SOURCE_MODEL_ERROR_NONE: return "none";
	case SOURCE_MODEL_ERROR_ARGUMENT: return "argument";
	case SOURCE_MODEL_ERROR_RANGE: return "range";
	case SOURCE_MODEL_ERROR_INSTANCE_BASE: return "instance-base";
	case SOURCE_MODEL_ERROR_VERTICES_RANGE: return "vertices-range";
	case SOURCE_MODEL_ERROR_POINT_NORMALS_RANGE: return "point-normals-range";
	case SOURCE_MODEL_ERROR_POLY_BLOCK_RANGE: return "poly-block-range";
	case SOURCE_MODEL_ERROR_POLY_RECORD_RANGE: return "poly-record-range";
	case SOURCE_MODEL_ERROR_VERTEX_INDEX: return "vertex-index";
	case SOURCE_MODEL_ERROR_NORMAL_INDEX: return "normal-index";
	case SOURCE_MODEL_ERROR_UNSUPPORTED_PTYPE: return "unsupported-ptype";
	case SOURCE_MODEL_ERROR_LAYOUT_SIZE: return "layout-size";
	case SOURCE_MODEL_ERROR_UNSKIPPABLE_PTYPE: return "unskippable-ptype";
	default: return "unknown";
	}
}

SourceModelDecoderResult DecodeSourceModel(const unsigned char* begin, const unsigned char* end,
	const SourceModelView& model, SourceMesh* out, char* error, int capacity,
	SourceModelDecoderPolicy policy)
{
	if (error && capacity > 0)
		error[0] = '\0';

	if (!out)
		return Failure(SOURCE_MODEL_ERROR_ARGUMENT, "a destination mesh is required", error, capacity);

	*out = SourceMesh(); // never leave a partial result behind

	if (!begin || !end || end <= begin)
		return Failure(SOURCE_MODEL_ERROR_RANGE, "the model span is empty or invalid", error, capacity);

	const long long spanSize = (long long)(end - begin);

	// An instanced model shares the base model's vertex and point-normal arrays:
	// its own span supplies only the polygon block, and its polygon indices are
	// absolute into the base arrays. A missing or invalid base span cannot be
	// decoded.
	const bool instanced = (model.instanceNumber != -1);

	if (instanced && (!model.baseBegin || !model.baseEnd || model.baseEnd <= model.baseBegin))
		return Failure(SOURCE_MODEL_ERROR_INSTANCE_BASE,
			"the instanced model's base span is missing or invalid", error, capacity);

	// Vertices come from the base span for an instance and from the model span
	// otherwise; the instance's own vertex fields are ignored. The declared
	// vertex array must be wholly inside its span.
	const unsigned char* vertexSpanBegin = instanced ? model.baseBegin : begin;
	const long long vertexSpanSize = instanced ? (long long)(model.baseEnd - model.baseBegin) : spanSize;
	const int vertexArrayOffset = instanced ? model.baseVertices : model.vertices;
	const int normalArrayOffset = instanced ? model.basePointNormals : model.pointNormals;
	const unsigned int declaredVertices = instanced ? model.baseNumVertices : model.numVertices;
	const unsigned int declaredPointNormals = instanced ? model.baseNumPointNormals : model.numPointNormals;

	if (declaredVertices > 0)
	{
		const long long needed = (long long)declaredVertices * 8;
		if (vertexArrayOffset < 0 || (long long)vertexArrayOffset + needed > vertexSpanSize)
			return Failure(SOURCE_MODEL_ERROR_VERTICES_RANGE,
				instanced ? "the base vertex array lies outside the base span"
				          : "the vertex array lies outside the model span", error, capacity);
	}

	SourceMesh mesh;
	bool pointNormalsValidated = false;
	bool sawGouraud = false;
	bool sawColor = false;
	unsigned int skippedCount = 0;
	int firstSkippedIndex = -1;
	unsigned int firstSkippedType = 0;
	long long firstSkippedOffset = -1;

	long long offset = (long long)model.polyBlock;

	for (unsigned int i = 0; i < model.numPolys; ++i)
	{
		if (offset < 0 || offset + 4 > spanSize)
			return Failure(SOURCE_MODEL_ERROR_POLY_BLOCK_RANGE,
				"the polygon block start lies outside the model span", error, capacity);

		const unsigned int ptype = (unsigned int)(begin[offset] & 0x1f);
		const int stride = StrideForType(ptype);

		if (!IsSupportedType(ptype))
		{
			if (policy == SourceModelDecoderPolicy::Strict)
			{
				char message[256];
				snprintf(message, sizeof(message),
					"unsupported polygon type %u at polygon %u (offset %lld)", ptype, i, offset);
				return Failure(SOURCE_MODEL_ERROR_UNSUPPORTED_PTYPE, message, error, capacity,
					(int)i, ptype, offset);
			}

			// SkipUnsupported: advance by the runtime stride. A zero stride
			// cannot be advanced and a record that does not fit the span is
			// still a hard error, never a silent skip.
			if (stride == 0)
			{
				char message[256];
				snprintf(message, sizeof(message),
					"polygon %u type %u has no record stride and cannot be skipped", i, ptype);
				return Failure(SOURCE_MODEL_ERROR_UNSKIPPABLE_PTYPE, message, error, capacity,
					(int)i, ptype, offset);
			}
			if (offset + stride > spanSize)
			{
				char message[256];
				snprintf(message, sizeof(message),
					"skipped polygon %u (type %u) extends beyond the model span", i, ptype);
				return Failure(SOURCE_MODEL_ERROR_POLY_RECORD_RANGE, message, error, capacity,
					(int)i, ptype, offset);
			}

			if (skippedCount == 0)
			{
				firstSkippedIndex = (int)i;
				firstSkippedType = ptype;
				firstSkippedOffset = offset;
			}
			skippedCount++;
			offset += stride;
			continue;
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
			if (declaredPointNormals > 0)
			{
				const long long needed = (long long)declaredPointNormals * 8;
				if (normalArrayOffset < 0 || (long long)normalArrayOffset + needed > vertexSpanSize)
					return Failure(SOURCE_MODEL_ERROR_POINT_NORMALS_RANGE,
						instanced ? "the base point-normal array lies outside the base span"
						          : "the point-normal array lies outside the model span", error, capacity);
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

		// The record's first three vertex bytes, as the runtime's normalIndex
		// reads them (`*(u_int*)&polys->v0`, draw.c:742). They are the shading
		// input for the normal-shaded types; the combo types leave them zero.
		unsigned char recordV0 = 0, recordV1 = 0, recordV2 = 0;

		if (ptype == 10)
		{
			// PL 3-vertex record in the 16-byte stride; no RGB. ODT decodes
			// case 4/10/20 as POLYFT3 (v0/v1/v2, uv0/uv1/uv2, no color for 10).
			LayoutPLFT4 record;
			memcpy(&record, begin + offset, sizeof(record));
			vertexIndex[0] = record.v0; vertexIndex[1] = record.v1; vertexIndex[2] = record.v2;
			uv[0] = record.uv0; uv[1] = record.uv1; uv[2] = record.uv2;
			recordV0 = record.v0; recordV1 = record.v1; recordV2 = record.v2;
			cornerCount = 3;
			primitiveClass = SourcePrimitiveClass::TexturedPlTriangle;
			storesColor = false;
		}
		else if (ptype == 11)
		{
			// PL_POLYFT4 has no RGB; `th` is a light/normal index, ignored here.
			LayoutPLFT4 record;
			memcpy(&record, begin + offset, sizeof(record));
			vertexIndex[0] = record.v0; vertexIndex[1] = record.v1;
			vertexIndex[2] = record.v3; vertexIndex[3] = record.v2;
			uv[0] = record.uv0; uv[1] = record.uv1; uv[2] = record.uv3; uv[3] = record.uv2;
			recordV0 = record.v0; recordV1 = record.v1; recordV2 = record.v2;
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
			recordV0 = record.v0; recordV1 = record.v1; recordV2 = record.v2;
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
			recordV0 = record.v0; recordV1 = record.v1; recordV2 = record.v2;
			cornerCount = 4;
			primitiveClass = SourcePrimitiveClass::TexturedGouraudQuad;
			sawGouraud = true;
		}

		// Validate every index before writing anything.
		for (unsigned int c = 0; c < cornerCount; ++c)
		{
			if ((unsigned int)vertexIndex[c] >= declaredVertices)
			{
				char message[256];
				snprintf(message, sizeof(message),
					"polygon %u vertex index %u is outside the %u declared vertices",
					i, (unsigned int)vertexIndex[c], declaredVertices);
				return Failure(SOURCE_MODEL_ERROR_VERTEX_INDEX, message, error, capacity,
					(int)i, ptype, offset);
			}
			if (gouraud && (unsigned int)normalIndex[c] >= declaredPointNormals)
			{
				char message[256];
				snprintf(message, sizeof(message),
					"polygon %u normal index %u is outside the %u declared point normals",
					i, (unsigned int)normalIndex[c], declaredPointNormals);
				return Failure(SOURCE_MODEL_ERROR_NORMAL_INDEX, message, error, capacity,
					(int)i, ptype, offset);
			}
		}

		// Legacy shading bucket (draw.c:740-743): the normal-shaded types
		// {10,11,22,23} use the stored `th` when its 0x80 flag is set, otherwise
		// the recomputed normal index; the combo types {20,21} use 0.
		unsigned char shade = 0;
		if (ptype == 10 || ptype == 11 || ptype == 22 || ptype == 23)
		{
			const unsigned char storedTh = begin[offset + 3];
			shade = (storedTh & 0x80)
				? (unsigned char)(storedTh & 31)
				: NormalIndexBucket(vertexSpanBegin, (long long)vertexArrayOffset,
					recordV0, recordV1, recordV2);
		}

		const unsigned int baseVertex = (unsigned int)(mesh.positions.size() / 3);
		const unsigned int firstIndex = (unsigned int)mesh.indices.size();

		for (unsigned int c = 0; c < cornerCount; ++c)
		{
			const long long vertexOffset = (long long)vertexArrayOffset + (long long)vertexIndex[c] * 8;
			mesh.positions.push_back((float)ReadInt16(vertexSpanBegin, vertexOffset + 0));
			mesh.positions.push_back((float)ReadInt16(vertexSpanBegin, vertexOffset + 2));
			mesh.positions.push_back((float)ReadInt16(vertexSpanBegin, vertexOffset + 4));

			mesh.uvs.push_back((float)uv[c].u);
			mesh.uvs.push_back((float)uv[c].v);

			if (gouraud)
			{
				const long long normalOffset = (long long)normalArrayOffset + (long long)normalIndex[c] * 8;
				mesh.normals.push_back((float)ReadInt16(vertexSpanBegin, normalOffset + 0));
				mesh.normals.push_back((float)ReadInt16(vertexSpanBegin, normalOffset + 2));
				mesh.normals.push_back((float)ReadInt16(vertexSpanBegin, normalOffset + 4));
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

			mesh.shadeIndices.push_back(shade);
		}

		// Raw cyclic corners A,B,C,D are stored as A,B,D,C. Cover ABD+BCD
		// with matching winding; the triangle path remains (0,1,2).
		mesh.indices.push_back(baseVertex + 0);
		mesh.indices.push_back(baseVertex + 1);
		mesh.indices.push_back(baseVertex + 2);
		if (cornerCount == 4)
		{
			mesh.indices.push_back(baseVertex + 1);
			mesh.indices.push_back(baseVertex + 3);
			mesh.indices.push_back(baseVertex + 2);
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

	SourceModelDecoderResult result = Success();
	result.skippedCount = skippedCount;
	result.firstSkippedIndex = firstSkippedIndex;
	result.firstSkippedType = firstSkippedType;
	result.firstSkippedOffset = firstSkippedOffset;
	return result;
}
