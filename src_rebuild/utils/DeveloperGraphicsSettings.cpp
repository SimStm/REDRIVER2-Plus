#include "DeveloperGraphicsSettings.h"
#include "HdTextureOverrides.h"

#include "driver2.h"
#include "C/camera.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#else
#include <errno.h>
#include <unistd.h>
#endif

#include "PsyX/PsyX_public.h"

extern int gDrawDistance;
extern int gDisplayDrawStats;

namespace
{
const char* const kSettingsFilename = "developer_graphics.ini";
const char* const kTemporaryFilename = "developer_graphics.ini.tmp";
const char* const kBackupFilename = "developer_graphics.ini.bak";
const char* const kBackupTemporaryFilename = "developer_graphics.ini.bak.tmp";

int Clamp(int value, int minimum, int maximum)
{
	return value < minimum ? minimum : (value > maximum ? maximum : value);
}

bool FlushAndSync(FILE* file)
{
	if (fflush(file) != 0)
		return false;

#ifdef _WIN32
	return _commit(_fileno(file)) == 0;
#else
	return fsync(fileno(file)) == 0;
#endif
}

bool ReplaceSettingsFile()
{
#ifdef _WIN32
	const DWORD attributes = GetFileAttributesA(kSettingsFilename);
	if (attributes == INVALID_FILE_ATTRIBUTES)
	{
		if (GetLastError() != ERROR_FILE_NOT_FOUND)
			return false;
		return MoveFileExA(kTemporaryFilename, kSettingsFilename, MOVEFILE_WRITE_THROUGH) != 0;
	}

	return ReplaceFileA(kSettingsFilename, kTemporaryFilename, kBackupFilename,
		REPLACEFILE_WRITE_THROUGH, NULL, NULL) != 0;
#else
	FILE* source = fopen(kSettingsFilename, "rb");
	if (source)
	{
		FILE* backup = fopen(kBackupTemporaryFilename, "wb");
		if (!backup)
		{
			fclose(source);
			return false;
		}

		char buffer[4096];
		size_t bytesRead;
		bool copied = true;
		while ((bytesRead = fread(buffer, 1, sizeof(buffer), source)) != 0)
		{
			if (fwrite(buffer, 1, bytesRead, backup) != bytesRead)
			{
				copied = false;
				break;
			}
		}

		copied = copied && ferror(source) == 0 && FlushAndSync(backup);
		const int closeBackupResult = fclose(backup);
		copied = copied && closeBackupResult == 0;
		fclose(source);
		if (!copied || rename(kBackupTemporaryFilename, kBackupFilename) != 0)
			return false;
	}
	else if (errno != ENOENT)
	{
		return false;
	}

	return rename(kTemporaryFilename, kSettingsFilename) == 0;
#endif
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
	FILE* file = fopen(kTemporaryFilename, "wb");
	if (!file)
		return false;

	const int written = fprintf(file,
		"# REDRIVER2 developer graphics settings\n"
		"# This file is managed separately and never modifies config.ini.\n"
		"schemaVersion=2\n"
		"bilinearFiltering=%d\n"
		"pgxpTextureMapping=%d\n"
		"pgxpZBuffer=%d\n"
		"vsync=%d\n"
		"drawDistance=%d\n"
		"fieldOfView=%d\n"
		"showLegacyStats=%d\n"
		"hdTextureOverrides=%d\n",
		settings.bilinearFiltering, settings.pgxpTextureMapping, settings.pgxpZBuffer,
		settings.vsync, settings.drawDistance, settings.fieldOfView, settings.showLegacyStats,
		settings.hdTextureOverrides);

	bool writeOk = written > 0 && FlushAndSync(file);
	const int closeResult = fclose(file);
	writeOk = writeOk && closeResult == 0;
	if (!writeOk)
		return false;

	return ReplaceSettingsFile();
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
	DeveloperGraphicsSettings_Apply(defaults);
}
