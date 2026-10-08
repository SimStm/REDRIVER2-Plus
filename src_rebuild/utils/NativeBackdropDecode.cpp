#include "NativeBackdropDecode.h"
#include <algorithm>
#include <cstring>
#include <limits>
#include <new>
#include <utility>
namespace NativeAsset
{
namespace
{
unsigned int Word(const unsigned char* p) { return p[0] | unsigned(p[1])<<8; }
int SignedWord(const unsigned char* p) { const unsigned int value = Word(p); return value < 32768 ? int(value) : int(value)-65536; }
size_t Dword(const unsigned char* p) { return Word(p) | uint32_t(Word(p+2))<<16; }
bool Contains(Bytes span, size_t offset, size_t count)
{
	return span.data && offset <= span.size && count <= span.size-offset;
}
Result Reject(Failure& failure, Result result, unsigned int face, const char* reason)
{
	failure.result = result; failure.polygon = face; failure.reason = reason; return result;
}
Result Decode(Bytes model, const BackdropFace* faces, size_t faceCount, Mesh& mesh, Failure& failure)
{
	if (!Contains(model,0,36) || !faces) return Reject(failure,Result::Invalid,0,"Truncated horizon header or missing art bindings");
	if (SignedWord(model.data+4) != -1) return Reject(failure,Result::Unsupported,0,"Instanced horizon vertex source");
	mesh.source = Source::OriginalBackdrop;
	mesh.shapeFlags = (unsigned short)Word(model.data); mesh.secondaryFlags = (unsigned short)Word(model.data+2);
	mesh.sourceVertices = Word(model.data+12); mesh.sourcePolygons = Word(model.data+14);
	const size_t vertexOffset = Dword(model.data+16);
	size_t polygonOffset = Dword(model.data+20);
	// The original sky projection has35 scratch vertices and40 art-table entries.
	if (!mesh.sourceVertices || mesh.sourceVertices > 35 || !mesh.sourcePolygons || mesh.sourcePolygons > 40 ||
		faceCount != mesh.sourcePolygons || vertexOffset < 36 || polygonOffset < 36 ||
		!Contains(model,vertexOffset,size_t(mesh.sourceVertices)*8))
		return Reject(failure,Result::Invalid,0,"Invalid horizon source span or art count");
	for (int axis = 0; axis < 3; ++axis)
	{ mesh.boundsMin[axis] = std::numeric_limits<float>::max(); mesh.boundsMax[axis] = -std::numeric_limits<float>::max(); }
	for (unsigned int face = 0; face < mesh.sourcePolygons; ++face)
	{
		if (!Contains(model,polygonOffset,1)) return Reject(failure,Result::Invalid,face,"Truncated horizon polygon id");
		const unsigned int type = model.data[polygonOffset]&31;
		if (type != 5 && type != 11 && type != 21) return Reject(failure,Result::Unsupported,face,"Unsupported horizon quad family");
		const size_t stride = type == 11 ? 16 : 20;
		if (!Contains(model,polygonOffset,stride) || faces[face].texture >= 28)
			return Reject(failure,Result::Invalid,face,"Truncated horizon quad or invalid sky texture");
		const unsigned char* polygon = model.data+polygonOffset;
		Part part;
		part.material.polygonType = (unsigned char)type; part.material.textureIndex = faces[face].texture;
		const unsigned int uvCorner[] = { 2,3,1,0 }; // original PlotSkyPoly geometry/art correspondence
		for (unsigned int corner = 0; corner < 4; ++corner)
		{
			if (polygon[4+corner] >= mesh.sourceVertices) return Reject(failure,Result::Invalid,face,"Horizon index outside source vertices");
			const unsigned char* point = model.data+vertexOffset+size_t(polygon[4+corner])*8;
			PsyXNativeVertex vertex = {};
			for (int axis = 0; axis < 3; ++axis)
			{
				vertex.position[axis] = float(SignedWord(point+axis*2))*(axis == 1 ? -1 : 1);
				mesh.boundsMin[axis] = std::min(mesh.boundsMin[axis],vertex.position[axis]);
				mesh.boundsMax[axis] = std::max(mesh.boundsMax[axis],vertex.position[axis]);
			}
			vertex.uv[0] = (faces[face].uv[uvCorner[corner]*2]+.5f)/256;
			vertex.uv[1] = (faces[face].uv[uvCorner[corner]*2+1]+.5f)/256;
			for (unsigned int channel = 0; channel < 4; ++channel) vertex.color[channel] = 1;
			part.vertices.push_back(vertex);
		}
		// Same source quad topology, reflected once to native Y-up.
		const uint32_t indices[] = { 0,3,1,3,2,1 };
		part.indices.assign(indices,indices+6);
		mesh.parts.push_back(std::move(part)); mesh.triangles += 2;
		polygonOffset += stride;
	}
	return Result::Complete;
}
}
Result DecodeBackdrop(Bytes model, const BackdropFace* faces, size_t faceCount, Mesh& output, Failure& failure)
{
	output = Mesh(); failure = Failure();
	try
	{
		Mesh next;
		const Result result = Decode(model,faces,faceCount,next,failure);
		if (result == Result::Complete) output = std::move(next);
		return result;
	}
	catch (const std::bad_alloc&) { failure.result = Result::OutOfBudget; failure.reason.clear(); return Result::OutOfBudget; }
}
}
