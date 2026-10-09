#include "DeterministicBaseline.h"
#include "ProjectClock.h"

#include "driver2.h"
#include "C/camera.h"
#include "C/cars.h"
#include "C/glaunch.h"
#include "C/main.h"
#include "C/mission.h"
#include "C/pause.h"
#include "C/players.h"
#include "C/replays.h"
#include "C/debris.h"
#include "C/denting.h"
#include "C/loadview.h"

extern int gHubcapTime;

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Canonical field-wise simulation record for engine-migration determinism
 * checks. See DeterministicBaseline.h for the contract. Everything written
 * comes from explicit gameplay/effect fields; no struct memory is copied or hashed.
 */

namespace
{
FILE* g_file = NULL;
int g_every = 1;
int g_lastTick = -1;
unsigned int g_records = 0;
unsigned long long g_totalHash = 1469598103934665603ULL;

const unsigned long long kFnvOffset = 1469598103934665603ULL;
const unsigned long long kFnvPrime = 1099511628211ULL;

unsigned long long Fnv1a(const char* data, size_t length)
{
	unsigned long long hash = kFnvOffset;
	for (size_t i = 0; i < length; ++i)
	{
		hash ^= (unsigned char)data[i];
		hash *= kFnvPrime;
	}
	return hash;
}

/* Bounded append so a pathological record cannot overflow the buffer. */
void Append(char* buffer, size_t capacity, size_t* offset, const char* text)
{
	if (*offset >= capacity)
		return;

	const size_t remaining = capacity - *offset;
	const int written = snprintf(buffer + *offset, remaining, "%s", text);
	if (written <= 0)
		return;

	*offset += (size_t)written < remaining ? (size_t)written : remaining - 1;
}

void AppendFormat(char* buffer, size_t capacity, size_t* offset, const char* format, ...)
{
	if (*offset >= capacity)
		return;

	va_list args;
	va_start(args, format);
	const size_t remaining = capacity - *offset;
	const int written = vsnprintf(buffer + *offset, remaining, format, args);
	va_end(args);
	if (written <= 0)
		return;

	*offset += (size_t)written < remaining ? (size_t)written : remaining - 1;
}

bool IsActiveCar(const CAR_DATA& car)
{
	if (car.controlType == CONTROL_TYPE_NONE)
		return false;

	// The camera collider is a presentation proxy recomputed from the camera
	// in the draw path (PlaceCameraFollowCar), not an authoritative entity.
	if (car.controlType == CONTROL_TYPE_CAMERACOLLIDER)
		return false;

	return true;
}

size_t BuildRecord(char* buffer, size_t capacity)
{
	size_t offset = 0;
	buffer[0] = '\0';

	const PLAYER& mainPlayer = MainPlayer;
	// Authoritative player state only: world position and heading. Camera
	// fields are presentation state and must not gate simulation determinism.
	AppendFormat(buffer, capacity, &offset,
		"player %d x %d y %d z %d dir %d\n",
		(int)mainPlayer.playerCarId,
		mainPlayer.pos[0], mainPlayer.pos[1], mainPlayer.pos[2], mainPlayer.dir);

	for (int slot = 0; slot < MAX_CARS + 2; ++slot)
	{
		const CAR_DATA& car = car_data[slot];
		if (!IsActiveCar(car))
			continue;

		AppendFormat(buffer, capacity, &offset,
			"car %d ctl %d flags %d model %d pal %d x %d y %d z %d speed %d wspeed %d dir %d wangle %d tdamage %d d0 %d d1 %d d2 %d d3 %d d4 %d d5 %d vx %d vy %d vz %d avx %d avy %d avz %d\n",
			slot, (int)car.controlType, (int)car.controlFlags, (int)car.ap.model,
			(int)car.ap.palette,
			car.hd.where.t[0], car.hd.where.t[1], car.hd.where.t[2],
			car.hd.speed, car.hd.wheel_speed, car.hd.direction, car.wheel_angle,
			(int)car.totalDamage,
			(int)car.ap.damage[0], (int)car.ap.damage[1], (int)car.ap.damage[2],
			(int)car.ap.damage[3], (int)car.ap.damage[4], (int)car.ap.damage[5],
			car.st.n.linearVelocity[0], car.st.n.linearVelocity[1], car.st.n.linearVelocity[2],
			car.st.n.angularVelocity[0], car.st.n.angularVelocity[1], car.st.n.angularVelocity[2]);
	}

	// Mission/AI timers: acceptance requires timer outcomes to match across
	// presentation schedules, not only car transforms.
	AppendFormat(buffer, capacity, &offset,
		"missionstate active %d t0count %d t0min %d t0sec %d t0frac %d t1count %d t1min %d t1sec %d t1frac %d msg0 %d msg1 %d tanner %d\n",
		Mission.active,
		(int)Mission.timer[0].count, (int)Mission.timer[0].min, (int)Mission.timer[0].sec, (int)Mission.timer[0].frac,
		(int)Mission.timer[1].count, (int)Mission.timer[1].min, (int)Mission.timer[1].sec, (int)Mission.timer[1].frac,
		(int)Mission.message_timer[0], (int)Mission.message_timer[1], tannerDeathTimer);

	// Replay cursors: run-length playback position per stream. Only streams that
	// own a buffer (replay/director/chase modes) are meaningful.
	for (int i = 0; i < NumReplayStreams; ++i)
	{
		const REPLAY_STREAM& stream = ReplayStreams[i];
		if (stream.InitialPadRecordBuffer == NULL)
			continue;

		AppendFormat(buffer, capacity, &offset,
			"replay %d cursor %d run %d\n",
			i, (int)(stream.PadRecordBuffer - stream.InitialPadRecordBuffer), (int)stream.playbackrun);
	}

	AppendFormat(buffer, capacity, &offset, "effectsstate pools %u inputlock %d title %d hubcaptime %d\n",
		Debris_GetTimingDigest(), gStopPadReads, bMissionTitleFade, gHubcapTime);
	for (int slot = 0; slot < MAX_CARS; ++slot)
	{
		const CAR_DATA& car = car_data[slot];
		if (!IsActiveCar(car)) continue;
		AppendFormat(buffer, capacity, &offset, "carphase %d indicator %d siren %d\n", slot, (int)car.ap.life, (int)car.ap.coplife);
	}
	return offset;
}

void WriteRecord(void)
{
	char buffer[16384];
	const size_t offset = BuildRecord(buffer, sizeof(buffer));
	const unsigned long long hash = Fnv1a(buffer, offset);
	g_totalHash ^= hash;
	g_totalHash *= kFnvPrime;

	fprintf(g_file, "tick %d frame %d level %d type %d mission %d players %d pause %d hash %llu\n",
		CameraCnt, FrameCnt, GameLevel, (int)GameType, gCurrentMissionNumber,
		(int)NumPlayers, pauseflag, hash);
	fprintf(g_file, "%s", buffer);
	fflush(g_file);

	g_records++;
}
}

extern "C" void DeterministicBaseline_ConfigureFromCommandLine(int argc, char** argv)
{
	const char* filename = NULL;

	for (int i = 1; i < argc; ++i)
	{
		if (!strcmp(argv[i], "-statedump") && i + 1 < argc)
		{
			filename = argv[++i];
		}
		else if (!strcmp(argv[i], "-statedumpevery") && i + 1 < argc)
		{
			g_every = atoi(argv[++i]);
			if (g_every < 1)
				g_every = 1;
		}
	}

	if (!filename)
		return;

	g_file = fopen(filename, "wb");
	if (!g_file)
	{
		printWarning("DeterministicBaseline: cannot open '%s' for writing\n", filename);
		return;
	}

	const char* region = ProjectClock_GetVideoMode() == PROJECT_VIDEO_PAL ? "PAL" : "NTSC";

	fprintf(g_file,
		"# REDRIVER2-Plus deterministic baseline dump\n"
		"# format 1 region %s every %d\n"
		"# Gameplay and tick-owned effects; FrameCnt is context, not hashed.\n",
		region, g_every);
	printInfo("DeterministicBaseline: dumping to '%s' (every %d ticks)\n", filename, g_every);
}

extern "C" void DeterministicBaseline_AfterTick(void)
{
	if (!g_file)
		return;

	if (CameraCnt == g_lastTick)
		return;

	if (CameraCnt < g_lastTick)
		fprintf(g_file, "reset %d\n", CameraCnt);

	g_lastTick = CameraCnt;

	if (g_every > 1 && (CameraCnt % g_every) != 0)
		return;

	WriteRecord();
}

extern "C" void DeterministicBaseline_Shutdown(void)
{
	if (!g_file)
		return;

	fprintf(g_file, "# records %u totalhash %llu\n", g_records, g_totalHash);
	fclose(g_file);
	g_file = NULL;
	printInfo("DeterministicBaseline: %u records, total hash %llu\n", g_records, g_totalHash);
}

namespace
{
char g_presentationRecord[16384];
size_t g_presentationSize = 0;
unsigned int g_presentationMutations = 0;
}

extern "C" void DeterministicBaseline_BeginPresentation(void)
{
	if (g_file) g_presentationSize = BuildRecord(g_presentationRecord, sizeof(g_presentationRecord));
}

extern "C" void DeterministicBaseline_EndPresentation(void)
{
	if (!g_file) return;
	char after[16384];
	const size_t size = BuildRecord(after, sizeof(after));
	if (size == g_presentationSize && memcmp(after, g_presentationRecord, size) == 0) return;
	if (g_presentationMutations++ == 0)
	{
		fprintf(g_file, "presentation-mutation tick %d frame %d\nbefore:\n%safter:\n%s", CameraCnt, FrameCnt, g_presentationRecord, after);
		printWarning("DeterministicBaseline: drawing mutated recorded tick state at tick %d\n", CameraCnt);
	}
}
