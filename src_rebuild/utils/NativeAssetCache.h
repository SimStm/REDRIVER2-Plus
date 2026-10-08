#ifndef NATIVE_ASSET_CACHE_H
#define NATIVE_ASSET_CACHE_H

#include "NativeAssetDecode.h"
#include "../Game/C/assetcatalog.h"

namespace NativeAsset
{
// Geometry identity is independent of texture content. A PNG/CLUT reload must
// rebuild materials without rereading a MODEL already preprocessed by the GTE.
struct GeometryKey
{
	AssetCatalogHandle model, vertices;
	unsigned int decoderVersion;
	int lodParent, highDetail, lowerDetail;
	GeometryKey();
	bool operator==(const GeometryKey& other) const;
};

struct CachedGeometry
{
	GeometryKey key;
	std::string stableId;
	uint64_t sourceHash, vertexSourceHash; // Read-only pre-GTE source evidence.
	Mesh mesh;
	Failure failure;
	size_t ownedBytes;
	CachedGeometry();
};

// Render-thread owner. No source pointers are retained. Returned entries are
// borrowed from this owner only until its next mutation; GPU uploads copy them.
// Find revalidates BOTH catalog handles, even if a streamed slot reused memory.
class GeometryCache
{
public:
	GeometryCache(size_t maxEntries = 128, size_t maxBytes = 32 * 1024 * 1024);
	const CachedGeometry* Find(const GeometryKey& key) const;
	Result Capture(const GeometryKey& key, Bytes model, Bytes vertexSource, const CachedGeometry** entry);
	void PruneStale();
	void Clear();
	size_t Count() const { return entries.size(); }
	size_t OwnedBytes() const { return ownedBytes; }
private:
	std::vector<CachedGeometry> entries;
	size_t maxEntries, maxBytes, ownedBytes;
};
}
#endif
