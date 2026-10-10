#include "GltfWriter.h"

#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>

namespace
{
void AppendFloats(std::vector<unsigned char>& bin, const float* values, size_t count)
{
	const unsigned char* p = (const unsigned char*)values;
	bin.insert(bin.end(), p, p + count * sizeof(float));
}

void AppendU32s(std::vector<unsigned char>& bin, const unsigned int* values, size_t count)
{
	const unsigned char* p = (const unsigned char*)values;
	bin.insert(bin.end(), p, p + count * sizeof(unsigned int));
}

void AppendBytes(std::vector<unsigned char>& bin, const unsigned char* values, size_t count)
{
	if (count == 0)
		return;
	bin.insert(bin.end(), values, values + count);
}

std::string FloatText(float value)
{
	char buffer[32];
	snprintf(buffer, sizeof(buffer), "%.9g", value);
	return std::string(buffer);
}

void AppendFloat(std::string& text, float value)
{
	text += FloatText(value);
}

void WriteU32(std::vector<unsigned char>& out, unsigned int value)
{
	out.push_back((unsigned char)(value & 0xff));
	out.push_back((unsigned char)((value >> 8) & 0xff));
	out.push_back((unsigned char)((value >> 16) & 0xff));
	out.push_back((unsigned char)((value >> 24) & 0xff));
}

// --- minimal deterministic PNG encoder (stored deflate blocks) --------------
//
// PNG is a sequence of big-endian length + type + data + CRC32 chunks around a
// zlib stream. The zlib stream here is a header, one or more stored
// (uncompressed) deflate blocks and an Adler-32 trailer; no entropy coding is
// used, so the bytes are a pure function of the RGBA input and every scanline
// uses filter type None. This is exactly the subset GltfLoader's multi record
// decodes, so the writer and loader land together without an external codec.

void WriteU32BETo(unsigned char* out, unsigned int value)
{
	out[0] = (unsigned char)((value >> 24) & 0xff);
	out[1] = (unsigned char)((value >> 16) & 0xff);
	out[2] = (unsigned char)((value >> 8) & 0xff);
	out[3] = (unsigned char)(value & 0xff);
}

const unsigned int* Crc32Table()
{
	static unsigned int table[256];
	static bool initialised = false;
	if (!initialised)
	{
		for (unsigned int i = 0; i < 256; i++)
		{
			unsigned int c = i;
			for (int bit = 0; bit < 8; bit++)
				c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
			table[i] = c;
		}
		initialised = true;
	}
	return table;
}

unsigned int Crc32(const unsigned char* data, size_t length)
{
	const unsigned int* table = Crc32Table();
	unsigned int crc = 0xFFFFFFFFu;
	for (size_t i = 0; i < length; i++)
		crc = table[(crc ^ data[i]) & 0xFFu] ^ (crc >> 8);
	return crc ^ 0xFFFFFFFFu;
}

unsigned int Adler32(const unsigned char* data, size_t length)
{
	unsigned int a = 1, b = 0;
	for (size_t i = 0; i < length; i++)
	{
		a = (a + data[i]) % 65521u;
		b = (b + a) % 65521u;
	}
	return (b << 16) | a;
}

void AppendPngChunk(std::vector<unsigned char>& png, const char type[4],
	const unsigned char* data, size_t length)
{
	unsigned char header[4];
	WriteU32BETo(header, (unsigned int)length);
	png.insert(png.end(), header, header + 4);

	const size_t crcStart = png.size();
	png.push_back((unsigned char)type[0]);
	png.push_back((unsigned char)type[1]);
	png.push_back((unsigned char)type[2]);
	png.push_back((unsigned char)type[3]);
	if (length > 0)
		png.insert(png.end(), data, data + length);

	unsigned char crcBytes[4];
	WriteU32BETo(crcBytes, Crc32(png.data() + crcStart, png.size() - crcStart));
	png.insert(png.end(), crcBytes, crcBytes + 4);
}

// Encodes `rgba` (width*height*4 bytes, RGBA8 row-major) as a PNG. Returns false
// only for invalid dimensions or an overflowing size; validation upstream keeps
// this defensive.
bool EncodePngRgba(const unsigned char* rgba, int width, int height, std::vector<unsigned char>& png)
{
	if (!rgba || width <= 0 || height <= 0)
		return false;
	const size_t w = (size_t)width;
	const size_t h = (size_t)height;
	if (w > (size_t)-1 / 4u || w * 4u > ((size_t)-1 - 1u) / h)
		return false;
	const size_t rowBytes = w * 4u;
	const size_t rawSize = (rowBytes + 1u) * h;

	// Filtered scanlines: one filter-type byte (None = 0) then the row bytes.
	std::vector<unsigned char> raw;
	raw.reserve(rawSize);
	for (int y = 0; y < height; y++)
	{
		raw.push_back(0);
		const unsigned char* row = rgba + (size_t)y * rowBytes;
		raw.insert(raw.end(), row, row + rowBytes);
	}

	// zlib stream: header, stored deflate blocks, Adler-32 trailer.
	std::vector<unsigned char> zlib;
	zlib.push_back(0x78); // CM=8 (deflate), CINFO=7 (32K window)
	zlib.push_back(0x01); // FCHECK so (0x78 << 8 | 0x01) % 31 == 0, no dictionary

	size_t offset = 0;
	do
	{
		const size_t remaining = raw.size() - offset;
		const size_t block = remaining > 65535u ? 65535u : remaining;
		const bool final = (offset + block >= raw.size());
		zlib.push_back(final ? 1 : 0);
		zlib.push_back((unsigned char)(block & 0xff));
		zlib.push_back((unsigned char)((block >> 8) & 0xff));
		const unsigned int nlen = (~(unsigned int)block) & 0xffffu;
		zlib.push_back((unsigned char)(nlen & 0xff));
		zlib.push_back((unsigned char)((nlen >> 8) & 0xff));
		if (block > 0)
			zlib.insert(zlib.end(), raw.begin() + offset, raw.begin() + offset + block);
		offset += block;
	} while (offset < raw.size());

	unsigned char adlerBytes[4];
	WriteU32BETo(adlerBytes, Adler32(raw.data(), raw.size()));
	zlib.insert(zlib.end(), adlerBytes, adlerBytes + 4);

	png.clear();
	static const unsigned char signature[8] = { 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A };
	png.insert(png.end(), signature, signature + 8);

	unsigned char ihdr[13];
	WriteU32BETo(ihdr, (unsigned int)width);
	WriteU32BETo(ihdr + 4, (unsigned int)height);
	ihdr[8] = 8;  // bit depth
	ihdr[9] = 6;  // colour type: RGBA
	ihdr[10] = 0; // compression: deflate
	ihdr[11] = 0; // filter method: adaptive
	ihdr[12] = 0; // interlace: none
	AppendPngChunk(png, "IHDR", ihdr, sizeof(ihdr));
	AppendPngChunk(png, "IDAT", zlib.data(), zlib.size());
	AppendPngChunk(png, "IEND", NULL, 0);
	return true;
}

// One primitive that has passed validation, with the exact element counts and
// pointers the serializer uses. `hasNormals`/`hasUvs` mean the array covers
// every vertex; `hasIndices` means a triangle list is present.
struct PrimitivePlan
{
	int vertexCount;
	bool hasNormals;
	bool hasUvs;
	bool hasColors;
	bool hasIndices;
	const float* positions;
	const float* normals;
	const float* uvs;
	const unsigned char* colors;
	const unsigned int* indices;
	size_t indexCount;
	float baseColorFactor[4];
	float metallicFactor;
	float roughnessFactor;
	bool hasImage;
	int imageWidth;
	int imageHeight;
	const unsigned char* imageRgba;
};

// Validates one primitive and records how it will be laid out. Returns false
// and writes a named reason on the first problem; nothing is written in that
// case. The checks mirror the historical single-primitive writer (a bounded
// read of positions/normals/UVs) and add triangle/index bounds so a malformed
// primitive is rejected instead of serialized.
bool PlanPrimitive(const GltfPrimitiveData& primitive, size_t index, PrimitivePlan* plan,
	char* error, int errorCapacity)
{
	const int vertexCount = primitive.vertexCount > 0 ? primitive.vertexCount : (int)(primitive.positions.size() / 3);
	if (vertexCount <= 0 || primitive.positions.size() < (size_t)vertexCount * 3)
	{
		snprintf(error, errorCapacity, "primitive %u has no positions to write", (unsigned)index);
		return false;
	}

	plan->vertexCount = vertexCount;
	plan->positions = primitive.positions.data();
	plan->hasNormals = primitive.normals.size() >= (size_t)vertexCount * 3;
	plan->normals = plan->hasNormals ? primitive.normals.data() : NULL;
	plan->hasUvs = primitive.uvs.size() >= (size_t)vertexCount * 2;
	plan->uvs = plan->hasUvs ? primitive.uvs.data() : NULL;
	// Optional per-vertex colour: RGBA8, exactly one colour per vertex when
	// present. A partially filled array is a named rejection, never a silent
	// truncation.
	plan->hasColors = false;
	plan->colors = NULL;
	if (!primitive.colors.empty())
	{
		if (primitive.colors.size() != (size_t)vertexCount * 4)
		{
			snprintf(error, errorCapacity, "primitive %u has a colour count that disagrees with POSITION", (unsigned)index);
			return false;
		}
		plan->hasColors = true;
		plan->colors = primitive.colors.data();
	}
	plan->hasIndices = !primitive.indices.empty();
	plan->indices = plan->hasIndices ? primitive.indices.data() : NULL;
	plan->indexCount = primitive.indices.size();
	for (int i = 0; i < 4; i++)
		plan->baseColorFactor[i] = primitive.baseColorFactor[i];
	plan->metallicFactor = primitive.metallicFactor;
	plan->roughnessFactor = primitive.roughnessFactor;

	// Optional baked base-colour image: an empty image means untextured, and the
	// dimensions must be zero then. A non-empty image must be RGBA8 with exactly
	// width*height*4 bytes (checked overflow-safe), so a bad image is a named
	// rejection before any file is written.
	plan->hasImage = false;
	plan->imageWidth = 0;
	plan->imageHeight = 0;
	plan->imageRgba = NULL;
	if (primitive.imageRgba.empty())
	{
		if (primitive.imageWidth != 0 || primitive.imageHeight != 0)
		{
			snprintf(error, errorCapacity, "primitive %u has image dimensions but no image data", (unsigned)index);
			return false;
		}
	}
	else
	{
		if (primitive.imageWidth <= 0 || primitive.imageHeight <= 0)
		{
			snprintf(error, errorCapacity, "primitive %u has image data but no valid dimensions", (unsigned)index);
			return false;
		}
		const size_t w = (size_t)primitive.imageWidth;
		const size_t h = (size_t)primitive.imageHeight;
		if (w > (size_t)-1 / 4u || w * 4u > (size_t)-1 / h)
		{
			snprintf(error, errorCapacity, "primitive %u has image dimensions that overflow their size", (unsigned)index);
			return false;
		}
		if (primitive.imageRgba.size() != w * h * 4u)
		{
			snprintf(error, errorCapacity, "primitive %u has an image size that disagrees with its dimensions", (unsigned)index);
			return false;
		}
		plan->hasImage = true;
		plan->imageWidth = primitive.imageWidth;
		plan->imageHeight = primitive.imageHeight;
		plan->imageRgba = primitive.imageRgba.data();
	}

	if (plan->hasIndices)
	{
		if (plan->indexCount % 3 != 0)
		{
			snprintf(error, errorCapacity, "primitive %u has an index count that is not a whole number of triangles", (unsigned)index);
			return false;
		}
		for (size_t k = 0; k < plan->indexCount; k++)
		{
			if (plan->indices[k] >= (unsigned int)vertexCount)
			{
				snprintf(error, errorCapacity, "primitive %u has an index outside the vertex range", (unsigned)index);
				return false;
			}
		}
	}
	return true;
}
} // namespace

int Gltf_WriteFileMulti(const char* path, const GltfMultiMeshData& mesh, char* error, int errorCapacity)
{
	if (!path || !error || errorCapacity <= 0)
		return -1;
	error[0] = '\0';

	if (mesh.primitives.empty())
	{
		snprintf(error, errorCapacity, "the mesh has no primitives to write");
		return -1;
	}

	// Validate every primitive before anything is written, so a rejected input
	// never leaves a partial file behind.
	std::vector<PrimitivePlan> plans(mesh.primitives.size());
	for (size_t i = 0; i < mesh.primitives.size(); i++)
	{
		if (!PlanPrimitive(mesh.primitives[i], i, &plans[i], error, errorCapacity))
			return -1;
	}

	// Encode each textured primitive's image as one PNG. This is deterministic and
	// happens before any byte is written, so an encoding failure (impossible after
	// PlanPrimitive, but kept explicit) leaves no file.
	std::vector<std::vector<unsigned char> > pngs(plans.size());
	for (size_t i = 0; i < plans.size(); i++)
	{
		if (!plans[i].hasImage)
			continue;
		if (!EncodePngRgba(plans[i].imageRgba, plans[i].imageWidth, plans[i].imageHeight, pngs[i]))
		{
			snprintf(error, errorCapacity, "primitive %u image could not be encoded as PNG", (unsigned)i);
			return -1;
		}
	}

	// One shared BIN buffer: per primitive, POSITION, then NORMAL/TEXCOORD_0 when
	// present, then the index list, then the PNG image when present. This is the
	// same fixed order the historical single-primitive writer used, so a
	// one-primitive mesh with no image is byte-identical.
	std::vector<unsigned char> bin;
	std::vector<int> viewOffsets, viewLengths;

	struct PrimitiveViews
	{
		int position;
		int normal;
		int uv;
		int color;
		int index;
		int image;
	};
	std::vector<PrimitiveViews> primitiveViews(plans.size());

	for (size_t i = 0; i < plans.size(); i++)
	{
		const PrimitivePlan& plan = plans[i];
		PrimitiveViews& view = primitiveViews[i];

		view.position = (int)viewOffsets.size();
		viewOffsets.push_back((int)bin.size());
		AppendFloats(bin, plan.positions, (size_t)plan.vertexCount * 3);
		viewLengths.push_back((int)bin.size() - viewOffsets.back());

		view.normal = -1;
		if (plan.hasNormals)
		{
			view.normal = (int)viewOffsets.size();
			viewOffsets.push_back((int)bin.size());
			AppendFloats(bin, plan.normals, (size_t)plan.vertexCount * 3);
			viewLengths.push_back((int)bin.size() - viewOffsets.back());
		}

		view.uv = -1;
		if (plan.hasUvs)
		{
			view.uv = (int)viewOffsets.size();
			viewOffsets.push_back((int)bin.size());
			AppendFloats(bin, plan.uvs, (size_t)plan.vertexCount * 2);
			viewLengths.push_back((int)bin.size() - viewOffsets.back());
		}

		view.color = -1;
		if (plan.hasColors)
		{
			view.color = (int)viewOffsets.size();
			viewOffsets.push_back((int)bin.size());
			AppendBytes(bin, plan.colors, (size_t)plan.vertexCount * 4);
			viewLengths.push_back((int)bin.size() - viewOffsets.back());
		}

		view.index = -1;
		if (plan.hasIndices)
		{
			view.index = (int)viewOffsets.size();
			viewOffsets.push_back((int)bin.size());
			AppendU32s(bin, plan.indices, plan.indexCount);
			viewLengths.push_back((int)bin.size() - viewOffsets.back());
		}

		view.image = -1;
		if (plan.hasImage)
		{
			view.image = (int)viewOffsets.size();
			viewOffsets.push_back((int)bin.size());
			bin.insert(bin.end(), pngs[i].begin(), pngs[i].end());
			viewLengths.push_back((int)bin.size() - viewOffsets.back());
			// Keep the next view 4-byte aligned: PNG bytes are not, and glTF
			// accessors require a 4-byte-aligned bufferView offset.
			while (bin.size() % 4 != 0)
				bin.push_back(0);
		}
	}

	// The k-th textured primitive owns image/texture/sampler k; untextured
	// primitives map to -1. This keeps the three arrays in primitive order.
	std::vector<int> textureForPrimitive(plans.size(), -1);
	int textureCount = 0;
	for (size_t i = 0; i < plans.size(); i++)
		if (plans[i].hasImage)
			textureForPrimitive[i] = textureCount++;
	const bool anyImage = textureCount > 0;

	// Accessors are emitted in the same per-primitive attribute order and are
	// numbered from zero. Each primitive references its own material (index i).
	struct PrimitiveAccessors
	{
		int position;
		int normal;
		int uv;
		int color;
		int index;
	};
	std::vector<PrimitiveAccessors> primitiveAccessors(plans.size());
	int nextAccessor = 0;
	for (size_t i = 0; i < plans.size(); i++)
	{
		PrimitiveAccessors& accessor = primitiveAccessors[i];
		const PrimitivePlan& plan = plans[i];
		accessor.position = nextAccessor++;
		accessor.normal = plan.hasNormals ? nextAccessor++ : -1;
		accessor.uv = plan.hasUvs ? nextAccessor++ : -1;
		accessor.color = plan.hasColors ? nextAccessor++ : -1;
		accessor.index = plan.hasIndices ? nextAccessor++ : -1;
	}

	std::string json = "{\"asset\":{\"version\":\"2.0\",\"generator\":\"REDRIVER2-Plus E02\"}";
	json += ",\"scene\":0,\"scenes\":[{\"nodes\":[0]}]";

	json += ",\"nodes\":[{\"mesh\":0,\"matrix\":[";
	for (int i = 0; i < 16; i++)
	{
		if (i) json += ",";
		AppendFloat(json, mesh.transform[i]);
	}
	json += "]}]";

	json += ",\"meshes\":[{\"primitives\":[";
	for (size_t i = 0; i < plans.size(); i++)
	{
		const PrimitivePlan& plan = plans[i];
		const PrimitiveAccessors& accessor = primitiveAccessors[i];
		if (i) json += ",";
		json += "{\"attributes\":{\"POSITION\":";
		json += std::to_string(accessor.position);
		if (plan.hasNormals) { json += ",\"NORMAL\":"; json += std::to_string(accessor.normal); }
		if (plan.hasUvs) { json += ",\"TEXCOORD_0\":"; json += std::to_string(accessor.uv); }
		if (plan.hasColors) { json += ",\"COLOR_0\":"; json += std::to_string(accessor.color); }
		json += "}";
		if (plan.hasIndices) { json += ",\"indices\":"; json += std::to_string(accessor.index); }
		json += ",\"material\":";
		json += std::to_string((int)i);
		json += "}";
	}
	json += "]}]";

	json += ",\"materials\":[";
	for (size_t i = 0; i < plans.size(); i++)
	{
		const PrimitivePlan& plan = plans[i];
		if (i) json += ",";
		json += "{\"pbrMetallicRoughness\":{\"baseColorFactor\":[";
		for (int c = 0; c < 4; c++)
		{
			if (c) json += ",";
			AppendFloat(json, plan.baseColorFactor[c]);
		}
		json += "],\"metallicFactor\":";
		AppendFloat(json, plan.metallicFactor);
		json += ",\"roughnessFactor\":";
		AppendFloat(json, plan.roughnessFactor);
		if (plan.hasImage)
		{
			json += ",\"baseColorTexture\":{\"index\":";
			json += std::to_string(textureForPrimitive[i]);
			json += "}";
		}
		json += "}}";
	}
	json += "]";

	// Image/texture/sampler arrays exist only when at least one primitive is
	// textured, so an untextured mesh keeps its historical JSON byte for byte.
	if (anyImage)
	{
		json += ",\"images\":[";
		bool firstImage = true;
		for (size_t i = 0; i < plans.size(); i++)
		{
			if (!plans[i].hasImage)
				continue;
			if (!firstImage) json += ",";
			firstImage = false;
			json += "{\"bufferView\":" + std::to_string(primitiveViews[i].image) + ",\"mimeType\":\"image/png\"}";
		}
		json += "]";

		json += ",\"textures\":[";
		for (int t = 0; t < textureCount; t++)
		{
			if (t) json += ",";
			json += "{\"sampler\":" + std::to_string(t) + ",\"source\":" + std::to_string(t) + "}";
		}
		json += "]";

		// Nearest filtering and clamped edges match the PSX look; mipmaps are not
		// generated, so minFilter is NEAREST too.
		json += ",\"samplers\":[";
		for (int t = 0; t < textureCount; t++)
		{
			if (t) json += ",";
			json += "{\"magFilter\":9728,\"minFilter\":9728,\"wrapS\":33071,\"wrapT\":33071}";
		}
		json += "]";
	}

	json += ",\"accessors\":[";
	bool firstAccessor = true;
	for (size_t i = 0; i < plans.size(); i++)
	{
		const PrimitivePlan& plan = plans[i];
		const PrimitiveViews& view = primitiveViews[i];
		if (!firstAccessor) json += ",";
		json += "{\"bufferView\":" + std::to_string(view.position) + ",\"componentType\":5126,\"count\":" + std::to_string(plan.vertexCount) + ",\"type\":\"VEC3\"}";
		firstAccessor = false;
		if (plan.hasNormals)
			json += ",{\"bufferView\":" + std::to_string(view.normal) + ",\"componentType\":5126,\"count\":" + std::to_string(plan.vertexCount) + ",\"type\":\"VEC3\"}";
		if (plan.hasUvs)
			json += ",{\"bufferView\":" + std::to_string(view.uv) + ",\"componentType\":5126,\"count\":" + std::to_string(plan.vertexCount) + ",\"type\":\"VEC2\"}";
		if (plan.hasColors)
			json += ",{\"bufferView\":" + std::to_string(view.color) + ",\"componentType\":5121,\"count\":" + std::to_string(plan.vertexCount) + ",\"type\":\"VEC4\",\"normalized\":true}";
		if (plan.hasIndices)
			json += ",{\"bufferView\":" + std::to_string(view.index) + ",\"componentType\":5125,\"count\":" + std::to_string(plan.indexCount) + ",\"type\":\"SCALAR\"}";
	}
	json += "]";

	json += ",\"bufferViews\":[";
	for (size_t i = 0; i < viewOffsets.size(); i++)
	{
		if (i) json += ",";
		json += "{\"buffer\":0,\"byteOffset\":" + std::to_string(viewOffsets[i]) +
			",\"byteLength\":" + std::to_string(viewLengths[i]) + "}";
	}
	json += "]";

	json += ",\"buffers\":[{\"byteLength\":" + std::to_string(bin.size()) + "}]}";

	// Pad the JSON chunk with spaces and the BIN chunk with zeros to 4 bytes.
	while (json.size() % 4 != 0)
		json += " ";
	while (bin.size() % 4 != 0)
		bin.push_back(0);

	const unsigned int jsonLength = (unsigned int)json.size();
	const unsigned int binLength = (unsigned int)bin.size();
	const unsigned int totalLength = 12 + 8 + jsonLength + 8 + binLength;

	std::vector<unsigned char> glb;
	glb.reserve(totalLength);
	WriteU32(glb, 0x46546C67); // "glTF"
	WriteU32(glb, 2);
	WriteU32(glb, totalLength);
	WriteU32(glb, jsonLength);
	WriteU32(glb, 0x4E4F534A); // "JSON"
	glb.insert(glb.end(), json.begin(), json.end());
	WriteU32(glb, binLength);
	WriteU32(glb, 0x004E4942); // "BIN\0"
	glb.insert(glb.end(), bin.begin(), bin.end());

	FILE* file = fopen(path, "wb");
	if (!file)
	{
		snprintf(error, errorCapacity, "cannot create %s", path);
		return -1;
	}
	const bool wrote = fwrite(glb.data(), 1, glb.size(), file) == glb.size();
	const bool closed = fclose(file) == 0;
	if (!wrote || !closed)
	{
		remove(path); // no partial output on a failed write
		snprintf(error, errorCapacity, "cannot write %s", path);
		return -1;
	}
	return 0;
}

int Gltf_WriteFile(const char* path, const GltfMeshData& mesh, char* error, int errorCapacity)
{
	if (!path || !error || errorCapacity <= 0)
		return -1;
	error[0] = '\0';

	// Keep the historical single-primitive diagnostic for an empty mesh.
	const int vertexCount = mesh.vertexCount > 0 ? mesh.vertexCount : (int)(mesh.positions.size() / 3);
	if (vertexCount <= 0 || mesh.positions.size() < (size_t)vertexCount * 3)
	{
		snprintf(error, errorCapacity, "the mesh has no positions to write");
		return -1;
	}

	GltfMultiMeshData multi;
	GltfPrimitiveData primitive;
	primitive.positions = mesh.positions;
	primitive.normals = mesh.normals;
	primitive.uvs = mesh.uvs;
	primitive.colors = mesh.colors;
	primitive.indices = mesh.indices;
	for (int i = 0; i < 4; i++)
		primitive.baseColorFactor[i] = mesh.baseColorFactor[i];
	primitive.metallicFactor = mesh.metallicFactor;
	primitive.roughnessFactor = mesh.roughnessFactor;
	primitive.vertexCount = mesh.vertexCount;
	primitive.triangleCount = mesh.triangleCount;
	multi.primitives.push_back(primitive);
	memcpy(multi.transform, mesh.transform, sizeof(multi.transform));

	return Gltf_WriteFileMulti(path, multi, error, errorCapacity);
}
