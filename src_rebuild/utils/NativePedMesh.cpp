#include "NativePedMesh.h"
#include <cmath>
#include <stdio.h>
#include <string.h>

namespace {
bool Fail(char* error, int capacity, const char* message)
{
	if (error && capacity > 0) snprintf(error, capacity, "%s", message);
	return false;
}
}
bool NativePedMesh_SameComponent(const NativePedComponentIdentity& a, const NativePedComponentIdentity& b)
{
	return a.component == b.component && a.source == b.source && a.revision == b.revision &&
		a.page == b.page && a.clut == b.clut && a.uvIdentity == b.uvIdentity && a.baseSource == b.baseSource && a.baseRevision == b.baseRevision;
}
bool NativePedMesh_IsCurrentActor(const NativePedFrame& frame, unsigned incarnation, int type, int palette,
	unsigned currentGeneration, unsigned capturedGeneration)
{
	return currentGeneration == capturedGeneration && frame.active && frame.incarnation == incarnation && frame.type == type && frame.palette == palette;
}

bool NativePedMesh_Decode(const unsigned char* begin, const unsigned char* end, const SourceModelView& view,
	NativePedComponent* out, char* error, int capacity)
{
	if (!out || !begin || !end || end <= begin || end - begin > 1024 * 1024 || !view.numPolys || view.numPolys > 1024 ||
		(view.instanceNumber == -1 ? view.numVertices : view.baseNumVertices) > 256)
		return Fail(error, capacity, "invalid pedestrian model byte span");
	*out = NativePedComponent();
	SourceMesh decoded;
	if (DecodeSourceModel(begin, end, view, &decoded, error, capacity).error != SOURCE_MODEL_ERROR_NONE) return false;
	// Decode the same owned polygon bytes with vertex-index tags. This reuses
	// the decoder's exact triangulation/UV ordering without copying its parser.
	std::vector<unsigned char> tagged(begin, end), taggedBase;
	SourceModelView tags = view;
	unsigned char* vertices = nullptr;
	unsigned count = view.numVertices;
	if (view.instanceNumber == -1) vertices = tagged.data() + view.vertices;
	else
	{
		if (!view.baseBegin || !view.baseEnd || view.baseEnd <= view.baseBegin || view.baseEnd - view.baseBegin > 1024 * 1024)
			return Fail(error, capacity, "invalid pedestrian instance base span");
		taggedBase.assign(view.baseBegin, view.baseEnd);
		tags.baseBegin = taggedBase.data(); tags.baseEnd = taggedBase.data() + taggedBase.size();
		vertices = taggedBase.data() + view.baseVertices; count = view.baseNumVertices;
	}
	if (!count || count > 256 || decoded.positions.size() / 3 > NATIVE_PED_MAX_VERTICES)
		return Fail(error, capacity, "pedestrian model vertex capacity exceeded");
	for (unsigned v = 0; v < count; ++v)
	{
		vertices[v * 8] = (unsigned char)v; vertices[v * 8 + 1] = 0;
		memset(vertices + v * 8 + 2, 0, 4);
	}
	SourceMesh indexMesh;
	if (DecodeSourceModel(tagged.data(), tagged.data() + tagged.size(), tags, &indexMesh, error, capacity).error != SOURCE_MODEL_ERROR_NONE) return false;
	if (decoded.indices != indexMesh.indices || decoded.positions.size() != indexMesh.positions.size())
		return Fail(error, capacity, "pedestrian decoder binding inventory differs");
	for (size_t v = 0; v < indexMesh.positions.size() / 3; ++v) out->poseVertices.push_back((unsigned)indexMesh.positions[v * 3]);
	out->conversion.source = std::move(decoded);
	return true;
}

bool NativePedMesh_SamePoseSource(const NativePedFrame& frame, const NativePedMeshCapture& capture)
{
	if (frame.parts.size() != capture.identities.size()) return false;
	for (size_t p = 0; p < frame.parts.size(); ++p)
		if (!frame.parts[p].component || !NativePedMesh_SameComponent(frame.parts[p].component->identity, capture.identities[p])) return false;
	return true;
}

bool NativePedMesh_Build(const NativePedFrame& frame, NativePedMeshCapture* out, char* error, int capacity)
{
	if (!out) return Fail(error, capacity, "missing pedestrian mesh destination");
	*out = NativePedMeshCapture();
	if (frame.parts.empty() || frame.parts.size() > NATIVE_PED_COMPONENTS) return Fail(error, capacity, "invalid pedestrian component inventory");
	NativePedMeshCapture built;
	size_t pixelBytes = 0;
	for (size_t p = 0; p < frame.parts.size(); ++p)
	{
		if (!frame.parts[p].component) return Fail(error, capacity, "missing pedestrian component");
		const NativePedComponent& part = *frame.parts[p].component;
		const WorldMeshConversionInput& input = part.conversion;
		if (input.source.submeshes.size() != input.images.size() || part.poseVertices.size() * 3 != input.source.positions.size())
			return Fail(error, capacity, "pedestrian component binding inventory differs");
		built.identities.push_back(part.identity);
		const int pixelBase = (int)built.conversion.pixels.size();
		for (size_t image = 0; image < input.pixels.size(); ++image)
		{
			pixelBytes += input.pixels[image].size();
			if (pixelBytes > 16u * 1024u * 1024u || built.conversion.pixels.size() == 64)
				return Fail(error, capacity, "pedestrian texture capacity exceeded");
			built.conversion.pixels.push_back(input.pixels[image]);
		}
		for (size_t s = 0; s < input.source.submeshes.size(); ++s)
		{
			const SourceSubmesh& source = input.source.submeshes[s];
			if (source.firstIndex > input.source.indices.size() || source.indexCount > input.source.indices.size() - source.firstIndex)
				return Fail(error, capacity, "pedestrian submesh index range differs");
			SourceSubmesh target = source;
			target.hasColors = false; target.hasNormals = false; // explicit neutral prelit actor baseline
			target.firstIndex = (unsigned)built.conversion.source.indices.size();
			for (unsigned i = 0; i < source.indexCount; ++i)
			{
				const unsigned vertex = input.source.indices[source.firstIndex + i];
				if (vertex >= part.poseVertices.size() || input.source.uvs.size() < (size_t)(vertex + 1) * 2 || built.bindings.size() == NATIVE_PED_MAX_VERTICES)
					return Fail(error, capacity, "pedestrian expanded corner capacity exceeded");
				built.bindings.push_back({ (unsigned)p, part.poseVertices[vertex] });
				built.conversion.source.indices.push_back((unsigned)built.bindings.size() - 1);
				// Cache topology/materials independently of animation and camera pose.
				// Current positions are uploaded before the first instance publication.
				built.conversion.source.positions.insert(built.conversion.source.positions.end(), 3, 0.0f);
				for (int axis = 0; axis < 2; ++axis) built.immutableUvs.push_back(input.source.uvs[vertex * 2 + axis]);
				built.conversion.legacyColors.insert(built.conversion.legacyColors.end(), 3, 128);
			}
			built.conversion.source.submeshes.push_back(target);
			WorldConversionImage image = input.images[s];
			if (image.pixelIndex < 0 || image.pixelIndex >= (int)input.pixels.size()) return Fail(error, capacity, "pedestrian original texture unavailable");
			image.pixelIndex += pixelBase; built.conversion.images.push_back(image);
		}
	}
	built.conversion.source.uvs = built.immutableUvs;
	*out = std::move(built);
	return true;
}

bool NativePedMesh_ExpandPose(const NativePedFrame& frame, const NativePedMeshCapture& capture,
	std::vector<float>* positions, std::vector<float>* uvs, char* error, int capacity)
{
	if (!positions || !uvs || !NativePedMesh_SamePoseSource(frame, capture) || capture.bindings.empty() || capture.bindings.size() > NATIVE_PED_MAX_VERTICES || capture.immutableUvs.size() != capture.bindings.size() * 2)
		return Fail(error, capacity, "pedestrian pose source differs");
	// Validate the entire pose before changing either output inventory.
	for (size_t c = 0; c < capture.bindings.size(); ++c)
	{
		const NativePedBinding& binding = capture.bindings[c];
		if (binding.part >= frame.parts.size()) return Fail(error, capacity, "pedestrian pose part out of range");
		const NativePedComponentPose& pose = frame.parts[binding.part];
		if (pose.positions.size() < (size_t)(binding.vertex + 1) * 3 || (!pose.uvs.empty() && pose.uvs.size() < (size_t)(binding.vertex + 1) * 2))
			return Fail(error, capacity, "pedestrian pose corner out of range");
		for (int axis = 0; axis < 3; ++axis) if (!std::isfinite(pose.positions[binding.vertex * 3 + axis])) return Fail(error, capacity, "pedestrian pose position is not finite");
		for (int axis = 0; axis < 2; ++axis) if (!pose.uvs.empty() && !std::isfinite(pose.uvs[binding.vertex * 2 + axis])) return Fail(error, capacity, "pedestrian pose UV is not finite");
	}
	positions->resize(capture.bindings.size() * 3); *uvs = capture.immutableUvs;
	for (size_t c = 0; c < capture.bindings.size(); ++c)
	{
		const NativePedBinding& binding = capture.bindings[c]; const NativePedComponentPose& pose = frame.parts[binding.part];
		for (int axis = 0; axis < 3; ++axis) (*positions)[c * 3 + axis] = pose.positions[binding.vertex * 3 + axis];
		for (int axis = 0; axis < 2; ++axis) if (!pose.uvs.empty()) (*uvs)[c * 2 + axis] = pose.uvs[binding.vertex * 2 + axis];
	}
	return true;
}

bool NativePedMesh_UnprojectSprite(const float xy[8], const float depth[2], float h, float ofx, float ofy,
	const short rotation[3][3], const int camera[3], const int actor[3], float out[12])
{
	if (!xy || !depth || !rotation || !camera || !actor || !out || !std::isfinite(h) || h <= 0 || depth[0] <= 0 || depth[1] <= 0) return false;
	float result[12];
	for (int v = 0; v < 4; ++v)
	{
		const float z = depth[v / 2];
		const float point[3] = { (xy[v * 2] - ofx) * z / h, (xy[v * 2 + 1] - ofy) * z / h, z };
		for (int axis = 0; axis < 3; ++axis)
		{
			float world = (float)camera[axis] - actor[axis];
			for (int row = 0; row < 3; ++row) world += rotation[row][axis] * point[row] / 4096.0f;
			if (!std::isfinite(world)) return false;
			result[v * 3 + axis] = world;
		}
	}
	memcpy(out, result, sizeof(result)); return true;
}
