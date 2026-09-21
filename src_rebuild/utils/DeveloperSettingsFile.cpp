#include "DeveloperSettingsFile.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace
{
const char* const kTemporarySuffix = ".tmp";
const char* const kBackupSuffix = ".bak";
const char* const kBackupTemporarySuffix = ".bak.tmp";
const int kMaxLineLength = 512;

std::string WithSuffix(const char* path, const char* suffix)
{
	std::string result(path);
	result += suffix;
	return result;
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

// Puts the finished temporary file in place, keeping the previous contents as a
// backup. The temporary file is left behind when this fails, so the caller can
// remove it without having touched the real file.
bool ReplaceFile(const char* path, const char* temporaryPath)
{
	const std::string backupPath = WithSuffix(path, kBackupSuffix);

#ifdef _WIN32
	const DWORD attributes = GetFileAttributesA(path);
	if (attributes == INVALID_FILE_ATTRIBUTES)
	{
		if (GetLastError() != ERROR_FILE_NOT_FOUND)
			return false;
		return MoveFileExA(temporaryPath, path, MOVEFILE_WRITE_THROUGH) != 0;
	}

	return ReplaceFileA(path, temporaryPath, backupPath.c_str(),
		REPLACEFILE_WRITE_THROUGH, NULL, NULL) != 0;
#else
	FILE* source = fopen(path, "rb");
	if (source)
	{
		const std::string backupTemporaryPath = WithSuffix(path, kBackupTemporarySuffix);
		FILE* backup = fopen(backupTemporaryPath.c_str(), "wb");
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
		if (!copied || rename(backupTemporaryPath.c_str(), backupPath.c_str()) != 0)
			return false;
	}
	else if (errno != ENOENT)
	{
		return false;
	}

	return rename(temporaryPath, path) == 0;
#endif
}

// Reads the existing file into lines. A missing file is not an error: there is
// simply nothing to preserve.
bool ReadLines(const char* path, std::vector<std::string>& lines)
{
	FILE* file = fopen(path, "rb");
	if (!file)
		return errno == ENOENT;

	char buffer[kMaxLineLength];
	while (fgets(buffer, sizeof(buffer), file))
	{
		size_t length = strlen(buffer);
		while (length > 0 && (buffer[length - 1] == '\n' || buffer[length - 1] == '\r'))
			buffer[--length] = '\0';
		lines.push_back(std::string(buffer));
	}

	const bool readOk = ferror(file) == 0;
	fclose(file);
	return readOk;
}

// Key of a `key=value` line, or NULL when the line is a comment, blank, or not
// a key/value pair at all. The returned pointer is valid until `line` changes.
const char* KeyOfLine(std::string& line)
{
	const size_t equals = line.find('=');
	if (equals == std::string::npos || equals == 0)
		return NULL;

	// Only `key=value` lines carry settings; anything else is preserved as-is.
	if (line[0] == '#' || line[0] == ';')
		return NULL;

	return line.c_str();
}

int FindKey(const char* const* keys, int keyCount, const char* lineKey, size_t keyLength)
{
	for (int i = 0; i < keyCount; i++)
	{
		if (strlen(keys[i]) == keyLength && strncmp(keys[i], lineKey, keyLength) == 0)
			return i;
	}

	return -1;
}

bool IsCanonicalComment(const char* const* commentLines, int commentLineCount, const std::string& line)
{
	for (int i = 0; i < commentLineCount; i++)
	{
		if (line == commentLines[i])
			return true;
	}

	return false;
}
}

bool DeveloperSettingsFile_WriteKeys(const char* path,
	const char* const* commentLines, int commentLineCount,
	const char* const* knownKeys, int knownKeyCount,
	const char* const* keys, const int* values, int keyCount)
{
	if (!path || !commentLines || !knownKeys || !keys || !values ||
		commentLineCount < 0 || knownKeyCount < 0 || keyCount < 0)
		return false;

	std::vector<std::string> existing;
	if (!ReadLines(path, existing))
		return false;

	const std::string temporaryPath = WithSuffix(path, kTemporarySuffix);
	FILE* file = fopen(temporaryPath.c_str(), "wb");
	if (!file)
		return false;

	bool ok = true;
	std::vector<char> written(keyCount, 0);

	for (int i = 0; i < commentLineCount && ok; i++)
		ok = fputs(commentLines[i], file) >= 0 && fputc('\n', file) != EOF;

	for (size_t i = 0; i < existing.size() && ok; i++)
	{
		std::string& line = existing[i];

		// The canonical header is emitted once, above; an identical comment line
		// read back from the file must not be duplicated.
		if (IsCanonicalComment(commentLines, commentLineCount, line))
			continue;

		const char* lineKey = KeyOfLine(line);
		if (!lineKey)
		{
			ok = fputs(line.c_str(), file) >= 0 && fputc('\n', file) != EOF;
			continue;
		}

		const size_t keyLength = line.find('=');
		const bool owned = FindKey(knownKeys, knownKeyCount, lineKey, keyLength) >= 0;
		const int index = FindKey(keys, keyCount, lineKey, keyLength);

		if (!owned)
		{
			// Not ours: keep the line exactly as it was found.
			ok = fputs(line.c_str(), file) >= 0 && fputc('\n', file) != EOF;
			continue;
		}

		// Ours but no longer wanted: drop it.
		if (index < 0 || written[index])
			continue;

		char replacement[kMaxLineLength];
		snprintf(replacement, sizeof(replacement), "%s=%d", keys[index], values[index]);
		ok = fputs(replacement, file) >= 0 && fputc('\n', file) != EOF;
		written[index] = 1;
	}

	for (int i = 0; i < keyCount && ok; i++)
	{
		if (written[i])
			continue;

		char replacement[kMaxLineLength];
		snprintf(replacement, sizeof(replacement), "%s=%d", keys[i], values[i]);
		ok = fputs(replacement, file) >= 0 && fputc('\n', file) != EOF;
	}

	ok = ok && FlushAndSync(file);
	const int closeResult = fclose(file);
	ok = ok && closeResult == 0;

	if (!ok)
	{
		remove(temporaryPath.c_str());
		return false;
	}

	return ReplaceFile(path, temporaryPath.c_str());
}
