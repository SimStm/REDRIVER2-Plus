#ifndef INSPECTOR_EXPORT_H
#define INSPECTOR_EXPORT_H

// Publish completed exports only. A failed encoder must not truncate the last
// usable export, and every successful click may replace the previous export.
#ifdef _WIN32
#include <windows.h>
#include <stdio.h>
#include <string.h>

inline bool InspectorExport_Temporary(const char* destination, char* temporary, char* status, int capacity)
{
	char directory[MAX_PATH];
	if (strlen(destination) >= sizeof(directory))
	{
		snprintf(status, capacity, "Export path exceeds the supported Windows path length.");
		return false;
	}
	strcpy(directory, destination);
	char* slash = strrchr(directory, '/');
	char* backslash = strrchr(directory, '\\');
	if (!slash || (backslash && backslash > slash)) slash = backslash;
	if (slash) *slash = '\0'; else strcpy(directory, ".");
	if (GetTempFileNameA(directory, "r2x", 0, temporary)) return true;
	snprintf(status, capacity, "Cannot create export temporary file (Windows error %lu).", GetLastError());
	return false;
}

inline bool InspectorExport_Commit(const char* temporary, const char* destination, char* status, int capacity)
{
	WIN32_FILE_ATTRIBUTE_DATA data = {};
	if (!GetFileAttributesExA(temporary, GetFileExInfoStandard, &data) ||
		(data.nFileSizeHigh == 0 && data.nFileSizeLow == 0))
	{
		DeleteFileA(temporary);
		snprintf(status, capacity, "Export produced no bytes; the previous destination was preserved.");
		return false;
	}
	if (!MoveFileExA(temporary, destination, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
	{
		const DWORD error = GetLastError();
		DeleteFileA(temporary);
		snprintf(status, capacity, "Cannot replace export (Windows error %lu); previous destination preserved.", error);
		return false;
	}
	snprintf(status, capacity, "Exported %lu bytes: %s", data.nFileSizeLow, destination);
	return true;
}

inline bool InspectorExport_WriteText(const char* destination, const char* content, char* status, int capacity)
{
	char temporary[MAX_PATH];
	if (!InspectorExport_Temporary(destination, temporary, status, capacity)) return false;
	HANDLE file = CreateFileA(temporary, GENERIC_WRITE, 0, NULL, TRUNCATE_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	DWORD written = 0;
	const DWORD size = (DWORD)strlen(content);
	bool ok = file != INVALID_HANDLE_VALUE && size > 0 && WriteFile(file, content, size, &written, NULL) && written == size;
	if (file != INVALID_HANDLE_VALUE)
	{
		if (ok) ok = FlushFileBuffers(file) != 0;
		if (!CloseHandle(file)) ok = false;
	}
	if (!ok)
	{
		DeleteFileA(temporary);
		snprintf(status, capacity, "Export write failed; previous destination preserved.");
		return false;
	}
	return InspectorExport_Commit(temporary, destination, status, capacity);
}
#endif
#endif
