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

// --- embedded PNG decoding (writer's stored-block subset) --------------------
//
// The multi record's image is the PNG GltfWriter embeds: 8-bit RGBA, no
// interlace, filter type None per scanline, and an IDAT zlib stream built only
// from stored (uncompressed) deflate blocks. This decoder supports exactly that
// subset so the writer and multi loader land together; any other PNG variant
// (dynamic/fixed Huffman, other colour types/bit depths, interlacing, non-None
// filters) is a named error, never a silent drop.

inline unsigned int ReadU32BE(const unsigned char* p)
{
	return ((unsigned int)p[0] << 24) | ((unsigned int)p[1] << 16) | ((unsigned int)p[2] << 8) | (unsigned int)p[3];
}

unsigned int Adler32Of(const unsigned char* data, size_t length)
{
	unsigned int a = 1, b = 0;
	for (size_t i = 0; i < length; i++)
	{
		a = (a + data[i]) % 65521u;
		b = (b + a) % 65521u;
	}
	return (b << 16) | a;
}

// Standard PNG CRC-32 (reflected, polynomial 0xEDB88320) over a chunk's type and
// data, matching the value each PNG chunk stores after its payload.
unsigned int Crc32Of(const unsigned char* data, size_t length)
{
	unsigned int crc = 0xFFFFFFFFu;
	for (size_t i = 0; i < length; i++)
	{
		crc ^= data[i];
		for (int bit = 0; bit < 8; bit++)
			crc = (crc >> 1) ^ (0xEDB88320u & (unsigned int)(-(int)(crc & 1)));
	}
	return crc ^ 0xFFFFFFFFu;
}

// Inflates a zlib stream that uses only stored deflate blocks. Writes a named
// reason and returns false for any other block type or a malformed stream.
bool InflateStoredBlocks(const unsigned char* data, size_t size, std::vector<unsigned char>& out,
	char* error, int errorCapacity)
{
	if (size < 2)
	{
		snprintf(error, errorCapacity, "the PNG zlib stream is truncated");
		return false;
	}
	if ((data[0] & 0x0f) != 8)
	{
		snprintf(error, errorCapacity, "unsupported PNG compression method");
		return false;
	}

	size_t pos = 2;
	bool final = false;
	while (!final)
	{
		if (pos >= size)
		{
			snprintf(error, errorCapacity, "the PNG deflate stream is truncated");
			return false;
		}
		const unsigned char header = data[pos++];
		final = (header & 1) != 0;
		const unsigned int blockType = (header >> 1) & 3;
		if (blockType != 0)
		{
			snprintf(error, errorCapacity,
				"unsupported PNG deflate block type %u (only stored blocks are supported)", blockType);
			return false;
		}
		if (pos + 4 > size)
		{
			snprintf(error, errorCapacity, "the PNG stored block is truncated");
			return false;
		}
		const unsigned int length = (unsigned int)data[pos] | ((unsigned int)data[pos + 1] << 8);
		const unsigned int nlength = (unsigned int)data[pos + 2] | ((unsigned int)data[pos + 3] << 8);
		pos += 4;
		if ((length ^ 0xffffu) != nlength)
		{
			snprintf(error, errorCapacity, "the PNG stored block length is inconsistent");
			return false;
		}
		if (pos + length > size)
		{
			snprintf(error, errorCapacity, "the PNG stored block is truncated");
			return false;
		}
		out.insert(out.end(), data + pos, data + pos + length);
		pos += length;
	}

	if (pos + 4 > size)
	{
		snprintf(error, errorCapacity, "the PNG zlib stream is missing its Adler-32 trailer");
		return false;
	}
	const unsigned int stored = ReadU32BE(data + pos);
	if (stored != Adler32Of(out.data(), out.size()))
	{
		snprintf(error, errorCapacity, "the PNG zlib checksum does not match");
		return false;
	}
	return true;
}

// Decodes a PNG from memory into RGBA8. Returns false, clears `outRGBA` and
// writes a named reason for anything outside the supported subset.
bool DecodePngRgba(const unsigned char* data, size_t size, std::vector<unsigned char>& outRGBA,
	int& width, int& height, char* error, int errorCapacity)
{
	outRGBA.clear();
	width = 0;
	height = 0;

	static const unsigned char signature[8] = { 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A };
	if (size < 8 || memcmp(data, signature, 8) != 0)
	{
		snprintf(error, errorCapacity, "the embedded image is not a PNG");
		return false;
	}

	size_t pos = 8;
	bool haveHeader = false;
	unsigned int imageWidth = 0, imageHeight = 0;
	int bitDepth = 0, colorType = 0, compression = 0, filterMethod = 0, interlace = 0;
	std::vector<unsigned char> idat;

	while (pos + 8 <= size)
	{
		const unsigned int length = ReadU32BE(data + pos);
		const unsigned char* type = data + pos + 4;
		pos += 8;
		if (pos + length + 4 > size)
		{
			snprintf(error, errorCapacity, "a PNG chunk is truncated");
			return false;
		}
		const unsigned char* chunkData = data + pos;
		const unsigned int expectedCrc = ReadU32BE(data + pos + length);
		if (Crc32Of(data + pos - 4, (size_t)length + 4) != expectedCrc)
		{
			snprintf(error, errorCapacity, "a PNG chunk CRC does not match");
			return false;
		}

		if (memcmp(type, "IHDR", 4) == 0)
		{
			if (length < 13)
			{
				snprintf(error, errorCapacity, "the PNG header is truncated");
				return false;
			}
			imageWidth = ReadU32BE(chunkData);
			imageHeight = ReadU32BE(chunkData + 4);
			bitDepth = chunkData[8];
			colorType = chunkData[9];
			compression = chunkData[10];
			filterMethod = chunkData[11];
			interlace = chunkData[12];
			haveHeader = true;
		}
		else if (memcmp(type, "IDAT", 4) == 0)
		{
			idat.insert(idat.end(), chunkData, chunkData + length);
		}
		else if (memcmp(type, "IEND", 4) == 0)
		{
			break;
		}
		pos += length + 4;
	}

	if (!haveHeader)
	{
		snprintf(error, errorCapacity, "the PNG has no header chunk");
		return false;
	}
	if (bitDepth != 8 || colorType != 6 || compression != 0 || filterMethod != 0 || interlace != 0)
	{
		snprintf(error, errorCapacity,
			"unsupported PNG variant (bit depth %d, colour type %d, interlace %d)",
			bitDepth, colorType, interlace);
		return false;
	}
	if (imageWidth == 0 || imageHeight == 0 || imageWidth > 65535 || imageHeight > 65535)
	{
		snprintf(error, errorCapacity, "unsupported PNG dimensions");
		return false;
	}
	if (idat.empty())
	{
		snprintf(error, errorCapacity, "the PNG has no image data");
		return false;
	}

	std::vector<unsigned char> raw;
	if (!InflateStoredBlocks(idat.data(), idat.size(), raw, error, errorCapacity))
		return false;

	const size_t rowBytes = (size_t)imageWidth * 4u;
	if (raw.size() != (rowBytes + 1u) * (size_t)imageHeight)
	{
		snprintf(error, errorCapacity, "the PNG data size does not match its dimensions");
		return false;
	}

	outRGBA.resize(rowBytes * (size_t)imageHeight);
	for (unsigned int y = 0; y < imageHeight; y++)
	{
		const unsigned char* row = raw.data() + (rowBytes + 1u) * (size_t)y;
		if (row[0] != 0)
		{
			outRGBA.clear();
			snprintf(error, errorCapacity, "unsupported PNG filter type %u", row[0]);
			return false;
		}
		memcpy(outRGBA.data() + rowBytes * (size_t)y, row + 1, rowBytes);
	}

	width = (int)imageWidth;
	height = (int)imageHeight;
	return true;
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
	if (count < 0)
	{
		snprintf(error, errorCapacity, "accessor %d has a negative count", accessorIndex);
		return false;
	}
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
	if (count < 0)
	{
		snprintf(error, errorCapacity, "index accessor has a negative count");
		return false;
	}
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

// Reads an optional COLOR_0 accessor into RGBA8 (4 bytes per vertex). Only
// unsigned-byte components (5121) with VEC3 or VEC4 are accepted, matching the
// writer's normalised ubyte VEC4; a VEC3 expands with alpha 255. Any other
// component type or type is a named error, never a silent drop.
bool ReadAccessorColors(const JsonValue& root, const std::vector<unsigned char>& bin,
	const std::vector<BufferView>& views, int accessorIndex, std::vector<unsigned char>& out,
	int& count, char* error, int errorCapacity)
{
	const JsonValue* accessors = root.Find("accessors");
	if (!accessors || accessors->type != JsonValue::Array || accessorIndex < 0 || (size_t)accessorIndex >= accessors->array.size())
	{
		snprintf(error, errorCapacity, "colour accessor %d is missing", accessorIndex);
		return false;
	}
	const JsonValue& accessor = accessors->array[accessorIndex];
	const JsonValue* viewIndex = accessor.Find("bufferView");
	const JsonValue* componentType = accessor.Find("componentType");
	const JsonValue* type = accessor.Find("type");
	if (!viewIndex || !componentType || !type)
	{
		snprintf(error, errorCapacity, "colour accessor %d is not a readable buffer view", accessorIndex);
		return false;
	}
	if ((int)componentType->NumberOr(-1) != 5121)
	{
		snprintf(error, errorCapacity, "colour accessor %d is not unsigned-byte data", accessorIndex);
		return false;
	}

	int components = 0;
	if (type->str == "VEC3") components = 3;
	else if (type->str == "VEC4") components = 4;
	else
	{
		snprintf(error, errorCapacity, "colour accessor %d has unsupported type", accessorIndex);
		return false;
	}

	const int view = (int)viewIndex->NumberOr(-1);
	if (view < 0 || (size_t)view >= views.size())
	{
		snprintf(error, errorCapacity, "colour accessor %d points to a missing bufferView", accessorIndex);
		return false;
	}

	count = (int)NumberField(&accessor, "count", 0);
	if (count < 0)
	{
		snprintf(error, errorCapacity, "colour accessor %d has a negative count", accessorIndex);
		return false;
	}
	const size_t offset = views[view].byteOffset + (size_t)NumberField(&accessor, "byteOffset", 0);
	const size_t elementSize = (size_t)components;
	const size_t stride = views[view].byteStride ? views[view].byteStride : elementSize;

	out.resize((size_t)count * 4u);
	for (int i = 0; i < count; i++)
	{
		const size_t base = offset + stride * (size_t)i;
		if (base + elementSize > bin.size())
		{
			snprintf(error, errorCapacity, "colour accessor %d reads outside the binary chunk", accessorIndex);
			return false;
		}
		out[(size_t)i * 4 + 0] = bin[base + 0];
		out[(size_t)i * 4 + 1] = bin[base + 1];
		out[(size_t)i * 4 + 2] = bin[base + 2];
		out[(size_t)i * 4 + 3] = components == 4 ? bin[base + 3] : 255;
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

// Composes the first node's placement into a column-major matrix. glTF nodes
// are either a full `matrix` or a translation/rotation/scale triple; a node with
// neither is the identity. The placement is retained so source geometry and its
// transform are compared as one record.
void ReadNodeTransform(const JsonValue& root, float out[16])
{
	for (int i = 0; i < 16; i++)
		out[i] = (i % 5 == 0) ? 1.0f : 0.0f;

	const JsonValue* node = GetArrayEntry(root.Find("nodes"), 0);
	if (!node)
		return;

	const JsonValue* matrix = node->Find("matrix");
	if (matrix && matrix->type == JsonValue::Array && matrix->array.size() == 16)
	{
		for (int i = 0; i < 16; i++)
			out[i] = (float)matrix->array[i].NumberOr(0.0);
		return;
	}

	float translation[3] = { 0.0f, 0.0f, 0.0f };
	float rotation[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	float scale[3] = { 1.0f, 1.0f, 1.0f };
	const JsonValue* t = node->Find("translation");
	const JsonValue* r = node->Find("rotation");
	const JsonValue* s = node->Find("scale");
	for (int i = 0; t && t->type == JsonValue::Array && i < 3 && i < (int)t->array.size(); i++)
		translation[i] = (float)t->array[i].NumberOr(0.0);
	for (int i = 0; r && r->type == JsonValue::Array && i < 4 && i < (int)r->array.size(); i++)
		rotation[i] = (float)r->array[i].NumberOr(i == 3 ? 1.0 : 0.0);
	for (int i = 0; s && s->type == JsonValue::Array && i < 3 && i < (int)s->array.size(); i++)
		scale[i] = (float)s->array[i].NumberOr(1.0);

	const float x = rotation[0], y = rotation[1], z = rotation[2], w = rotation[3];
	const float xx = x * x, yy = y * y, zz = z * z;
	const float xy = x * y, xz = x * z, yz = y * z;
	const float wx = w * x, wy = w * y, wz = w * z;

	// Rotation matrix (column-major), scaled per column, translation in the
	// last column.
	float m[16] = {
		1 - 2 * (yy + zz), 2 * (xy + wz),     2 * (xz - wy),     0,
		2 * (xy - wz),     1 - 2 * (xx + zz), 2 * (yz + wx),     0,
		2 * (xz + wy),     2 * (yz - wx),     1 - 2 * (xx + yy), 0,
		translation[0],    translation[1],    translation[2],    1,
	};
	for (int col = 0; col < 3; col++)
		for (int row = 0; row < 3; row++)
			m[col * 4 + row] *= scale[col];

	for (int i = 0; i < 16; i++)
		out[i] = m[i];
}

// --- shared loader helpers --------------------------------------------------

bool ReadGlbContainer(const char* path, std::string& jsonText, std::vector<unsigned char>& bin,
	char* error, int errorCapacity)
{
	FILE* file = fopen(path, "rb");
	if (!file)
	{
		snprintf(error, errorCapacity, "cannot open %s", path);
		return false;
	}
	fseek(file, 0, SEEK_END);
	long fileSize = ftell(file);
	fseek(file, 0, SEEK_SET);
	if (fileSize <= 12)
	{
		fclose(file);
		snprintf(error, errorCapacity, "%s is too small to be a glTF/GLB file", path);
		return false;
	}

	std::vector<unsigned char> bytes((size_t)fileSize);
	if (fread(bytes.data(), 1, bytes.size(), file) != bytes.size())
	{
		fclose(file);
		snprintf(error, errorCapacity, "cannot read %s", path);
		return false;
	}
	fclose(file);

	if (ReadU32(bytes.data()) != 0x46546C67)
	{
		snprintf(error, errorCapacity, "only binary .glb files are supported");
		return false;
	}

	const unsigned int version = ReadU32(bytes.data() + 4);
	if (version != 2)
	{
		snprintf(error, errorCapacity, "GLB version %u is not supported", version);
		return false;
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
			return false;
		}
		if (chunkType == 0x4E4F534A) // JSON
			jsonText.assign((const char*)bytes.data() + offset, chunkLength);
		else if (chunkType == 0x004E4942) // BIN
			bin.assign(bytes.begin() + offset, bytes.begin() + offset + chunkLength);
		offset += chunkLength;
	}

	if (jsonText.empty())
	{
		snprintf(error, errorCapacity, "the GLB has no JSON chunk");
		return false;
	}
	return true;
}

bool ParseRoot(const std::string& jsonText, JsonValue& root, char* error, int errorCapacity)
{
	JsonParser parser(jsonText.c_str(), (int)jsonText.size(), error, errorCapacity);
	if (!parser.ParseValue(root, 0) || root.type != JsonValue::Object)
	{
		if (error[0] == '\0')
			snprintf(error, errorCapacity, "the glTF JSON could not be parsed");
		return false;
	}
	return true;
}

void BuildBufferViews(const JsonValue& root, std::vector<BufferView>& views)
{
	const JsonValue* viewArray = root.Find("bufferViews");
	if (!viewArray || viewArray->type != JsonValue::Array)
		return;
	for (size_t i = 0; i < viewArray->array.size(); i++)
	{
		const JsonValue& v = viewArray->array[i];
		BufferView view;
		view.buffer = (int)NumberField(&v, "buffer", 0);
		view.byteOffset = (size_t)NumberField(&v, "byteOffset", 0);
		view.byteLength = (size_t)NumberField(&v, "byteLength", 0);
		view.byteStride = (size_t)NumberField(&v, "byteStride", 0);
		views.push_back(view);
	}
}

// True when any object key in this JSON subtree ends in "Texture". Every glTF
// texture reference property uses that suffix (baseColorTexture, diffuseTexture
// in KHR_materials_pbrSpecularGlossiness, specularTexture in
// KHR_materials_specular, ...), so this finds references inside a material's
// `extensions` without enumerating each extension. Recurses through nested
// objects and arrays.
bool ContainsTextureKey(const JsonValue& value)
{
	if (value.type == JsonValue::Object)
	{
		for (size_t i = 0; i < value.object.size(); i++)
		{
			const std::string& key = value.object[i].first;
			if (key.size() >= 7 && key.compare(key.size() - 7, 7, "Texture") == 0)
				return true;
			if (ContainsTextureKey(value.object[i].second))
				return true;
		}
	}
	else if (value.type == JsonValue::Array)
	{
		for (size_t i = 0; i < value.array.size(); i++)
			if (ContainsTextureKey(value.array[i]))
				return true;
	}
	return false;
}

// True when a material references a texture the multi record cannot represent
// (anything other than the optional base-colour image it now holds).
bool MaterialReferencesUnsupportedTexture(const JsonValue& material)
{
	const JsonValue* pbr = material.Find("pbrMetallicRoughness");
	if (pbr && pbr->Find("metallicRoughnessTexture"))
		return true;
	if (material.Find("normalTexture") || material.Find("emissiveTexture") || material.Find("occlusionTexture"))
		return true;

	// A texture reference expressed through an extension (e.g. the spec-gloss
	// diffuse/specularGlossiness maps) must be rejected too, so an external GLB
	// cannot silently drop it.
	const JsonValue* extensions = material.Find("extensions");
	return extensions && ContainsTextureKey(*extensions);
}

// Decodes the base-colour image referenced by a glTF textureInfo into RGBA8.
// The multi record only holds the writer's embedded stored-block PNG; a data:
// URI, an external file, a missing image or an unsupported PNG variant is a
// named reason, never a silent drop.
bool DecodeBaseColorTexture(const JsonValue& root, const std::vector<unsigned char>& bin,
	const std::vector<BufferView>& views, const JsonValue* textureInfo,
	std::vector<unsigned char>& outRGBA, int& outWidth, int& outHeight,
	char* error, int errorCapacity)
{
	outRGBA.clear();
	outWidth = 0;
	outHeight = 0;

	if (!textureInfo)
	{
		snprintf(error, errorCapacity, "the base-colour texture reference is missing");
		return false;
	}

	const JsonValue* textures = root.Find("textures");
	if (!textures || textures->type != JsonValue::Array)
	{
		snprintf(error, errorCapacity, "the base-colour texture is not defined");
		return false;
	}
	const JsonValue* texture = GetArrayEntry(textures, (int)NumberField(textureInfo, "index", -1));
	if (!texture)
	{
		snprintf(error, errorCapacity, "the base-colour texture index is missing");
		return false;
	}

	const JsonValue* source = texture->Find("source");
	const JsonValue* image = source ? GetArrayEntry(root.Find("images"), (int)source->NumberOr(-1)) : NULL;
	if (!image)
	{
		snprintf(error, errorCapacity, "the base-colour texture image is missing");
		return false;
	}

	const JsonValue* imageView = image->Find("bufferView");
	if (!imageView || imageView->type != JsonValue::Number)
	{
		snprintf(error, errorCapacity, "the base-colour texture image is not embedded in the binary chunk");
		return false;
	}
	const int viewIndex = (int)imageView->number;
	if (viewIndex < 0 || (size_t)viewIndex >= views.size())
	{
		snprintf(error, errorCapacity, "the base-colour texture image points to a missing bufferView");
		return false;
	}
	const BufferView& view = views[viewIndex];
	if (view.byteOffset + view.byteLength > bin.size())
	{
		snprintf(error, errorCapacity, "the base-colour texture image reads outside the binary chunk");
		return false;
	}

	const JsonValue* mimeType = image->Find("mimeType");
	if (mimeType && mimeType->type == JsonValue::String && mimeType->str != "image/png")
	{
		snprintf(error, errorCapacity, "unsupported base-colour texture mime type '%s'", mimeType->str.c_str());
		return false;
	}

	return DecodePngRgba(bin.data() + view.byteOffset, view.byteLength, outRGBA, outWidth, outHeight,
		error, errorCapacity);
}

// Reads one primitive's geometry and material factors. `strict` adds the
// bounds/count checks the multi-primitive consumer needs (attribute counts agree
// with POSITION, indices are in range and form whole triangles); the single
// historical loader keeps its tolerant contract by passing false.
// `loadBaseColorImage` is set by the multi loader: it decodes the material's
// embedded base-colour PNG into the primitive and rejects any other texture
// reference. The single loader passes false and retains images through WIC.
bool LoadPrimitiveCore(const JsonValue& root, const std::vector<unsigned char>& bin,
	const std::vector<BufferView>& views, int primitiveIndex, const JsonValue& primitive,
	bool strict, bool loadBaseColorImage, GltfPrimitiveData& out, int& materialIndex,
	char* error, int errorCapacity)
{
	out = GltfPrimitiveData();
	materialIndex = -1;

	char detail[192] = "";

	const JsonValue* attributes = primitive.Find("attributes");
	if (!attributes)
	{
		snprintf(error, errorCapacity, "primitive %d has no attributes", primitiveIndex);
		return false;
	}

	int count = 0;
	if (!ReadAccessorFloats(root, bin, views, (int)NumberField(attributes, "POSITION", -1), 3,
			out.positions, count, detail, sizeof(detail)))
	{
		snprintf(error, errorCapacity, "primitive %d: %s", primitiveIndex, detail);
		return false;
	}
	out.vertexCount = count;
	if (out.vertexCount <= 0 || out.vertexCount > 65535)
	{
		snprintf(error, errorCapacity, "primitive %d has an unsupported vertex count (%d)", primitiveIndex, out.vertexCount);
		return false;
	}

	const JsonValue* normalAccessor = attributes->Find("NORMAL");
	if (normalAccessor && normalAccessor->type == JsonValue::Number)
	{
		int normalCount = 0;
		if (!ReadAccessorFloats(root, bin, views, (int)normalAccessor->number, 3,
				out.normals, normalCount, detail, sizeof(detail)))
		{
			snprintf(error, errorCapacity, "primitive %d: %s", primitiveIndex, detail);
			return false;
		}
		if (strict && normalCount != out.vertexCount)
		{
			snprintf(error, errorCapacity, "primitive %d has a NORMAL count that disagrees with POSITION", primitiveIndex);
			return false;
		}
	}

	const JsonValue* uvAccessor = attributes->Find("TEXCOORD_0");
	if (uvAccessor && uvAccessor->type == JsonValue::Number)
	{
		int uvCount = 0;
		if (!ReadAccessorFloats(root, bin, views, (int)uvAccessor->number, 2,
				out.uvs, uvCount, detail, sizeof(detail)))
		{
			snprintf(error, errorCapacity, "primitive %d: %s", primitiveIndex, detail);
			return false;
		}
		if (strict && uvCount != out.vertexCount)
		{
			snprintf(error, errorCapacity, "primitive %d has a TEXCOORD_0 count that disagrees with POSITION", primitiveIndex);
			return false;
		}
	}

	const JsonValue* colorAccessor = attributes->Find("COLOR_0");
	const JsonValue* shadeAccessor = attributes->Find("_RED2_WORLD_SHADE");
	if (shadeAccessor)
	{
		int shadeCount = 0;
		if (shadeAccessor->type != JsonValue::Number ||
			!ReadAccessorFloats(root, bin, views, (int)shadeAccessor->number, 1,
				out.worldShadeIndices, shadeCount, detail, sizeof(detail)))
		{
			snprintf(error, errorCapacity, "primitive %d has an unreadable _RED2_WORLD_SHADE accessor", primitiveIndex);
			return false;
		}
		if (shadeCount != out.vertexCount)
		{
			snprintf(error, errorCapacity, "primitive %d has a world shade count that disagrees with POSITION", primitiveIndex);
			return false;
		}
		for (int v = 0; v < shadeCount; ++v)
		{
			const float shade = out.worldShadeIndices[v];
			if (!(shade >= -1.0f && shade <= 31.0f) || shade != (float)(int)shade)
			{
				snprintf(error, errorCapacity, "primitive %d has an invalid world shade index", primitiveIndex);
				return false;
			}
		}
	}
	if (colorAccessor && colorAccessor->type == JsonValue::Number)
	{
		int colorCount = 0;
		if (!ReadAccessorColors(root, bin, views, (int)colorAccessor->number,
				out.colors, colorCount, detail, sizeof(detail)))
		{
			snprintf(error, errorCapacity, "primitive %d: %s", primitiveIndex, detail);
			return false;
		}
		if (strict && colorCount != out.vertexCount)
		{
			snprintf(error, errorCapacity, "primitive %d has a COLOR_0 count that disagrees with POSITION", primitiveIndex);
			return false;
		}
	}

	const JsonValue* indices = primitive.Find("indices");
	if (indices && indices->type == JsonValue::Number)
	{
		if (!ReadAccessorIndices(root, bin, views, (int)indices->number, out.indices, detail, sizeof(detail)))
		{
			snprintf(error, errorCapacity, "primitive %d: %s", primitiveIndex, detail);
			return false;
		}
		if (strict)
		{
			if (out.indices.size() % 3 != 0)
			{
				snprintf(error, errorCapacity, "primitive %d has an index count that is not a whole number of triangles", primitiveIndex);
				return false;
			}
			for (size_t k = 0; k < out.indices.size(); k++)
			{
				if (out.indices[k] >= (unsigned int)out.vertexCount)
				{
					snprintf(error, errorCapacity, "primitive %d has an index outside the vertex range", primitiveIndex);
					return false;
				}
			}
		}
		out.triangleCount = (int)(out.indices.size() / 3);
	}
	else
	{
		if (strict && out.vertexCount % 3 != 0)
		{
			snprintf(error, errorCapacity, "primitive %d has a vertex count that is not a whole number of triangles", primitiveIndex);
			return false;
		}
		out.triangleCount = out.vertexCount / 3;
	}

	materialIndex = (int)NumberField(&primitive, "material", -1);
	const JsonValue* material = GetArrayEntry(root.Find("materials"), materialIndex);
	if (material)
	{
		const JsonValue* pbr = material->Find("pbrMetallicRoughness");
		if (pbr)
		{
			const JsonValue* factor = pbr->Find("baseColorFactor");
			if (factor && factor->type == JsonValue::Array && factor->array.size() == 4)
				for (int i = 0; i < 4; i++)
					out.baseColorFactor[i] = (float)factor->array[i].NumberOr(1.0);

			out.metallicFactor = (float)(pbr->Find("metallicFactor") ? pbr->Find("metallicFactor")->NumberOr(1.0) : 1.0);
			out.roughnessFactor = (float)(pbr->Find("roughnessFactor") ? pbr->Find("roughnessFactor")->NumberOr(1.0) : 1.0);
		}
		if (loadBaseColorImage)
		{
			const JsonValue* base = pbr ? pbr->Find("baseColorTexture") : NULL;
			if (base)
			{
				char textureDetail[192] = "";
				if (!DecodeBaseColorTexture(root, bin, views, base, out.imageRgba, out.imageWidth, out.imageHeight,
						textureDetail, sizeof(textureDetail)))
				{
					snprintf(error, errorCapacity, "primitive %d base-colour texture: %s", primitiveIndex, textureDetail);
					return false;
				}
			}
			if (MaterialReferencesUnsupportedTexture(*material))
			{
				snprintf(error, errorCapacity, "primitive %d references a texture that the multi-primitive record cannot hold", primitiveIndex);
				return false;
			}
		}
	}
	return true;
}
} // namespace

int Gltf_LoadFile(const char* path, GltfMeshData* out, char* error, int errorCapacity)
{
	if (!path || !out || !error || errorCapacity <= 0)
		return -1;
	error[0] = '\0';
	*out = GltfMeshData();

	std::string jsonText;
	std::vector<unsigned char> bin;
	if (!ReadGlbContainer(path, jsonText, bin, error, errorCapacity))
		return -1;

	JsonValue root;
	if (!ParseRoot(jsonText, root, error, errorCapacity))
		return -1;

	std::vector<BufferView> views;
	BuildBufferViews(root, views);

	// First mesh primitive only: the single record is unchanged for existing
	// callers, including its tolerant (non-strict) attribute reading.
	const JsonValue* meshes = root.Find("meshes");
	const JsonValue* primitive = NULL;
	if (meshes && meshes->type == JsonValue::Array && !meshes->array.empty())
		primitive = GetArrayEntry(meshes->array[0].Find("primitives"), 0);
	if (!primitive)
	{
		snprintf(error, errorCapacity, "the glTF has no readable mesh primitive");
		return -1;
	}

	GltfPrimitiveData core;
	int materialIndex = -1;
	if (!LoadPrimitiveCore(root, bin, views, 0, *primitive, false, false, core, materialIndex, error, errorCapacity))
		return -1;

	out->positions.swap(core.positions);
	out->normals.swap(core.normals);
	out->uvs.swap(core.uvs);
	out->colors.swap(core.colors);
	out->worldShadeIndices.swap(core.worldShadeIndices);
	out->indices.swap(core.indices);
	for (int i = 0; i < 4; i++)
		out->baseColorFactor[i] = core.baseColorFactor[i];
	out->metallicFactor = core.metallicFactor;
	out->roughnessFactor = core.roughnessFactor;
	out->vertexCount = core.vertexCount;
	out->triangleCount = core.triangleCount;

	// The single record still retains an embedded image and the emissive factor.
	const JsonValue* material = GetArrayEntry(root.Find("materials"), materialIndex);
	if (material)
	{
		const JsonValue* pbr = material->Find("pbrMetallicRoughness");
		if (pbr)
		{
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

	ReadNodeTransform(root, out->transform);

	return 0;
}

int Gltf_LoadFileMulti(const char* path, GltfMultiMeshData* out, char* error, int errorCapacity)
{
	if (!path || !out || !error || errorCapacity <= 0)
		return -1;
	error[0] = '\0';
	*out = GltfMultiMeshData();

	std::string jsonText;
	std::vector<unsigned char> bin;
	if (!ReadGlbContainer(path, jsonText, bin, error, errorCapacity))
		return -1;

	JsonValue root;
	if (!ParseRoot(jsonText, root, error, errorCapacity))
		return -1;

	std::vector<BufferView> views;
	BuildBufferViews(root, views);

	const JsonValue* meshes = root.Find("meshes");
	const JsonValue* primitives = NULL;
	if (meshes && meshes->type == JsonValue::Array && !meshes->array.empty())
		primitives = meshes->array[0].Find("primitives");
	if (!primitives || primitives->type != JsonValue::Array || primitives->array.empty())
	{
		snprintf(error, errorCapacity, "the glTF has no readable mesh primitive");
		return -1;
	}

	GltfMultiMeshData result;
	result.primitives.reserve(primitives->array.size());
	for (size_t i = 0; i < primitives->array.size(); i++)
	{
		GltfPrimitiveData primitive;
		int materialIndex = -1;
		if (!LoadPrimitiveCore(root, bin, views, (int)i, primitives->array[i], true, true,
				primitive, materialIndex, error, errorCapacity))
			return -1; // `out` was reset above: no partial output
		result.primitives.push_back(primitive);
	}

	ReadNodeTransform(root, result.transform);
	*out = result;
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
