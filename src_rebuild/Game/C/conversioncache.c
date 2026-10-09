#include "conversioncache.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// E02 conversion cache keying. Pure data: no PsyCross, GPU or game state.

static void CopyField(char* destination, int capacity, const char* source)
{
	int i = 0;
	if (capacity <= 0)
		return;
	if (!source)
		source = "";
	for (; source[i] && i < capacity - 1; ++i)
		destination[i] = source[i];
	destination[i] = '\0';
}

unsigned long long ConversionCache_HashBytes(const void* data, int length)
{
	unsigned long long hash = 1469598103934665603ULL;
	const unsigned char* bytes = (const unsigned char*)data;
	if (bytes && length > 0)
	{
		for (int i = 0; i < length; ++i)
		{
			hash ^= bytes[i];
			hash *= 1099511628211ULL;
		}
	}
	return hash;
}

void ConversionCache_MakeKey(ConversionCacheKey* key, const char* converter,
	unsigned long long sourceHash, unsigned long long optionsHash,
	unsigned long long modsHash, int schemaVersion)
{
	if (!key)
		return;
	memset(key, 0, sizeof(*key));
	CopyField(key->converter, sizeof(key->converter), converter);
	key->sourceHash = sourceHash;
	key->optionsHash = optionsHash;
	key->modsHash = modsHash;
	key->schemaVersion = schemaVersion;
}

int ConversionCache_FormatKey(const ConversionCacheKey* key, char* out, int capacity)
{
	if (!key || !out || capacity <= 0)
		return -1;

	const int written = snprintf(out, capacity, "%s|%016llx|%016llx|%016llx|%d",
		key->converter, key->sourceHash, key->optionsHash, key->modsHash, key->schemaVersion);
	if (written <= 0 || written >= capacity)
		return -1;
	return written;
}

static int ParseHex64(const char* text, const char* end, unsigned long long* out)
{
	if (end - text != 16)
		return 0;
	unsigned long long value = 0;
	for (const char* p = text; p < end; ++p)
	{
		const char c = *p;
		int digit;
		if (c >= '0' && c <= '9') digit = c - '0';
		else if (c >= 'a' && c <= 'f') digit = c - 'a' + 10;
		else if (c >= 'A' && c <= 'F') digit = c - 'A' + 10;
		else return 0;
		value = (value << 4) | (unsigned long long)digit;
	}
	*out = value;
	return 1;
}

int ConversionCache_ParseKey(ConversionCacheKey* key, const char* text, char* error, int errorCapacity)
{
	if (error && errorCapacity > 0)
		error[0] = '\0';
	if (!key || !text)
	{
		if (error && errorCapacity > 0)
			snprintf(error, errorCapacity, "no cache key");
		return 0;
	}

	// Fields: converter | source | options | mods | schema
	const char* field[4] = { NULL, NULL, NULL, NULL };
	const char* fieldEnd[4] = { NULL, NULL, NULL, NULL };
	const char* cursor = text;

	for (int i = 0; i < 4; ++i)
	{
		field[i] = cursor;
		const char* bar = strchr(cursor, '|');
		if (!bar)
		{
			if (error && errorCapacity > 0)
				snprintf(error, errorCapacity, "cache key is missing a field");
			return 0;
		}
		fieldEnd[i] = bar;
		cursor = bar + 1;
	}

	memset(key, 0, sizeof(*key));
	const int converterLength = (int)(fieldEnd[0] - field[0]);
	if (converterLength <= 0 || converterLength >= (int)sizeof(key->converter))
	{
		if (error && errorCapacity > 0)
			snprintf(error, errorCapacity, "cache key has an invalid converter");
		return 0;
	}
	memcpy(key->converter, field[0], (size_t)converterLength);
	key->converter[converterLength] = '\0';

	if (!ParseHex64(field[1], fieldEnd[1], &key->sourceHash) ||
		!ParseHex64(field[2], fieldEnd[2], &key->optionsHash) ||
		!ParseHex64(field[3], fieldEnd[3], &key->modsHash))
	{
		if (error && errorCapacity > 0)
			snprintf(error, errorCapacity, "cache key has an invalid hash");
		return 0;
	}

	char* schemaEnd = NULL;
	const long schema = strtol(cursor, &schemaEnd, 10);
	if (schemaEnd == cursor || *schemaEnd != '\0' || schema <= 0)
	{
		if (error && errorCapacity > 0)
			snprintf(error, errorCapacity, "cache key has an invalid schema version");
		return 0;
	}
	key->schemaVersion = (int)schema;
	return 1;
}

int ConversionCache_Staleness(const ConversionCacheKey* cached, const ConversionCacheKey* current)
{
	if (!cached || !current)
		return CONVERSION_CACHE_INVALID;

	if (strcmp(cached->converter, current->converter) != 0)
		return CONVERSION_CACHE_STALE_CONVERTER;
	if (cached->schemaVersion != current->schemaVersion)
		return CONVERSION_CACHE_STALE_SCHEMA;
	if (cached->sourceHash != current->sourceHash)
		return CONVERSION_CACHE_STALE_SOURCE;
	if (cached->optionsHash != current->optionsHash)
		return CONVERSION_CACHE_STALE_OPTIONS;
	if (cached->modsHash != current->modsHash)
		return CONVERSION_CACHE_STALE_MODS;
	return CONVERSION_CACHE_MATCH;
}

const char* ConversionCache_StalenessName(int reason)
{
	switch (reason)
	{
	case CONVERSION_CACHE_MATCH: return "match";
	case CONVERSION_CACHE_STALE_SCHEMA: return "stale-schema";
	case CONVERSION_CACHE_STALE_SOURCE: return "stale-source";
	case CONVERSION_CACHE_STALE_CONVERTER: return "stale-converter";
	case CONVERSION_CACHE_STALE_OPTIONS: return "stale-options";
	case CONVERSION_CACHE_STALE_MODS: return "stale-mods";
	case CONVERSION_CACHE_INVALID: return "invalid";
	default: return "unknown";
	}
}

int ConversionCache_MakeCachePath(const char* cacheDirectory, const ConversionCacheKey* key,
	char* out, int capacity)
{
	if (!cacheDirectory || !key || !out || capacity <= 0)
		return 0;

	char formatted[CONVERSION_CACHE_KEY_CAPACITY];
	if (ConversionCache_FormatKey(key, formatted, sizeof(formatted)) < 0)
		return 0;

	// The key's '|' separators are not filename-safe; use '_' in the file name.
	for (char* p = formatted; *p; ++p)
	{
		if (*p == '|')
			*p = '_';
	}

	const size_t directoryLength = strlen(cacheDirectory);
	const bool needsSlash = directoryLength > 0 && cacheDirectory[directoryLength - 1] != '/' &&
		cacheDirectory[directoryLength - 1] != '\\';
	const int written = snprintf(out, capacity, "%s%s%s.bin",
		cacheDirectory, needsSlash ? "/" : "", formatted);
	if (written <= 0 || written >= capacity)
		return 0;
	return 1;
}
