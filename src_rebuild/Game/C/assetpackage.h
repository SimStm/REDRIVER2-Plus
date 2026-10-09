#ifndef ASSET_PACKAGE_H
#define ASSET_PACKAGE_H

// E02 native asset package manifest (pure data module).
//
// A package is a readable JSON manifest listing converted native resources
// (models, textures, sounds, ...) with provenance: a stable id, a resource
// type, the package-relative file, a canonical content hash and the ids it
// depends on. Loading and canonical serialization never touch PsyCross, the
// GPU or game state, so a package can be validated and compared in isolation
// and two identical conversions produce byte-identical canonical output.

#define ASSET_PACKAGE_SCHEMA_VERSION	1

// A full package is a few hundred kilobytes, so callers should not place one on
// the stack; the test suite uses static instances. The entry cap is explicit and
// reported as ASSET_PACKAGE_ERR_TOO_MANY_ENTRIES rather than truncating.
#define ASSET_PACKAGE_ID_CAPACITY		96
#define ASSET_PACKAGE_TYPE_CAPACITY		24
#define ASSET_PACKAGE_PATH_CAPACITY		192
#define ASSET_PACKAGE_HASH_CAPACITY		20
#define ASSET_PACKAGE_ERROR_CAPACITY	192
#define ASSET_PACKAGE_MAX_ENTRIES		128
#define ASSET_PACKAGE_MAX_DEPENDENCIES	8

// Result codes. OK is 0; every other value is an explicit, named failure.
enum
{
	ASSET_PACKAGE_OK = 0,
	ASSET_PACKAGE_ERR_ARGUMENT,
	ASSET_PACKAGE_ERR_SYNTAX,
	ASSET_PACKAGE_ERR_SCHEMA_VERSION,
	ASSET_PACKAGE_ERR_MISSING_FIELD,
	ASSET_PACKAGE_ERR_DUPLICATE_ID,
	ASSET_PACKAGE_ERR_UNKNOWN_DEPENDENCY,
	ASSET_PACKAGE_ERR_TOO_MANY_ENTRIES,
	ASSET_PACKAGE_ERR_TOO_LONG,
};

typedef struct
{
	char id[ASSET_PACKAGE_ID_CAPACITY];
	char type[ASSET_PACKAGE_TYPE_CAPACITY];
	char file[ASSET_PACKAGE_PATH_CAPACITY];
	char contentHash[ASSET_PACKAGE_HASH_CAPACITY];
	char dependencies[ASSET_PACKAGE_MAX_DEPENDENCIES][ASSET_PACKAGE_ID_CAPACITY];
	int dependencyCount;
} AssetPackageEntry;

typedef struct
{
	int schemaVersion;
	char id[ASSET_PACKAGE_ID_CAPACITY];
	AssetPackageEntry entries[ASSET_PACKAGE_MAX_ENTRIES];
	int entryCount;
} AssetPackage;

#ifdef __cplusplus
extern "C" {
#endif

// Clears the package to an empty, invalid state.
void AssetPackage_Reset(AssetPackage* package);

// Parses and validates a manifest. Returns ASSET_PACKAGE_OK, or a named error
// and a human-readable diagnostic in `error`. A failed load leaves `package`
// reset. `length` may be -1 for a NUL-terminated string.
int AssetPackage_LoadFromText(AssetPackage* package, const char* text, int length,
	char* error, int errorCapacity);

// Writes a deterministic canonical manifest (entries in load order, fixed
// formatting). Returns the byte count, or -1 when the buffer is too small.
int AssetPackage_SerializeCanonical(const AssetPackage* package, char* out, int capacity);

// Looks up an entry by id. Returns the index, or -1.
int AssetPackage_FindEntry(const AssetPackage* package, const char* id);

// Sets the package id (schemaVersion is fixed to the supported version).
// Returns 1 on success, 0 when the id is empty or too long.
int AssetPackage_SetId(AssetPackage* package, const char* id);

// Appends an entry. Returns the entry index, or -1 when a field is empty/too
// long, the id is already present, the entry cap is reached, or a dependency
// exceeds its capacity. Dependencies are not resolved here; LoadFromText
// validates them, so a builder may add entries in any order.
int AssetPackage_AddEntry(AssetPackage* package, const char* id, const char* type,
	const char* file, const char* contentHash, const char* const* dependencyIds,
	int dependencyCount);

// Stable name for a result code (for logs/tests).
const char* AssetPackage_ErrorName(int code);

#ifdef __cplusplus
}
#endif

#endif
