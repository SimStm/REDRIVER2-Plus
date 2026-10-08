#include "NativeAssetCache.h"
#include <new>
#include <utility>

namespace NativeAsset
{
namespace
{
bool Same(AssetCatalogHandle a, AssetCatalogHandle b)
{
	return a.generation == b.generation && a.revision == b.revision && a.index == b.index;
}
bool Live(const GeometryKey& key)
{
	return key.decoderVersion == DecoderVersion && AssetCatalog_HandleValid(key.model) && AssetCatalog_HandleValid(key.vertices);
}
uint64_t Hash(Bytes bytes)
{
	uint64_t value = UINT64_C(14695981039346656037);
	for (size_t i = 0; i < bytes.size; ++i) { value ^= bytes.data[i]; value *= UINT64_C(1099511628211); }
	return value;
}
size_t PayloadBytes(const CachedGeometry& entry)
{
	size_t bytes = sizeof(CachedGeometry) + entry.stableId.capacity() + 1 + entry.failure.reason.capacity() + 1;
	bytes += entry.mesh.parts.capacity() * sizeof(Part);
	for (size_t i = 0; i < entry.mesh.parts.size(); ++i)
		bytes += entry.mesh.parts[i].vertices.capacity() * sizeof(PsyXNativeVertex) +
			entry.mesh.parts[i].indices.capacity() * sizeof(uint32_t);
	return bytes;
}
}

GeometryKey::GeometryKey() : model(), vertices(), decoderVersion(DecoderVersion), lodParent(-1), highDetail(-1), lowerDetail(-1) {}
bool GeometryKey::operator==(const GeometryKey& other) const
{
	return Same(model, other.model) && Same(vertices, other.vertices) && decoderVersion == other.decoderVersion &&
		lodParent == other.lodParent && highDetail == other.highDetail && lowerDetail == other.lowerDetail;
}
CachedGeometry::CachedGeometry() : sourceHash(0), vertexSourceHash(0), ownedBytes(0) {}
GeometryCache::GeometryCache(size_t count, size_t bytes) : maxEntries(count), maxBytes(bytes), ownedBytes(0) {}

const CachedGeometry* GeometryCache::Find(const GeometryKey& key) const
{
	if (!Live(key)) return NULL;
	for (size_t i = 0; i < entries.size(); ++i) if (entries[i].key == key) return &entries[i];
	return NULL;
}
Result GeometryCache::Capture(const GeometryKey& key, Bytes model, Bytes source, const CachedGeometry** output)
{
	if (output) *output = NULL;
	if (!Live(key)) return Result::Invalid;
	PruneStale();
	const CachedGeometry* cached = Find(key);
	if (cached) { if (output) *output = cached; return cached->failure.result; }
	if (entries.size() >= maxEntries) return Result::OutOfBudget;
	try
	{
		CachedGeometry entry;
		entry.key = key;
		int index = -1;
		char id[ASSET_CATALOG_ID_CAPACITY];
		if (!AssetCatalog_GetModel(key.model.index, &index, NULL, 0, NULL, NULL, NULL) ||
			!AssetCatalog_MakeModelId(index, id, sizeof(id))) return Result::Invalid;
		entry.stableId = id;
		const Result result = DecodeStatic(model, source, entry.mesh, entry.failure);
		// Failed resources are cached explicitly too, never as a partial mesh.
		// Hash only bounded loader spans after the decoder accepted their headers.
		if (model.data && source.data && model.size <= 8 * 1024 * 1024 && source.size <= 8 * 1024 * 1024)
		{
			entry.sourceHash = Hash(model); entry.vertexSourceHash = Hash(source);
		}
		entry.ownedBytes = PayloadBytes(entry);
		if (entry.ownedBytes > maxBytes - ownedBytes) return Result::OutOfBudget;
		entries.push_back(std::move(entry));
		ownedBytes += entries.back().ownedBytes;
		if (output) *output = &entries.back();
		return result;
	}
	catch (const std::bad_alloc&) { return Result::OutOfBudget; }
}
void GeometryCache::PruneStale()
{
	for (size_t i = 0; i < entries.size();)
	{
		if (Live(entries[i].key)) { ++i; continue; }
		ownedBytes -= entries[i].ownedBytes;
		entries.erase(entries.begin() + i);
	}
}
void GeometryCache::Clear() { std::vector<CachedGeometry>().swap(entries); ownedBytes = 0; }
}
