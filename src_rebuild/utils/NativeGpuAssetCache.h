#ifndef NATIVE_GPU_ASSET_CACHE_H
#define NATIVE_GPU_ASSET_CACHE_H
#include "NativeAssetCache.h"
#include "NativeTextureDecode.h"
#include <memory>

namespace NativeAsset
{
// One resolved original texture region. Artwork can have another resolution
// (PNG override); UVs still refer to this original byte-coordinate rectangle.
struct ResolvedMaterial
{
	enum class UvPolicy { InsideRegion, ClampOverride };
	Texture image;
	// Immutable owned PNG bytes can be shared by the adapter and GPU cache.
	// Original VRAM decodes and test callers retain the ordinary owned vector.
	std::shared_ptr<const std::vector<uint8_t> > sharedRgba;
	const std::vector<uint8_t>& Pixels() const { return sharedRgba ? *sharedRgba : image.rgba; }
	unsigned int regionU, regionV, regionWidth, regionHeight;
	uint64_t revision;
	PsyXNativeFilter filter;
	PsyXNativeCull cull;
	UvPolicy uvPolicy;
	ResolvedMaterial() : regionU(0), regionV(0), regionWidth(0), regionHeight(0), revision(0), filter(PSYX_NATIVE_NEAREST), cull(PSYX_NATIVE_CULL_BACK), uvPolicy(UvPolicy::InsideRegion) {}
};
struct GpuGeometry
{
	// Hash copied upload bytes once, so capture inspection can compare source
	// positions/attributes, indices and artwork independently of draw counters.
	struct UploadEvidence { uint64_t vertices, indices, rgba; };
	GeometryKey key;
	uint64_t sceneGeneration;
	std::vector<uint64_t> materialSignatures;
	std::vector<PsyXNativeMeshHandle> meshes;
	std::vector<PsyXNativeMaterialHandle> materials;
	std::vector<UploadEvidence> uploads;
};

// Render-thread owner. No source pointers or legacy texture IDs retained.
// Prune invalidates both model/vertex-source catalog identities. A whole model
// is published only after every part/material has an owned resource handle.
class GpuAssetCache
{
public:
	PsyXNativeResult Ensure(const CachedGeometry& geometry, const std::vector<ResolvedMaterial>& materials,
		const GpuGeometry** output);
	void Prune();
	void Clear();
	size_t Count() const { return entries.size(); }
private:
	struct SharedMaterial
	{
		uint64_t signature, sceneGeneration;
		ResolvedMaterial source; // owned exact content; hash alone cannot prove equality
		PsyXNativeMaterialHandle handle;
		size_t references;
	};
	enum class Retirement { CachedEntry, InterruptedCreation };
	std::vector<GpuGeometry> entries;
	std::vector<SharedMaterial> sharedMaterials;
	PsyXNativeResult AcquireMaterial(uint64_t signature, const ResolvedMaterial& source,
		uint64_t sceneGeneration, PsyXNativeMaterialHandle& handle);
	void ReleaseMaterial(PsyXNativeMaterialHandle handle, Retirement mode);
	void Retire(const GpuGeometry& entry, Retirement mode = Retirement::CachedEntry);
};
}
#endif
