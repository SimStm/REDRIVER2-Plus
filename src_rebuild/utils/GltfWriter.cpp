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
} // namespace

int Gltf_WriteFile(const char* path, const GltfMeshData& mesh, char* error, int errorCapacity)
{
	if (!path || !error || errorCapacity <= 0)
		return -1;
	error[0] = '\0';

	const int vertexCount = mesh.vertexCount > 0 ? mesh.vertexCount : (int)(mesh.positions.size() / 3);
	if (vertexCount <= 0 || mesh.positions.size() < (size_t)vertexCount * 3)
	{
		snprintf(error, errorCapacity, "the mesh has no positions to write");
		return -1;
	}

	const bool hasNormals = mesh.normals.size() >= (size_t)vertexCount * 3;
	const bool hasUvs = mesh.uvs.size() >= (size_t)vertexCount * 2;
	const bool hasIndices = !mesh.indices.empty();

	// BIN chunk, one bufferView per attribute in a fixed order.
	std::vector<unsigned char> bin;
	std::vector<int> viewOffsets, viewLengths;
	int positionView = -1, normalView = -1, uvView = -1, indexView = -1;

	positionView = (int)viewOffsets.size();
	viewOffsets.push_back((int)bin.size());
	AppendFloats(bin, mesh.positions.data(), (size_t)vertexCount * 3);
	viewLengths.push_back((int)bin.size() - viewOffsets.back());

	if (hasNormals)
	{
		normalView = (int)viewOffsets.size();
		viewOffsets.push_back((int)bin.size());
		AppendFloats(bin, mesh.normals.data(), (size_t)vertexCount * 3);
		viewLengths.push_back((int)bin.size() - viewOffsets.back());
	}
	if (hasUvs)
	{
		uvView = (int)viewOffsets.size();
		viewOffsets.push_back((int)bin.size());
		AppendFloats(bin, mesh.uvs.data(), (size_t)vertexCount * 2);
		viewLengths.push_back((int)bin.size() - viewOffsets.back());
	}
	if (hasIndices)
	{
		indexView = (int)viewOffsets.size();
		viewOffsets.push_back((int)bin.size());
		AppendU32s(bin, mesh.indices.data(), mesh.indices.size());
		viewLengths.push_back((int)bin.size() - viewOffsets.back());
	}

	// Accessors reference the views; POSITION is always accessor 0.
	const int positionAccessor = 0;
	const int normalAccessor = hasNormals ? 1 : -1;
	const int uvAccessor = hasUvs ? (hasNormals ? 2 : 1) : -1;
	const int indexAccessor = hasIndices ? (1 + (hasNormals ? 1 : 0) + (hasUvs ? 1 : 0)) : -1;

	std::string json = "{\"asset\":{\"version\":\"2.0\",\"generator\":\"REDRIVER2-Plus E02\"}";
	json += ",\"scene\":0,\"scenes\":[{\"nodes\":[0]}]";

	json += ",\"nodes\":[{\"mesh\":0,\"matrix\":[";
	for (int i = 0; i < 16; i++)
	{
		if (i) json += ",";
		AppendFloat(json, mesh.transform[i]);
	}
	json += "]}]";

	json += ",\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":";
	json += std::to_string(positionAccessor);
	if (hasNormals) { json += ",\"NORMAL\":"; json += std::to_string(normalAccessor); }
	if (hasUvs) { json += ",\"TEXCOORD_0\":"; json += std::to_string(uvAccessor); }
	json += "}";
	if (hasIndices) { json += ",\"indices\":"; json += std::to_string(indexAccessor); }
	json += ",\"material\":0}]}]";

	json += ",\"materials\":[{\"pbrMetallicRoughness\":{\"baseColorFactor\":[";
	for (int i = 0; i < 4; i++)
	{
		if (i) json += ",";
		AppendFloat(json, mesh.baseColorFactor[i]);
	}
	json += "],\"metallicFactor\":";
	AppendFloat(json, mesh.metallicFactor);
	json += ",\"roughnessFactor\":";
	AppendFloat(json, mesh.roughnessFactor);
	json += "}}]";

	json += ",\"accessors\":[";
	json += "{\"bufferView\":" + std::to_string(positionView) + ",\"componentType\":5126,\"count\":" + std::to_string(vertexCount) + ",\"type\":\"VEC3\"}";
	if (hasNormals)
		json += ",{\"bufferView\":" + std::to_string(normalView) + ",\"componentType\":5126,\"count\":" + std::to_string(vertexCount) + ",\"type\":\"VEC3\"}";
	if (hasUvs)
		json += ",{\"bufferView\":" + std::to_string(uvView) + ",\"componentType\":5126,\"count\":" + std::to_string(vertexCount) + ",\"type\":\"VEC2\"}";
	if (hasIndices)
		json += ",{\"bufferView\":" + std::to_string(indexView) + ",\"componentType\":5125,\"count\":" + std::to_string(mesh.indices.size()) + ",\"type\":\"SCALAR\"}";
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
		snprintf(error, errorCapacity, "cannot write %s", path);
		return -1;
	}
	return 0;
}
