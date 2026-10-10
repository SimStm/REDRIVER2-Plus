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

// One primitive that has passed validation, with the exact element counts and
// pointers the serializer uses. `hasNormals`/`hasUvs` mean the array covers
// every vertex; `hasIndices` means a triangle list is present.
struct PrimitivePlan
{
	int vertexCount;
	bool hasNormals;
	bool hasUvs;
	bool hasIndices;
	const float* positions;
	const float* normals;
	const float* uvs;
	const unsigned int* indices;
	size_t indexCount;
	float baseColorFactor[4];
	float metallicFactor;
	float roughnessFactor;
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
	plan->hasIndices = !primitive.indices.empty();
	plan->indices = plan->hasIndices ? primitive.indices.data() : NULL;
	plan->indexCount = primitive.indices.size();
	for (int i = 0; i < 4; i++)
		plan->baseColorFactor[i] = primitive.baseColorFactor[i];
	plan->metallicFactor = primitive.metallicFactor;
	plan->roughnessFactor = primitive.roughnessFactor;

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

	// One shared BIN buffer: per primitive, POSITION, then NORMAL/TEXCOORD_0 when
	// present, then the index list. This is the same fixed order the historical
	// single-primitive writer used, so a one-primitive mesh is byte-identical.
	std::vector<unsigned char> bin;
	std::vector<int> viewOffsets, viewLengths;

	struct PrimitiveViews
	{
		int position;
		int normal;
		int uv;
		int index;
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

		view.index = -1;
		if (plan.hasIndices)
		{
			view.index = (int)viewOffsets.size();
			viewOffsets.push_back((int)bin.size());
			AppendU32s(bin, plan.indices, plan.indexCount);
			viewLengths.push_back((int)bin.size() - viewOffsets.back());
		}
	}

	// Accessors are emitted in the same per-primitive attribute order and are
	// numbered from zero. Each primitive references its own material (index i).
	struct PrimitiveAccessors
	{
		int position;
		int normal;
		int uv;
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
		json += "}}";
	}
	json += "]";

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
