#ifndef DEVELOPER_GRAPHICS_SETTINGS_H
#define DEVELOPER_GRAPHICS_SETTINGS_H

struct DeveloperGraphicsSettings
{
	int bilinearFiltering;
	int pgxpTextureMapping;
	int pgxpZBuffer;
	int vsync;
	int presentationFps;
	int videoMode;
	int drawDistance;
	int fieldOfView;
	int showLegacyStats;
	int hdTextureOverrides;
	int organizeTextureExports;
	int exportBaseColours;
	int overrideProportionalAlpha;
	int modernRenderer;
	int dynamicLights;
	int widescreenOverlays;
	int fastLoadingScreens;
	int fullscreen;
	int windowWidth;
	int windowHeight;
};

DeveloperGraphicsSettings DeveloperGraphicsSettings_ReadRuntime();
void DeveloperGraphicsSettings_Apply(const DeveloperGraphicsSettings& settings);
// Applies only keys present in the developer file over the current runtime
// baseline (startup defaults/config.ini); omitted keys preserve that baseline.
bool DeveloperGraphicsSettings_LoadAndApply();
bool DeveloperGraphicsSettings_SaveRuntime();
// Autosaves only frame controls; provisional resolution and unrelated session
// edits are left untouched in the file.
bool DeveloperGraphicsSettings_SaveFrameSettings();
void DeveloperGraphicsSettings_RestoreDefaults();

#endif
