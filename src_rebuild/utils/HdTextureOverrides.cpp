#include "HdTextureOverrides.h"
#include "InspectorExport.h"

#include "PsyX/PsyX_public.h"
#include "PsyX/PsyX_render.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>

#ifdef _WIN32
#include <windows.h>
#include <wincodec.h>
#pragma comment(lib, "windowscodecs.lib")
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

namespace
{
const int kMaximumMods = 32;
const int kMaximumManifestEntries = 128;
const int kMaximumKnownTextures = 4096;
const int kMaximumActiveOverrides = 128;
const int kMaximumManifestBytes = 64 * 1024;

struct ModInfo
{
	char id[48];
	char name[96];
	char description[160];
	char directory[192];
	int enabled;
	int textureEntries;
};

struct ManifestEntry
{
	char textureName[48];
	char imagePath[192];
	int modIndex;
	int texturePage;
	int textureIndex;
	unsigned int textureId;
	int imageWidth;
	int imageHeight;
	int attempted;
};

struct ActiveOverride
{
	int texturePage;
	int textureIndex;
	int overrideId;
	int manifestIndex;
};

struct KnownTexture
{
	char textureName[48];
	int texturePage;
	int textureIndex;
	unsigned short tpage;
	unsigned short clut;
	unsigned short u;
	unsigned short v;
	unsigned short width;
	unsigned short height;
};

struct JsonReader
{
	const char* cursor;
	const char* end;
};

ModInfo g_mods[kMaximumMods];
ManifestEntry g_manifestEntries[kMaximumManifestEntries];
ActiveOverride g_activeOverrides[kMaximumActiveOverrides];
KnownTexture g_knownTextures[kMaximumKnownTextures];
int g_modCount = 0;
int g_manifestEntryCount = 0;
int g_activeOverrideCount = 0;
int g_knownTextureCount = 0;
int g_manifestLoaded = 0;
int g_manifestFound = 0;
int g_enabled = 1;
char g_status[160] = "Mod manifests have not been checked";
char g_modsDirectory[192] = "mods";

void SetStatus(const char* text)
{
	strncpy(g_status, text, sizeof(g_status) - 1);
	g_status[sizeof(g_status) - 1] = '\0';
}

void CopyText(char* destination, size_t capacity, const char* source)
{
	if (capacity == 0)
		return;
	strncpy(destination, source ? source : "", capacity - 1);
	destination[capacity - 1] = '\0';
}

char* Trim(char* text)
{
	while (*text == ' ' || *text == '\t' || *text == '\r' || *text == '\n')
		++text;
	char* end = text + strlen(text);
	while (end > text && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n'))
		--end;
	*end = '\0';
	return text;
}

bool IsSafeRelativeAssetPath(const char* path)
{
	return path && path[0] != '\0' && path[0] != '/' && path[0] != '\\' &&
		strchr(path, ':') == NULL && strstr(path, "..") == NULL;
}

bool IsSafeModId(const char* id)
{
	if (!id || id[0] == '\0')
		return false;
	for (const char* character = id; *character; ++character)
	{
		const bool alpha = (*character >= 'a' && *character <= 'z') || (*character >= 'A' && *character <= 'Z');
		const bool numeric = *character >= '0' && *character <= '9';
		if (!alpha && !numeric && *character != '-' && *character != '_')
			return false;
	}
	return true;
}

bool JoinPath(char* destination, size_t capacity, const char* first, const char* second, const char* third)
{
	const int written = snprintf(destination, capacity, "%s/%s%s%s", first, second,
		third ? "/" : "", third ? third : "");
	return written >= 0 && (size_t)written < capacity;
}

bool IsDirectory(const char* path)
{
#ifdef _WIN32
	const DWORD attributes = GetFileAttributesA(path);
	return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
#else
	struct stat status;
	return stat(path, &status) == 0 && S_ISDIR(status.st_mode);
#endif
}

void DetermineModsDirectory()
{
	if (IsDirectory("mods"))
		CopyText(g_modsDirectory, sizeof(g_modsDirectory), "mods");
	else if (IsDirectory("../mods"))
		CopyText(g_modsDirectory, sizeof(g_modsDirectory), "../mods");
}

char* ReadTextFile(const char* path, int* byteCount)
{
	*byteCount = 0;
	FILE* file = fopen(path, "rb");
	if (!file)
		return NULL;
	if (fseek(file, 0, SEEK_END) != 0)
	{
		fclose(file);
		return NULL;
	}
	const long length = ftell(file);
	if (length < 0 || length > kMaximumManifestBytes || fseek(file, 0, SEEK_SET) != 0)
	{
		fclose(file);
		return NULL;
	}
	char* text = (char*)malloc((size_t)length + 1);
	if (!text)
	{
		fclose(file);
		return NULL;
	}
	const size_t read = fread(text, 1, (size_t)length, file);
	const bool readOk = read == (size_t)length && ferror(file) == 0;
	fclose(file);
	if (!readOk)
	{
		free(text);
		return NULL;
	}
	text[length] = '\0';
	*byteCount = (int)length;
	return text;
}

void SkipWhitespace(JsonReader* reader)
{
	while (reader->cursor < reader->end && (*reader->cursor == ' ' || *reader->cursor == '\t' ||
		*reader->cursor == '\r' || *reader->cursor == '\n'))
		++reader->cursor;
}

bool Consume(JsonReader* reader, char expected)
{
	SkipWhitespace(reader);
	if (reader->cursor >= reader->end || *reader->cursor != expected)
		return false;
	++reader->cursor;
	return true;
}

bool ParseString(JsonReader* reader, char* destination, size_t capacity)
{
	SkipWhitespace(reader);
	if (reader->cursor >= reader->end || *reader->cursor++ != '"' || capacity == 0)
		return false;
	size_t length = 0;
	while (reader->cursor < reader->end)
	{
		char character = *reader->cursor++;
		if (character == '"')
		{
			destination[length] = '\0';
			return true;
		}
		if (character == '\\')
		{
			if (reader->cursor >= reader->end) return false;
			character = *reader->cursor++;
			if (character == 'u')
			{
				if (reader->end - reader->cursor < 4) return false;
				reader->cursor += 4;
				character = '?';
			}
			else if (character == 'n') character = '\n';
			else if (character == 'r') character = '\r';
			else if (character == 't') character = '\t';
		}
		if ((unsigned char)character < 0x20 || length + 1 >= capacity) return false;
		destination[length++] = character;
	}
	return false;
}

bool SkipString(JsonReader* reader)
{
	SkipWhitespace(reader);
	if (reader->cursor >= reader->end || *reader->cursor++ != '"') return false;
	while (reader->cursor < reader->end)
	{
		char character = *reader->cursor++;
		if (character == '"') return true;
		if (character == '\\')
		{
			if (reader->cursor >= reader->end) return false;
			if (*reader->cursor++ == 'u')
			{
				if (reader->end - reader->cursor < 4) return false;
				reader->cursor += 4;
			}
		}
	}
	return false;
}

bool ParseInteger(JsonReader* reader, int* value)
{
	SkipWhitespace(reader);
	char* end = NULL;
	const long result = strtol(reader->cursor, &end, 10);
	if (end == reader->cursor || end > reader->end || result < -32768 || result > 32767) return false;
	reader->cursor = end;
	*value = (int)result;
	return true;
}

bool ParseBoolean(JsonReader* reader, int* value)
{
	SkipWhitespace(reader);
	if (reader->end - reader->cursor >= 4 && strncmp(reader->cursor, "true", 4) == 0)
	{
		reader->cursor += 4;
		*value = 1;
		return true;
	}
	if (reader->end - reader->cursor >= 5 && strncmp(reader->cursor, "false", 5) == 0)
	{
		reader->cursor += 5;
		*value = 0;
		return true;
	}
	return false;
}

bool SkipValue(JsonReader* reader, int depth);

bool SkipArray(JsonReader* reader, int depth)
{
	if (!Consume(reader, '[')) return false;
	if (Consume(reader, ']')) return true;
	while (true)
	{
		if (!SkipValue(reader, depth + 1)) return false;
		if (Consume(reader, ']')) return true;
		if (!Consume(reader, ',')) return false;
	}
}

bool SkipObject(JsonReader* reader, int depth)
{
	if (!Consume(reader, '{')) return false;
	if (Consume(reader, '}')) return true;
	char key[64];
	while (true)
	{
		if (!ParseString(reader, key, sizeof(key)) || !Consume(reader, ':') || !SkipValue(reader, depth + 1)) return false;
		if (Consume(reader, '}')) return true;
		if (!Consume(reader, ',')) return false;
	}
}

bool SkipValue(JsonReader* reader, int depth)
{
	if (depth > 16) return false;
	SkipWhitespace(reader);
	if (reader->cursor >= reader->end) return false;
	if (*reader->cursor == '{') return SkipObject(reader, depth);
	if (*reader->cursor == '[') return SkipArray(reader, depth);
	if (*reader->cursor == '"') return SkipString(reader);
	int ignoredInteger;
	if (ParseInteger(reader, &ignoredInteger)) return true;
	int ignoredBoolean;
	return ParseBoolean(reader, &ignoredBoolean);
}

bool ParseTextureObject(JsonReader* reader, int modIndex)
{
	if (!Consume(reader, '{')) return false;
	char textureName[48] = {};
	char imagePath[192] = {};
	int texturePage = -1;
	int textureIndex = -1;
	while (!Consume(reader, '}'))
	{
		char key[64];
		if (!ParseString(reader, key, sizeof(key)) || !Consume(reader, ':')) return false;
		if (strcmp(key, "texture") == 0 || strcmp(key, "name") == 0)
		{
			if (!ParseString(reader, textureName, sizeof(textureName))) return false;
		}
		else if (strcmp(key, "file") == 0)
		{
			if (!ParseString(reader, imagePath, sizeof(imagePath))) return false;
		}
		else if (strcmp(key, "texturePage") == 0)
		{
			if (!ParseInteger(reader, &texturePage)) return false;
		}
		else if (strcmp(key, "textureIndex") == 0)
		{
			if (!ParseInteger(reader, &textureIndex)) return false;
		}
		else if (!SkipValue(reader, 0)) return false;
		if (Consume(reader, '}')) break;
		if (!Consume(reader, ',')) return false;
	}
	if (textureName[0] == '\0' || !IsSafeRelativeAssetPath(imagePath) || g_manifestEntryCount >= kMaximumManifestEntries) return true;
	ManifestEntry* entry = &g_manifestEntries[g_manifestEntryCount++];
	CopyText(entry->textureName, sizeof(entry->textureName), textureName);
	CopyText(entry->imagePath, sizeof(entry->imagePath), imagePath);
	entry->modIndex = modIndex;
	entry->texturePage = texturePage;
	entry->textureIndex = textureIndex;
	++g_mods[modIndex].textureEntries;
	return true;
}

bool ParseTextureArray(JsonReader* reader, int modIndex)
{
	if (!Consume(reader, '[')) return false;
	if (Consume(reader, ']')) return true;
	while (true)
	{
		if (!ParseTextureObject(reader, modIndex)) return false;
		if (Consume(reader, ']')) return true;
		if (!Consume(reader, ',')) return false;
	}
}

bool ParseModManifest(const char* path, const char* directory)
{
	int byteCount = 0;
	char* text = ReadTextFile(path, &byteCount);
	if (!text || g_modCount >= kMaximumMods)
	{
		free(text);
		return false;
	}
	const int entryStart = g_manifestEntryCount;
	JsonReader reader = { text, text + byteCount };
	ModInfo candidate = {};
	CopyText(candidate.directory, sizeof(candidate.directory), directory);
	g_mods[g_modCount] = candidate;
	if (!Consume(&reader, '{')) { free(text); return false; }
	bool valid = true;
	while (valid && !Consume(&reader, '}'))
	{
		char key[64];
		if (!ParseString(&reader, key, sizeof(key)) || !Consume(&reader, ':')) { valid = false; break; }
		if (strcmp(key, "id") == 0) valid = ParseString(&reader, candidate.id, sizeof(candidate.id));
		else if (strcmp(key, "name") == 0) valid = ParseString(&reader, candidate.name, sizeof(candidate.name));
		else if (strcmp(key, "description") == 0) valid = ParseString(&reader, candidate.description, sizeof(candidate.description));
		else if (strcmp(key, "textures") == 0) valid = ParseTextureArray(&reader, g_modCount);
		else valid = SkipValue(&reader, 0);
		if (!valid) break;
		if (Consume(&reader, '}')) break;
		if (!Consume(&reader, ',')) { valid = false; break; }
	}
	SkipWhitespace(&reader);
	valid = valid && reader.cursor == reader.end && IsSafeModId(candidate.id);
	for (int i = 0; valid && i < g_modCount; ++i)
		valid = strcmp(g_mods[i].id, candidate.id) != 0;
	if (!valid)
	{
		g_manifestEntryCount = entryStart;
		free(text);
		return false;
	}
	if (candidate.name[0] == '\0') CopyText(candidate.name, sizeof(candidate.name), candidate.id);
	candidate.textureEntries = g_mods[g_modCount].textureEntries;
	g_mods[g_modCount++] = candidate;
	g_manifestFound = 1;
	free(text);
	return true;
}

void SwapMods(int first, int second)
{
	ModInfo temporary = g_mods[first];
	g_mods[first] = g_mods[second];
	g_mods[second] = temporary;
	for (int i = 0; i < g_manifestEntryCount; ++i)
	{
		if (g_manifestEntries[i].modIndex == first) g_manifestEntries[i].modIndex = second;
		else if (g_manifestEntries[i].modIndex == second) g_manifestEntries[i].modIndex = first;
	}
}

void SortModsById()
{
	for (int i = 0; i < g_modCount; ++i)
		for (int j = i + 1; j < g_modCount; ++j)
			if (strcmp(g_mods[i].id, g_mods[j].id) > 0) SwapMods(i, j);
}

void DiscoverModManifests()
{
#ifdef _WIN32
	WIN32_FIND_DATAA findData;
	char searchPath[256];
	snprintf(searchPath, sizeof(searchPath), "%s/*", g_modsDirectory);
	HANDLE find = FindFirstFileA(searchPath, &findData);
	if (find == INVALID_HANDLE_VALUE) return;
	do
	{
		if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0 || findData.cFileName[0] == '.') continue;
		char directory[192];
		char manifestPath[256];
		if (JoinPath(directory, sizeof(directory), g_modsDirectory, findData.cFileName, NULL) &&
			JoinPath(manifestPath, sizeof(manifestPath), directory, "manifest.json", NULL))
			ParseModManifest(manifestPath, directory);
	} while (FindNextFileA(find, &findData));
	FindClose(find);
#else
	DIR* directory = opendir(g_modsDirectory);
	if (!directory) return;
	struct dirent* entry;
	while ((entry = readdir(directory)) != NULL)
	{
		if (entry->d_name[0] == '.') continue;
		char modDirectory[192];
		char manifestPath[256];
		struct stat status;
		if (!JoinPath(modDirectory, sizeof(modDirectory), g_modsDirectory, entry->d_name, NULL) ||
			stat(modDirectory, &status) != 0 || !S_ISDIR(status.st_mode) ||
			!JoinPath(manifestPath, sizeof(manifestPath), modDirectory, "manifest.json", NULL)) continue;
		ParseModManifest(manifestPath, modDirectory);
	}
	closedir(directory);
#endif
	SortModsById();
}

int FindModById(const char* id)
{
	for (int i = 0; i < g_modCount; ++i)
		if (strcmp(g_mods[i].id, id) == 0) return i;
	return -1;
}

bool ParseEnabledModObject(JsonReader* reader, char* id, size_t capacity, int* enabled)
{
	if (!Consume(reader, '{')) return false;
	*enabled = 1;
	while (!Consume(reader, '}'))
	{
		char key[64];
		if (!ParseString(reader, key, sizeof(key)) || !Consume(reader, ':')) return false;
		if (strcmp(key, "id") == 0)
		{
			if (!ParseString(reader, id, capacity)) return false;
		}
		else if (strcmp(key, "enabled") == 0)
		{
			if (!ParseBoolean(reader, enabled)) return false;
		}
		else if (!SkipValue(reader, 0)) return false;
		if (Consume(reader, '}')) break;
		if (!Consume(reader, ',')) return false;
	}
	return id[0] != '\0';
}

void ApplyEnabledMods()
{
	for (int i = 0; i < g_modCount; ++i) g_mods[i].enabled = 1;
	int byteCount = 0;
	char enabledModsPath[256];
	if (!JoinPath(enabledModsPath, sizeof(enabledModsPath), g_modsDirectory, "enabled.json", NULL)) return;
	char* text = ReadTextFile(enabledModsPath, &byteCount);
	if (!text) return;
	for (int i = 0; i < g_modCount; ++i) g_mods[i].enabled = 0;
	JsonReader reader = { text, text + byteCount };
	bool valid = Consume(&reader, '{');
	int order = 0;
	while (valid && !Consume(&reader, '}'))
	{
		char key[64];
		valid = ParseString(&reader, key, sizeof(key)) && Consume(&reader, ':');
		if (!valid) break;
		if (strcmp(key, "mods") == 0)
		{
			valid = Consume(&reader, '[');
			while (valid && !Consume(&reader, ']'))
			{
				char id[48] = {};
				int enabled = 1;
				valid = ParseEnabledModObject(&reader, id, sizeof(id), &enabled);
				const int index = valid ? FindModById(id) : -1;
				if (index >= 0)
				{
					g_mods[index].enabled = enabled;
					if (enabled && index != order) SwapMods(index, order);
					if (enabled) ++order;
				}
				if (Consume(&reader, ']')) break;
				valid = valid && Consume(&reader, ',');
			}
		}
		else valid = SkipValue(&reader, 0);
		if (Consume(&reader, '}')) break;
		valid = valid && Consume(&reader, ',');
	}
	SkipWhitespace(&reader);
	if (!valid || reader.cursor != reader.end)
	{
		for (int i = 0; i < g_modCount; ++i) g_mods[i].enabled = 0;
		SetStatus("mods/enabled.json is invalid; no JSON mods are enabled");
	}
	free(text);
}

void LoadLegacyManifest()
{
	char legacyDirectory[256];
	char legacyManifestPath[320];
	if (!JoinPath(legacyDirectory, sizeof(legacyDirectory), g_modsDirectory, "hd_textures", NULL) ||
		!JoinPath(legacyManifestPath, sizeof(legacyManifestPath), legacyDirectory, "manifest.ini", NULL)) return;
	FILE* file = fopen(legacyManifestPath, "rb");
	if (!file || g_modCount >= kMaximumMods)
	{
		if (file) fclose(file);
		return;
	}
	const int modIndex = g_modCount++;
	CopyText(g_mods[modIndex].id, sizeof(g_mods[modIndex].id), "legacy-hd-textures");
	CopyText(g_mods[modIndex].name, sizeof(g_mods[modIndex].name), "Legacy HD textures");
	CopyText(g_mods[modIndex].description, sizeof(g_mods[modIndex].description), "Compatibility manifest at mods/hd_textures/manifest.ini");
	CopyText(g_mods[modIndex].directory, sizeof(g_mods[modIndex].directory), legacyDirectory);
	g_mods[modIndex].enabled = 1;
	g_manifestFound = 1;
	char line[256];
	int inTextureSection = 0;
	while (fgets(line, sizeof(line), file))
	{
		char* value = Trim(line);
		if (value[0] == '\0' || value[0] == '#' || value[0] == ';') continue;
		if (value[0] == '[') { inTextureSection = strcmp(value, "[texture_overrides]") == 0; continue; }
		if (!inTextureSection) continue;
		char* equals = strchr(value, '=');
		if (!equals) continue;
		*equals = '\0';
		char* textureName = Trim(value);
		char* imagePath = Trim(equals + 1);
		if (textureName[0] == '\0' || !IsSafeRelativeAssetPath(imagePath) || g_manifestEntryCount >= kMaximumManifestEntries) continue;
		ManifestEntry* entry = &g_manifestEntries[g_manifestEntryCount++];
		CopyText(entry->textureName, sizeof(entry->textureName), textureName);
		CopyText(entry->imagePath, sizeof(entry->imagePath), imagePath);
		entry->modIndex = modIndex;
		entry->texturePage = -1;
		entry->textureIndex = -1;
		++g_mods[modIndex].textureEntries;
	}
	fclose(file);
}

void LoadManifests()
{
	if (g_manifestLoaded) return;
	g_manifestLoaded = 1;
	DetermineModsDirectory();
	DiscoverModManifests();
	if (g_modCount > 0) ApplyEnabledMods();
	else LoadLegacyManifest();
	if (!g_manifestFound) SetStatus("No mod manifests found; original TIM/VRAM textures are active");
	else if (g_manifestEntryCount == 0) SetStatus("Mod manifests contain no valid texture entries");
	else if (strncmp(g_status, "mods/enabled.json is invalid", 28) != 0) SetStatus("Mod manifests loaded; original TIM/VRAM textures remain available");
}

ManifestEntry* FindManifestEntry(const char* textureName, int texturePage, int textureIndex)
{
	ManifestEntry* bestEntry = NULL;
	for (int i = g_manifestEntryCount - 1; i >= 0; --i)
	{
		ManifestEntry* entry = &g_manifestEntries[i];
		if (!g_mods[entry->modIndex].enabled || strcmp(entry->textureName, textureName) != 0) continue;
		if (entry->texturePage >= 0 && entry->texturePage != texturePage) continue;
		if (entry->textureIndex >= 0 && entry->textureIndex != textureIndex) continue;
		if (!bestEntry || entry->modIndex >= bestEntry->modIndex)
			bestEntry = entry;
	}
	return bestEntry;
}

#ifdef _WIN32
bool MakeWidePath(const char* path, wchar_t* widePath, int widePathCapacity)
{
	if (!path || !widePath || widePathCapacity <= 0) return false;
	const int length = MultiByteToWideChar(CP_UTF8, 0, path, -1, widePath, widePathCapacity);
	return length > 0 && length <= widePathCapacity;
}

bool LoadPngRgba(const char* path, unsigned char** pixels, int* width, int* height)
{
	*pixels = NULL; *width = 0; *height = 0;
	wchar_t widePath[MAX_PATH];
	if (!MakeWidePath(path, widePath, MAX_PATH)) return false;
	const HRESULT initialiseResult = CoInitializeEx(NULL, COINIT_MULTITHREADED);
	const bool shouldUninitialise = SUCCEEDED(initialiseResult);
	IWICImagingFactory* factory = NULL;
	IWICBitmapDecoder* decoder = NULL;
	IWICBitmapFrameDecode* frame = NULL;
	IWICFormatConverter* converter = NULL;
	HRESULT result = CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
	if (SUCCEEDED(result)) result = factory->CreateDecoderFromFilename(widePath, NULL, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder);
	if (SUCCEEDED(result)) result = decoder->GetFrame(0, &frame);
	if (SUCCEEDED(result)) result = factory->CreateFormatConverter(&converter);
	if (SUCCEEDED(result)) result = converter->Initialize(frame, GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, NULL, 0.0, WICBitmapPaletteTypeCustom);
	UINT imageWidth = 0, imageHeight = 0;
	if (SUCCEEDED(result)) result = converter->GetSize(&imageWidth, &imageHeight);
	if (SUCCEEDED(result) && (imageWidth == 0 || imageHeight == 0 || imageWidth > 16384 || imageHeight > 16384)) result = E_FAIL;
	const size_t byteCount = (size_t)imageWidth * (size_t)imageHeight * 4;
	unsigned char* data = NULL;
	if (SUCCEEDED(result)) { data = (unsigned char*)malloc(byteCount); if (!data) result = E_OUTOFMEMORY; }
	if (SUCCEEDED(result)) result = converter->CopyPixels(NULL, imageWidth * 4, (UINT)byteCount, data);
	if (SUCCEEDED(result)) { *pixels = data; *width = (int)imageWidth; *height = (int)imageHeight; }
	else free(data);
	if (converter) converter->Release();
	if (frame) frame->Release();
	if (decoder) decoder->Release();
	if (factory) factory->Release();
	if (shouldUninitialise) CoUninitialize();
	return SUCCEEDED(result);
}

bool SavePngRgba(const char* path, const unsigned char* pixels, int width, int height)
{
	if (!path || !pixels || width <= 0 || height <= 0)
		return false;
	wchar_t widePath[MAX_PATH];
	if (!MakeWidePath(path, widePath, MAX_PATH))
		return false;
	const HRESULT initialiseResult = CoInitializeEx(NULL, COINIT_MULTITHREADED);
	const bool shouldUninitialise = SUCCEEDED(initialiseResult);
	IWICImagingFactory* factory = NULL;
	IWICStream* stream = NULL;
	IWICBitmapEncoder* encoder = NULL;
	IWICBitmapFrameEncode* frame = NULL;
	IPropertyBag2* properties = NULL;
	HRESULT result = CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
	if (SUCCEEDED(result)) result = factory->CreateStream(&stream);
	if (SUCCEEDED(result)) result = stream->InitializeFromFilename(widePath, GENERIC_WRITE);
	if (SUCCEEDED(result)) result = factory->CreateEncoder(GUID_ContainerFormatPng, NULL, &encoder);
	if (SUCCEEDED(result)) result = encoder->Initialize(stream, WICBitmapEncoderNoCache);
	if (SUCCEEDED(result)) result = encoder->CreateNewFrame(&frame, &properties);
	if (SUCCEEDED(result)) result = frame->Initialize(properties);
	if (SUCCEEDED(result)) result = frame->SetSize((UINT)width, (UINT)height);
	WICPixelFormatGUID pixelFormat = GUID_WICPixelFormat32bppBGRA;
	if (SUCCEEDED(result)) result = frame->SetPixelFormat(&pixelFormat);
	if (SUCCEEDED(result) && !IsEqualGUID(pixelFormat, GUID_WICPixelFormat32bppBGRA)) result = WINCODEC_ERR_UNSUPPORTEDPIXELFORMAT;
	const size_t byteCount = (size_t)width * (size_t)height * 4;
	BYTE* bgra = SUCCEEDED(result) ? (BYTE*)malloc(byteCount) : NULL;
	if (SUCCEEDED(result) && !bgra) result = E_OUTOFMEMORY;
	if (SUCCEEDED(result))
	{
		for (size_t i = 0; i < byteCount; i += 4)
		{
			bgra[i] = pixels[i + 2]; bgra[i + 1] = pixels[i + 1];
			bgra[i + 2] = pixels[i]; bgra[i + 3] = pixels[i + 3];
		}
		result = frame->WritePixels((UINT)height, (UINT)(width * 4), (UINT)byteCount, bgra);
	}
	free(bgra);
	if (SUCCEEDED(result)) result = frame->Commit();
	if (SUCCEEDED(result)) result = encoder->Commit();
	if (properties) properties->Release();
	if (frame) frame->Release();
	if (encoder) encoder->Release();
	if (stream) stream->Release();
	if (factory) factory->Release();
	if (shouldUninitialise) CoUninitialize();
	if (FAILED(result)) snprintf(g_status, sizeof(g_status), "PNG encoder failed (HRESULT 0x%08lX).", (unsigned long)result);
	return SUCCEEDED(result);
}
#else
bool LoadPngRgba(const char* path, unsigned char** pixels, int* width, int* height)
{
	(void)path; *pixels = NULL; *width = 0; *height = 0; return false;
}

bool SavePngRgba(const char* path, const unsigned char* pixels, int width, int height)
{
	(void)path; (void)pixels; (void)width; (void)height; return false;
}
#endif

bool EnsureImageLoaded(ManifestEntry* entry)
{
	if (entry->textureId != 0) return true;
	if (entry->attempted) return false;
	entry->attempted = 1;
	char imagePath[384];
	if (!JoinPath(imagePath, sizeof(imagePath), g_mods[entry->modIndex].directory, entry->imagePath, NULL))
	{
		SetStatus("A mod texture path is too long");
		return false;
	}
	unsigned char* pixels = NULL;
	int width = 0, height = 0;
	if (!LoadPngRgba(imagePath, &pixels, &width, &height))
	{
#ifdef _WIN32
		SetStatus("Could not decode a PNG listed by a mod manifest");
#else
		SetStatus("PNG texture loading is currently available on Windows only");
#endif
		return false;
	}
	entry->textureId = PsyX_CreateRGBATexture(width, height, pixels);
	free(pixels);
	if (entry->textureId == 0) { SetStatus("PsyCross could not create a mod RGBA texture"); return false; }
	entry->imageWidth = width;
	entry->imageHeight = height;
	return true;
}

void RemoveActiveOverrideAt(int index)
{
	PsyX_RemoveTextureOverride(g_activeOverrides[index].overrideId);
	memmove(&g_activeOverrides[index], &g_activeOverrides[index + 1], sizeof(g_activeOverrides[0]) * (g_activeOverrideCount - index - 1));
	--g_activeOverrideCount;
}

int ComparableTPage(unsigned short tpage)
{
	return tpage & 0x19F;
}

void RegisterKnownTexture(int texturePage, int textureIndex, const char* textureName,
	unsigned short tpage, unsigned short clut, unsigned short u, unsigned short v,
	unsigned short width, unsigned short height)
{
	for (int i = 0; i < g_knownTextureCount; ++i)
	{
		if (g_knownTextures[i].texturePage == texturePage && g_knownTextures[i].textureIndex == textureIndex)
		{
			CopyText(g_knownTextures[i].textureName, sizeof(g_knownTextures[i].textureName), textureName);
			g_knownTextures[i].tpage = tpage;
			g_knownTextures[i].clut = clut;
			g_knownTextures[i].u = u;
			g_knownTextures[i].v = v;
			g_knownTextures[i].width = width;
			g_knownTextures[i].height = height;
			return;
		}
	}
	if (g_knownTextureCount >= kMaximumKnownTextures)
		return;
	KnownTexture* known = &g_knownTextures[g_knownTextureCount++];
	CopyText(known->textureName, sizeof(known->textureName), textureName);
	known->texturePage = texturePage;
	known->textureIndex = textureIndex;
	known->tpage = tpage;
	known->clut = clut;
	known->u = u;
	known->v = v;
	known->width = width;
	known->height = height;
}

bool FindTexturesArraySpan(const char* text, int byteCount, const char** spanBegin, const char** spanEnd)
{
	if (!text || byteCount <= 0 || !spanBegin || !spanEnd) return false;
	JsonReader reader = { text, text + byteCount };
	if (!Consume(&reader, '{')) return false;
	while (true)
	{
		char key[64];
		if (!ParseString(&reader, key, sizeof(key)) || !Consume(&reader, ':')) return false;
		if (strcmp(key, "textures") == 0)
		{
			SkipWhitespace(&reader);
			if (reader.cursor >= reader.end || *reader.cursor != '[') return false;
			JsonReader probe = reader;
			if (!SkipArray(&probe, 0)) return false;
			*spanBegin = reader.cursor + 1;
			*spanEnd = probe.cursor - 1;
			return true;
		}
		if (!SkipValue(&reader, 0)) return false;
		if (Consume(&reader, '}')) return false;
		if (!Consume(&reader, ',')) return false;
	}
}

bool ManifestTexturesContain(const char* spanBegin, const char* spanEnd,
	const char* textureName, int texturePage, int textureIndex)
{
	JsonReader reader = { spanBegin, spanEnd };
	SkipWhitespace(&reader);
	if (reader.cursor >= reader.end || *reader.cursor == ']') return false;
	while (true)
	{
		if (!Consume(&reader, '{')) return false;
		char candidateName[48] = {};
		int candidatePage = -1;
		int candidateIndex = -1;
		while (true)
		{
			char key[64];
			if (!ParseString(&reader, key, sizeof(key)) || !Consume(&reader, ':')) return false;
			if (strcmp(key, "texture") == 0 || strcmp(key, "name") == 0)
			{
				if (!ParseString(&reader, candidateName, sizeof(candidateName))) return false;
			}
			else if (strcmp(key, "texturePage") == 0)
			{
				if (!ParseInteger(&reader, &candidatePage)) return false;
			}
			else if (strcmp(key, "textureIndex") == 0)
			{
				if (!ParseInteger(&reader, &candidateIndex)) return false;
			}
			else if (!SkipValue(&reader, 0)) return false;
			if (Consume(&reader, '}')) break;
			if (!Consume(&reader, ',')) return false;
		}
		if (candidateName[0] != '\0' && strcmp(candidateName, textureName) == 0 &&
			candidatePage == texturePage && candidateIndex == textureIndex)
			return true;
		if (Consume(&reader, ']')) return false;
		if (!Consume(&reader, ',')) return false;
	}
}

std::string BuildManifestEntry(const std::string& escapedTextureName, int texturePage, int textureIndex, const char* safeName)
{
	char entry[640];
	snprintf(entry, sizeof(entry),
		"{ \"texture\": \"%s\", \"texturePage\": %d, \"textureIndex\": %d, \"file\": \"assets/inspector/%s_p%d_i%d.png\" }",
		escapedTextureName.c_str(), texturePage, textureIndex, safeName, texturePage, textureIndex);
	return std::string(entry);
}

std::string MergeManifestEntry(const char* text, int byteCount, const char* spanBegin, const char* spanEnd, const std::string& entry)
{
	std::string inner(spanBegin, spanEnd);
	size_t endTrim = inner.size();
	while (endTrim > 0 && (inner[endTrim - 1] == ' ' || inner[endTrim - 1] == '\t' ||
		inner[endTrim - 1] == '\r' || inner[endTrim - 1] == '\n'))
		--endTrim;
	const std::string trimmed = inner.substr(0, endTrim);
	const std::string trailing = inner.substr(endTrim);
	const size_t prefixLength = (size_t)(spanBegin - text);
	const size_t suffixOffset = (size_t)(spanEnd - text);
	std::string result(text, prefixLength);
	if (trimmed.empty())
		result += "\n    " + entry + "\n  ";
	else
		result += trimmed + ",\n    " + entry + trailing;
	result.append(text + suffixOffset, (size_t)byteCount - suffixOffset);
	return result;
}

#ifdef _WIN32
bool PublishManifestFile(const char* path, const char* originalText, int originalBytes,
	const std::string& content, char* status, int statusCapacity)
{
	char temporary[MAX_PATH];
	if (!InspectorExport_Temporary(path, temporary, status, statusCapacity)) return false;
	HANDLE file = CreateFileA(temporary, GENERIC_WRITE, 0, NULL, TRUNCATE_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	DWORD written = 0;
	const DWORD size = (DWORD)content.size();
	bool ok = file != INVALID_HANDLE_VALUE && size > 0 &&
		WriteFile(file, content.data(), size, &written, NULL) && written == size;
	if (file != INVALID_HANDLE_VALUE)
	{
		if (ok) ok = FlushFileBuffers(file) != 0;
		if (!CloseHandle(file)) ok = false;
	}
	if (!ok)
	{
		DeleteFileA(temporary);
		snprintf(status, statusCapacity, "Manifest write failed; the previous manifest was preserved.");
		return false;
	}
	int currentBytes = 0;
	char* current = ReadTextFile(path, &currentBytes);
	const bool unchanged = originalText
		? (current && currentBytes == originalBytes && memcmp(current, originalText, (size_t)originalBytes) == 0)
		: (current == NULL);
	free(current);
	if (!unchanged)
	{
		DeleteFileA(temporary);
		snprintf(status, statusCapacity, "Manifest changed on disk; the PNG was exported but not registered. Retry to merge it.");
		return false;
	}
	return InspectorExport_Commit(temporary, path, status, statusCapacity);
}
#endif
}

void HdTextureOverrides_Reset()
{
	PsyX_Inspector_ClearSelection();
	for (int i = g_activeOverrideCount - 1; i >= 0; --i) RemoveActiveOverrideAt(i);
	g_knownTextureCount = 0;
}

void HdTextureOverrides_BeginPage(int texturePage)
{
	LoadManifests();
	for (int i = g_activeOverrideCount - 1; i >= 0; --i)
		if (g_activeOverrides[i].texturePage == texturePage) RemoveActiveOverrideAt(i);
	for (int i = g_knownTextureCount - 1; i >= 0; --i)
	{
		if (g_knownTextures[i].texturePage == texturePage)
		{
			memmove(&g_knownTextures[i], &g_knownTextures[i + 1],
				sizeof(g_knownTextures[0]) * (g_knownTextureCount - i - 1));
			--g_knownTextureCount;
		}
	}
}

void HdTextureOverrides_RegisterTexture(int texturePage, int textureIndex, const char* textureName,
	unsigned short tpage, unsigned short clut, unsigned short u, unsigned short v,
	unsigned short width, unsigned short height)
{
	if (!textureName || width == 0 || height == 0) return;
	LoadManifests();
	RegisterKnownTexture(texturePage, textureIndex, textureName, tpage, clut, u, v, width, height);
	if (g_activeOverrideCount >= kMaximumActiveOverrides) return;
	ManifestEntry* entry = FindManifestEntry(textureName, texturePage, textureIndex);
	if (!entry || !EnsureImageLoaded(entry)) return;
	PsyXTextureOverride descriptor = {};
	descriptor.tpage = tpage; descriptor.clut = clut; descriptor.u = u; descriptor.v = v;
	descriptor.width = width; descriptor.height = height; descriptor.textureId = entry->textureId;
	const int overrideId = PsyX_RegisterTextureOverride(&descriptor);
	if (overrideId == 0) { SetStatus("PsyCross could not register another mod texture override"); return; }
	ActiveOverride* active = &g_activeOverrides[g_activeOverrideCount++];
	active->texturePage = texturePage; active->textureIndex = textureIndex; active->overrideId = overrideId;
	active->manifestIndex = (int)(entry - g_manifestEntries);
	snprintf(g_status, sizeof(g_status), "Applied %s from mod %s as %d x %d RGBA", textureName,
		g_mods[entry->modIndex].id, entry->imageWidth, entry->imageHeight);
}

void HdTextureOverrides_SetEnabled(int enabled)
{
	g_enabled = enabled != 0;
	PsyX_SetTextureOverridesEnabled(g_enabled);
}

bool HdTextureOverrides_Reload()
{
	static KnownTexture knownTextures[kMaximumKnownTextures];
	const int knownTextureCount = g_knownTextureCount;
	memcpy(knownTextures, g_knownTextures, sizeof(KnownTexture) * knownTextureCount);

	for (int i = g_activeOverrideCount - 1; i >= 0; --i)
		RemoveActiveOverrideAt(i);
	for (int i = 0; i < g_manifestEntryCount; ++i)
	{
		if (g_manifestEntries[i].textureId != 0)
			PsyX_DestroyRGBATexture(g_manifestEntries[i].textureId);
	}
	memset(g_mods, 0, sizeof(g_mods));
	memset(g_manifestEntries, 0, sizeof(g_manifestEntries));
	memset(g_activeOverrides, 0, sizeof(g_activeOverrides));
	g_modCount = 0;
	g_manifestEntryCount = 0;
	g_activeOverrideCount = 0;
	g_manifestLoaded = 0;
	g_manifestFound = 0;
	g_knownTextureCount = knownTextureCount;
	memcpy(g_knownTextures, knownTextures, sizeof(KnownTexture) * knownTextureCount);

	LoadManifests();
	for (int i = 0; i < g_knownTextureCount; ++i)
	{
		const KnownTexture& known = g_knownTextures[i];
		HdTextureOverrides_RegisterTexture(known.texturePage, known.textureIndex, known.textureName,
			known.tpage, known.clut, known.u, known.v, known.width, known.height);
	}
	SetStatus("Mod manifests and already-loaded texture overrides were reloaded");
	return true;
}

void HdTextureOverrides_GetDiagnostics(HdTextureOverrideDiagnostics* diagnostics)
{
	if (!diagnostics) return;
	LoadManifests();
	PsyXTextureOverrideStats rendererStats = {};
	PsyX_GetTextureOverrideStats(&rendererStats);
	int loadedImages = 0;
	int activeMods = 0;
	for (int i = 0; i < g_modCount; ++i) activeMods += g_mods[i].enabled != 0;
	for (int i = 0; i < g_manifestEntryCount; ++i) loadedImages += g_manifestEntries[i].textureId != 0;
	diagnostics->supported =
#ifdef _WIN32
		1;
#else
		0;
#endif
	diagnostics->enabled = rendererStats.enabled;
	diagnostics->manifestFound = g_manifestFound;
	diagnostics->manifestEntries = g_manifestEntryCount;
	diagnostics->discoveredMods = g_modCount;
	diagnostics->activeMods = activeMods;
	diagnostics->loadedImages = loadedImages;
	diagnostics->registeredOverrides = rendererStats.registeredCount;
	CopyText(diagnostics->status, sizeof(diagnostics->status), g_status);
}

int HdTextureOverrides_GetModCount()
{
	LoadManifests();
	return g_modCount;
}

bool HdTextureOverrides_GetModInfo(int index, HdTextureOverrideModInfo* info)
{
	LoadManifests();
	if (!info || index < 0 || index >= g_modCount) return false;
	CopyText(info->id, sizeof(info->id), g_mods[index].id);
	CopyText(info->name, sizeof(info->name), g_mods[index].name);
	CopyText(info->description, sizeof(info->description), g_mods[index].description);
	info->enabled = g_mods[index].enabled;
	info->textureEntries = g_mods[index].textureEntries;
	return true;
}

int HdTextureOverrides_GetEntryCount()
{
	LoadManifests();
	return g_manifestEntryCount;
}

bool HdTextureOverrides_GetEntryInfo(int index, HdTextureOverrideEntryInfo* info)
{
	LoadManifests();
	if (!info || index < 0 || index >= g_manifestEntryCount) return false;
	const ManifestEntry* entry = &g_manifestEntries[index];
	CopyText(info->modId, sizeof(info->modId), g_mods[entry->modIndex].id);
	CopyText(info->textureName, sizeof(info->textureName), entry->textureName);
	CopyText(info->assetPath, sizeof(info->assetPath), entry->imagePath);
	info->texturePage = entry->texturePage;
	info->textureIndex = entry->textureIndex;
	info->active = entry->textureId != 0;
	return true;
}

bool HdTextureOverrides_FindTextureInfo(unsigned short tpage, unsigned short clut,
	unsigned short u, unsigned short v, unsigned short width, unsigned short height,
	HdTextureInspectorInfo* info)
{
	LoadManifests();
	if (!info)
		return false;
	memset(info, 0, sizeof(*info));
	const KnownTexture* best = NULL;
	int bestArea = 0;
	for (int i = 0; i < g_knownTextureCount; ++i)
	{
		const KnownTexture* known = &g_knownTextures[i];
		if (ComparableTPage(known->tpage) != ComparableTPage(tpage) || known->clut != clut)
			continue;
		const int right = (int)u + width;
		const int bottom = (int)v + height;
		const int knownRight = (int)known->u + known->width;
		const int knownBottom = (int)known->v + known->height;
		if (u < known->u || v < known->v || right > knownRight || bottom > knownBottom)
			continue;
		const int area = known->width * known->height;
		if (!best || area < bestArea)
		{
			best = known;
			bestArea = area;
		}
	}
	if (!best)
		return false;
	CopyText(info->textureName, sizeof(info->textureName), best->textureName);
	info->texturePage = best->texturePage;
	info->textureIndex = best->textureIndex;
	info->u = best->u;
	info->v = best->v;
	info->width = best->width;
	info->height = best->height;
	ManifestEntry* overrideEntry = FindManifestEntry(best->textureName, best->texturePage, best->textureIndex);
	if (overrideEntry)
	{
		CopyText(info->modId, sizeof(info->modId), g_mods[overrideEntry->modIndex].id);
		CopyText(info->overridePath, sizeof(info->overridePath), overrideEntry->imagePath);
		info->hasOverride = overrideEntry->textureId != 0;
		info->previewTextureId = overrideEntry->textureId;
	}
	return true;
}

void HdTextureOverrides_GetModsDirectory(char* path, int capacity)
{
	if (!path || capacity <= 0) return;
	LoadManifests();
#ifdef _WIN32
	const DWORD length = GetFullPathNameA(g_modsDirectory, capacity, path, NULL);
	if (length > 0 && length < (DWORD)capacity) return;
#endif
	CopyText(path, capacity, g_modsDirectory);
}

bool HdTextureOverrides_ExportInspectorReport(const char* modId, const char* report, char* status, int capacity)
{
	if (!status || capacity <= 0) return false;
	if (!IsSafeModId(modId) || !report)
	{
		snprintf(status, capacity, "Choose a valid mod id before exporting selection details.");
		return false;
	}
#ifdef _WIN32
	LoadManifests();
	char modPath[256], assetsPath[320], directory[384], path[448];
	if (!JoinPath(modPath, sizeof(modPath), g_modsDirectory, modId, NULL) ||
		!JoinPath(assetsPath, sizeof(assetsPath), modPath, "assets", NULL) ||
		!JoinPath(directory, sizeof(directory), assetsPath, "inspector", NULL) ||
		!JoinPath(path, sizeof(path), directory, "selection.txt", NULL))
	{
		snprintf(status, capacity, "The report export path is too long.");
		return false;
	}
	CreateDirectoryA(g_modsDirectory, NULL);
	CreateDirectoryA(modPath, NULL);
	CreateDirectoryA(assetsPath, NULL);
	CreateDirectoryA(directory, NULL);
	return InspectorExport_WriteText(path, report, status, capacity);
#else
	snprintf(status, capacity, "Report export currently requires Windows.");
	return false;
#endif
}

bool HdTextureOverrides_ExportTexture(unsigned short tpage, unsigned short clut,
	unsigned short u, unsigned short v, unsigned short width, unsigned short height,
	const char* textureName, int texturePage, int textureIndex, const char* modId,
	char* status, int statusCapacity)
{
	if (!status || statusCapacity <= 0)
		return false;
	status[0] = '\0';
	LoadManifests();
	if (!IsSafeModId(modId) || !textureName || textureName[0] == '\0' || width == 0 || height == 0 || u + width > 256 || v + height > 256)
	{
		snprintf(status, statusCapacity, "Choose a valid mod id and a non-empty texture region");
		return false;
	}

#ifndef _WIN32
	snprintf(status, statusCapacity, "Texture export is currently available on Windows only");
	return false;
#else
	const int format = (tpage >> 7) & 3;
	const int pageX = (tpage & 0x0f) << 6;
	const int pageY = (tpage & 0x10) ? 256 : 0;
	if (format == 3 || pageX < 0 || pageY < 0 || pageY + v + height > VRAM_HEIGHT)
	{
		snprintf(status, statusCapacity, "The selected texture uses an unsupported VRAM format or region");
		return false;
	}

	unsigned short* vram = (unsigned short*)malloc(VRAM_WIDTH * VRAM_HEIGHT * sizeof(unsigned short));
	unsigned char* rgba = (unsigned char*)malloc((size_t)width * height * 4);
	if (!vram || !rgba)
	{
		free(vram);
		free(rgba);
		snprintf(status, statusCapacity, "Not enough memory to export the selected texture");
		return false;
	}
	GR_ReadVRAM(vram, 0, 0, VRAM_WIDTH, VRAM_HEIGHT);
	const int clutX = (clut & 0x3f) << 4;
	const int clutY = clut >> 6;
	bool valid = clutY >= 0 && clutY < VRAM_HEIGHT && clutX >= 0 && clutX < VRAM_WIDTH;
	for (int yPixel = 0; valid && yPixel < height; ++yPixel)
	{
		for (int xPixel = 0; xPixel < width; ++xPixel)
		{
			const int sourceU = u + xPixel;
			const int sourceV = v + yPixel;
			int wordX = pageX;
			unsigned short colour = 0;
			if (format == 0)
			{
				wordX += sourceU >> 2;
				if (wordX >= VRAM_WIDTH) { valid = false; break; }
				const unsigned short packed = vram[(pageY + sourceV) * VRAM_WIDTH + wordX];
				const int colourIndex = (packed >> ((sourceU & 3) * 4)) & 15;
				colour = vram[clutY * VRAM_WIDTH + clutX + colourIndex];
			}
			else if (format == 1)
			{
				wordX += sourceU >> 1;
				if (wordX >= VRAM_WIDTH || clutX + 255 >= VRAM_WIDTH) { valid = false; break; }
				const unsigned short packed = vram[(pageY + sourceV) * VRAM_WIDTH + wordX];
				const int colourIndex = (packed >> ((sourceU & 1) * 8)) & 255;
				colour = vram[clutY * VRAM_WIDTH + clutX + colourIndex];
			}
			else
			{
				wordX += sourceU;
				if (wordX >= VRAM_WIDTH) { valid = false; break; }
				colour = vram[(pageY + sourceV) * VRAM_WIDTH + wordX];
			}
			unsigned char* pixel = rgba + ((size_t)yPixel * width + xPixel) * 4;
			pixel[0] = (unsigned char)(((colour & 31) << 3) | ((colour & 31) >> 2));
			pixel[1] = (unsigned char)((((colour >> 5) & 31) << 3) | ((colour >> 5) & 31) >> 2);
			pixel[2] = (unsigned char)((((colour >> 10) & 31) << 3) | ((colour >> 10) & 31) >> 2);
			pixel[3] = colour == 0 ? 0 : 255;
		}
	}
	if (!valid)
	{
		free(vram);
		free(rgba);
		snprintf(status, statusCapacity, "The selected texture region is outside VRAM or its CLUT");
		return false;
	}

	char safeName[48] = {};
	for (int i = 0; textureName[i] && i < (int)sizeof(safeName) - 1; ++i)
	{
		const char character = textureName[i];
		safeName[i] = (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') ||
			(character >= '0' && character <= '9') || character == '_' || character == '-' ? character : '_';
	}
	char modDirectory[256];
	char assetsDirectory[320];
	char inspectorDirectory[384];
	char outputPath[448];
	if (!JoinPath(modDirectory, sizeof(modDirectory), g_modsDirectory, modId, NULL) ||
		!JoinPath(assetsDirectory, sizeof(assetsDirectory), modDirectory, "assets", NULL) ||
		!JoinPath(inspectorDirectory, sizeof(inspectorDirectory), assetsDirectory, "inspector", NULL) ||
		snprintf(outputPath, sizeof(outputPath), "%s/%s_p%d_i%d.png", inspectorDirectory, safeName, texturePage, textureIndex) < 0)
	{
		free(vram); free(rgba);
		snprintf(status, statusCapacity, "The export path is too long");
		return false;
	}
	CreateDirectoryA(g_modsDirectory, NULL);
	CreateDirectoryA(modDirectory, NULL);
	CreateDirectoryA(assetsDirectory, NULL);
	CreateDirectoryA(inspectorDirectory, NULL);
	char temporary[MAX_PATH];
	if (!InspectorExport_Temporary(outputPath, temporary, status, statusCapacity))
	{
		free(vram); free(rgba); return false;
	}
	const bool wroteImage = SavePngRgba(temporary, rgba, width, height);
	free(vram);
	free(rgba);
	if (!wroteImage)
	{
		DeleteFileA(temporary);
		snprintf(status, statusCapacity, "%s Previous export preserved.", g_status);
		return false;
	}
	// Decode the completed PNG before publishing it; catches corrupt/empty output.
	unsigned char* verified = NULL;
	int verifiedWidth = 0, verifiedHeight = 0;
	const bool validPng = LoadPngRgba(temporary, &verified, &verifiedWidth, &verifiedHeight) &&
		verifiedWidth == width && verifiedHeight == height;
	free(verified);
	if (!validPng)
	{
		DeleteFileA(temporary);
		snprintf(status, statusCapacity, "PNG verification failed; previous export preserved.");
		return false;
	}
	if (!InspectorExport_Commit(temporary, outputPath, status, statusCapacity)) return false;

	char manifestPath[320];
	JoinPath(manifestPath, sizeof(manifestPath), modDirectory, "manifest.json", NULL);

	std::string escapedTextureName;
	for (const unsigned char* c = (const unsigned char*)textureName; *c; ++c)
	{
		if (*c == '"' || *c == '\\') escapedTextureName += '\\';
		if (*c < 32)
		{
			char escaped[7];
			snprintf(escaped, sizeof(escaped), "\\u%04x", (unsigned int)*c);
			escapedTextureName += escaped;
		}
		else escapedTextureName += (char)*c;
	}
	const std::string entry = BuildManifestEntry(escapedTextureName, texturePage, textureIndex, safeName);

	int existingBytes = 0;
	char* existingText = ReadTextFile(manifestPath, &existingBytes);
	if (existingText)
	{
		const char* spanBegin = NULL;
		const char* spanEnd = NULL;
		if (!FindTexturesArraySpan(existingText, existingBytes, &spanBegin, &spanEnd))
		{
			free(existingText);
			snprintf(status, statusCapacity, "PNG exported, but the existing manifest.json has no readable textures array; it was not modified.");
			return false;
		}
		if (ManifestTexturesContain(spanBegin, spanEnd, textureName, texturePage, textureIndex))
		{
			free(existingText);
			snprintf(status, statusCapacity, "Exported and replaced %s; it was already registered in mod %s", textureName, modId);
			return true;
		}
		const std::string merged = MergeManifestEntry(existingText, existingBytes, spanBegin, spanEnd, entry);
		const bool published = PublishManifestFile(manifestPath, existingText, existingBytes, merged, status, statusCapacity);
		free(existingText);
		if (published)
			snprintf(status, statusCapacity, "Exported PNG and appended %s to mod %s; reload mod manifests to apply it", textureName, modId);
		return published;
	}

	char manifestText[2048];
	const int manifestBytes = snprintf(manifestText, sizeof(manifestText),
		"{\n  \"schemaVersion\": 1,\n  \"id\": \"%s\",\n  \"name\": \"%s Inspector Export\",\n"
		"  \"description\": \"Texture exported locally by the 3D inspector.\",\n  \"textures\": [\n"
		"    %s\n  ]\n}\n",
		modId, modId, entry.c_str());
	if (manifestBytes <= 0 || manifestBytes >= (int)sizeof(manifestText))
	{
		snprintf(status, statusCapacity, "PNG exported, but generated manifest exceeds its size limit.");
		return false;
	}
	const bool wroteManifest = InspectorExport_WriteText(manifestPath, manifestText, status, statusCapacity);
	snprintf(status, statusCapacity, wroteManifest ? "Exported PNG and created mod manifest; reload manifests to apply it" : "Exported PNG, but manifest.json could not be completed");
	return wroteManifest;
#endif
}
