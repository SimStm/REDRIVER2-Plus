#include "DeveloperWorldShadow.h"
#include "DeveloperStaticScene.h"
#include "driver2.h"
#include "C/cell.h"
#include "C/draw.h"
#include "C/map.h"
#include "C/mission.h"
#include "C/models.h"
#include "C/spool.h"
#include "PsyX/PsyX_public.h"

#include <bitset>
#include <vector>
#include <array>
#include <math.h>

#if !defined(PSX) && !defined(__ANDROID__) && !defined(__EMSCRIPTEN__)
#include <SDL.h>

namespace
{
struct Point { float x, y, z; };
std::vector<float> s_positions;
DeveloperWorldShadowStats s_diagnostics = {};

// Complete-volume quality tiers, not partial triangle lists. A failed tier
// drops immediately; recovery waits 120 successful frames before one probe.
struct RangePolicy
{
	float nearExtent = 0, requestedFar = 0;
	int tier = 0, stableFrames = 0;
} s_rangePolicy;

struct Plane
{
	float x, y, z, offset;
	float Distance(const Point& p) const { return x * p.x + y * p.y + z * p.z + offset; }
};

struct Volume
{
	PsyXModernShadowVolume description;
	Plane planes[6];
	bool Initialize(float extent)
	{
		if (!PsyX_ModernShadow_GetVolume(extent, &description)) return false;
		const float* m = description.worldToClip;
		for (int face = 0; face < 6; face++)
		{
			const int row = face / 2;
			const float sign = (face & 1) ? -1.0f : 1.0f;
			// Vulkan: -w <= x,y <= w, 0 <= z <= w. The near plane
			// is row z alone, unlike OpenGL's row w + row z.
			const float w = face == 4 ? 0.0f : 1.0f;
			Plane& p = planes[face];
			p = { w * m[3] + sign * m[row], w * m[7] + sign * m[row + 4],
				w * m[11] + sign * m[row + 8], w * m[15] + sign * m[row + 12] };
			const float length = sqrtf(p.x * p.x + p.y * p.y + p.z * p.z);
			if (!(length > 0)) return false;
			p.x /= length; p.y /= length; p.z /= length; p.offset /= length;
		}
		return true;
	}
	bool IntersectsSphere(const Point& centre, float radius) const
	{
		for (const Plane& p : planes) if (p.Distance(centre) < -radius - 1.0f) return false;
		return true;
	}
	bool IntersectsCell(int x, int z, float radius) const
	{
		// Conservative vertical column: do not invent a terrain-height bound.
		const float x0 = (float)x * MAP_CELL_SIZE - units_across_halved - radius;
		const float z0 = (float)z * MAP_CELL_SIZE - units_down_halved - radius;
		const float x1 = x0 + MAP_CELL_SIZE + 2 * radius;
		const float z1 = z0 + MAP_CELL_SIZE + 2 * radius;
		for (const Plane& p : planes)
		{
			const Point farthest = { p.x >= 0 ? x1 : x0,
				p.y >= 0 ? description.boundsMax[1] + radius : description.boundsMin[1] - radius,
				p.z >= 0 ? z1 : z0 };
			if (p.Distance(farthest) < -1.0f) return false;
		}
		return true;
	}
	bool IntersectsTriangle(const Point& a, const Point& b, const Point& c) const
	{
		// Reject only a shared outside plane. A triangle crossing a boundary
		// stays intact for GPU clipping, even if all vertices are outside.
		for (const Plane& p : planes)
			if (p.Distance(a) < -1.0f && p.Distance(b) < -1.0f && p.Distance(c) < -1.0f) return false;
		return true;
	}
};

struct ShadowDemand
{
	Volume volumes[2];
	int count = 1;
	bool Initialize(float nearExtent, float farExtent)
	{
		count = farExtent > nearExtent ? 2 : 1;
		return volumes[0].Initialize(nearExtent) && (count == 1 || volumes[1].Initialize(farExtent));
	}
	bool IntersectsSphere(const Point& p, float radius) const
	{
		for (int i = 0; i < count; i++) if (volumes[i].IntersectsSphere(p, radius)) return true;
		return false;
	}
	bool IntersectsCell(int x, int z, float radius) const
	{
		for (int i = 0; i < count; i++) if (volumes[i].IntersectsCell(x, z, radius)) return true;
		return false;
	}
	bool IntersectsTriangle(const Point& a, const Point& b, const Point& c) const
	{
		for (int i = 0; i < count; i++) if (volumes[i].IntersectsTriangle(a, b, c)) return true;
		return false;
	}
};

struct CollectionStats
{
	int residentCells = 0;
	int unavailableCells = 0;
	int excludedObjects = 0;
	int objects = 0;
	int skippedModels = 0;
	int outsideObjects = 0;
	int testedTriangles = 0;
	int outsideTriangles = 0;
};

// Match the owning cell iterator's residency guard. An unavailable cell is
// different from a resident cell containing no objects; never request loads here.
bool IsCellResident(int x, int z)
{
	const int regionX = x / MAP_REGION_SIZE;
	const int regionZ = z / MAP_REGION_SIZE;
	const int slot = (regionX & 1) + (regionZ & 1) * 2;
	return loading_region[slot] == -1 &&
		RoadMapRegions[slot] == regionX + regionZ * (cells_across / MAP_REGION_SIZE);
}

enum class CollectionStatus { Submitted, CapacityExceeded, Rejected };

void LogCollection(const CollectionStats& stats, Uint64 started, CollectionStatus status, float extent)
{
	static double totalMicros = 0.0, peakMicros = 0.0;
	static unsigned int samples = 0, failedSubmissions = 0;
	const double micros = (double)(SDL_GetPerformanceCounter() - started) *
		1000000.0 / SDL_GetPerformanceFrequency();
	totalMicros += micros;
	peakMicros = MAX(peakMicros, micros);
	if (status != CollectionStatus::Submitted) failedSubmissions++;
	if (++samples != 120)
		return;
	const char* statusName = status == CollectionStatus::Submitted ? "ok" :
		(status == CollectionStatus::CapacityExceeded ? "capacity" : "rejected");
	// Counts describe the last sample, timing/failures the complete window.
	printInfo("WorldShadow: halfSize=%.0f objects=%d triangles=%d skippedModels=%d build_us=%.1f peak_us=%.1f "
		"residentCells=%d unavailableCells=%d excludedObjects=%d outsideObjects=%d testedTriangles=%d outsideTriangles=%d payloadBytes=%u reservedBytes=%u "
		"failedSubmissions=%u status=%s regions=%d,%d,%d,%d resident-only\n",
		extent, stats.objects, (int)s_positions.size() / 9, stats.skippedModels, totalMicros / samples, peakMicros,
		stats.residentCells, stats.unavailableCells, stats.excludedObjects,
		stats.outsideObjects, stats.testedTriangles, stats.outsideTriangles,
		(unsigned int)(s_positions.size() * sizeof(float)), (unsigned int)(s_positions.capacity() * sizeof(float)),
		failedSubmissions, statusName, RoadMapRegions[0], RoadMapRegions[1], RoadMapRegions[2], RoadMapRegions[3]);
	samples = failedSubmissions = 0;
	totalMicros = peakMicros = 0.0;
}

bool IsOpaqueStatic(const MODEL* model)
{
	return model && model != &dummyModel && model->num_polys &&
		!(model->shape_flags & (SHAPE_FLAG_SPRITE | SHAPE_FLAG_ALPHA |
			SHAPE_FLAG_TRANS | SHAPE_FLAG_WATER)) &&
		!(model->flags2 & (MODEL_FLAG_ANIMOBJ | MODEL_FLAG_SMASHABLE));
}

struct ModelBounds
{
	Point centre = {};
	float radius = 0;
	bool valid = false;
};

ModelBounds ReadModelBounds(MODEL* model)
{
	ModelBounds bounds;
	MODEL* source = model;
	if (model->instance_number != -1)
	{
		if (model->instance_number < 0 || model->instance_number >= MAX_MODEL_SLOTS) return bounds;
		source = modelpointers[model->instance_number];
	}
	if (!source || source == &dummyModel || !source->num_vertices || !source->vertices || !model->vertices) return bounds;
	const SVECTOR* vertices = GET_MODEL_DATA(SVECTOR, model, vertices);
	Point low = { (float)vertices[0].vx, (float)vertices[0].vy, (float)vertices[0].vz }, high = low;
	for (int i = 1; i < source->num_vertices; i++)
	{
		low.x = MIN(low.x, vertices[i].vx); low.y = MIN(low.y, vertices[i].vy); low.z = MIN(low.z, vertices[i].vz);
		high.x = MAX(high.x, vertices[i].vx); high.y = MAX(high.y, vertices[i].vy); high.z = MAX(high.z, vertices[i].vz);
	}
	bounds.centre = { (low.x + high.x) * 0.5f, (low.y + high.y) * 0.5f, (low.z + high.z) * 0.5f };
	const float x = high.x - low.x, y = high.y - low.y, z = high.z - low.z;
	// Include Q12 yaw-table rounding in the otherwise rotation-invariant sphere.
	bounds.radius = sqrtf(x * x + y * y + z * z) * 0.5005f;
	bounds.valid = true;
	return bounds;
}

Point WorldBoundsCentre(const ModelBounds& bounds, const CELL_OBJECT& object)
{
	const MATRIX2& rotation = matrixtable[object.yang & 63];
	const Point& p = bounds.centre;
	return { object.pos.vx + (rotation.m[0][0] * p.x + rotation.m[0][1] * p.y + rotation.m[0][2] * p.z) / 4096.0f,
		object.pos.vy + (rotation.m[1][0] * p.x + rotation.m[1][1] * p.y + rotation.m[1][2] * p.z) / 4096.0f,
		object.pos.vz + (rotation.m[2][0] * p.x + rotation.m[2][1] * p.y + rotation.m[2][2] * p.z) / 4096.0f };
}

Point WorldPoint(const SVECTOR& vertex, const CELL_OBJECT& object)
{
	const MATRIX2& rotation = matrixtable[object.yang & 63];
	const float scale = 1.0f / 4096.0f;
	Point point;
	point.x = object.pos.vx + scale * (rotation.m[0][0] * vertex.vx + rotation.m[0][1] * vertex.vy + rotation.m[0][2] * vertex.vz);
	point.y = object.pos.vy + scale * (rotation.m[1][0] * vertex.vx + rotation.m[1][1] * vertex.vy + rotation.m[1][2] * vertex.vz);
	point.z = object.pos.vz + scale * (rotation.m[2][0] * vertex.vx + rotation.m[2][1] * vertex.vy + rotation.m[2][2] * vertex.vz);
	return point;
}

bool AppendTriangle(const Point& a, const Point& b, const Point& c,
	const ShadowDemand& demand, CollectionStats& stats)
{
	stats.testedTriangles++;
	if (!demand.IntersectsTriangle(a, b, c)) { stats.outsideTriangles++; return true; }
	if (s_positions.size() + 9 > PSYX_WORLD_SHADOW_MAX_VERTICES * 3) return false;
	const Point points[3] = { a, b, c };
	for (int i = 0; i < 3; i++)
	{
		s_positions.push_back(points[i].x);
		s_positions.push_back(points[i].y);
		s_positions.push_back(points[i].z);
	}
	return true;
}

// Read current model storage each frame; never retain pointers/decoded meshes
// across slot reuse or region/level changes. Do not call ConvertPolygonTypes:
// it mutates game-owned models and is unnecessary for shadow triangulation.
enum class AppendResult { Complete, InvalidModel, CapacityExceeded };

AppendResult AppendModel(MODEL* model, const CELL_OBJECT& object,
	const ShadowDemand& demand, CollectionStats& stats)
{
	MODEL* source = model;
	if (source->instance_number != -1)
	{
		if (source->instance_number < 0 || source->instance_number >= MAX_MODEL_SLOTS)
			return AppendResult::InvalidModel;
		source = modelpointers[source->instance_number];
	}
	if (!source || source == &dummyModel || !source->num_vertices || !source->vertices || !model->poly_block)
		return AppendResult::InvalidModel;
	const SVECTOR* vertices = GET_MODEL_DATA(SVECTOR, model, vertices);
	const unsigned char* polygon = GET_MODEL_DATA(unsigned char, model, poly_block);
	const size_t start = s_positions.size();
	for (int i = 0; i < model->num_polys; i++)
	{
		const int type = polygon[0] & 31;
		const int stride = PolySizes[type];
		// These are the source FT3/FT4 and building/road equivalents. Reject
		// an unsupported whole model instead of fabricating partial geometry.
		if ((type != 10 && type != 11 && type != 20 && type != 21 && type != 22 && type != 23) || stride < 8)
		{
			s_positions.resize(start);
			return AppendResult::InvalidModel;
		}
		const bool quad = (type & 1) != 0;
		const unsigned char* indices = polygon + 4;
		for (int v = 0; v < (quad ? 4 : 3); v++)
			if (indices[v] >= source->num_vertices)
			{
				s_positions.resize(start);
				return AppendResult::InvalidModel;
			}
		const Point a = WorldPoint(vertices[indices[0]], object);
		const Point b = WorldPoint(vertices[indices[1]], object);
		const Point c = WorldPoint(vertices[indices[2]], object);
		bool appended;
		if (quad)
		{
			const Point d = WorldPoint(vertices[indices[3]], object);
			appended = AppendTriangle(a, b, d, demand, stats);
			if (appended && indices[3] != indices[2]) // converted FT3 has a degenerate second triangle
				appended = AppendTriangle(d, b, c, demand, stats);
		}
		else
			appended = AppendTriangle(a, b, c, demand, stats);
		if (!appended)
		{
			s_positions.resize(start);
			return AppendResult::CapacityExceeded;
		}
		polygon += stride;
	}
	return AppendResult::Complete;
}
}

void DeveloperWorldShadow_Clear(void)
{
	s_positions.clear();
	PsyX_ModernShadow_SetWorldTriangles(NULL, 0);
	s_rangePolicy = RangePolicy();
	s_diagnostics = {};
}

void DeveloperWorldShadow_GetStats(DeveloperWorldShadowStats* stats)
{
	if (stats) *stats = s_diagnostics;
}

static CollectionStatus Collect(float nearExtent, float farExtent)
{
	s_diagnostics.collectionAttempts++;
	s_positions.clear();
	PsyX_ModernShadow_SetWorldTriangles(NULL, 0);
	if (NumPlayers != 1 || !cell_ptrs || !cells || !cell_objects || MAP_CELL_SIZE <= 0 ||
		MAP_REGION_SIZE <= 0 || cells_across <= 0 || cells_down <= 0)
		return CollectionStatus::Rejected;
	const Uint64 started = SDL_GetPerformanceCounter();
	ShadowDemand demand;
	if (!demand.Initialize(nearExtent, farExtent)) return CollectionStatus::Rejected;
	const float extent = MAX(nearExtent, farExtent);
	if (s_positions.capacity() < PSYX_WORLD_SHADOW_MAX_VERTICES * 3)
		s_positions.reserve(PSYX_WORLD_SHADOW_MAX_VERTICES * 3);
	int maxRadius = 0;
	std::array<ModelBounds, MAX_MODEL_SLOTS> modelBounds;
	for (int i = 0; i < MAX_MODEL_SLOTS; i++)
		if (IsOpaqueStatic(modelpointers[i]))
		{
			ModelBounds& bounds = modelBounds[i];
			bounds = ReadModelBounds(modelpointers[i]);
			if (!bounds.valid) continue;
			const Point& p = bounds.centre;
			// Legacy bounds are camera-culling hints, not guaranteed 3D spheres.
			// Source bounds are recomputed per collection, including instance data.
			maxRadius = MAX(maxRadius, (int)ceilf(bounds.radius + sqrtf(p.x * p.x + p.y * p.y + p.z * p.z) * 1.001f));
		}
	float low[3], high[3];
	for (int axis = 0; axis < 3; axis++)
	{
		low[axis] = demand.volumes[0].description.boundsMin[axis];
		high[axis] = demand.volumes[0].description.boundsMax[axis];
		if (demand.count == 2)
		{
			low[axis] = MIN(low[axis], demand.volumes[1].description.boundsMin[axis]);
			high[axis] = MAX(high[axis], demand.volumes[1].description.boundsMax[axis]);
		}
	}
	// Cover both independently snapped volumes. Padding retains straddlers;
	// camera/PVS selection and gameplay cell-usage state are never consulted.
	const float padding = maxRadius + MAP_CELL_SIZE + 1.0f;
	const int x0 = MAX(0, (int)floorf((low[0] - padding + units_across_halved) / MAP_CELL_SIZE));
	const int x1 = MIN(cells_across - 1, (int)floorf((high[0] + padding + units_across_halved) / MAP_CELL_SIZE));
	const int z0 = MAX(0, (int)floorf((low[2] - padding + units_down_halved) / MAP_CELL_SIZE));
	const int z1 = MIN(cells_down - 1, (int)floorf((high[2] + padding + units_down_halved) / MAP_CELL_SIZE));
	std::bitset<16384> seen; // cell references are 14-bit; straddlers repeat in cells
	CollectionStats stats;
	for (int z = z0; z <= z1; z++)
		for (int x = x0; x <= x1; x++)
		{
			if (!demand.IntersectsCell(x, z, (float)maxRadius)) continue;
			if (!IsCellResident(x, z))
			{
				stats.unavailableCells++;
				continue;
			}
			stats.residentCells++;
			CELL_ITERATOR iterator;
			// use_computed=0 does not touch the gameplay's cell-usage bitset.
			// GetFirstPackedCop rejects missing/reused/loading region slots.
			for (PACKED_CELL_OBJECT* packed = GetFirstPackedCop(x, z, &iterator, 0);
				packed; packed = GetNextPackedCop(&iterator))
			{
				const ptrdiff_t index = packed - cell_objects;
				if (index < 0 || index >= 16384 || seen.test((size_t)index))
					continue;
				seen.set((size_t)index);
				CELL_OBJECT object;
				QuickUnpackCellObject(packed, &iterator.nearCell, &object);
				if (object.type < 0 || object.type >= MAX_MODEL_SLOTS)
					continue;
				// The converted instance casts from its persistent colour mesh.
				if (DeveloperStaticScene_Owns(&object))
					continue;
				MODEL* model = modelpointers[object.type];
				if (!IsOpaqueStatic(model))
				{
					stats.excludedObjects++;
					continue;
				}
				const ModelBounds& bounds = modelBounds[object.type];
				if (!bounds.valid) { stats.skippedModels++; continue; }
				if (!demand.IntersectsSphere(WorldBoundsCentre(bounds, object), bounds.radius))
					{ stats.outsideObjects++; continue; }
				const AppendResult result = AppendModel(model, object, demand, stats);
				if (result == AppendResult::CapacityExceeded)
				{
					s_positions.clear();
					static bool warned = false;
					if (!warned)
						printWarning("WorldShadow: capacity exceeded; rejecting this collection radius\n");
					warned = true;
					LogCollection(stats, started, CollectionStatus::CapacityExceeded, extent);
					s_diagnostics.capacityFailures++;
					return CollectionStatus::CapacityExceeded;
				}
				if (result == AppendResult::Complete) stats.objects++; else stats.skippedModels++;
			}
		}
	const int count = (int)s_positions.size() / 3;
	const bool submitted = PsyX_ModernShadow_SetWorldTriangles(count ? s_positions.data() : NULL, count) != 0;
	if (!submitted)
		printWarning("WorldShadow: submission rejected (%d vertices)\n", count);
	const CollectionStatus status = submitted ? CollectionStatus::Submitted : CollectionStatus::Rejected;
	if (submitted)
	{
		s_diagnostics.residentCells = stats.residentCells;
		s_diagnostics.unavailableCells = stats.unavailableCells;
		s_diagnostics.triangles = count / 3;
		s_diagnostics.outsideObjects = stats.outsideObjects;
		s_diagnostics.outsideTriangles = stats.outsideTriangles;
		s_diagnostics.effectiveFarExtent = farExtent;
	}
	LogCollection(stats, started, status, extent);
	return status;
}

float DeveloperWorldShadow_Submit(float nearExtent, float requestedFarExtent)
{
	const float requested = requestedFarExtent > nearExtent ? requestedFarExtent : 0.0f;
	s_diagnostics = {};
	s_diagnostics.requestedFarExtent = requested;
	if (s_rangePolicy.nearExtent != nearExtent || s_rangePolicy.requestedFar != requested)
	{
		s_rangePolicy = RangePolicy();
		s_rangePolicy.nearExtent = nearExtent;
		s_rangePolicy.requestedFar = requested;
	}
	if (!requested) { Collect(nearExtent, 0); return 0; }
	const float tiers[4] = { requested, requested * 0.75f, requested * 0.5f, 0 };
	if (s_rangePolicy.tier && s_rangePolicy.stableFrames >= 120)
	{
		s_rangePolicy.tier--;
		s_rangePolicy.stableFrames = 0;
	}
	for (; s_rangePolicy.tier < 4; s_rangePolicy.tier++)
	{
		const float far = tiers[s_rangePolicy.tier] > nearExtent ? tiers[s_rangePolicy.tier] : 0;
		// Ignore collapsed intermediate tiers, but always try a complete near map.
		if (!far && s_rangePolicy.tier < 3) continue;
		const CollectionStatus status = Collect(nearExtent, far);
		if (status == CollectionStatus::Submitted)
		{
			s_rangePolicy.stableFrames = MIN(s_rangePolicy.stableFrames + 1, 120);
			return far;
		}
		s_rangePolicy.stableFrames = 0;
		if (status != CollectionStatus::CapacityExceeded) break;
	}
	s_rangePolicy.tier = 3;
	return 0;
}

#else
void DeveloperWorldShadow_Clear(void) {}
float DeveloperWorldShadow_Submit(float, float) { return 0; }
void DeveloperWorldShadow_GetStats(DeveloperWorldShadowStats* stats)
{
	if (stats) *stats = {};
}
#endif
