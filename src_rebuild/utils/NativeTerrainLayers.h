#ifndef NATIVE_TERRAIN_LAYERS_H
#define NATIVE_TERRAIN_LAYERS_H

#include "NativeAssetDecode.h"
#include <algorithm>
#include <cmath>

namespace NativeAsset
{
// Original tiles author paint as smaller coplanar polygons over a base face.
// Classify geometry, not texture names or a legacy ordering table. Limit the
// one-time pair search; exhaustion rejects the complete resource.
class TerrainLayers
{
	struct Face
	{
		float points[4][3];
		unsigned int count, layer;
		double area;
	};
	std::vector<Face> faces;
	static double Edge(const float* a, const float* b, const float* p)
	{
		return (double(b[0])-a[0])*(double(p[2])-a[2]) -
			(double(b[2])-a[2])*(double(p[0])-a[0]);
	}
	static bool Contains(const Face& base, const Face& overlay)
	{
		if (base.points[0][1] != overlay.points[0][1] || base.area*overlay.area <= 0 ||
			std::fabs(base.area) <= std::fabs(overlay.area)) return false;
		for (unsigned int p=0; p<overlay.count; ++p)
			for (unsigned int edge=0; edge<base.count; ++edge)
				if (Edge(base.points[edge],base.points[(edge+1)%base.count],overlay.points[p])*base.area < 0)
					return false;
		return true;
	}
public:
	Result Classify(const PsyXNativeVertex* corners, unsigned int count, uint32_t& layer)
	{
		layer=0;
		Face face={}; face.count=count;
		for (unsigned int i=0; i<count; ++i)
		{
			for (unsigned int axis=0; axis<3; ++axis) face.points[i][axis]=corners[i].position[axis];
			if (face.points[i][1] != face.points[0][1]) return Result::Complete;
		}
		for (unsigned int i=1; i+1<count; ++i) face.area+=Edge(face.points[0],face.points[i],face.points[i+1]);
		if (!face.area) return Result::Complete;
		// Concave or degenerate quads cannot establish a containing base face.
		for (unsigned int i=0; i<count; ++i)
			if (Edge(face.points[i],face.points[(i+1)%count],face.points[(i+2)%count])*face.area <= 0)
				return Result::Complete;
		if (faces.size() >= 1024) return Result::OutOfBudget;
		for (size_t i=0; i<faces.size(); ++i)
			if (Contains(faces[i],face)) layer=std::max(layer,faces[i].layer+1);
		if (layer > PSYX_NATIVE_MAX_DEPTH_LAYER) return Result::OutOfBudget;
		face.layer=layer; faces.push_back(face);
		return Result::Complete;
	}
};
}
#endif
