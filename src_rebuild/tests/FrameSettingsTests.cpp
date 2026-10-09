// Production settings/persistence with deterministic SDL/window stand-ins.
#include <SDL.h>
#include "driver2.h"
#include "C/camera.h"
#include "PsyX/PsyX_public.h"
#include "../utils/DeveloperGraphicsSettings.h"
#include "../utils/DeveloperModernMesh.h"
#include "../utils/HdTextureOverrides.h"
#include "../utils/ProjectClock.h"
#include <string>
#include <stdio.h>
#include <string.h>

int g_cfg_swapInterval = 1, g_cfg_pgxpZBuffer = 1, g_cfg_bilinearFiltering = 1;
int g_cfg_pgxpTextureCorrection = 1, g_cfg_overrideProportionalAlpha = 0;
int gDrawDistance = 1800, gDisplayDrawStats = 0, gEnableDlights = 1;
int gWidescreenOverlayAlign = 1, gFastLoadingScreens = 1;
short gCameraDefaultScrZ = 256;
static int width = 1280, height = 720, fullscreen = 0;
static int modern = 0, textures = 1, organized = 0, baseColours = 1;
extern "C" SDL_Window* PsyX_GetSDLWindow() { return (SDL_Window*)1; }
extern "C" Uint32 SDLCALL SDL_GetWindowFlags(SDL_Window*) { return fullscreen ? SDL_WINDOW_FULLSCREEN : 0; }
extern "C" void SDLCALL SDL_GetWindowSize(SDL_Window*, int* w, int* h) { *w = width; *h = height; }
extern "C" int PsyX_ApplyWindowMode(int f, int w, int h, int*, int*)
{
	fullscreen = f; width = w; height = h; return 1;
}
extern "C" Uint64 SDLCALL SDL_GetPerformanceCounter() { return 100; }
extern "C" Uint64 SDLCALL SDL_GetPerformanceFrequency() { return 1000003; }
extern "C" void SDLCALL SDL_Delay(Uint32) {}
void DeveloperModernMesh_SetEnabled(int v) { modern = v != 0; }
int DeveloperModernMesh_GetEnabled() { return modern; }
void HdTextureOverrides_GetDiagnostics(HdTextureOverrideDiagnostics* d) { d->enabled = textures; }
void HdTextureOverrides_SetEnabled(int v) { textures = v != 0; }
void HdTextureOverrides_SetOrganizedExport(int v) { organized = v != 0; }
int HdTextureOverrides_IsOrganizedExportEnabled() { return organized; }
void HdTextureOverrides_SetBaseColourExport(int v) { baseColours = v != 0; }
int HdTextureOverrides_IsBaseColourExportEnabled() { return baseColours; }

static int checks = 0, failures = 0;
static void Check(bool ok, const char* label)
{
	++checks;
	if (!ok) { ++failures; printf("FAIL: %s\n", label); }
}
static void Write(const char* path, const char* text)
{
	FILE* f = fopen(path, "wb");
	if (!f) { Check(false, "fixture write opened"); return; }
	fputs(text, f); fclose(f);
}
static std::string Read(const char* path)
{
	std::string text;
	FILE* f = fopen(path, "rb");
	if (!f) return text;
	char line[512];
	while (fgets(line, sizeof(line), f)) text += line;
	fclose(f); return text;
}

int main()
{
	const int active = ProjectClock_GetVideoMode();
	Check(ProjectClock_GetPresentationFps() == (active == PROJECT_VIDEO_PAL ? 25 : 30), "fresh regional FPS default");
	Write("config.ini", "# shipped configuration stays untouched\nvsync=1\n");
	const std::string shipped = Read("config.ini");
	Write("developer_graphics.ini", "# user comment\nschemaVersion=8\nwindowWidth=1280\nwindowHeight=720\nmodernRenderer=0\nfutureKey=77\nvideoMode=1\n");
	Check(DeveloperGraphicsSettings_LoadAndApply(), "loads schema 8 without requiring new keys");
	Check(ProjectClock_GetPresentationFps() == 25, "missing FPS uses saved PAL preference's default");
	Check(ProjectClock_GetVideoMode() == active && ProjectClock_GetPreferredVideoMode() == PROJECT_VIDEO_PAL,
		"load only selects preference until startup activates it");
	ProjectClock_SetPresentationFps(144);
	g_cfg_swapInterval = 0;
	width = 1920; height = 1080; modern = 1; // unconfirmed display/session edits
	const std::string previous = Read("developer_graphics.ini");
	Check(DeveloperGraphicsSettings_SaveFrameSettings(), "autosave succeeds");
	const std::string saved = Read("developer_graphics.ini");
	Check(saved.find("presentationFps=144\n") != std::string::npos && saved.find("vsync=0\n") != std::string::npos,
		"autosave writes live frame controls");
	Check(saved.find("windowWidth=1280\n") != std::string::npos && saved.find("windowHeight=720\n") != std::string::npos,
		"autosave never persists provisional resolution");
	Check(saved.find("modernRenderer=0\n") != std::string::npos, "autosave preserves unrelated session preferences");
	Check(saved.find("# user comment\n") != std::string::npos && saved.find("futureKey=77\n") != std::string::npos,
		"autosave preserves comments and future keys");
	Check(Read("developer_graphics.ini.bak") == previous, "atomic replacement rotates previous contents");
	Check(Read("config.ini") == shipped, "panel persistence never modifies shipped config.ini");
	ProjectClock_SetPresentationFps(30); g_cfg_swapInterval = 1;
	Check(DeveloperGraphicsSettings_LoadAndApply(), "saved preferences reload");
	Check(ProjectClock_GetPresentationFps() == 144 && g_cfg_swapInterval == 0, "FPS and VSync survive reload");
	ProjectClock_ApplyStartupVideoMode(); ProjectClock_Init();
	Check(ProjectClock_GetVideoMode() == PROJECT_VIDEO_PAL && ProjectClock_GetTickPeriodMicros() == 40000,
		"same executable activates PAL on next startup");
	ProjectClock_SetPreferredVideoMode(PROJECT_VIDEO_NTSC);
	Check(DeveloperGraphicsSettings_SaveFrameSettings(), "confirmed next-startup NTSC preference saves");
	Check(ProjectClock_GetVideoMode() == PROJECT_VIDEO_PAL, "saved NTSC preference does not retime PAL session");
	Check(Read("developer_graphics.ini").find("videoMode=0\n") != std::string::npos, "next-startup preference is persisted");
	Write("developer_graphics.ini", "presentationFps=-1\nvideoMode=99\nvsync=0\n");
	Check(DeveloperGraphicsSettings_LoadAndApply(), "edited configuration loads");
	Check(ProjectClock_GetPresentationFps() == 15 && ProjectClock_GetPreferredVideoMode() == PROJECT_VIDEO_NTSC,
		"invalid settings are sanitized before use");
	Check(DeveloperGraphicsSettings_SaveRuntime(), "full explicit save still succeeds");
	Check(Read("developer_graphics.ini").find("schemaVersion=9\n") != std::string::npos, "new schema is written");
	printf("%d checks, %d failures (frame settings)\n", checks, failures);
	return failures ? 1 : 0;
}
