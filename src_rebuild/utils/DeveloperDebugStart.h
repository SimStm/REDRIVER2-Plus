#ifndef DEVELOPER_DEBUG_START_H
#define DEVELOPER_DEBUG_START_H

/*
 * Reproducible developer start snapshots.
 *
 * The Developer Graphics Panel captures the current mission, level, vehicle,
 * position, player count and chase, and can either generate the equivalent
 * command line or persist it to developer_debug_start.ini. A Debug or
 * Release_dev build applies that snapshot at startup, before the frontend,
 * unless a -mission or -replay argument already started a session.
 */

struct DeveloperDebugStartState
{
	int enabled;
	int mission;
	int level;
	int gameType;
	int car;
	int startX;
	int startZ;
	int startDir;
	int players;
	int chase;
};

/* Records the current playable state. Returns false when the game is in the
   frontend or otherwise has no reproducible session. */
bool DeveloperDebugStart_CaptureState(DeveloperDebugStartState* state);

/* Builds the command line that reproduces the captured state. Returns false
   when the build does not support a directed debug start. */
bool DeveloperDebugStart_BuildCommandLine(char* buffer, int capacity);

/* Program name used in generated commands, normally set from argv[0]. */
void DeveloperDebugStart_SetProgramName(const char* programName);

/* developer_debug_start.ini persistence. Status is written on every call. */
bool DeveloperDebugStart_SaveSnapshot(char* status, int statusCapacity);
bool DeveloperDebugStart_SetEnabled(int enabled, char* status, int statusCapacity);
bool DeveloperDebugStart_Clear(char* status, int statusCapacity);
void DeveloperDebugStart_GetStatus(char* buffer, int capacity);
const char* DeveloperDebugStart_GetFilePath(void);

/* Applies the snapshot at startup. Returns non-zero when the caller should
   jump straight into the saved session instead of the frontend. */
int DeveloperDebugStart_TryApply(void);

/* Scripted capture: after the given number of rendered game seconds, save
   SCREENSHOT.BMP once. Zero disables it. */
void DeveloperDebugStart_ConfigureCapture(int seconds);

/* Called once per rendered game frame. Cheap when capture is disabled. */
void DeveloperDebugStart_Tick(void);

#endif
