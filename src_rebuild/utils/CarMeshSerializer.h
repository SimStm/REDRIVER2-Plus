#ifndef CAR_MESH_SERIALIZER_H
#define CAR_MESH_SERIALIZER_H

#include "AssetConverter.h"

/*
 * E02-B car mesh serializer: adapter from game-side car geometry to the E02
 * conversion pipeline.
 *
 * The module is pure data: the caller supplies raw positions and 32-bit
 * triangle indices (cars.c builds them from the resident car model), and the
 * module derives a deterministic conversion-cache key and publishes a cached
 * native GLB through AssetConverter_ConvertMesh. It has no game types, no
 * PsyCross and no GPU dependency.
 *
 * Canonical key inputs:
 *  - sourceHash: one FNV-1a over the concatenation of the raw position bytes
 *    (3 floats per vertex), the raw index bytes (3 unsigned ints per triangle)
 *    and the sourceIdentity string bytes (without a terminator), in that
 *    order, so identical geometry and identity reuse one cache entry.
 *  - optionsHash: 0 (the converter currently has no options).
 *  - modsHash: one FNV-1a over every active mod string in caller order, each
 *    followed by a '\n' byte (for example "alpha\nbeta\n"); 0 when the list is
 *    NULL or empty. Callers pass validated mod ids (letters, digits, '-' and
 *    '_'), so the separator cannot occur inside an id.
 * The key uses converter "car-mesh" and CONVERSION_CACHE_SCHEMA_VERSION.
 */

/* Serializes `vertexCount` positions (3 floats each) and `triangleCount`
   triangles (`triangleIndices` carries 3 unsigned ints per triangle) into a
   native GLB cached under `cacheDirectory`; `activeMods` is the ordered mod id
   list the conversion is keyed by. On success fills `out` and returns 0
   (ASSET_CONVERTER_OK). On failure returns a named ASSET_CONVERTER_ERR_* code
   and zeroes `out`, so no partial path or hash is reported. `error` is
   optional: when it is NULL or empty the module uses a local diagnostic buffer
   internally, so a valid conversion can still succeed and publish; a NULL
   caller buffer is never written. */
int CarMeshSerializer_Export(const char* cacheDirectory, const char* sourceIdentity,
	const float* positions, int vertexCount, const unsigned int* triangleIndices, int triangleCount,
	const char* const* activeMods, int activeModCount,
	AssetConverterResult* out, char* error, int errorCapacity);

#endif // CAR_MESH_SERIALIZER_H
