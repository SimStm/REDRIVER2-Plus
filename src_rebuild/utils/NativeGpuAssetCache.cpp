#include "NativeGpuAssetCache.h"
#include "PsyX/PsyX_globals.h"
#include <cmath>
#include <cstring>
#include <new>
#include <utility>

namespace NativeAsset
{
namespace
{
void HashBytes(uint64_t& hash, const void* data, size_t size)
{
	const unsigned char* bytes = static_cast<const unsigned char*>(data);
	for (size_t i = 0; i < size; ++i) { hash ^= bytes[i]; hash *= UINT64_C(1099511628211); }
}
bool ValidateMaterial(const Part& part, const ResolvedMaterial& material)
{
	if (part.depthLayer > PSYX_NATIVE_MAX_DEPTH_LAYER) return false;
	const Texture& image = material.image;
	if (!material.regionWidth || !material.regionHeight || material.regionWidth > 256 || material.regionHeight > 256 ||
		material.regionU > 256-material.regionWidth || material.regionV > 256-material.regionHeight ||
		!image.width || !image.height || image.width > 4096 || image.height > 4096 ||
		uint64_t(image.width)*image.height*4 != material.Pixels().size() || !material.revision ||
		(material.filter != PSYX_NATIVE_NEAREST && material.filter != PSYX_NATIVE_LINEAR && material.filter != PSYX_NATIVE_TRILINEAR) ||
		(material.cull != PSYX_NATIVE_CULL_BACK && material.cull != PSYX_NATIVE_CULL_NONE) ||
		(material.uvPolicy != ResolvedMaterial::UvPolicy::InsideRegion && material.uvPolicy != ResolvedMaterial::UvPolicy::ClampOverride)) return false;
	for (size_t i = 0; i < part.vertices.size(); ++i)
	{
		const float u = part.vertices[i].uv[0]*256-material.regionU;
		const float v = part.vertices[i].uv[1]*256-material.regionV;
		if (!std::isfinite(u) || !std::isfinite(v) || part.vertices[i].uv[0] < 0 || part.vertices[i].uv[1] < 0 ||
			part.vertices[i].uv[0] >= 1 || part.vertices[i].uv[1] >= 1) return false;
		if (material.uvPolicy == ResolvedMaterial::UvPolicy::InsideRegion &&
			(u < 0 || v < 0 || u >= material.regionWidth || v >= material.regionHeight)) return false;
	}
	return true;
}
uint64_t MaterialSignature(const Part& part, const ResolvedMaterial& material)
{
	const Texture& image = material.image;
	uint64_t signature = UINT64_C(14695981039346656037);
	const unsigned int context[] = { part.material.textureSet, part.material.textureIndex, image.tpage, image.clut,
		material.regionU, material.regionV, material.regionWidth, material.regionHeight, image.width, image.height,
		unsigned(material.filter), unsigned(material.uvPolicy), unsigned(material.cull) };
	HashBytes(signature, context, sizeof(context));
	HashBytes(signature, &material.revision, sizeof(material.revision));
	HashBytes(signature, material.Pixels().data(), material.Pixels().size());
	return signature;
}
bool SameMaterial(const ResolvedMaterial& a, const ResolvedMaterial& b)
{
	return a.regionU == b.regionU && a.regionV == b.regionV && a.regionWidth == b.regionWidth &&
		a.regionHeight == b.regionHeight && a.revision == b.revision && a.filter == b.filter && a.uvPolicy == b.uvPolicy && a.cull == b.cull &&
		a.image.width == b.image.width && a.image.height == b.image.height && a.image.tpage == b.image.tpage &&
		a.image.clut == b.image.clut && ((a.sharedRgba && a.sharedRgba == b.sharedRgba) || a.Pixels() == b.Pixels());
}
}
PsyXNativeResult GpuAssetCache::AcquireMaterial(uint64_t signature, const ResolvedMaterial& source,
	uint64_t sceneGeneration, PsyXNativeMaterialHandle& handle)
{
	for (size_t i = 0; i < sharedMaterials.size(); ++i)
	{
		SharedMaterial& material = sharedMaterials[i];
		if (material.sceneGeneration == sceneGeneration && material.signature == signature && SameMaterial(material.source, source))
		{ ++material.references; handle = material.handle; return PSYX_NATIVE_OK; }
	}
	SharedMaterial material;
	material.signature = signature; material.sceneGeneration = sceneGeneration;
	material.source = source; material.references = 1;
	PsyXNativeMaterialDesc descriptor = {};
	descriptor.size = sizeof(descriptor); descriptor.version = PSYX_NATIVE_VERSION;
	descriptor.rgba = source.Pixels().data(); descriptor.byteCount = source.Pixels().size();
	descriptor.width = source.image.width; descriptor.height = source.image.height;
	descriptor.filter = source.filter; descriptor.alphaCutoff = .5f;
	descriptor.cull = source.cull;
	PsyXNativeResult result = PsyX_Native_CreateMaterial(&descriptor, &material.handle);
	if (result != PSYX_NATIVE_PENDING && result != PSYX_NATIVE_OK) return result;
	if (sceneGeneration != PsyX_Native_GetSceneGeneration())
	{
		// Reset can precede the actual create; this handle may be live in the
		// new scene. Always validate and destroy it before rejecting the acquire.
		const PsyXNativeResult retired = PsyX_Native_DestroyMaterial(material.handle);
		if (retired != PSYX_NATIVE_OK && retired != PSYX_NATIVE_STALE)
			PsyX_Log_Warning("Native GPU cache: interrupted material retirement failed (%d)\n", int(retired));
		return PSYX_NATIVE_STALE;
	}
	handle = material.handle;
	sharedMaterials.push_back(std::move(material)); // capacity reserved before create
	return result;
}
void GpuAssetCache::ReleaseMaterial(PsyXNativeMaterialHandle handle, Retirement mode)
{
	for (size_t i = 0; i < sharedMaterials.size(); ++i)
	{
		SharedMaterial& material = sharedMaterials[i];
		if (material.handle.slot != handle.slot || material.handle.generation != handle.generation) continue;
		if (--material.references) return;
		const bool sceneChanged = material.sceneGeneration != PsyX_Native_GetSceneGeneration();
		if (!sceneChanged || mode == Retirement::InterruptedCreation)
		{
			const PsyXNativeResult result = PsyX_Native_DestroyMaterial(handle);
			if (result != PSYX_NATIVE_OK && !(sceneChanged && result == PSYX_NATIVE_STALE))
				PsyX_Log_Warning("Native GPU cache: material retirement failed (%d)\n", int(result));
		}
		sharedMaterials.erase(sharedMaterials.begin()+i);
		return;
	}
	PsyX_Log_Warning("Native GPU cache: missing shared material reference\n");
}
void GpuAssetCache::Retire(const GpuGeometry& entry, Retirement mode)
{
	const bool sceneChanged = entry.sceneGeneration != PsyX_Native_GetSceneGeneration();
	// For an interrupted transaction, still validate each handle: a
	// reset interrupting creation can leave a new live handle in this entry.
	// Slot generations prevent an old handle from destroying a reused slot.
	for (size_t i = 0; i < entry.meshes.size(); ++i)
	{
		if (sceneChanged && mode == Retirement::CachedEntry) continue;
		const PsyXNativeResult result = PsyX_Native_DestroyMesh(entry.meshes[i]);
		if (result != PSYX_NATIVE_OK && !(sceneChanged && result == PSYX_NATIVE_STALE))
			PsyX_Log_Warning("Native GPU cache: mesh retirement failed (%d)\n", int(result));
	}
	for (size_t i = 0; i < entry.materials.size(); ++i)
		ReleaseMaterial(entry.materials[i], mode);
}
void GpuAssetCache::Clear()
{
	for (size_t i = 0; i < entries.size(); ++i) Retire(entries[i]);
	entries.clear();
}
void GpuAssetCache::Prune()
{
	for (size_t i = 0; i < entries.size();)
	{
		if (entries[i].sceneGeneration == PsyX_Native_GetSceneGeneration() &&
			AssetCatalog_HandleValid(entries[i].key.model) && AssetCatalog_HandleValid(entries[i].key.vertices)) { ++i; continue; }
		Retire(entries[i]);
		entries.erase(entries.begin()+i);
	}
}

PsyXNativeResult GpuAssetCache::Ensure(const CachedGeometry& geometry, const std::vector<ResolvedMaterial>& materials,
	const GpuGeometry** output)
{
	if (output) *output = NULL;
	if (geometry.failure.result != Result::Complete) return PSYX_NATIVE_UNSUPPORTED;
	if (!AssetCatalog_HandleValid(geometry.key.model) || !AssetCatalog_HandleValid(geometry.key.vertices)) return PSYX_NATIVE_STALE;
	if (materials.size() != geometry.mesh.parts.size() || materials.empty()) return PSYX_NATIVE_INVALID;
	if (materials.size() > PSYX_NATIVE_MAX_MESHES || materials.size() > PSYX_NATIVE_MAX_MATERIALS) return PSYX_NATIVE_OUT_OF_BUDGET;
	GpuGeometry next;
	next.key = geometry.key; next.sceneGeneration = PsyX_Native_GetSceneGeneration();
	try
	{
		for (size_t i = 0; i < materials.size(); ++i)
			if (!ValidateMaterial(geometry.mesh.parts[i], materials[i])) return PSYX_NATIVE_INVALID;
		Prune();
		size_t existing = entries.size();
		for (size_t i = 0; i < entries.size(); ++i)
		{
			if (!(entries[i].key == geometry.key)) continue;
			bool unchanged = entries[i].materials.size() == materials.size();
			for (size_t part = 0; unchanged && part < materials.size(); ++part)
			{
				const PsyXNativeMaterialHandle handle = entries[i].materials[part];
				unchanged = false;
				for (size_t shared = 0; shared < sharedMaterials.size(); ++shared)
				{
					const SharedMaterial& owned = sharedMaterials[shared];
					if (owned.handle.slot == handle.slot && owned.handle.generation == handle.generation)
					{ unchanged = SameMaterial(owned.source, materials[part]); break; }
				}
			}
			// Compare owned bytes directly. Rehashing tens of MiB of unchanged
			// PNGs every frame is unnecessary, and hashes alone cannot prove reuse.
			if (unchanged)
			{
				if (output) *output = &entries[i];
				return PSYX_NATIVE_OK;
			}
			existing = i; break;
		}
		next.materialSignatures.resize(materials.size());
		for (size_t i = 0; i < materials.size(); ++i)
			next.materialSignatures[i] = MaterialSignature(geometry.mesh.parts[i], materials[i]);
		// Reserve every container before creating handles: allocation failure
		// cannot strand a successful create that was not yet tracked for rollback.
		next.meshes.reserve(materials.size()); next.materials.reserve(materials.size());
		sharedMaterials.reserve(sharedMaterials.size()+materials.size());
		next.uploads.resize(materials.size());
		if (existing == entries.size()) entries.reserve(entries.size()+1);
		for (size_t i = 0; i < materials.size(); ++i)
		{
			const ResolvedMaterial& source = materials[i];
			next.uploads[i].rgba = UINT64_C(14695981039346656037);
			HashBytes(next.uploads[i].rgba, source.Pixels().data(), source.Pixels().size());
			PsyXNativeMaterialHandle handle = {};
			PsyXNativeResult result = AcquireMaterial(next.materialSignatures[i], source, next.sceneGeneration, handle);
			if (result != PSYX_NATIVE_PENDING && result != PSYX_NATIVE_OK) { Retire(next, Retirement::InterruptedCreation); return result; }
			next.materials.push_back(handle);
			std::vector<PsyXNativeVertex> vertices = geometry.mesh.parts[i].vertices;
			for (size_t vertex = 0; vertex < vertices.size(); ++vertex)
			{
				vertices[vertex].uv[0] = (vertices[vertex].uv[0]*256-source.regionU)/source.regionWidth;
				vertices[vertex].uv[1] = (vertices[vertex].uv[1]*256-source.regionV)/source.regionHeight;
			}
			const Part& part = geometry.mesh.parts[i];
			next.uploads[i].vertices = next.uploads[i].indices = UINT64_C(14695981039346656037);
			HashBytes(next.uploads[i].vertices, vertices.data(), vertices.size()*sizeof(PsyXNativeVertex));
			HashBytes(next.uploads[i].indices, part.indices.data(), part.indices.size()*sizeof(uint32_t));
			PsyXNativeMeshDesc mesh = {};
			mesh.size = sizeof(mesh); mesh.version = PSYX_NATIVE_VERSION; mesh.material = handle;
			mesh.vertices = vertices.data(); mesh.vertexCount = uint32_t(vertices.size());
			mesh.indices = part.indices.data(); mesh.indexCount = uint32_t(part.indices.size());
			mesh.depthLayer = part.depthLayer;
			memcpy(mesh.boundsMin, geometry.mesh.boundsMin, sizeof(mesh.boundsMin));
			memcpy(mesh.boundsMax, geometry.mesh.boundsMax, sizeof(mesh.boundsMax));
			PsyXNativeMeshHandle meshHandle = {};
			result = PsyX_Native_CreateMesh(&mesh, &meshHandle);
			if (result != PSYX_NATIVE_PENDING && result != PSYX_NATIVE_OK) { Retire(next, Retirement::InterruptedCreation); return result; }
			next.meshes.push_back(meshHandle);
		}
		if (next.sceneGeneration != PsyX_Native_GetSceneGeneration())
		{ Retire(next, Retirement::InterruptedCreation); return PSYX_NATIVE_STALE; }
		if (existing == entries.size()) entries.push_back(std::move(next));
		else { Retire(entries[existing]); entries[existing] = std::move(next); }
		if (output) *output = &entries[existing];
		return PSYX_NATIVE_PENDING;
	}
	catch (const std::bad_alloc&) { Retire(next, Retirement::InterruptedCreation); return PSYX_NATIVE_OUT_OF_BUDGET; }
}
}
