#ifndef DEVELOPER_SETTINGS_FILE_H
#define DEVELOPER_SETTINGS_FILE_H

/* Writes a flat `key=value` developer settings file without losing content it
   does not own.

   `knownKeys` is every key this owner is responsible for; `keys`/`values` is the
   subset that should exist in the file. Every other line - keys owned by nobody,
   comments, blank lines - is copied verbatim. A line whose key is known but is
   not in `keys` is dropped, which is how an owner removes a setting it no longer
   wants, while a missing written key is appended in the given order.

   The new contents go to `<path>.tmp` and replace `<path>` only once they are
   complete, keeping the previous contents as `<path>.bak`. Returns false and
   leaves the existing file untouched when anything fails, so an interrupted or
   failed save cannot corrupt or truncate the file. Values are integers because
   every developer settings file stores integers. */
bool DeveloperSettingsFile_WriteKeys(const char* path,
	const char* const* commentLines, int commentLineCount,
	const char* const* knownKeys, int knownKeyCount,
	const char* const* keys, const int* values, int keyCount);

#endif // DEVELOPER_SETTINGS_FILE_H
