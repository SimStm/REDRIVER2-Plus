#include "NativeModernDecode.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <new>
#include <utility>

namespace NativeAsset
{
namespace
{
Result Fail(Failure& failure, Result result, const char* reason)
{
	failure.result = result; failure.reason = reason; return result;
}
bool ImageValid(const std::vector<unsigned char>& bytes, int width, int height)
{
	if (bytes.empty()) return width == 0 && height == 0;
	return width > 0 && height > 0 && width <= 4096 && height <= 4096 &&
		uint64_t(width) * height * 4 == bytes.size();
}
void CopyImage(const std::vector<unsigned char>& bytes, int width, int height, Texture& image)
{
	image.rgba = bytes; image.width = width; image.height = height;
	for (size_t i = 3; i < bytes.size(); i += 4) if (bytes[i] < 255) image.hasCutout = true;
}
Result Decode(const GltfMeshData& source, ModernImport& result, Failure& failure)
{
	if (source.vertexCount <= 0 || source.vertexCount > 65535 ||
		source.positions.size() != size_t(source.vertexCount) * 3 ||
		source.normals.size() != source.positions.size() || source.uvs.size() != size_t(source.vertexCount) * 2)
		return Fail(failure, Result::Invalid, "Incomplete primitive position/normal/UV arrays");
	const size_t indexCount = source.indices.empty() ? size_t(source.vertexCount) : source.indices.size();
	if (!indexCount || indexCount % 3 || source.triangleCount < 0 || size_t(source.triangleCount) != indexCount / 3)
		return Fail(failure, Result::Invalid, "Incomplete triangle index list");
	if (indexCount > PSYX_NATIVE_MAX_BYTES / sizeof(uint32_t) ||
		size_t(source.vertexCount) * sizeof(PsyXNativeVertex) > PSYX_NATIVE_MAX_BYTES - indexCount * sizeof(uint32_t))
		return Fail(failure, Result::OutOfBudget, "Modern primitive exceeds geometry budget");
	for (int channel = 0; channel < 4; ++channel)
		if (!std::isfinite(source.baseColorFactor[channel]) || source.baseColorFactor[channel] < 0 || source.baseColorFactor[channel] > 1)
			return Fail(failure, Result::Invalid, "Invalid base colour factor");
	if (source.baseColorFactor[3] != 1)
		return Fail(failure, Result::Unsupported, "Transparent primitive requires later blend-context adapter");
	for (int channel = 0; channel < 3; ++channel)
		if (!std::isfinite(source.emissiveFactor[channel]) || source.emissiveFactor[channel] < 0)
			return Fail(failure, Result::Invalid, "Invalid emissive factor");
	if (!std::isfinite(source.metallicFactor) || !std::isfinite(source.roughnessFactor) ||
		source.metallicFactor < 0 || source.metallicFactor > 1 || source.roughnessFactor < 0 || source.roughnessFactor > 1)
		return Fail(failure, Result::Invalid, "Invalid metallic/roughness factor");
	if (!ImageValid(source.baseColorRGBA, source.baseColorWidth, source.baseColorHeight) ||
		!ImageValid(source.normalRGBA, source.normalWidth, source.normalHeight) ||
		!ImageValid(source.metallicRoughnessRGBA, source.metallicRoughnessWidth, source.metallicRoughnessHeight) ||
		!ImageValid(source.emissiveRGBA, source.emissiveWidth, source.emissiveHeight))
		return Fail(failure, Result::Invalid, "Incomplete material image span");
	const uint64_t imageBytes = uint64_t(source.baseColorRGBA.size()) + source.normalRGBA.size() +
		source.metallicRoughnessRGBA.size() + source.emissiveRGBA.size();
	if (imageBytes > PSYX_NATIVE_MAX_MATERIAL_BYTES)
		return Fail(failure, Result::OutOfBudget, "Modern material exceeds image budget");
	result.mesh.source = Source::ModernStatic;
	result.mesh.sourceVertices = source.vertexCount; result.mesh.sourcePolygons = result.mesh.triangles = unsigned(indexCount / 3);
	result.mesh.parts.resize(1);
	Part& part = result.mesh.parts[0];
	part.vertices.resize(source.vertexCount); part.indices.resize(indexCount);
	for (int vertex = 0; vertex < source.vertexCount; ++vertex)
	{
		PsyXNativeVertex& target = part.vertices[vertex];
		float normalLength = 0;
		for (int axis = 0; axis < 3; ++axis)
		{
			const float position = source.positions[vertex * 3 + axis], normal = source.normals[vertex * 3 + axis];
			if (!std::isfinite(position) || !std::isfinite(normal)) return Fail(failure, Result::Invalid, "Non-finite primitive geometry");
			target.position[axis] = position; target.normal[axis] = normal; normalLength += normal * normal;
			if (!vertex) result.mesh.boundsMin[axis] = result.mesh.boundsMax[axis] = position;
			else
			{
				result.mesh.boundsMin[axis] = std::min(result.mesh.boundsMin[axis], position);
				result.mesh.boundsMax[axis] = std::max(result.mesh.boundsMax[axis], position);
			}
		}
		if (!std::isfinite(normalLength) || normalLength < .000001f)
			return Fail(failure, Result::Invalid, "Invalid primitive normal");
		for (int axis = 0; axis < 2; ++axis)
		{
			target.uv[axis] = source.uvs[vertex * 2 + axis];
			if (!std::isfinite(target.uv[axis])) return Fail(failure, Result::Invalid, "Non-finite primitive UV");
		}
		std::memcpy(target.color, source.baseColorFactor, sizeof(target.color));
	}
	for (size_t index = 0; index < indexCount; ++index)
	{
		const uint32_t value = source.indices.empty() ? uint32_t(index) : source.indices[index];
		if (value >= unsigned(source.vertexCount)) return Fail(failure, Result::Invalid, "Primitive index outside vertices");
		part.indices[index] = value; // glTF is already Y up: no additional reflection.
	}
	CopyImage(source.baseColorRGBA, source.baseColorWidth, source.baseColorHeight, result.baseColor);
	CopyImage(source.normalRGBA, source.normalWidth, source.normalHeight, result.normal);
	CopyImage(source.metallicRoughnessRGBA, source.metallicRoughnessWidth, source.metallicRoughnessHeight, result.metallicRoughness);
	CopyImage(source.emissiveRGBA, source.emissiveWidth, source.emissiveHeight, result.emissive);
	std::memcpy(result.baseColorFactor, source.baseColorFactor, sizeof(result.baseColorFactor));
	std::memcpy(result.emissiveFactor, source.emissiveFactor, sizeof(result.emissiveFactor));
	result.metallicFactor = source.metallicFactor; result.roughnessFactor = source.roughnessFactor;
	return Result::Complete;
}
}
ModernImport::ModernImport() : metallicFactor(1), roughnessFactor(1)
{
	for (int i = 0; i < 4; ++i) baseColorFactor[i] = 1;
	for (int i = 0; i < 3; ++i) emissiveFactor[i] = 0;
}
Result DecodeModernPrimitive(const GltfMeshData& source, ModernImport& output, Failure& failure)
{
	output = ModernImport(); failure = Failure();
	try
	{
		ModernImport result;
		const Result decoded = Decode(source, result, failure);
		if (decoded == Result::Complete) output = std::move(result);
		return decoded;
	}
	catch (const std::bad_alloc&)
	{
		failure.result = Result::OutOfBudget; failure.reason.clear();
		return Result::OutOfBudget;
	}
}
}
