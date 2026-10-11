#include "WorldMeshBuilder.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

namespace
{
void SetError(char* error, int capacity, const char* message)
{
	if (error && capacity > 0)
		snprintf(error, (size_t)capacity, "%s", message);
}

void ZeroResult(WorldMeshBuilderResult* result)
{
	if (!result)
		return;

	memset(result, 0, sizeof(*result));
	result->error = WORLD_MESH_ERROR_NONE;
	result->primitiveClass = SourcePrimitiveClass::TexturedFlatQuad;
}

WorldMeshBuilderError Fail(WorldMeshBuilderResult* result, char* error, int capacity,
	WorldMeshBuilderError code, const char* message)
{
	SetError(error, capacity, message);
	if (result)
		result->error = code;
	return code;
}

// Flat geometric normal of one emitted triangle, in the decoder's world winding
// order. A zero-area triangle has no geometric normal, so the documented
// fallback (0,0,1) is used instead of an undefined vector.
void TriangleNormal(const float* p0, const float* p1, const float* p2, float out[3])
{
	const double ax = (double)p1[0] - p0[0];
	const double ay = (double)p1[1] - p0[1];
	const double az = (double)p1[2] - p0[2];
	const double bx = (double)p2[0] - p0[0];
	const double by = (double)p2[1] - p0[1];
	const double bz = (double)p2[2] - p0[2];

	double nx = ay * bz - az * by;
	double ny = az * bx - ax * bz;
	double nz = ax * by - ay * bx;
	const double length = sqrt(nx * nx + ny * ny + nz * nz);

	if (length > 0.0)
	{
		nx /= length;
		ny /= length;
		nz /= length;
	}
	else
	{
		nx = 0.0;
		ny = 0.0;
		nz = 1.0;
	}

	out[0] = (float)nx;
	out[1] = (float)ny;
	out[2] = (float)nz;
}

// Mean of the single submesh's raw stored face colours, over [0,1]. Deterministic
// (double accumulation in index order). Callers only reach this when hasColors is
// true and the colour array has been validated.
void AverageSubmeshColor(const SourceMesh& source, const SourceSubmesh& submesh, float factor[4])
{
	factor[0] = factor[1] = factor[2] = factor[3] = 1.0f;

	double sum[3] = { 0.0, 0.0, 0.0 };
	unsigned int count = 0;

	for (unsigned int i = submesh.firstIndex; i < submesh.firstIndex + submesh.indexCount; ++i)
	{
		const unsigned int vertex = source.indices[i];
		sum[0] += (double)source.colors[vertex * 3 + 0];
		sum[1] += (double)source.colors[vertex * 3 + 1];
		sum[2] += (double)source.colors[vertex * 3 + 2];
		count++;
	}

	if (count == 0)
		return;

	factor[0] = (float)(sum[0] / ((double)count * 255.0));
	factor[1] = (float)(sum[1] / ((double)count * 255.0));
	factor[2] = (float)(sum[2] / ((double)count * 255.0));
	factor[3] = 1.0f;
}
} // namespace

const char* WorldMeshBuilder_ErrorName(WorldMeshBuilderError error)
{
	switch (error)
	{
	case WORLD_MESH_ERROR_NONE: return "none";
	case WORLD_MESH_ERROR_ARGUMENT: return "argument";
	case WORLD_MESH_ERROR_EMPTY: return "empty";
	case WORLD_MESH_ERROR_MULTIPLE_SUBMESHES: return "multiple-submeshes-unsupported";
	case WORLD_MESH_ERROR_ATTRIBUTE_SIZE: return "attribute-size";
	case WORLD_MESH_ERROR_INDEX_RANGE: return "index-range";
	case WORLD_MESH_ERROR_UNCOVERED_INDICES: return "uncovered-indices";
	case WORLD_MESH_ERROR_IMAGE: return "image";
	default: return "unknown";
	}
}

int WorldMeshBuilder_Build(const SourceMesh& source, GltfMeshData* out,
	WorldMeshBuilderResult* result, char* error, int capacity)
{
	if (error && capacity > 0)
		error[0] = '\0';

	ZeroResult(result);

	if (!out)
		return (int)Fail(result, error, capacity, WORLD_MESH_ERROR_ARGUMENT,
			"a destination mesh is required");

	*out = GltfMeshData(); // never leave a partial result behind

	// Positions define the vertex count and must be whole vertices.
	if (source.positions.empty())
		return (int)Fail(result, error, capacity, WORLD_MESH_ERROR_EMPTY,
			"the source mesh has no vertices");

	if (source.positions.size() % 3 != 0)
		return (int)Fail(result, error, capacity, WORLD_MESH_ERROR_ATTRIBUTE_SIZE,
			"the source position array is not a whole number of vertices");

	const int vertexCount = (int)(source.positions.size() / 3);

	// UVs are required per vertex in this pipeline (the decoder always emits
	// them); colours are either absent or one RGB per vertex.
	if (source.uvs.size() != (size_t)vertexCount * 2)
		return (int)Fail(result, error, capacity, WORLD_MESH_ERROR_ATTRIBUTE_SIZE,
			"the source UV count does not match the vertex count");

	if (!source.colors.empty() && source.colors.size() != (size_t)vertexCount * 3)
		return (int)Fail(result, error, capacity, WORLD_MESH_ERROR_ATTRIBUTE_SIZE,
			"the source colour count does not match the vertex count");

	if (source.indices.empty() || source.indices.size() % 3 != 0)
		return (int)Fail(result, error, capacity, WORLD_MESH_ERROR_EMPTY,
			"the source mesh has no whole triangles");

	// Exactly one submesh is representable by the one-primitive writer.
	if (source.submeshes.empty())
		return (int)Fail(result, error, capacity, WORLD_MESH_ERROR_EMPTY,
			"the source mesh has no submesh");

	if (source.submeshes.size() > 1)
		return (int)Fail(result, error, capacity, WORLD_MESH_ERROR_MULTIPLE_SUBMESHES,
			"the source model has more than one submesh and the writer supports one primitive");

	const SourceSubmesh& submesh = source.submeshes[0];

	// The single submesh must be a whole-triangle run that covers every index,
	// so no supported geometry is silently dropped.
	if (submesh.indexCount == 0 || submesh.indexCount % 3 != 0 ||
		(size_t)submesh.firstIndex + submesh.indexCount > source.indices.size())
		return (int)Fail(result, error, capacity, WORLD_MESH_ERROR_INDEX_RANGE,
			"the submesh index run is not a whole-triangle range inside the index list");

	if (submesh.firstIndex != 0 || (size_t)submesh.indexCount != source.indices.size())
		return (int)Fail(result, error, capacity, WORLD_MESH_ERROR_UNCOVERED_INDICES,
			"the single submesh does not cover every index");

	// The colour presence flag and the colour array must agree.
	if (submesh.hasColors && source.colors.empty())
		return (int)Fail(result, error, capacity, WORLD_MESH_ERROR_ATTRIBUTE_SIZE,
			"the submesh declares face colours but the source has none");

	if (!submesh.hasColors && !source.colors.empty())
		return (int)Fail(result, error, capacity, WORLD_MESH_ERROR_ATTRIBUTE_SIZE,
			"the source has face colours but the submesh does not declare them");

	// Every index must address a real vertex.
	for (size_t i = 0; i < source.indices.size(); ++i)
	{
		if (source.indices[i] >= (unsigned int)vertexCount)
			return (int)Fail(result, error, capacity, WORLD_MESH_ERROR_INDEX_RANGE,
				"a triangle index is outside the vertex range");
	}

	const int triangleCount = (int)(submesh.indexCount / 3);
	const bool hasColors = submesh.hasColors;
	const bool hasUvs = !source.uvs.empty();

	GltfMeshData mesh;
	mesh.positions.reserve((size_t)triangleCount * 9);
	mesh.normals.reserve((size_t)triangleCount * 9);
	if (hasUvs)
		mesh.uvs.reserve((size_t)triangleCount * 6);
	mesh.indices.reserve((size_t)triangleCount * 3);

	for (int triangle = 0; triangle < triangleCount; ++triangle)
	{
		const unsigned int i0 = source.indices[submesh.firstIndex + triangle * 3 + 0];
		const unsigned int i1 = source.indices[submesh.firstIndex + triangle * 3 + 1];
		const unsigned int i2 = source.indices[submesh.firstIndex + triangle * 3 + 2];

		const float* p0 = &source.positions[(size_t)i0 * 3];
		const float* p1 = &source.positions[(size_t)i1 * 3];
		const float* p2 = &source.positions[(size_t)i2 * 3];

		float normal[3];
		TriangleNormal(p0, p1, p2, normal);

		const unsigned int corners[3] = { i0, i1, i2 };
		const unsigned int baseVertex = (unsigned int)(mesh.positions.size() / 3);

		for (int corner = 0; corner < 3; ++corner)
		{
			const unsigned int vertex = corners[corner];
			mesh.positions.push_back(source.positions[(size_t)vertex * 3 + 0]);
			mesh.positions.push_back(source.positions[(size_t)vertex * 3 + 1]);
			mesh.positions.push_back(source.positions[(size_t)vertex * 3 + 2]);

			mesh.normals.push_back(normal[0]);
			mesh.normals.push_back(normal[1]);
			mesh.normals.push_back(normal[2]);

			if (hasUvs)
			{
				mesh.uvs.push_back(source.uvs[(size_t)vertex * 2 + 0]);
				mesh.uvs.push_back(source.uvs[(size_t)vertex * 2 + 1]);
			}
		}

		mesh.indices.push_back(baseVertex + 0);
		mesh.indices.push_back(baseVertex + 1);
		mesh.indices.push_back(baseVertex + 2);
	}

	mesh.vertexCount = (int)(mesh.positions.size() / 3);
	mesh.triangleCount = triangleCount;

	// Base colour: mean of the raw stored face colours, or the documented
	// neutral when the record stored none. No draw-time shading is baked in.
	if (hasColors)
		AverageSubmeshColor(source, submesh, mesh.baseColorFactor);
	else
	{
		mesh.baseColorFactor[0] = 1.0f;
		mesh.baseColorFactor[1] = 1.0f;
		mesh.baseColorFactor[2] = 1.0f;
		mesh.baseColorFactor[3] = 1.0f;
	}

	*out = mesh;

	if (result)
	{
		result->error = WORLD_MESH_ERROR_NONE;
		result->submeshCount = 1;
		result->vertexCount = mesh.vertexCount;
		result->triangleCount = mesh.triangleCount;
		result->primitiveClass = submesh.primitiveClass;
		result->textureSet = submesh.textureSet;
		result->textureId = submesh.textureId;
		result->hasColors = hasColors;
	}

	return (int)WORLD_MESH_ERROR_NONE;
}

int WorldMeshBuilder_BuildMulti(const SourceMesh& source, GltfMultiMeshData* out,
	WorldMeshBuilderResult* result, char* error, int capacity,
	const WorldSubmeshImage* images, int imageCount,
	const unsigned char* vertexColorsRgb, size_t vertexColorBytes,
	const float* worldShadeIndices, size_t worldShadeCount)
{
	if (error && capacity > 0)
		error[0] = '\0';

	ZeroResult(result);

	if (!out)
		return (int)Fail(result, error, capacity, WORLD_MESH_ERROR_ARGUMENT,
			"a destination mesh is required");

	*out = GltfMultiMeshData(); // never leave a partial result behind

	// Positions define the vertex count and must be whole vertices.
	if (source.positions.empty())
		return (int)Fail(result, error, capacity, WORLD_MESH_ERROR_EMPTY,
			"the source mesh has no vertices");

	if (source.positions.size() % 3 != 0)
		return (int)Fail(result, error, capacity, WORLD_MESH_ERROR_ATTRIBUTE_SIZE,
			"the source position array is not a whole number of vertices");

	const int vertexCount = (int)(source.positions.size() / 3);
	if (worldShadeIndices || worldShadeCount)
	{
		if (!worldShadeIndices || worldShadeCount != (size_t)vertexCount)
			return (int)Fail(result, error, capacity, WORLD_MESH_ERROR_ATTRIBUTE_SIZE,
				"world shade count does not match the vertex count");
		for (size_t v = 0; v < worldShadeCount; ++v)
		{
			const float shade = worldShadeIndices[v];
			if (!(shade >= -1.0f && shade <= 31.0f) || shade != (float)(int)shade)
				return (int)Fail(result, error, capacity, WORLD_MESH_ERROR_ATTRIBUTE_SIZE,
					"invalid world shade index");
		}
	}

	// UVs are required per vertex in this pipeline; colours are either absent or
	// one global RGB array with per-submesh presence flags.
	if (source.uvs.size() != (size_t)vertexCount * 2)
		return (int)Fail(result, error, capacity, WORLD_MESH_ERROR_ATTRIBUTE_SIZE,
			"the source UV count does not match the vertex count");

	if (!source.colors.empty() && source.colors.size() != (size_t)vertexCount * 3)
		return (int)Fail(result, error, capacity, WORLD_MESH_ERROR_ATTRIBUTE_SIZE,
			"the source colour count does not match the vertex count");

	if (source.indices.empty() || source.indices.size() % 3 != 0)
		return (int)Fail(result, error, capacity, WORLD_MESH_ERROR_EMPTY,
			"the source mesh has no whole triangles");

	if (source.submeshes.empty())
		return (int)Fail(result, error, capacity, WORLD_MESH_ERROR_EMPTY,
			"the source mesh has no submesh");

	// The submesh runs must be an ordered, contiguous partition of the index
	// list, so every index is emitted exactly once and none is dropped.
	size_t expectedFirst = 0;
	for (size_t s = 0; s < source.submeshes.size(); ++s)
	{
		const SourceSubmesh& submesh = source.submeshes[s];

		if (submesh.indexCount == 0 || submesh.indexCount % 3 != 0 ||
			(size_t)submesh.firstIndex + submesh.indexCount > source.indices.size())
			return (int)Fail(result, error, capacity, WORLD_MESH_ERROR_INDEX_RANGE,
				"a submesh index run is not a whole-triangle range inside the index list");

		if ((size_t)submesh.firstIndex != expectedFirst)
			return (int)Fail(result, error, capacity, WORLD_MESH_ERROR_UNCOVERED_INDICES,
				"the submesh index runs do not contiguously cover the index list");

		if (submesh.hasColors && source.colors.empty())
			return (int)Fail(result, error, capacity, WORLD_MESH_ERROR_ATTRIBUTE_SIZE,
				"a submesh declares face colours but the source has none");

		expectedFirst += submesh.indexCount;
	}

	if (expectedFirst != source.indices.size())
		return (int)Fail(result, error, capacity, WORLD_MESH_ERROR_UNCOVERED_INDICES,
			"the submesh index runs do not cover every index");

	// Every index must address a real vertex.
	for (size_t i = 0; i < source.indices.size(); ++i)
	{
		if (source.indices[i] >= (unsigned int)vertexCount)
			return (int)Fail(result, error, capacity, WORLD_MESH_ERROR_INDEX_RANGE,
				"a triangle index is outside the vertex range");
	}

	// Optional per-submesh images: validate the whole array before any output is
	// produced, so a bad image leaves an empty record. A null entry is
	// untextured; a non-null entry needs positive dimensions whose
	// width*height*4 size matches the declared byte count.
	if (imageCount != 0)
	{
		if (!images || imageCount != (int)source.submeshes.size())
			return (int)Fail(result, error, capacity, WORLD_MESH_ERROR_ARGUMENT,
				"the submesh image array must have one entry per submesh");

		for (size_t s = 0; s < source.submeshes.size(); ++s)
		{
			const WorldSubmeshImage& image = images[s];
			if (!image.rgba)
				continue; // untextured submesh

			if (image.width <= 0 || image.height <= 0)
				return (int)Fail(result, error, capacity, WORLD_MESH_ERROR_IMAGE,
					"a submesh image has non-positive dimensions");

			const size_t w = (size_t)image.width;
			const size_t h = (size_t)image.height;
			if (w > (size_t)-1 / 4u || w * 4u > (size_t)-1 / h)
				return (int)Fail(result, error, capacity, WORLD_MESH_ERROR_IMAGE,
					"a submesh image is too large");

			if (image.byteCount != w * h * 4u)
				return (int)Fail(result, error, capacity, WORLD_MESH_ERROR_IMAGE,
					"a submesh image byte count does not match its dimensions");
		}
	}

	const bool hasUvs = !source.uvs.empty();

	// Optional per-vertex colours: one RGB triple per source vertex. Both the
	// pointer and a matching byte count are required; the count must be exactly
	// vertexCount*3 so the copy is bounded.
	const bool hasVertexColors = vertexColorsRgb != NULL && vertexColorBytes > 0;
	if (hasVertexColors && vertexColorBytes != (size_t)vertexCount * 3u)
		return (int)Fail(result, error, capacity, WORLD_MESH_ERROR_ATTRIBUTE_SIZE,
			"the per-vertex colour byte count does not match the vertex count");

	out->primitives.reserve(source.submeshes.size());

	int totalVertices = 0;
	int totalTriangles = 0;
	bool anyColors = false;

	for (size_t s = 0; s < source.submeshes.size(); ++s)
	{
		const SourceSubmesh& submesh = source.submeshes[s];
		const int triangleCount = (int)(submesh.indexCount / 3);

		out->primitives.push_back(GltfPrimitiveData());
		GltfPrimitiveData& primitive = out->primitives.back();
		primitive.positions.reserve((size_t)triangleCount * 9);
		primitive.normals.reserve((size_t)triangleCount * 9);
		if (hasUvs)
			primitive.uvs.reserve((size_t)triangleCount * 6);
		if (hasVertexColors)
			primitive.colors.reserve((size_t)triangleCount * 12);
		primitive.indices.reserve((size_t)triangleCount * 3);

		for (int triangle = 0; triangle < triangleCount; ++triangle)
		{
			const unsigned int i0 = source.indices[submesh.firstIndex + triangle * 3 + 0];
			const unsigned int i1 = source.indices[submesh.firstIndex + triangle * 3 + 1];
			const unsigned int i2 = source.indices[submesh.firstIndex + triangle * 3 + 2];

			const float* p0 = &source.positions[(size_t)i0 * 3];
			const float* p1 = &source.positions[(size_t)i1 * 3];
			const float* p2 = &source.positions[(size_t)i2 * 3];

			float normal[3];
			TriangleNormal(p0, p1, p2, normal);

			const unsigned int corners[3] = { i0, i1, i2 };
			const unsigned int baseVertex = (unsigned int)(primitive.positions.size() / 3);

			for (int corner = 0; corner < 3; ++corner)
			{
				const unsigned int vertex = corners[corner];
				if (worldShadeIndices)
					primitive.worldShadeIndices.push_back(worldShadeIndices[vertex]);
				primitive.positions.push_back(source.positions[(size_t)vertex * 3 + 0]);
				primitive.positions.push_back(source.positions[(size_t)vertex * 3 + 1]);
				primitive.positions.push_back(source.positions[(size_t)vertex * 3 + 2]);

				primitive.normals.push_back(normal[0]);
				primitive.normals.push_back(normal[1]);
				primitive.normals.push_back(normal[2]);

				if (hasUvs)
				{
					primitive.uvs.push_back(source.uvs[(size_t)vertex * 2 + 0]);
					primitive.uvs.push_back(source.uvs[(size_t)vertex * 2 + 1]);
				}

				if (hasVertexColors)
				{
					primitive.colors.push_back(vertexColorsRgb[(size_t)vertex * 3 + 0]);
					primitive.colors.push_back(vertexColorsRgb[(size_t)vertex * 3 + 1]);
					primitive.colors.push_back(vertexColorsRgb[(size_t)vertex * 3 + 2]);
					primitive.colors.push_back(255);
				}
			}

			primitive.indices.push_back(baseVertex + 0);
			primitive.indices.push_back(baseVertex + 1);
			primitive.indices.push_back(baseVertex + 2);
		}

		primitive.vertexCount = (int)(primitive.positions.size() / 3);
		primitive.triangleCount = triangleCount;

		// Per-primitive base colour, exactly as the single path applies it to
		// its one submesh: the mean of that submesh's stored face colours, or
		// the documented neutral when the submesh stored none. When explicit
		// per-vertex colours are supplied the factor is the neutral white, so
		// the vertex colour is the only per-primitive modulation.
		if (hasVertexColors)
		{
			primitive.baseColorFactor[0] = 1.0f;
			primitive.baseColorFactor[1] = 1.0f;
			primitive.baseColorFactor[2] = 1.0f;
			primitive.baseColorFactor[3] = 1.0f;
		}
		else if (submesh.hasColors)
			AverageSubmeshColor(source, submesh, primitive.baseColorFactor);
		else
		{
			primitive.baseColorFactor[0] = 1.0f;
			primitive.baseColorFactor[1] = 1.0f;
			primitive.baseColorFactor[2] = 1.0f;
			primitive.baseColorFactor[3] = 1.0f;
		}

		// Attach this submesh's baked image, if any. The array was validated
		// above, so the copy is bounded and cannot fail here.
		if (images && images[s].rgba)
		{
			const WorldSubmeshImage& image = images[s];
			const size_t imageBytes = (size_t)image.width * (size_t)image.height * 4u;
			primitive.imageRgba.assign(image.rgba, image.rgba + imageBytes);
			primitive.imageWidth = image.width;
			primitive.imageHeight = image.height;
		}

		totalVertices += primitive.vertexCount;
		totalTriangles += triangleCount;
		anyColors = anyColors || submesh.hasColors;
	}

	if (result)
	{
		result->error = WORLD_MESH_ERROR_NONE;
		result->submeshCount = (int)source.submeshes.size();
		result->vertexCount = totalVertices;
		result->triangleCount = totalTriangles;
		result->primitiveClass = source.submeshes[0].primitiveClass;
		result->textureSet = source.submeshes[0].textureSet;
		result->textureId = source.submeshes[0].textureId;
		result->hasColors = anyColors;
	}

	return (int)WORLD_MESH_ERROR_NONE;
}
