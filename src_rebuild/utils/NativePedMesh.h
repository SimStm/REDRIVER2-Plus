#ifndef NATIVE_PED_MESH_H
#define NATIVE_PED_MESH_H
#include "WorldMeshConversion.h"
#include <memory>

enum { NATIVE_PED_SLOTS = 28, NATIVE_PED_COMPONENTS = 24, NATIVE_PED_MAX_VERTICES = 4096 };
struct NativePedComponentIdentity
{
	int component;
	const void* source; // identity only, never dereferenced by conversion workers
	unsigned revision;
	unsigned short page, clut;
	unsigned long long uvIdentity;
	const void* baseSource;
	unsigned baseRevision;
};
bool NativePedMesh_SameComponent(const NativePedComponentIdentity& a, const NativePedComponentIdentity& b);
struct NativePedComponent
{
	NativePedComponentIdentity identity = {};
	WorldMeshConversionInput conversion;
	std::vector<unsigned> poseVertices; // source vertex -> evaluated model/sprite vertex
};
struct NativePedComponentPose
{
	std::shared_ptr<const NativePedComponent> component;
	std::vector<float> positions;
	std::vector<float> uvs; // raw texel UVs; empty means immutable model UVs
};
struct NativePedFrame
{
	bool active = false, selected = false, hidden = false, failed = false;
	unsigned incarnation = 0;
	int type = 0, palette = 0, pad = -1, position[3] = {};
	std::vector<NativePedComponentPose> parts;
	char error[192] = {};
};
struct NativePedFrameEpoch
{
	bool enabled = false, valid = false;
	int view = -1;
	void Begin(bool active) { enabled = active; valid = false; view = -1; }
	void Select(int selected) { if (enabled) { view = selected; valid = true; } }
	bool Consume(int selected) { if (!enabled || !valid || view != selected) return false; valid = false; return true; }
};
bool NativePedMesh_IsCurrentActor(const NativePedFrame& frame, unsigned incarnation, int type, int palette,
	unsigned currentGeneration, unsigned capturedGeneration);
struct NativePedBinding { unsigned part, vertex; };
struct NativePedMeshCapture
{
	WorldMeshConversionInput conversion;
	std::vector<NativePedComponentIdentity> identities;
	std::vector<NativePedBinding> bindings;
	std::vector<float> immutableUvs;
};
bool NativePedMesh_Decode(const unsigned char* begin, const unsigned char* end, const SourceModelView& view,
	NativePedComponent* out, char* error, int capacity);
bool NativePedMesh_Build(const NativePedFrame& frame, NativePedMeshCapture* out, char* error, int capacity);
bool NativePedMesh_SamePoseSource(const NativePedFrame& frame, const NativePedMeshCapture& capture);
bool NativePedMesh_ExpandPose(const NativePedFrame& frame, const NativePedMeshCapture& capture,
	std::vector<float>* positions, std::vector<float>* uvs, char* error, int capacity);
// Undo the original GTE projection in raw units, then inverse camera rotation.
// endpoint depths, h and OFX/OFY are captured from that same CPU evaluation.
bool NativePedMesh_UnprojectSprite(const float xy[8], const float depth[2], float h, float ofx, float ofy,
	const short inverseCamera[3][3], const int camera[3], const int actor[3], float out[12]);
#endif
