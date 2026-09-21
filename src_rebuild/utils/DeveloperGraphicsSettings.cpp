#include "DeveloperGraphicsSettings.h"
#include "DeveloperModernMesh.h"
#include "DeveloperSettingsFile.h"
#include "HdTextureOverrides.h"

#include "driver2.h"
#include "C/camera.h"

#include <stdio.h>
#include <string.h>

#include "PsyX/PsyX_public.h"

extern int gDrawDistance;
extern int gDisplayDrawStats;
extern int gEnableDlights;
extern int gWidescreenOverlayAlign;
extern int gFastLoadingScreens;

namespace
{
const char* const kSettingsFilename = "developer_graphics.ini";

// Every key the panel owns, in the order new keys are appended. `schemaVersion`
// is written like any other key so a future version can migrate the file.
const char* const kSettingKeys[] =
{
	"schemaVersion",
	"bilinearFiltering",
	"pgxpTextureMapping",
	"pgxpZBuffer",
	"vsync",
	"drawDistance",
	"fieldOfView",
	"showLegacyStats",
	"hdTextureOverrides",
	"organizeTextureExports",
	"exportBaseColours",
	"overrideProportionalAlpha",
	"modernRenderer",
	"dynamicLights",
	"widescreenOverlays",
	"fastLoadingScreens",
	"fullscreen",
	"windowWidth",
	"windowHeight",
};
const int kSettingKeyCount = (int)(sizeof(kSettingKeys) / sizeof(kSettingKeys[0]));

const char* const kSettingComments[] =
{
	"# REDRIVER2 developer graphics settings",
	"# This file is managed separately and never modifies config.ini.",
};
const int kSettingCommentCount = (int)(sizeof(kSettingComments) / sizeof(kSettingComments[0]));

// Shipped defaults from config.ini [render], used when the app started in
// fullscreen so no windowed size has been observed yet.
const int kDefaultWindowWidth = 1280;
const int kDefaultWindowHeight = 720;

// The window mode is not a variable anything owns: it lives in the SDL window.
// These cache the last observed state so it can be reported and persisted, and
// keep the last *windowed* size while running in fullscreen, which is the size
// to return to and to save.
int s_displayFullscreen = 0;
int s_windowedWidth = kDefaultWindowWidth;
int s_windowedHeight = kDefaultWindowHeight;

void RefreshDisplayState()
{
	SDL_Window* window = PsyX_GetSDLWindow();
	if (!window)
		return;

	s_displayFullscreen = (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) != 0;

	int width = 0;
	int height = 0;
	SDL_GetWindowSize(window, &width, &height);
	if (!s_displayFullscreen && width > 0 && height > 0)
	{
		s_windowedWidth = width;
		s_windowedHeight = height;
	}
}

int Clamp(int value, int minimum, int maximum)
{
	return value < minimum ? minimum : (value > maximum ? maximum : value);
}

}

DeveloperGraphicsSettings DeveloperGraphicsSettings_ReadRuntime()
{
	DeveloperGraphicsSettings settings = {};
	settings.bilinearFiltering = g_cfg_bilinearFiltering != 0;
	settings.pgxpTextureMapping = g_cfg_pgxpTextureCorrection != 0;
	settings.pgxpZBuffer = g_cfg_pgxpZBuffer != 0;
	settings.vsync = g_cfg_swapInterval != 0;
	settings.drawDistance = gDrawDistance;
	settings.fieldOfView = gCameraDefaultScrZ;

	settings.showLegacyStats = gDisplayDrawStats != 0;
	HdTextureOverrideDiagnostics hdTextures = {};
	HdTextureOverrides_GetDiagnostics(&hdTextures);
	settings.hdTextureOverrides = hdTextures.enabled;
	settings.organizeTextureExports = HdTextureOverrides_IsOrganizedExportEnabled();
	settings.exportBaseColours = HdTextureOverrides_IsBaseColourExportEnabled();
	settings.overrideProportionalAlpha = g_cfg_overrideProportionalAlpha != 0;
	settings.modernRenderer = DeveloperModernMesh_GetEnabled();
	settings.dynamicLights = gEnableDlights != 0;
	settings.widescreenOverlays = gWidescreenOverlayAlign != 0;
	settings.fastLoadingScreens = gFastLoadingScreens != 0;
	RefreshDisplayState();
	settings.fullscreen = s_displayFullscreen;
	settings.windowWidth = s_windowedWidth;
	settings.windowHeight = s_windowedHeight;
	return settings;
}

void DeveloperGraphicsSettings_Apply(const DeveloperGraphicsSettings& settings)
{
	g_cfg_bilinearFiltering = settings.bilinearFiltering != 0;
	g_cfg_pgxpTextureCorrection = settings.pgxpTextureMapping != 0;
	g_cfg_pgxpZBuffer = settings.pgxpZBuffer != 0;
	g_cfg_swapInterval = settings.vsync != 0;
	gDrawDistance = Clamp(settings.drawDistance, 441, 1800);
	gCameraDefaultScrZ = (short)Clamp(settings.fieldOfView, 128, 384);

	gDisplayDrawStats = settings.showLegacyStats != 0;
	HdTextureOverrides_SetEnabled(settings.hdTextureOverrides);
	HdTextureOverrides_SetOrganizedExport(settings.organizeTextureExports);
	HdTextureOverrides_SetBaseColourExport(settings.exportBaseColours);
	g_cfg_overrideProportionalAlpha = settings.overrideProportionalAlpha != 0;
	DeveloperModernMesh_SetEnabled(settings.modernRenderer);
	gEnableDlights = settings.dynamicLights != 0;
	gWidescreenOverlayAlign = settings.widescreenOverlays != 0;
	gFastLoadingScreens = settings.fastLoadingScreens != 0;

	// Display mode last: it resets the render device, so it must not run in the
	// middle of applying the settings above. Identical requests are skipped so a
	// settings load does not reset the device for no reason.
	RefreshDisplayState();
	const int wantedWidth = Clamp(settings.windowWidth, 320, 7680);
	const int wantedHeight = Clamp(settings.windowHeight, 240, 4320);
	const int fullscreen = settings.fullscreen != 0;
	if (fullscreen != s_displayFullscreen ||
		(!fullscreen && (wantedWidth != s_windowedWidth || wantedHeight != s_windowedHeight)))
	{
		PsyX_ApplyWindowMode(fullscreen, wantedWidth, wantedHeight, NULL, NULL);
		RefreshDisplayState();
	}
}

bool DeveloperGraphicsSettings_LoadAndApply()
{
	FILE* file = fopen(kSettingsFilename, "rb");
	if (!file)
		return false;

	DeveloperGraphicsSettings settings = DeveloperGraphicsSettings_ReadRuntime();
	char line[128];
	char key[64];
	int value;

	while (fgets(line, sizeof(line), file))
	{
		if (sscanf(line, " %63[^=]=%d", key, &value) != 2)
			continue;

		if (!strcmp(key, "bilinearFiltering")) settings.bilinearFiltering = value;
		else if (!strcmp(key, "pgxpTextureMapping")) settings.pgxpTextureMapping = value;
		else if (!strcmp(key, "pgxpZBuffer")) settings.pgxpZBuffer = value;
		else if (!strcmp(key, "vsync")) settings.vsync = value;
		else if (!strcmp(key, "drawDistance")) settings.drawDistance = value;
		else if (!strcmp(key, "fieldOfView")) settings.fieldOfView = value;
		else if (!strcmp(key, "showLegacyStats")) settings.showLegacyStats = value;
		else if (!strcmp(key, "hdTextureOverrides")) settings.hdTextureOverrides = value;
		else if (!strcmp(key, "organizeTextureExports")) settings.organizeTextureExports = value;
		else if (!strcmp(key, "exportBaseColours")) settings.exportBaseColours = value;
		else if (!strcmp(key, "overrideProportionalAlpha")) settings.overrideProportionalAlpha = value;
		else if (!strcmp(key, "modernRenderer")) settings.modernRenderer = value;
		else if (!strcmp(key, "dynamicLights")) settings.dynamicLights = value;
		else if (!strcmp(key, "widescreenOverlays")) settings.widescreenOverlays = value;
		else if (!strcmp(key, "fastLoadingScreens")) settings.fastLoadingScreens = value;
		else if (!strcmp(key, "fullscreen")) settings.fullscreen = value;
		else if (!strcmp(key, "windowWidth")) settings.windowWidth = value;
		else if (!strcmp(key, "windowHeight")) settings.windowHeight = value;
	}

	const bool readOk = ferror(file) == 0;
	fclose(file);
	if (readOk)
		DeveloperGraphicsSettings_Apply(settings);
	return readOk;
}

bool DeveloperGraphicsSettings_SaveRuntime()
{
	const DeveloperGraphicsSettings settings = DeveloperGraphicsSettings_ReadRuntime();

	// Values in the same order as kSettingKeys.
	const int values[] =
	{
		8,											// schemaVersion
		settings.bilinearFiltering,
		settings.pgxpTextureMapping,
		settings.pgxpZBuffer,
		settings.vsync,
		settings.drawDistance,
		settings.fieldOfView,
		settings.showLegacyStats,
		settings.hdTextureOverrides,
		settings.organizeTextureExports,
		settings.exportBaseColours,
		settings.overrideProportionalAlpha,
		settings.modernRenderer,
		settings.dynamicLights,
		settings.widescreenOverlays,
		settings.fastLoadingScreens,
		settings.fullscreen,
		settings.windowWidth,
		settings.windowHeight,
	};

	return DeveloperSettingsFile_WriteKeys(kSettingsFilename,
		kSettingComments, kSettingCommentCount,
		kSettingKeys, kSettingKeyCount,
		kSettingKeys, values, kSettingKeyCount);
}

void DeveloperGraphicsSettings_RestoreDefaults()
{
	DeveloperGraphicsSettings defaults = {};
	defaults.bilinearFiltering = 1;
	defaults.pgxpTextureMapping = 1;
	defaults.pgxpZBuffer = 1;
	defaults.vsync = 1;
	defaults.drawDistance = 1800;
	defaults.fieldOfView = 256;
	defaults.showLegacyStats = 0;
	defaults.hdTextureOverrides = 1;
	defaults.organizeTextureExports = 0;
	defaults.exportBaseColours = 1;
	defaults.overrideProportionalAlpha = 0;
	defaults.modernRenderer = 0;
	defaults.dynamicLights = 1;
	defaults.widescreenOverlays = 1;
	defaults.fastLoadingScreens = 1;
	defaults.fullscreen = 0;
	defaults.windowWidth = kDefaultWindowWidth;
	defaults.windowHeight = kDefaultWindowHeight;
	DeveloperGraphicsSettings_Apply(defaults);
}
