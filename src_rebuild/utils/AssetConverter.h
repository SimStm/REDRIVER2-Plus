#ifndef ASSET_CONVERTER_H
#define ASSET_CONVERTER_H

#include "GltfLoader.h"
#include "../Game/C/conversioncache.h"

/*
 * E02 asset converter: turns a source mesh record into a cached native GLB.
 *
 * The converter is the transactional publish step of the conversion pipeline.
 * It keys the output by the source hash, converter, schema version, options and
 * active mods (ConversionCacheKey); a fresh cached GLB is reused without
 * rewriting, and a stale one is rebuilt. The new GLB is written to a temporary
 * file, verified by reading it back, then renamed into place, so a partial
 * conversion is never published as ready.
 *
 * It returns the GLB's content hash (the same canonical identity the package
 * manifest records) and whether the cached artifact was reused.
 */

/* Result codes. */
enum
{
	ASSET_CONVERTER_OK = 0,
	ASSET_CONVERTER_ERR_ARGUMENT,
	ASSET_CONVERTER_ERR_EMPTY_MESH,
	ASSET_CONVERTER_ERR_WRITE,
	ASSET_CONVERTER_ERR_VERIFY,
	ASSET_CONVERTER_ERR_PUBLISH,
};

typedef struct
{
	int converted;				// 1 when a new GLB was written, 0 when reused
	int reused;
	char glbPath[CONVERSION_CACHE_PATH_CAPACITY];
	char contentHash[CONVERSION_CACHE_HASH_CAPACITY];
} AssetConverterResult;

/* Converts `mesh` to a native GLB under `cacheDirectory` for `key`. On success
   fills `out` and returns ASSET_CONVERTER_OK; otherwise returns a named error
   and writes a diagnostic into `error`. */
int AssetConverter_ConvertMesh(const char* cacheDirectory, const ConversionCacheKey* key,
	const GltfMeshData& mesh, AssetConverterResult* out, char* error, int errorCapacity);

/* Stable name for a result code. */
const char* AssetConverter_ErrorName(int code);

#endif
