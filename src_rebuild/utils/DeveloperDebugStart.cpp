#include "DeveloperDebugStart.h"

#include "driver2.h"
#include "C/cars.h"
#include "C/glaunch.h"
#include "C/main.h"
#include "C/mission.h"
#include "C/players.h"
#include "C/system.h"

#include <SDL.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "PsyX/PsyX_public.h"

#ifdef _WIN32
#include <windows.h>
#endif

extern int gChaseNumber;

namespace
{
const char* const kSnapshotFilename = "developer_debug_start.ini";
const char* const kTemporaryFilename = "developer_debug_start.ini.tmp";

char g_programName[260] = "REDRIVER2_dev.exe";

unsigned int g_captureStartTicks = 0;
unsigned int g_captureDelayMs = 0;
int g_captureTaken = 0;

bool IsFiniteInt(int value)
{
	return value >= -100000000 && value <= 100000000;
}

void NormalizeSeparators(char* path)
{
	for (char* character = path; *character; ++character)
		if (*character == '\\')
			*character = '/';
}

bool ReplaceSnapshotFile()
{
#ifdef _WIN32
	return MoveFileExA(kTemporaryFilename, kSnapshotFilename,
		MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
	return rename(kTemporaryFilename, kSnapshotFilename) == 0;
#endif
}

bool WriteSnapshot(const DeveloperDebugStartState& state)
{
	FILE* file = fopen(kTemporaryFilename, "wb");
	if (!file)
		return false;

	const int written = fprintf(file,
		"# REDRIVER2-Plus developer debug start snapshot\n"
		"# Applied at startup when enabled=1 and no -mission/-replay argument is present.\n"
		"# Managed by the Developer Graphics Panel; safe to delete.\n"
		"[debug_start]\n"
		"enabled=%d\n"
		"mission=%d\n"
		"level=%d\n"
		"gameType=%d\n"
		"car=%d\n"
		"startX=%d\n"
		"startZ=%d\n"
		"startDir=%d\n"
		"players=%d\n"
		"chase=%d\n",
		state.enabled ? 1 : 0, state.mission, state.level, state.gameType, state.car,
		state.startX, state.startZ, state.startDir, state.players, state.chase);

	bool ok = written > 0 && fflush(file) == 0;
	const int closeResult = fclose(file);
	ok = ok && closeResult == 0;
	if (!ok)
	{
		remove(kTemporaryFilename);
		return false;
	}

	return ReplaceSnapshotFile();
}

bool ReadSnapshot(DeveloperDebugStartState* state)
{
	if (!state)
		return false;
	memset(state, 0, sizeof(*state));

	FILE* file = fopen(kSnapshotFilename, "rb");
	if (!file)
		return false;

	char line[128];
	char key[64];
	int value;
	bool any = false;
	while (fgets(line, sizeof(line), file))
	{
		if (sscanf(line, " %63[^=]=%d", key, &value) != 2)
			continue;

		if (!strcmp(key, "enabled")) { state->enabled = value; any = true; }
		else if (!strcmp(key, "mission")) { state->mission = value; any = true; }
		else if (!strcmp(key, "level")) { state->level = value; any = true; }
		else if (!strcmp(key, "gameType")) { state->gameType = value; any = true; }
		else if (!strcmp(key, "car")) { state->car = value; any = true; }
		else if (!strcmp(key, "startX")) { state->startX = value; any = true; }
		else if (!strcmp(key, "startZ")) { state->startZ = value; any = true; }
		else if (!strcmp(key, "startDir")) { state->startDir = value; any = true; }
		else if (!strcmp(key, "players")) { state->players = value; any = true; }
		else if (!strcmp(key, "chase")) { state->chase = value; any = true; }
	}

	const bool readOk = ferror(file) == 0;
	fclose(file);
	return readOk && any;
}
}

bool DeveloperDebugStart_CaptureState(DeveloperDebugStartState* state)
{
	if (!state)
		return false;
	memset(state, 0, sizeof(*state));

	if (gInFrontend && GameType != GAME_IDLEDEMO)
		return false;

	state->enabled = 1;
	state->mission = gCurrentMissionNumber;
	state->level = GameLevel;
	state->gameType = (int)GameType;
	state->players = NumPlayers > 0 ? NumPlayers : 1;
	state->chase = gChaseNumber;

	const int playerCar = MainPlayer.playerCarId;
	if (playerCar >= 0 && playerCar < MAX_CARS)
	{
		state->car = car_data[playerCar].ap.model;
		VECTOR* position = (VECTOR*)car_data[playerCar].hd.where.t;
		state->startX = position->vx;
		state->startZ = position->vz;
		state->startDir = car_data[playerCar].hd.direction & 0xFFF;
	}
	else
	{
		state->car = wantedCar[0];
		state->startX = MainPlayer.pos[0];
		state->startZ = MainPlayer.pos[2];
		state->startDir = MainPlayer.dir & 0xFFF;
	}

	if (state->mission <= 0 || !IsFiniteInt(state->startX) || !IsFiniteInt(state->startZ))
		return false;

	return true;
}

bool DeveloperDebugStart_BuildCommandLine(char* buffer, int capacity)
{
	if (!buffer || capacity <= 0)
		return false;
	buffer[0] = '\0';

#ifdef DEBUG_OPTIONS
	DeveloperDebugStartState state;
	if (!DeveloperDebugStart_CaptureState(&state))
		return false;

	if (gLoadedReplay && state.mission >= 400)
	{
		char replayPath[320];
		snprintf(replayPath, sizeof(replayPath), "%sREPLAYS/ATTRACT.%d", gDataFolder, state.mission);
		NormalizeSeparators(replayPath);
		snprintf(buffer, capacity, "%s -nointro -replay \"%s\"", g_programName, replayPath);
		return true;
	}

	int written = snprintf(buffer, capacity,
		"%s -nointro -mission %d -gametype %d -level %d",
		g_programName, state.mission, state.gameType, state.level);
	bool ok = written > 0 && written < capacity;

	if (ok && state.car >= 0)
	{
		const int appended = snprintf(buffer + written, capacity - written, " -playercar %d", state.car);
		ok = appended > 0 && written + appended < capacity;
		written += appended;
	}

	if (ok && IsFiniteInt(state.startX) && IsFiniteInt(state.startZ))
	{
		const int appended = snprintf(buffer + written, capacity - written, " -startpos %d %d -startdir %d",
			state.startX, state.startZ, state.startDir & 0xFFF);
		ok = appended > 0 && written + appended < capacity;
		written += appended;
	}

	if (ok)
	{
		const int appended = snprintf(buffer + written, capacity - written, " -players %d", state.players);
		ok = appended > 0 && written + appended < capacity;
		written += appended;
	}

	if (ok && state.chase != 0)
	{
		const int appended = snprintf(buffer + written, capacity - written, " -chase %d", state.chase);
		ok = appended > 0 && written + appended < capacity;
	}

	return ok;
#else
	snprintf(buffer, capacity,
		"Directed debug start requires a Debug or Release_dev build (DEBUG_OPTIONS); this build only supports -nointro, -nofmv and -replay.");
	return false;
#endif
}

void DeveloperDebugStart_SetProgramName(const char* programName)
{
	if (!programName || programName[0] == '\0')
		return;
	snprintf(g_programName, sizeof(g_programName), "%s", programName);
}

bool DeveloperDebugStart_SaveSnapshot(char* status, int statusCapacity)
{
	DeveloperDebugStartState state;
	if (!DeveloperDebugStart_CaptureState(&state))
	{
		snprintf(status, statusCapacity, "No reproducible session yet; start a mission or a replay first.");
		return false;
	}

	if (!WriteSnapshot(state))
	{
		snprintf(status, statusCapacity, "Could not write %s.", kSnapshotFilename);
		return false;
	}

	snprintf(status, statusCapacity, "Debug start saved: mission %d, car %d at (%d, %d).",
		state.mission, state.car, state.startX, state.startZ);
	return true;
}

bool DeveloperDebugStart_SetEnabled(int enabled, char* status, int statusCapacity)
{
	DeveloperDebugStartState state;
	if (!ReadSnapshot(&state))
	{
		if (!enabled)
		{
			snprintf(status, statusCapacity, "No %s to disable.", kSnapshotFilename);
			return false;
		}
		return DeveloperDebugStart_SaveSnapshot(status, statusCapacity);
	}

	state.enabled = enabled ? 1 : 0;
	if (!WriteSnapshot(state))
	{
		snprintf(status, statusCapacity, "Could not update %s.", kSnapshotFilename);
		return false;
	}

	snprintf(status, statusCapacity, "Debug start %s.", enabled ? "enabled" : "disabled");
	return true;
}

bool DeveloperDebugStart_Clear(char* status, int statusCapacity)
{
	remove(kTemporaryFilename);
	if (remove(kSnapshotFilename) != 0)
	{
		snprintf(status, statusCapacity, "No %s to delete.", kSnapshotFilename);
		return false;
	}
	snprintf(status, statusCapacity, "Deleted %s.", kSnapshotFilename);
	return true;
}

void DeveloperDebugStart_GetStatus(char* buffer, int capacity)
{
	if (!buffer || capacity <= 0)
		return;

	DeveloperDebugStartState state;
	if (!ReadSnapshot(&state))
	{
		snprintf(buffer, capacity, "No snapshot yet (%s).", kSnapshotFilename);
		return;
	}

	snprintf(buffer, capacity, "%s: %s - mission %d, car %d at (%d, %d) dir %d, %d player(s)%s.",
		kSnapshotFilename, state.enabled ? "enabled" : "disabled",
		state.mission, state.car, state.startX, state.startZ, state.startDir & 0xFFF,
		state.players, state.chase ? ", chase" : "");
}

const char* DeveloperDebugStart_GetFilePath(void)
{
	return kSnapshotFilename;
}

void DeveloperDebugStart_ConfigureCapture(int seconds)
{
	g_captureDelayMs = seconds > 0 ? (unsigned int)seconds * 1000u : 0u;
	g_captureStartTicks = 0;
	g_captureTaken = 0;
}

void DeveloperDebugStart_Tick(void)
{
	if (g_captureDelayMs == 0 || g_captureTaken)
		return;

	const unsigned int now = SDL_GetTicks();
	if (g_captureStartTicks == 0)
	{
		g_captureStartTicks = now;
		return;
	}

	if (now - g_captureStartTicks >= g_captureDelayMs)
	{
		g_captureTaken = 1;
#if !defined(__EMSCRIPTEN__) && !defined(__ANDROID__)
		PsyX_TakeScreenshot();
#endif
	}
}

int DeveloperDebugStart_ShouldSkipIntro(void)
{
#ifdef DEBUG_OPTIONS
	DeveloperDebugStartState state;
	if (!ReadSnapshot(&state) || !state.enabled)
		return 0;

	return state.mission > 0;
#else
	return 0;
#endif
}

int DeveloperDebugStart_TryApply(void)
{
#ifdef DEBUG_OPTIONS
	DeveloperDebugStartState state;
	if (!ReadSnapshot(&state) || !state.enabled)
		return 0;
	if (state.mission <= 0 || !IsFiniteInt(state.startX) || !IsFiniteInt(state.startZ))
		return 0;

	gCurrentMissionNumber = state.mission;
	GameLevel = state.level >= 0 ? state.level : 0;
	GameType = (state.gameType >= GAME_MISSION && state.gameType <= GAME_CAPTURETHEFLAG)
		? (GAMETYPE)state.gameType : GAME_TAKEADRIVE;
	NumPlayers = (unsigned char)(state.players > 0 ? state.players : 1);
	gChaseNumber = state.chase;

	if (state.car >= 0)
		wantedCar[0] = state.car;

	gStartPos.x = state.startX;
	gStartPos.z = state.startZ;
	gStartDir = state.startDir & 0xFFF;

	return 1;
#else
	return 0;
#endif
}
