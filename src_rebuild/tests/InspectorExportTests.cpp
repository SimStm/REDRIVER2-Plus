// Standalone Windows regression test; no game assets or GL context required.
// Include the implementation to exercise the same private WIC encoder.
#include "../utils/HdTextureOverrides.cpp"

// Renderer stubs: export tests supply synthetic VRAM and never create GL objects.
unsigned int PsyX_CreateRGBATexture(int, int, const unsigned char*) { return 1; }
void PsyX_DestroyRGBATexture(unsigned int) {}
int PsyX_RegisterTextureOverride(const PsyXTextureOverride*) { return 1; }
void PsyX_RemoveTextureOverride(int) {}
void PsyX_Inspector_ClearSelection() {}
void PsyX_SetTextureOverridesEnabled(int) {}
void PsyX_GetTextureOverrideStats(PsyXTextureOverrideStats* stats) { memset(stats, 0, sizeof(*stats)); }
void GR_ReadVRAM(unsigned short* dst, int, int, int width, int height)
{
	for (int i = 0; i < width * height; ++i) dst[i] = 0x001f;
}

static int failures = 0;
static void Check(bool passed, const char* description)
{
	printf("%s: %s\n", passed ? "PASS" : "FAIL", description);
	if (!passed) ++failures;
}

int main()
{
	unsigned char pixels[4 * 4 * 4];
	for (int i = 0; i < 16; ++i)
	{
		pixels[i * 4] = 10; pixels[i * 4 + 1] = 40;
		pixels[i * 4 + 2] = 200; pixels[i * 4 + 3] = (unsigned char)(i * 16);
	}
	const bool saved = SavePngRgba("test.png", pixels, 4, 4);
	WIN32_FILE_ATTRIBUTE_DATA file = {};
	GetFileAttributesExA("test.png", GetFileExInfoStandard, &file);
	Check(saved && file.nFileSizeLow > 0, "WIC PNG encoder creates a nonempty file");
	unsigned char* decoded = NULL;
	int width = 0, height = 0;
	Check(LoadPngRgba("test.png", &decoded, &width, &height) && width == 4 && height == 4 &&
		!memcmp(pixels, decoded, sizeof(pixels)), "RGBA -> BGRA -> PNG -> RGBA preserves all channels including alpha");
	free(decoded);
	char status[512];
	Check(HdTextureOverrides_ExportInspectorReport("regression", "first report", status, sizeof(status)), "report first export");
	Check(HdTextureOverrides_ExportInspectorReport("regression", "replacement report", status, sizeof(status)), "report repeated export");
	int bytes = 0;
	char* report = ReadTextFile("mods/regression/assets/inspector/selection.txt", &bytes);
	Check(report && !strcmp(report, "replacement report"), "report replacement contains the new content");
	free(report);
	const char* destination = "mods/regression/assets/inspector/RED_p1_i1.png";
	Check(HdTextureOverrides_ExportTexture(256, 0, 0, 0, 4, 4, "RED", 1, 1, "regression", status, sizeof(status)), "VRAM texture first export");
	Check(HdTextureOverrides_ExportTexture(256, 0, 0, 0, 4, 4, "RED", 1, 1, "regression", status, sizeof(status)), "VRAM texture repeated export");
	decoded = NULL;
	Check(LoadPngRgba(destination, &decoded, &width, &height) && width == 4 && height == 4 &&
		decoded[0] == 255 && decoded[1] == 0 && decoded[2] == 0 && decoded[3] == 255, "exported original texture decodes to synthetic red VRAM pixels");
	free(decoded);
	char temporary[MAX_PATH];
	Check(InspectorExport_Temporary(destination, temporary, status, sizeof(status)) &&
		!InspectorExport_Commit(temporary, destination, status, sizeof(status)), "empty temporary export cannot replace a completed PNG");
	decoded = NULL;
	Check(LoadPngRgba(destination, &decoded, &width, &height), "previous PNG remains readable after failed replacement");
	free(decoded);
	Check(!HdTextureOverrides_ExportTexture(256, 0, 255, 0, 4, 4, "RED", 1, 1, "regression", status, sizeof(status)), "out-of-range source rejected");
	Check(!HdTextureOverrides_ExportInspectorReport("../outside", "invalid", status, sizeof(status)), "unsafe mod id rejected");
	Check(InspectorExport_WriteText("car.obj", "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n", status, sizeof(status)) &&
		InspectorExport_WriteText("car.obj", "v 0 0 1\nv 1 0 1\nv 0 1 1\nf 1 2 3\n", status, sizeof(status)), "OBJ text writer supports repeat export");
	printf("%d failures\n", failures);
	return failures ? 1 : 0;
}
