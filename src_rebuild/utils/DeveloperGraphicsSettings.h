#ifndef DEVELOPER_GRAPHICS_SETTINGS_H
#define DEVELOPER_GRAPHICS_SETTINGS_H

struct DeveloperGraphicsSettings
{
	int bilinearFiltering;
	int pgxpTextureMapping;
	int pgxpZBuffer;
	int vsync;
	int drawDistance;
	int fieldOfView;
	int showLegacyStats;
	int hdTextureOverrides;
	int organizeTextureExports;
	int exportBaseColours;
	int overrideProportionalAlpha;
	int modernRenderer;
};

DeveloperGraphicsSettings DeveloperGraphicsSettings_ReadRuntime();
void DeveloperGraphicsSettings_Apply(const DeveloperGraphicsSettings& settings);
bool DeveloperGraphicsSettings_LoadAndApply();
bool DeveloperGraphicsSettings_SaveRuntime();
void DeveloperGraphicsSettings_RestoreDefaults();

#endif
