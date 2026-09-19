#include "GltfLoader.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincodec.h>
#endif

namespace
{
// --- tiny JSON DOM ----------------------------------------------------------

struct JsonValue
{
	enum Type { Null, Bool, Number, String, Array, Object };

	Type type = Null;
	bool boolean = false;
	double number = 0.0;
	std::string str;
	std::vector<JsonValue> array;
	std::vector<std::pair<std::string, JsonValue> > object;

	const JsonValue* Find(const char* key) const
	{
		if (type != Object)
			return NULL;
		for (size_t i = 0; i < object.size(); i++)
			if (object[i].first == key)
				return &object[i].second;
		return NULL;
	}

	double NumberOr(double fallback) const
	{
		return type == Number ? number : fallback;
	}
};

struct JsonParser
{
	const char* p;
	const char* end;
	char* error;
	int errorCapacity;
	bool failed;

	JsonParser(const char* text, int length, char* err, int errCap)
		: p(text), end(text + length), error(err), errorCapacity(errCap), failed(false)
	{
	}

	void Fail(const char* message)
	{
		if (!failed)
		{
			failed = true;
			if (error && errorCapacity > 0)
				snprintf(error, errorCapacity, "%s", message);
		}
	}

	void SkipWhitespace()
	{
		while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r'))
			p++;
	}

	bool Consume(char c)
	{
		SkipWhitespace();
		if (p < end && *p == c)
		{
			p++;
			return true;
		}
		return false;
	}

	bool ParseString(std::string& out)
	{
		SkipWhitespace();
		if (p >= end || *p != '"')
		{
			Fail("expected a JSON string");
			return false;
		}
		p++;
		out.clear();
		while (p < end && *p != '"')
		{
			char c = *p++;
			if (c == '\\')
			{
				if (p >= end) { Fail("truncated JSON escape"); return false; }
				char e = *p++;
				switch (e)
				{
					case '"': out.push_back('"'); break;
					case '\\': out.push_back('\\'); break;
					case '/': out.push_back('/'); break;
					case 'b': out.push_back('\b'); break;
					case 'f': out.push_back('\f'); break;
					case 'n': out.push_back('\n'); break;
					case 'r': out.push_back('\r'); break;
					case 't': out.push_back('\t'); break;
					case 'u':
					{
						if (end - p < 4) { Fail("truncated JSON unicode escape"); return false; }
						unsigned int code = 0;
						for (int i = 0; i < 4; i++)
						{
							char h = *p++;
							code <<= 4;
							if (h >= '0' && h <= '9') code |= (unsigned)(h - '0');
							else if (h >= 'a' && h <= 'f') code |= (unsigned)(h - 'a' + 10);
							else if (h >= 'A' && h <= 'F') code |= (unsigned)(h - 'A' + 10);
							else { Fail("bad JSON unicode escape"); return false; }
						}
						// UTF-8 encode (BMP only, enough for glTF names/paths).
						if (code < 0x80) out.push_back((char)code);
						else if (code < 0x800)
						{
							out.push_back((char)(0xC0 | (code >> 6)));
							out.push_back((char)(0x80 | (code & 0x3F)));
						}
						else
						{
							out.push_back((char)(0xE0 | (code >> 12)));
							out.push_back((char)(0x80 | ((code >> 6) & 0x3F)));
							out.push_back((char)(0x80 | (code & 0x3F)));
						}
						break;
					}
					default: Fail("bad JSON escape"); return false;
				}
			}
			else
			{
				out.push_back(c);
			}
		}
		if (p >= end) { Fail("unterminated JSON string"); return false; }
		p++; // closing quote
		return true;
	}

	bool ParseValue(JsonValue& out, int depth)
	{
		if (depth > 64) { Fail("JSON nesting too deep"); return false; }
		SkipWhitespace();
		if (p >= end) { Fail("unexpected end of JSON"); return false; }

		char c = *p;
		if (c == '{')
		{
			p++;
			out.type = JsonValue::Object;
			SkipWhitespace();
			if (Consume('}')) return true;
			for (;;)
			{
				std::string key;
				if (!ParseString(key)) return false;
				if (!Consume(':')) { Fail("expected ':' in JSON object"); return false; }
				out.object.push_back(std::make_pair(key, JsonValue()));
				if (!ParseValue(out.object.back().second, depth + 1)) return false;
				if (Consume(',')) continue;
				if (Consume('}')) break;
				Fail("expected ',' or '}' in JSON object");
				return false;
			}
			return true;
		}
		if (c == '[')
		{
			p++;
			out.type = JsonValue::Array;
			SkipWhitespace();
			if (Consume(']')) return true;
			for (;;)
			{
				out.array.push_back(JsonValue());
				if (!ParseValue(out.array.back(), depth + 1)) return false;
				if (Consume(',')) continue;
				if (Consume(']')) break;
				Fail("expected ',' or ']' in JSON array");
				return false;
			}
			return true;
		}
		if (c == '"')
		{
			out.type = JsonValue::String;
			return ParseString(out.str);
		}
		if (c == 't' && end - p >= 4 && strncmp(p, "true", 4) == 0) { p += 4; out.type = JsonValue::Bool; out.boolean = true; return true; }
		if (c == 'f' && end - p >= 5 && strncmp(p, "false", 5) == 0) { p += 5; out.type = JsonValue::Bool; out.boolean = false; return true; }
		if (c == 'n' && end - p >= 4 && strncmp(p, "null", 4) == 0) { p += 4; out.type = JsonValue::Null; return true; }

		// number
		{
			char* numberEnd = NULL;
			double value = strtod(p, &numberEnd);
			if (numberEnd == p)
			{
				Fail("invalid JSON value");
				return false;
			}
			p = numberEnd;
			out.type = JsonValue::Number;
			out.number = value;
			return true;
		}
	}
};

// --- GLB container ----------------------------------------------------------

inline unsigned int ReadU32(const unsigned char* p)
{
	return (unsigned int)p[0] | ((unsigned int)p[1] << 8) | ((unsigned int)p[2] << 16) | ((unsigned int)p[3] << 24);
}

inline unsigned short ReadU16(const unsigned char* p)
{
	return (unsigned short)((unsigned int)p[0] | ((unsigned int)p[1] << 8));
}

// --- image decoding ---------------------------------------------------------

#ifdef _WIN32
int DecodeImageFileWIC(const char* path, std::vector<unsigned char>& rgba, int& width, int& height, char* error, int errorCapacity)
{
	IWICImagingFactory* factory = NULL;
	HRESULT result = CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
	if (FAILED(result) || !factory)
	{
		snprintf(error, errorCapacity, "WIC unavailable");
		return -1;
	}

	// Convert the UTF-8 path to UTF-16 for the WIC file API.
	wchar_t widePath[MAX_PATH];
	int converted = MultiByteToWideChar(CP_UTF8, 0, path, -1, widePath, MAX_PATH);
	if (converted <= 0)
	{
		factory->Release();
		snprintf(error, errorCapacity, "image path is not valid UTF-8");
		return -1;
	}

	IWICBitmapDecoder* decoder = NULL;
	IWICBitmapFrameDecode* frame = NULL;
	IWICFormatConverter* converter = NULL;

	result = factory->CreateDecoderFromFilename(widePath, NULL, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder);
	if (SUCCEEDED(result)) result = decoder->GetFrame(0, &frame);
	if (SUCCEEDED(result))
	{
		UINT w = 0, h = 0;
		result = frame->GetSize(&w, &h);
		if (SUCCEEDED(result))
		{
			result = factory->CreateFormatConverter(&converter);
			if (SUCCEEDED(result))
				result = converter->Initialize(frame, GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, NULL, 0.0, WICBitmapPaletteTypeCustom);
			if (SUCCEEDED(result))
			{
				const UINT stride = w * 4;
				rgba.resize((size_t)stride * h);
				result = converter->CopyPixels(NULL, stride, (UINT)rgba.size(), rgba.data());
				if (SUCCEEDED(result))
				{
					width = (int)w;
					height = (int)h;
				}
			}
		}
	}

	if (converter) converter->Release();
	if (frame) frame->Release();
	if (decoder) decoder->Release();
	factory->Release();

	if (FAILED(result))
	{
		rgba.clear();
		snprintf(error, errorCapacity, "image decode failed (HRESULT 0x%08lX)", (unsigned long)result);
		return -1;
	}

	return 0;
}
#else
int DecodeImageFileWIC(const char*, std::vector<unsigned char>&, int&, int&, char* error, int errorCapacity)
{
	snprintf(error, errorCapacity, "texture decoding is implemented on Windows only");
	return -1;
}
#endif

int WriteTempFile(const unsigned char* data, size_t size, char* path, int pathCapacity, char* error, int errorCapacity)
{
#ifdef _WIN32
	char tempDir[MAX_PATH];
	DWORD length = GetTempPathA(MAX_PATH, tempDir);
	if (length == 0 || length >= MAX_PATH)
	{
		snprintf(error, errorCapacity, "no temporary directory available");
		return -1;
	}
	snprintf(path, pathCapacity, "%sredriver2_gltf_tex_%lu.bin", tempDir, (unsigned long)GetCurrentProcessId());
	FILE* file = fopen(path, "wb");
	if (!file)
	{
		snprintf(error, errorCapacity, "cannot create a temporary image file");
		return -1;
	}
	if (size > 0 && fwrite(data, 1, size, file) != size)
	{
		fclose(file);
		remove(path);
		snprintf(error, errorCapacity, "cannot write the temporary image file");
		return -1;
	}
	fclose(file);
	return 0;
#else
	(void)data; (void)size; (void)path; (void)pathCapacity;
	snprintf(error, errorCapacity, "texture decoding is implemented on Windows only");
	return -1;
#endif
}

// --- base64 (for data: image URIs) ------------------------------------------

int DecodeBase64(const std::string& text, std::vector<unsigned char>& out)
{
	static const signed char table[256] = {
		-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
		-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
		-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,
		52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,
		-1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
		15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,
		-1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
		41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,
		-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
		-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
		-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
		-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
		-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
		-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
		-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
		-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
	};

	out.clear();
	int value = 0, bits = 0;
	for (size_t i = 0; i < text.size(); i++)
	{
		char c = text[i];
		if (c == '=' || c == '\n' || c == '\r' || c == ' ' || c == '\t')
			continue;
		signed char d = table[(unsigned char)c];
		if (d < 0)
			return -1;
		value = (value << 6) | d;
		bits += 6;
		if (bits >= 8)
		{
			bits -= 8;
			out.push_back((unsigned char)((value >> bits) & 0xFF));
		}
	}
	return 0;
}

// --- accessor reading -------------------------------------------------------

double NumberField(const JsonValue* object, const char* key, double fallback);

struct BufferView
{
	int buffer;
	size_t byteOffset;
	size_t byteLength;
	size_t byteStride;
};

bool ReadAccessorFloats(const JsonValue& root, const std::vector<unsigned char>& bin,
	const std::vector<BufferView>& views, int accessorIndex, int components,
	std::vector<float>& out, int& count, char* error, int errorCapacity)
{
	const JsonValue* accessors = root.Find("accessors");
	if (!accessors || accessors->type != JsonValue::Array || accessorIndex < 0 || (size_t)accessorIndex >= accessors->array.size())
	{
		snprintf(error, errorCapacity, "accessor %d is missing", accessorIndex);
		return false;
	}
	const JsonValue& accessor = accessors->array[accessorIndex];
	const JsonValue* viewIndex = accessor.Find("bufferView");
	const JsonValue* componentType = accessor.Find("componentType");
	const JsonValue* type = accessor.Find("type");
	if (!viewIndex || !componentType || !type)
	{
		snprintf(error, errorCapacity, "accessor %d is not a readable buffer view", accessorIndex);
		return false;
	}
	if ((int)componentType->NumberOr(-1) != 5126)
	{
		snprintf(error, errorCapacity, "accessor %d is not float data", accessorIndex);
		return false;
	}

	int expectedComponents = 0;
	if (type->str == "SCALAR") expectedComponents = 1;
	else if (type->str == "VEC2") expectedComponents = 2;
	else if (type->str == "VEC3") expectedComponents = 3;
	else if (type->str == "VEC4") expectedComponents = 4;
	else
	{
		snprintf(error, errorCapacity, "accessor %d has unsupported type", accessorIndex);
		return false;
	}
	if (expectedComponents != components)
	{
		snprintf(error, errorCapacity, "accessor %d has the wrong component count", accessorIndex);
		return false;
	}

	int view = (int)viewIndex->NumberOr(-1);
	if (view < 0 || (size_t)view >= views.size())
	{
		snprintf(error, errorCapacity, "accessor %d points to a missing bufferView", accessorIndex);
		return false;
	}

	count = (int)NumberField(&accessor, "count", 0);
	const size_t offset = views[view].byteOffset + (size_t)NumberField(&accessor, "byteOffset", 0);
	const size_t elementSize = (size_t)components * sizeof(float);
	const size_t stride = views[view].byteStride ? views[view].byteStride : elementSize;

	if (stride == elementSize)
	{
		if (offset + elementSize * count > bin.size())
		{
			snprintf(error, errorCapacity, "accessor %d reads outside the binary chunk", accessorIndex);
			return false;
		}
		out.resize((size_t)count * components);
		memcpy(out.data(), bin.data() + offset, (size_t)count * elementSize);
	}
	else
	{
		out.resize((size_t)count * components);
		for (int i = 0; i < count; i++)
		{
			const size_t base = offset + stride * i;
			if (base + elementSize > bin.size())
			{
				snprintf(error, errorCapacity, "accessor %d reads outside the binary chunk", accessorIndex);
				return false;
			}
			memcpy(out.data() + (size_t)i * components, bin.data() + base, elementSize);
		}
	}
	return true;
}

bool ReadAccessorIndices(const JsonValue& root, const std::vector<unsigned char>& bin,
	const std::vector<BufferView>& views, int accessorIndex, std::vector<unsigned int>& out, char* error, int errorCapacity)
{
	const JsonValue* accessors = root.Find("accessors");
	if (!accessors || accessorIndex < 0 || (size_t)accessorIndex >= accessors->array.size())
	{
		snprintf(error, errorCapacity, "index accessor is missing");
		return false;
	}
	const JsonValue& accessor = accessors->array[accessorIndex];
	const JsonValue* viewIndex = accessor.Find("bufferView");
	const JsonValue* componentType = accessor.Find("componentType");
	if (!viewIndex || !componentType)
	{
		snprintf(error, errorCapacity, "index accessor is incomplete");
		return false;
	}
	const int component = (int)componentType->NumberOr(0);
	if (component != 5121 && component != 5123 && component != 5125)
	{
		snprintf(error, errorCapacity, "index accessor uses an unsupported component type");
		return false;
	}

	int view = (int)viewIndex->NumberOr(-1);
	if (view < 0 || (size_t)view >= views.size())
	{
		snprintf(error, errorCapacity, "index accessor points to a missing bufferView");
		return false;
	}

	const int count = (int)NumberField(&accessor, "count", 0);
	const size_t componentSize = component == 5121 ? 1 : (component == 5123 ? 2 : 4);
	const size_t offset = views[view].byteOffset + (size_t)NumberField(&accessor, "byteOffset", 0);
	const size_t elementSize = componentSize;
	const size_t stride = views[view].byteStride ? views[view].byteStride : elementSize;

	out.resize(count);
	for (int i = 0; i < count; i++)
	{
		const size_t base = offset + stride * i;
		if (base + elementSize > bin.size())
		{
			snprintf(error, errorCapacity, "index accessor reads outside the binary chunk");
			return false;
		}
		if (component == 5121) out[i] = bin[base];
		else if (component == 5123) out[i] = ReadU16(bin.data() + base);
		else out[i] = ReadU32(bin.data() + base);
	}
	return true;
}

const JsonValue* GetArrayEntry(const JsonValue* array, int index)
{
	if (!array || array->type != JsonValue::Array || index < 0 || (size_t)index >= array->array.size())
		return NULL;
	return &array->array[index];
}

double NumberField(const JsonValue* object, const char* key, double fallback)
{
	const JsonValue* value = object ? object->Find(key) : NULL;
	return value ? value->NumberOr(fallback) : fallback;
}

// Decodes the image referenced by a glTF textureInfo ({ index }) into RGBA.
void LoadTextureInfo(const JsonValue& root, const std::vector<unsigned char>& bin,
	const std::vector<BufferView>& views, const JsonValue* textureInfo,
	std::vector<unsigned char>& outRGBA, int& outWidth, int& outHeight)
{
	const JsonValue* textures = root.Find("textures");
	const JsonValue* images = root.Find("images");
	if (!textureInfo || !textures || !images)
		return;

	const JsonValue* texture = GetArrayEntry(textures, (int)NumberField(textureInfo, "index", -1));
	const JsonValue* source = texture ? texture->Find("source") : NULL;
	const JsonValue* image = source ? GetArrayEntry(images, (int)source->NumberOr(-1)) : NULL;
	if (!image)
		return;

	std::vector<unsigned char> imageBytes;
	const JsonValue* uri = image->Find("uri");
	const JsonValue* imageView = image->Find("bufferView");

	if (imageView && imageView->type == JsonValue::Number)
	{
		const int viewIndex = (int)imageView->number;
		if (viewIndex >= 0 && (size_t)viewIndex < views.size())
		{
			const BufferView& view = views[viewIndex];
			if (view.byteOffset + view.byteLength <= bin.size())
				imageBytes.assign(bin.begin() + view.byteOffset, bin.begin() + view.byteOffset + view.byteLength);
		}
	}
	else if (uri && uri->str.compare(0, 5, "data:") == 0)
	{
		const size_t comma = uri->str.find(',');
		const size_t semicolon = uri->str.find(";base64");
		if (comma != std::string::npos && semicolon != std::string::npos && semicolon < comma)
			DecodeBase64(uri->str.substr(comma + 1), imageBytes);
	}

	if (imageBytes.empty())
		return;

	char tempPath[600];
	char decodeError[192] = "";
	if (WriteTempFile(imageBytes.data(), imageBytes.size(), tempPath, sizeof(tempPath), decodeError, sizeof(decodeError)) == 0)
	{
		DecodeImageFileWIC(tempPath, outRGBA, outWidth, outHeight, decodeError, sizeof(decodeError));
		remove(tempPath);
	}
}
} // namespace

int Gltf_LoadFile(const char* path, GltfMeshData* out, char* error, int errorCapacity)
{
	if (!path || !out || !error || errorCapacity <= 0)
		return -1;
	error[0] = '\0';

	FILE* file = fopen(path, "rb");
	if (!file)
	{
		snprintf(error, errorCapacity, "cannot open %s", path);
		return -1;
	}
	fseek(file, 0, SEEK_END);
	long fileSize = ftell(file);
	fseek(file, 0, SEEK_SET);
	if (fileSize <= 12)
	{
		fclose(file);
		snprintf(error, errorCapacity, "%s is too small to be a glTF/GLB file", path);
		return -1;
	}

	std::vector<unsigned char> bytes((size_t)fileSize);
	if (fread(bytes.data(), 1, bytes.size(), file) != bytes.size())
	{
		fclose(file);
		snprintf(error, errorCapacity, "cannot read %s", path);
		return -1;
	}
	fclose(file);

	// GLB container?
	std::string jsonText;
	std::vector<unsigned char> bin;

	if (ReadU32(bytes.data()) == 0x46546C67)
	{
		const unsigned int version = ReadU32(bytes.data() + 4);
		if (version != 2)
		{
			snprintf(error, errorCapacity, "GLB version %u is not supported", version);
			return -1;
		}

		size_t offset = 12;
		while (offset + 8 <= bytes.size())
		{
			const unsigned int chunkLength = ReadU32(bytes.data() + offset);
			const unsigned int chunkType = ReadU32(bytes.data() + offset + 4);
			offset += 8;
			if (offset + chunkLength > bytes.size())
			{
				snprintf(error, errorCapacity, "GLB chunk exceeds the file size");
				return -1;
			}
			if (chunkType == 0x4E4F534A) // JSON
				jsonText.assign((const char*)bytes.data() + offset, chunkLength);
			else if (chunkType == 0x004E4942) // BIN
				bin.assign(bytes.begin() + offset, bytes.begin() + offset + chunkLength);
			offset += chunkLength;
		}
	}
	else
	{
		snprintf(error, errorCapacity, "only binary .glb files are supported");
		return -1;
	}

	if (jsonText.empty())
	{
		snprintf(error, errorCapacity, "the GLB has no JSON chunk");
		return -1;
	}

	JsonValue root;
	JsonParser parser(jsonText.c_str(), (int)jsonText.size(), error, errorCapacity);
	if (!parser.ParseValue(root, 0) || root.type != JsonValue::Object)
	{
		if (error[0] == '\0')
			snprintf(error, errorCapacity, "the glTF JSON could not be parsed");
		return -1;
	}

	// BufferViews.
	std::vector<BufferView> views;
	const JsonValue* viewArray = root.Find("bufferViews");
	if (viewArray && viewArray->type == JsonValue::Array)
	{
		for (size_t i = 0; i < viewArray->array.size(); i++)
		{
			const JsonValue& v = viewArray->array[i];
			BufferView view;
			view.buffer = (int)v.Find("buffer")->NumberOr(0);
			view.byteOffset = (size_t)v.Find("byteOffset")->NumberOr(0);
			view.byteLength = (size_t)v.Find("byteLength")->NumberOr(0);
			const JsonValue* stride = v.Find("byteStride");
			view.byteStride = stride ? (size_t)stride->NumberOr(0) : 0;
			views.push_back(view);
		}
	}

	// First mesh primitive.
	const JsonValue* meshes = root.Find("meshes");
	const JsonValue* primitive = NULL;
	if (meshes && meshes->type == JsonValue::Array && !meshes->array.empty())
		primitive = GetArrayEntry(meshes->array[0].Find("primitives"), 0);
	const JsonValue* attributes = primitive ? primitive->Find("attributes") : NULL;
	if (!attributes)
	{
		snprintf(error, errorCapacity, "the glTF has no readable mesh primitive");
		return -1;
	}

	int count = 0;
	if (!ReadAccessorFloats(root, bin, views, (int)NumberField(attributes, "POSITION", -1), 3, out->positions, count, error, errorCapacity))
		return -1;
	out->vertexCount = count;
	if (out->vertexCount <= 0 || out->vertexCount > 65535)
	{
		snprintf(error, errorCapacity, "the mesh has an unsupported vertex count (%d)", out->vertexCount);
		return -1;
	}

	const JsonValue* normalAccessor = attributes->Find("NORMAL");
	if (normalAccessor && normalAccessor->type == JsonValue::Number)
		ReadAccessorFloats(root, bin, views, (int)normalAccessor->number, 3, out->normals, count, error, errorCapacity);

	const JsonValue* uvAccessor = attributes->Find("TEXCOORD_0");
	if (uvAccessor && uvAccessor->type == JsonValue::Number)
		ReadAccessorFloats(root, bin, views, (int)uvAccessor->number, 2, out->uvs, count, error, errorCapacity);

	const JsonValue* indices = primitive->Find("indices");
	if (indices && indices->type == JsonValue::Number)
	{
		if (!ReadAccessorIndices(root, bin, views, (int)indices->number, out->indices, error, errorCapacity))
			return -1;
		out->triangleCount = (int)(out->indices.size() / 3);
	}
	else
	{
		out->triangleCount = out->vertexCount / 3;
	}

	// Material.
	const JsonValue* materials = root.Find("materials");
	const JsonValue* material = GetArrayEntry(materials, (int)(primitive->Find("material") ? primitive->Find("material")->NumberOr(-1) : -1));
	if (material)
	{
		const JsonValue* pbr = material->Find("pbrMetallicRoughness");
		if (pbr)
		{
			const JsonValue* factor = pbr->Find("baseColorFactor");
			if (factor && factor->type == JsonValue::Array && factor->array.size() == 4)
				for (int i = 0; i < 4; i++)
					out->baseColorFactor[i] = (float)factor->array[i].NumberOr(1.0);

			out->metallicFactor = (float)(pbr->Find("metallicFactor") ? pbr->Find("metallicFactor")->NumberOr(1.0) : 1.0);
			out->roughnessFactor = (float)(pbr->Find("roughnessFactor") ? pbr->Find("roughnessFactor")->NumberOr(1.0) : 1.0);

			LoadTextureInfo(root, bin, views, pbr->Find("baseColorTexture"), out->baseColorRGBA, out->baseColorWidth, out->baseColorHeight);
			LoadTextureInfo(root, bin, views, pbr->Find("metallicRoughnessTexture"), out->metallicRoughnessRGBA, out->metallicRoughnessWidth, out->metallicRoughnessHeight);
			LoadTextureInfo(root, bin, views, material->Find("normalTexture"), out->normalRGBA, out->normalWidth, out->normalHeight);

			const JsonValue* emissive = material->Find("emissiveFactor");
			if (emissive && emissive->type == JsonValue::Array && emissive->array.size() == 3)
				for (int i = 0; i < 3; i++)
					out->emissiveFactor[i] = (float)emissive->array[i].NumberOr(0.0);
			LoadTextureInfo(root, bin, views, material->Find("emissiveTexture"), out->emissiveRGBA, out->emissiveWidth, out->emissiveHeight);
		}
	}

	return 0;
}

void Gltf_MakeMaterialId(const char* path, const GltfMeshData& mesh, char* id, int idCapacity)
{
	if (!id || idCapacity <= 0)
		return;

	const char* base = path;
	if (base)
	{
		for (const char* c = path; *c; c++)
			if (*c == '/' || *c == '\\')
				base = c + 1;
	}
	if (!base || !*base)
		base = "unknown";

	snprintf(id, idCapacity, "gltf:%s:%.3f_%.3f_%.3f", base,
		mesh.baseColorFactor[0], mesh.baseColorFactor[1], mesh.baseColorFactor[2]);
}
