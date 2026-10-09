#ifndef CONVERSION_CACHE_H
#define CONVERSION_CACHE_H

// E02 conversion cache contract (pure data module).
//
// A converted resource is cached by the exact inputs that determine it: the
// source content hash, the converter identity, the converter/schema version,
// the converter options and the active mod set. When any of these changes the
// cached entry is stale and must be rebuilt, so a stale schema or a changed mod
// never silently reuses an old conversion. The module computes canonical keys
// and deterministic cache paths and never touches PsyCross, the GPU or game
// state.

#define CONVERSION_CACHE_SCHEMA_VERSION		1

#define CONVERSION_CACHE_HASH_CAPACITY		20
#define CONVERSION_CACHE_CONVERTER_CAPACITY	48
#define CONVERSION_CACHE_KEY_CAPACITY		160
#define CONVERSION_CACHE_ERROR_CAPACITY		160
#define CONVERSION_CACHE_PATH_CAPACITY		320

// Why a cached entry cannot be reused. MATCH means it is current; INVALID is a
// caller error (a missing key).
enum
{
	CONVERSION_CACHE_MATCH = 0,
	CONVERSION_CACHE_STALE_SCHEMA,
	CONVERSION_CACHE_STALE_SOURCE,
	CONVERSION_CACHE_STALE_CONVERTER,
	CONVERSION_CACHE_STALE_OPTIONS,
	CONVERSION_CACHE_STALE_MODS,
	CONVERSION_CACHE_INVALID,
};

typedef struct
{
	char converter[CONVERSION_CACHE_CONVERTER_CAPACITY];
	unsigned long long sourceHash;
	unsigned long long optionsHash;
	unsigned long long modsHash;
	int schemaVersion;
} ConversionCacheKey;

#ifdef __cplusplus
extern "C" {
#endif

// FNV-1a 64 over arbitrary bytes, so callers hash source/options/mod lists the
// same way.
unsigned long long ConversionCache_HashBytes(const void* data, int length);

// Fills a key. A NULL converter or empty string is invalid.
void ConversionCache_MakeKey(ConversionCacheKey* key, const char* converter,
	unsigned long long sourceHash, unsigned long long optionsHash,
	unsigned long long modsHash, int schemaVersion);

// Canonical text form: "converter|source|options|mods|schema" in lowercase hex.
// Returns the byte count, or -1 when the buffer is too small.
int ConversionCache_FormatKey(const ConversionCacheKey* key, char* out, int capacity);

// Parses a key written by FormatKey. Returns 1 on success, or 0 with a
// diagnostic in `error` for a malformed key (a corrupt cache index entry).
int ConversionCache_ParseKey(ConversionCacheKey* key, const char* text,
	char* error, int errorCapacity);

// Compares a cached key against the current inputs and reports the first reason
// they differ (MATCH when the entry is reusable).
int ConversionCache_Staleness(const ConversionCacheKey* cached, const ConversionCacheKey* current);

// Stable name for a staleness reason (for logs/tests).
const char* ConversionCache_StalenessName(int reason);

// Deterministic cache file path: "<cacheDirectory>/<converter>/<key>.bin".
// Returns 1 on success, or 0 when a component is empty or too long.
int ConversionCache_MakeCachePath(const char* cacheDirectory, const ConversionCacheKey* key,
	char* out, int capacity);

#ifdef __cplusplus
}
#endif

#endif
