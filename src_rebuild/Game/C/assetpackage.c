#include "assetpackage.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// E02 native asset package manifest reader/writer. Pure data: no PsyCross, no
// GPU, no game state. The JSON subset it accepts is deliberately small
// (objects, arrays, strings, integers) so the format stays readable and the
// parser stays auditable.

typedef struct
{
	const char* cursor;
	const char* end;
	char* error;
	int errorCapacity;
	int failed;
	AssetPackage* package;
} PackageReader;

static void Fail(PackageReader* reader, int code, const char* message)
{
	if (!reader->failed)
	{
		reader->failed = code;
		if (reader->error && reader->errorCapacity > 0)
			snprintf(reader->error, reader->errorCapacity, "%s", message);

		// A failed load must leave the package reset, not half-parsed.
		if (reader->package)
			AssetPackage_Reset(reader->package);
	}
}

static void CopyField(char* destination, int capacity, const char* source)
{
	int i = 0;
	if (capacity <= 0)
		return;
	for (; source[i] && i < capacity - 1; ++i)
		destination[i] = source[i];
	destination[i] = '\0';
}

static void SkipWhitespace(PackageReader* reader)
{
	while (reader->cursor < reader->end)
	{
		const char c = *reader->cursor;
		if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
			reader->cursor++;
		else
			break;
	}
}

static int ConsumeChar(PackageReader* reader, char expected)
{
	SkipWhitespace(reader);
	if (reader->cursor < reader->end && *reader->cursor == expected)
	{
		reader->cursor++;
		return 1;
	}
	return 0;
}

// Reads a quoted string into `out`. Returns 0 on a syntax error or overflow.
static int ParseString(PackageReader* reader, char* out, int capacity)
{
	SkipWhitespace(reader);
	if (reader->cursor >= reader->end || *reader->cursor != '"')
	{
		Fail(reader, ASSET_PACKAGE_ERR_SYNTAX, "expected a quoted string");
		return 0;
	}
	reader->cursor++;

	int written = 0;
	while (reader->cursor < reader->end && *reader->cursor != '"')
	{
		char c = *reader->cursor++;
		if (c == '\\' && reader->cursor < reader->end)
		{
			const char escaped = *reader->cursor++;
			switch (escaped)
			{
			case 'n': c = '\n'; break;
			case 't': c = '\t'; break;
			case 'r': c = '\r'; break;
			case '"': c = '"'; break;
			case '\\': c = '\\'; break;
			case '/': c = '/'; break;
			default: c = escaped; break;
			}
		}
		if (written >= capacity - 1)
		{
			Fail(reader, ASSET_PACKAGE_ERR_TOO_LONG, "a string exceeds its field capacity");
			return 0;
		}
		out[written++] = c;
	}
	if (reader->cursor >= reader->end)
	{
		Fail(reader, ASSET_PACKAGE_ERR_SYNTAX, "unterminated string");
		return 0;
	}
	reader->cursor++;
	out[written] = '\0';
	return 1;
}

static int ParseInteger(PackageReader* reader, int* out)
{
	SkipWhitespace(reader);
	const char* start = reader->cursor;
	int sign = 1;
	if (reader->cursor < reader->end && (*reader->cursor == '-' || *reader->cursor == '+'))
	{
		if (*reader->cursor == '-')
			sign = -1;
		reader->cursor++;
	}
	if (reader->cursor >= reader->end || *reader->cursor < '0' || *reader->cursor > '9')
	{
		Fail(reader, ASSET_PACKAGE_ERR_SYNTAX, "expected an integer");
		return 0;
	}
	long value = 0;
	while (reader->cursor < reader->end && *reader->cursor >= '0' && *reader->cursor <= '9')
		value = value * 10 + (*reader->cursor++ - '0');
	(void)start;
	*out = (int)(sign * value);
	return 1;
}

// Skips any JSON value (used for keys this schema does not own).
static int SkipValue(PackageReader* reader)
{
	SkipWhitespace(reader);
	if (reader->cursor >= reader->end)
	{
		Fail(reader, ASSET_PACKAGE_ERR_SYNTAX, "unexpected end of manifest");
		return 0;
	}

	const char c = *reader->cursor;
	if (c == '"')
	{
		char discard[2];
		return ParseString(reader, discard, sizeof(discard)) ? 1 : 0;
	}
	if (c == '{' || c == '[')
	{
		const char open = c;
		const char close = c == '{' ? '}' : ']';
		int depth = 0;
		while (reader->cursor < reader->end)
		{
			const char current = *reader->cursor++;
			if (current == '"')
			{
				// Skip a nested string without interpreting escapes.
				while (reader->cursor < reader->end && *reader->cursor != '"')
				{
					if (*reader->cursor == '\\')
						reader->cursor++;
					reader->cursor++;
				}
				if (reader->cursor < reader->end)
					reader->cursor++;
				continue;
			}
			if (current == open)
				depth++;
			else if (current == close && --depth == 0)
				return 1;
		}
		Fail(reader, ASSET_PACKAGE_ERR_SYNTAX, "unbalanced object or array");
		return 0;
	}
	while (reader->cursor < reader->end && *reader->cursor != ',' &&
		*reader->cursor != '}' && *reader->cursor != ']')
		reader->cursor++;
	return 1;
}

static int ParseStringArray(PackageReader* reader, char out[][ASSET_PACKAGE_ID_CAPACITY], int capacity, int* count)
{
	*count = 0;
	if (!ConsumeChar(reader, '['))
	{
		Fail(reader, ASSET_PACKAGE_ERR_SYNTAX, "expected a dependency array");
		return 0;
	}
	if (ConsumeChar(reader, ']'))
		return 1;

	while (1)
	{
		if (*count >= capacity)
		{
			Fail(reader, ASSET_PACKAGE_ERR_TOO_MANY_ENTRIES, "too many dependencies");
			return 0;
		}
		if (!ParseString(reader, out[*count], ASSET_PACKAGE_ID_CAPACITY))
			return 0;
		(*count)++;
		if (ConsumeChar(reader, ']'))
			return 1;
		if (!ConsumeChar(reader, ','))
		{
			Fail(reader, ASSET_PACKAGE_ERR_SYNTAX, "expected ',' or ']' in a dependency array");
			return 0;
		}
	}
}

static int ParseEntry(PackageReader* reader, AssetPackageEntry* entry)
{
	if (!ConsumeChar(reader, '{'))
	{
		Fail(reader, ASSET_PACKAGE_ERR_SYNTAX, "expected an entry object");
		return 0;
	}
	if (ConsumeChar(reader, '}'))
	{
		Fail(reader, ASSET_PACKAGE_ERR_MISSING_FIELD, "an entry is empty");
		return 0;
	}

	while (1)
	{
		char key[32];
		if (!ParseString(reader, key, sizeof(key)) || !ConsumeChar(reader, ':'))
		{
			Fail(reader, ASSET_PACKAGE_ERR_SYNTAX, "expected a key and ':' in an entry");
			return 0;
		}

		if (strcmp(key, "id") == 0)
		{
			if (!ParseString(reader, entry->id, ASSET_PACKAGE_ID_CAPACITY)) return 0;
		}
		else if (strcmp(key, "type") == 0)
		{
			if (!ParseString(reader, entry->type, ASSET_PACKAGE_TYPE_CAPACITY)) return 0;
		}
		else if (strcmp(key, "file") == 0)
		{
			if (!ParseString(reader, entry->file, ASSET_PACKAGE_PATH_CAPACITY)) return 0;
		}
		else if (strcmp(key, "contentHash") == 0)
		{
			if (!ParseString(reader, entry->contentHash, ASSET_PACKAGE_HASH_CAPACITY)) return 0;
		}
		else if (strcmp(key, "dependencies") == 0)
		{
			if (!ParseStringArray(reader, entry->dependencies, ASSET_PACKAGE_MAX_DEPENDENCIES, &entry->dependencyCount)) return 0;
		}
		else if (!SkipValue(reader))
		{
			return 0;
		}

		if (ConsumeChar(reader, '}'))
			return 1;
		if (!ConsumeChar(reader, ','))
		{
			Fail(reader, ASSET_PACKAGE_ERR_SYNTAX, "expected ',' or '}' in an entry");
			return 0;
		}
	}
}

static int ParseEntries(PackageReader* reader, AssetPackage* package)
{
	if (!ConsumeChar(reader, '['))
	{
		Fail(reader, ASSET_PACKAGE_ERR_SYNTAX, "expected an entries array");
		return 0;
	}
	if (ConsumeChar(reader, ']'))
		return 1;

	while (1)
	{
		if (package->entryCount >= ASSET_PACKAGE_MAX_ENTRIES)
		{
			Fail(reader, ASSET_PACKAGE_ERR_TOO_MANY_ENTRIES, "too many entries");
			return 0;
		}
		if (!ParseEntry(reader, &package->entries[package->entryCount]))
			return 0;
		package->entryCount++;

		if (ConsumeChar(reader, ']'))
			return 1;
		if (!ConsumeChar(reader, ','))
		{
			Fail(reader, ASSET_PACKAGE_ERR_SYNTAX, "expected ',' or ']' in the entries array");
			return 0;
		}
	}
}

void AssetPackage_Reset(AssetPackage* package)
{
	if (package)
		memset(package, 0, sizeof(*package));
}

int AssetPackage_FindEntry(const AssetPackage* package, const char* id)
{
	if (!package || !id)
		return -1;
	for (int i = 0; i < package->entryCount; ++i)
	{
		if (strcmp(package->entries[i].id, id) == 0)
			return i;
	}
	return -1;
}

int AssetPackage_SetId(AssetPackage* package, const char* id)
{
	if (!package || !id || id[0] == '\0' || strlen(id) >= sizeof(package->id))
		return 0;
	CopyField(package->id, sizeof(package->id), id);
	package->schemaVersion = ASSET_PACKAGE_SCHEMA_VERSION;
	return 1;
}

int AssetPackage_AddEntry(AssetPackage* package, const char* id, const char* type,
	const char* file, const char* contentHash, const char* const* dependencyIds,
	int dependencyCount)
{
	if (!package || !id || !type || !file)
		return -1;
	if (id[0] == '\0' || type[0] == '\0' || file[0] == '\0')
		return -1;
	if (strlen(id) >= sizeof(package->entries[0].id) ||
		strlen(type) >= sizeof(package->entries[0].type) ||
		strlen(file) >= sizeof(package->entries[0].file) ||
		(contentHash && strlen(contentHash) >= sizeof(package->entries[0].contentHash)))
		return -1;
	if (dependencyCount < 0 || dependencyCount > ASSET_PACKAGE_MAX_DEPENDENCIES)
		return -1;
	if (package->entryCount >= ASSET_PACKAGE_MAX_ENTRIES)
		return -1;
	if (AssetPackage_FindEntry(package, id) >= 0)
		return -1;

	AssetPackageEntry* entry = &package->entries[package->entryCount];
	memset(entry, 0, sizeof(*entry));
	CopyField(entry->id, sizeof(entry->id), id);
	CopyField(entry->type, sizeof(entry->type), type);
	CopyField(entry->file, sizeof(entry->file), file);
	CopyField(entry->contentHash, sizeof(entry->contentHash), contentHash ? contentHash : "");
	for (int i = 0; i < dependencyCount; ++i)
	{
		if (!dependencyIds[i] || dependencyIds[i][0] == '\0' ||
			strlen(dependencyIds[i]) >= sizeof(entry->dependencies[0]))
		{
			memset(entry, 0, sizeof(*entry));
			return -1;
		}
		CopyField(entry->dependencies[i], sizeof(entry->dependencies[i]), dependencyIds[i]);
	}
	entry->dependencyCount = dependencyCount;
	package->entryCount++;
	return package->entryCount - 1;
}

const char* AssetPackage_ErrorName(int code)
{
	switch (code)
	{
	case ASSET_PACKAGE_OK: return "OK";
	case ASSET_PACKAGE_ERR_ARGUMENT: return "argument";
	case ASSET_PACKAGE_ERR_SYNTAX: return "syntax";
	case ASSET_PACKAGE_ERR_SCHEMA_VERSION: return "schema-version";
	case ASSET_PACKAGE_ERR_MISSING_FIELD: return "missing-field";
	case ASSET_PACKAGE_ERR_DUPLICATE_ID: return "duplicate-id";
	case ASSET_PACKAGE_ERR_UNKNOWN_DEPENDENCY: return "unknown-dependency";
	case ASSET_PACKAGE_ERR_TOO_MANY_ENTRIES: return "too-many-entries";
	case ASSET_PACKAGE_ERR_TOO_LONG: return "too-long";
	default: return "unknown";
	}
}

int AssetPackage_LoadFromText(AssetPackage* package, const char* text, int length, char* error, int errorCapacity)
{
	if (error && errorCapacity > 0)
		error[0] = '\0';
	if (!package || !text)
	{
		if (error && errorCapacity > 0)
			snprintf(error, errorCapacity, "no manifest text");
		return ASSET_PACKAGE_ERR_ARGUMENT;
	}

	AssetPackage_Reset(package);

	if (length < 0)
		length = (int)strlen(text);

	PackageReader reader = { text, text + length, error, errorCapacity, 0, package };
	if (!ConsumeChar(&reader, '{'))
	{
		Fail(&reader, ASSET_PACKAGE_ERR_SYNTAX, "a package manifest must be a JSON object");
		return reader.failed;
	}
	if (ConsumeChar(&reader, '}'))
	{
		Fail(&reader, ASSET_PACKAGE_ERR_MISSING_FIELD, "the manifest has no schemaVersion");
		return reader.failed;
	}

	int haveVersion = 0;
	while (1)
	{
		char key[32];
		if (!ParseString(&reader, key, sizeof(key)) || !ConsumeChar(&reader, ':'))
		{
			Fail(&reader, ASSET_PACKAGE_ERR_SYNTAX, "expected a key and ':' in the manifest");
			return reader.failed;
		}

		if (strcmp(key, "schemaVersion") == 0)
		{
			if (!ParseInteger(&reader, &package->schemaVersion)) return reader.failed;
			haveVersion = 1;
		}
		else if (strcmp(key, "id") == 0)
		{
			if (!ParseString(&reader, package->id, ASSET_PACKAGE_ID_CAPACITY)) return reader.failed;
		}
		else if (strcmp(key, "entries") == 0)
		{
			if (!ParseEntries(&reader, package)) return reader.failed;
		}
		else if (!SkipValue(&reader))
		{
			return reader.failed;
		}

		if (ConsumeChar(&reader, '}'))
			break;
		if (!ConsumeChar(&reader, ','))
		{
			Fail(&reader, ASSET_PACKAGE_ERR_SYNTAX, "expected ',' or '}' in the manifest");
			return reader.failed;
		}
	}

	SkipWhitespace(&reader);
	if (reader.cursor != reader.end)
	{
		Fail(&reader, ASSET_PACKAGE_ERR_SYNTAX, "trailing data after the manifest object");
		return reader.failed;
	}

	// Schema validation.
	if (!haveVersion)
	{
		Fail(&reader, ASSET_PACKAGE_ERR_MISSING_FIELD, "the manifest has no schemaVersion");
		return reader.failed;
	}
	if (package->schemaVersion != ASSET_PACKAGE_SCHEMA_VERSION)
	{
		Fail(&reader, ASSET_PACKAGE_ERR_SCHEMA_VERSION, "unsupported package schema version; rebuild the package");
		return reader.failed;
	}
	if (package->id[0] == '\0')
	{
		Fail(&reader, ASSET_PACKAGE_ERR_MISSING_FIELD, "the manifest has no package id");
		return reader.failed;
	}

	for (int i = 0; i < package->entryCount; ++i)
	{
		const AssetPackageEntry* entry = &package->entries[i];
		if (entry->id[0] == '\0' || entry->type[0] == '\0' || entry->file[0] == '\0')
		{
			Fail(&reader, ASSET_PACKAGE_ERR_MISSING_FIELD, "an entry is missing id, type or file");
			return reader.failed;
		}
		if (AssetPackage_FindEntry(package, entry->id) != i)
		{
			Fail(&reader, ASSET_PACKAGE_ERR_DUPLICATE_ID, "an entry id is duplicated");
			return reader.failed;
		}
		for (int d = 0; d < entry->dependencyCount; ++d)
		{
			if (AssetPackage_FindEntry(package, entry->dependencies[d]) < 0)
			{
				Fail(&reader, ASSET_PACKAGE_ERR_UNKNOWN_DEPENDENCY, "an entry references an unknown dependency id");
				return reader.failed;
			}
		}
	}

	package->schemaVersion = ASSET_PACKAGE_SCHEMA_VERSION;
	return ASSET_PACKAGE_OK;
}

int AssetPackage_SerializeCanonical(const AssetPackage* package, char* out, int capacity)
{
	if (!package || !out || capacity <= 0)
		return -1;

	int written = snprintf(out, capacity,
		"{\n  \"schemaVersion\": %d,\n  \"id\": \"%s\",\n  \"entries\": [",
		package->schemaVersion, package->id);
	if (written <= 0 || written >= capacity)
		return -1;

	for (int i = 0; i < package->entryCount; ++i)
	{
		const AssetPackageEntry* entry = &package->entries[i];
		const int line = snprintf(out + written, capacity - written,
			"%s\n    { \"id\": \"%s\", \"type\": \"%s\", \"file\": \"%s\", \"contentHash\": \"%s\", \"dependencies\": [",
			i == 0 ? "" : ",", entry->id, entry->type, entry->file, entry->contentHash);
		if (line <= 0 || line >= capacity - written)
			return -1;
		written += line;

		for (int d = 0; d < entry->dependencyCount; ++d)
		{
			const int item = snprintf(out + written, capacity - written, "%s\"%s\"",
				d == 0 ? "" : ", ", entry->dependencies[d]);
			if (item <= 0 || item >= capacity - written)
				return -1;
			written += item;
		}

		const int close = snprintf(out + written, capacity - written, "] }");
		if (close <= 0 || close >= capacity - written)
			return -1;
		written += close;
	}

	const int tail = snprintf(out + written, capacity - written, "\n  ]\n}\n");
	if (tail <= 0 || tail >= capacity - written)
		return -1;
	written += tail;

	return written;
}
