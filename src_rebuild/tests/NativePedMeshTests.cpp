#include "../utils/NativePedMesh.h"
#include "../utils/NativeCameraView.h"
#include <limits>
#include <stdio.h>
#include <string.h>
#include <cmath>
static int failures = 0;
#define CHECK(x) do { if (!(x)) { ++failures; printf("FAIL line %d: %s\n", __LINE__, #x); } } while (0)
static bool Near(float a, float b) { return fabsf(a - b) < 0.001f; }
static void Short(std::vector<unsigned char>& bytes, size_t offset, int value)
{
	bytes[offset] = (unsigned char)value; bytes[offset + 1] = (unsigned char)(value >> 8);
}
static std::shared_ptr<NativePedComponent> OriginalModel()
{
	std::vector<unsigned char> bytes(72, 0);
	for (int v = 0; v < 4; ++v) { Short(bytes, v * 8, v * 11); Short(bytes, v * 8 + 2, v * 17); Short(bytes, v * 8 + 4, v * -13); }
	// Original asymmetric triangle followed by a quad with a different material.
	bytes[32] = 20; bytes[33] = 2; bytes[34] = 3; bytes[36] = 3; bytes[37] = 0; bytes[38] = 2;
	bytes[40] = 17; bytes[41] = 23; bytes[42] = 31; bytes[43] = 41; bytes[44] = 53; bytes[45] = 61;
	bytes[52] = 21; bytes[53] = 4; bytes[54] = 5;
	for (int v = 0; v < 4; ++v) { bytes[56 + v] = (unsigned char)v; bytes[60 + v * 2] = (unsigned char)(71 + v * 7); bytes[61 + v * 2] = (unsigned char)(89 + v * 3); }
	SourceModelView view = {}; view.instanceNumber = -1; view.numVertices = 4; view.numPolys = 2; view.polyBlock = 32;
	std::shared_ptr<NativePedComponent> part(new NativePedComponent); char error[192] = {};
	CHECK(NativePedMesh_Decode(bytes.data(), bytes.data() + bytes.size(), view, part.get(), error, sizeof(error)));
	CHECK(part->conversion.source.indices.size() == 9 && part->poseVertices.size() == 7);
	CHECK(part->poseVertices[0] == 3 && part->poseVertices[1] == 0 && part->poseVertices[2] == 2);
	CHECK(part->poseVertices[3] == 0 && part->poseVertices[4] == 1 && part->poseVertices[5] == 3 && part->poseVertices[6] == 2);
	CHECK(part->conversion.source.uvs[0] == 17 && part->conversion.source.uvs[10] == 92);
	part->identity.component = 1; part->identity.source = (const void*)1; part->identity.revision = 2;
	part->conversion.pixels = { {255,0,0,255}, {0,255,0,255} }; part->conversion.images = { {0,1,1}, {1,1,1} };
	// Both decodes were owned: mutating the original blob cannot alter either map.
	bytes.assign(bytes.size(), 255); CHECK(part->poseVertices[0] == 3 && part->conversion.source.uvs[0] == 17);
	return part;
}
static void MixedOriginalComponentsAndPrimitiveOrder()
{
	NativePedFrame frame; frame.active = frame.selected = true; frame.incarnation = 7; frame.type = 3; frame.palette = 18;
	NativePedComponentPose model; model.component = OriginalModel();
	model.positions = { 10,20,30, 110,120,130, 210,220,230, 310,320,330 }; frame.parts.push_back(model);
	std::shared_ptr<NativePedComponent> sprite(new NativePedComponent);
	sprite->identity.component = 4; sprite->identity.uvIdentity = 9; sprite->poseVertices = {0,1,2,3};
	sprite->conversion.source.positions.assign(12, 0); sprite->conversion.source.uvs = {11,12,21,22,31,32,41,42};
	sprite->conversion.source.indices = {0,1,2,1,2,3}; SourceSubmesh submesh = {}; submesh.indexCount = 6;
	sprite->conversion.source.submeshes.push_back(submesh); sprite->conversion.pixels = {{0,0,255,255}}; sprite->conversion.images = {{0,1,1}};
	NativePedComponentPose billboard; billboard.component = sprite; billboard.positions = {410,420,430,510,520,530,610,620,630,710,720,730};
	billboard.uvs = {51,52,61,62,71,72,81,82}; frame.parts.push_back(billboard);
	NativePedMeshCapture capture; char error[192] = {}; CHECK(NativePedMesh_Build(frame, &capture, error, sizeof(error)));
	CHECK(capture.bindings.size() == 15 && capture.conversion.source.submeshes.size() == 3);
	std::vector<float> xyz, uv; CHECK(NativePedMesh_ExpandPose(frame, capture, &xyz, &uv, error, sizeof(error)));
	const unsigned original[15] = {3,0,2,0,1,3,1,2,3,0,1,2,1,2,3};
	for (int c = 0; c < 15; ++c)
	{
		CHECK(capture.bindings[c].part == (unsigned)(c < 9 ? 0 : 1));
		CHECK(capture.bindings[c].vertex == original[c]);
		CHECK(xyz[c * 3] == (c < 9 ? model.positions : billboard.positions)[original[c] * 3]);
	}
	CHECK(uv[0] == 17 && uv[18] == 51 && uv[28] == 81);
	NativePedFrame animated = frame;
	animated.parts[0].positions[0] += 900;
	animated.parts[1].uvs[0] += 32; // camera-dependent head atlas direction
	NativePedMeshCapture other; CHECK(NativePedMesh_Build(animated, &other, error, sizeof(error)));
	CHECK(other.conversion.source.positions == capture.conversion.source.positions && other.conversion.source.uvs == capture.conversion.source.uvs);
	CHECK(other.conversion.source.normals.empty() && capture.conversion.source.normals.empty());
	CHECK(other.conversion.legacyColors == capture.conversion.legacyColors && other.conversion.pixels == capture.conversion.pixels);
	std::vector<float> animatedXyz, animatedUv; CHECK(NativePedMesh_ExpandPose(animated, other, &animatedXyz, &animatedUv, error, sizeof(error)));
	CHECK(animatedXyz != xyz && animatedUv != uv);
	// The renderer consumes GLB primitive order, not raw model/part vertex order.
	capture.conversion.source.positions = xyz; capture.conversion.source.uvs = uv;
	std::vector<WorldSubmeshImage> images;
	for (size_t s = 0; s < capture.conversion.images.size(); ++s) { const WorldConversionImage& image = capture.conversion.images[s]; const std::vector<unsigned char>& pixels = capture.conversion.pixels[image.pixelIndex]; images.push_back({pixels.data(), image.width, image.height, pixels.size()}); }
	GltfMultiMeshData gltf; WorldMeshBuilderResult result = {};
	CHECK(WorldMeshBuilder_BuildMulti(capture.conversion.source, &gltf, &result, error, sizeof(error), images.data(), (int)images.size(), capture.conversion.legacyColors.data(), capture.conversion.legacyColors.size()) == 0);
	CHECK(gltf.primitives.size() == 3);
	size_t corner = 0;
	for (size_t p = 0; p < gltf.primitives.size(); ++p)
		for (size_t v = 0; v < gltf.primitives[p].positions.size() / 3; ++v, ++corner)
			{ CHECK(gltf.primitives[p].positions[v * 3] == xyz[corner * 3]); CHECK(gltf.primitives[p].uvs[v * 2] == uv[corner * 2]); }
	CHECK(corner == capture.bindings.size());
	const std::vector<float> oldXyz = xyz, oldUv = uv;
	frame.parts.back().uvs.back() = std::numeric_limits<float>::quiet_NaN();
	CHECK(!NativePedMesh_ExpandPose(frame, capture, &xyz, &uv, error, sizeof(error)) && xyz == oldXyz && uv == oldUv);
	frame.parts.back().uvs.back() = 82; frame.parts.back().positions.resize(3);
	CHECK(!NativePedMesh_ExpandPose(frame, capture, &xyz, &uv, error, sizeof(error)) && xyz == oldXyz);
	std::shared_ptr<NativePedComponent> changed(new NativePedComponent(*sprite)); ++changed->identity.revision;
	frame.parts.back().component = changed;
	CHECK(!NativePedMesh_SamePoseSource(frame, capture));
}
static void ProjectionAndFrameLifetime()
{
	const short rotation[3][3] = {{0,0,4096},{0,4096,0},{-4096,0,0}};
	const int camera[3] = {100,-200,300}, actor[3] = {1000,-500,2000};
	const float xy[8] = {148,114,172,114,142,142,178,142}, depth[2] = {1000,2000}; float points[12];
	CHECK(NativePedMesh_UnprojectSprite(xy, depth, 256, 160,128, rotation,camera,actor,points));
	// Known world point: R^-1 maps +camera Z to -world X, without car Y negation.
	CHECK(Near(points[0], -1900) && Near(points[1], 245.3125f) && Near(points[2], -1746.875f));
	for (int v = 0; v < 4; ++v)
	{
		float world[3], view[3] = {};
		for (int axis = 0; axis < 3; ++axis) world[axis] = points[v * 3 + axis] + actor[axis] - camera[axis];
		for (int row = 0; row < 3; ++row) for (int axis = 0; axis < 3; ++axis) view[row] += rotation[row][axis] * world[axis] / 4096.0f;
		CHECK(Near(160 + view[0] * 256 / view[2], xy[v * 2])); CHECK(Near(128 + view[1] * 256 / view[2], xy[v * 2 + 1]));
		float projection[16]; NativeCameraView_BuildProjection(256,320,240,160,128,16.0f/9,1,10000,projection);
		CHECK(Near((projection[0] * view[0] / view[2]) * (16.0f/9) * 120 + 160, xy[v * 2]));
		CHECK(Near(-(projection[5] * view[1] / view[2] + projection[9]) * 120 + 120, xy[v * 2 + 1]));
	}
	const float invalid[2] = {-1,1000}; CHECK(!NativePedMesh_UnprojectSprite(xy,invalid,256,160,128,rotation,camera,actor,points));
	NativePedFrameEpoch epoch; epoch.Begin(true); CHECK(!epoch.Consume(0)); epoch.Select(0); CHECK(epoch.Consume(0)); CHECK(!epoch.Consume(0));
	epoch.Select(0); epoch.Begin(true); CHECK(!epoch.Consume(0)); // draw/loading frame without a pose
	epoch.Select(0); epoch.Select(1); CHECK(!epoch.Consume(0)); CHECK(epoch.Consume(1)); epoch.Begin(false); epoch.Select(0); CHECK(!epoch.Consume(0));
	NativePedFrame frame; frame.active = true; frame.incarnation = 9; frame.type = 3; frame.palette = 18;
	CHECK(NativePedMesh_IsCurrentActor(frame,9,3,18,2,2)); CHECK(!NativePedMesh_IsCurrentActor(frame,8,3,18,2,2));
	CHECK(!NativePedMesh_IsCurrentActor(frame,9,3,18,3,2)); CHECK(!NativePedMesh_IsCurrentActor(frame,9,0,18,2,2));
	frame.active = false; CHECK(!NativePedMesh_IsCurrentActor(frame,9,3,18,2,2));
}
static void CaptureBounds()
{
	NativePedFrame frame; NativePedComponentPose pose; std::shared_ptr<NativePedComponent> component = OriginalModel(); pose.component = component; frame.parts.push_back(pose);
	NativePedMeshCapture capture; char error[192] = {};
	component->conversion.source.submeshes[0].indexCount = 4097;
	CHECK(!NativePedMesh_Build(frame,&capture,error,sizeof(error)) && capture.bindings.empty() && error[0]);
	component = OriginalModel(); frame.parts[0].component = component;
	component->conversion.pixels.resize(65);
	CHECK(!NativePedMesh_Build(frame,&capture,error,sizeof(error)) && capture.bindings.empty());
	component->conversion.pixels.resize(1); component->conversion.pixels[0].resize(16u*1024u*1024u+1);
	CHECK(!NativePedMesh_Build(frame,&capture,error,sizeof(error)) && capture.bindings.empty());
}
int main()
{
	MixedOriginalComponentsAndPrimitiveOrder(); ProjectionAndFrameLifetime(); CaptureBounds();
	printf("NativePedMeshTests: %d failures\n", failures); return failures ? 1 : 0;
}
