#include "ProjectClock.h"

#include <SDL.h>
#include <stdlib.h>

/*
 * Fixed-tick scheduler.
 *
 * Phase is elapsed performance-counter counts multiplied by the regional tick
 * rate. One tick is exactly g_frequency phase units. This preserves 30/25 Hz
 * even when the counter frequency is not divisible by the tick rate, without
 * truncating sub-microsecond frames or rounding NTSC to a 33333 us period.
 */

namespace
{
const unsigned long long kMicrosPerSecond = 1000000ULL;
const int kMaxCatchUpTicks = 4;

#ifdef PAL_VERSION
int g_videoMode = PROJECT_VIDEO_PAL;
int g_presentationFps = 25;
#else
int g_videoMode = PROJECT_VIDEO_NTSC;
int g_presentationFps = 30;
#endif
int g_preferredVideoMode = g_videoMode;
unsigned long long g_presentationDeadline = 0;
unsigned long long g_presentationRemainder = 0;

unsigned long long g_frequency = 0;
unsigned long long g_lastCounter = 0;
unsigned long long g_accumulatorPhase = 0;
unsigned long long g_elapsedCounts = 0;
int g_tickRate = 30;
int g_tickPeriodMicros = 33333; /* 30 Hz NTSC */
int g_paused = 0;
unsigned int g_tickCount = 0;
unsigned int g_droppedTicks = 0;
unsigned long long g_pendingJumpCounts = 0; /* one-shot stall injection (fixture) */
int g_firstFramePending = 0; /* re-base on the first frame so load time is skipped */
}

extern "C" int ProjectClock_GetVideoMode(void) { return g_videoMode; }
extern "C" int ProjectClock_GetPreferredVideoMode(void) { return g_preferredVideoMode; }
extern "C" int ProjectClock_GetTickRate(void) { return g_videoMode == PROJECT_VIDEO_PAL ? 25 : 30; }

extern "C" void ProjectClock_SetPreferredVideoMode(int mode)
{
	g_preferredVideoMode = mode == PROJECT_VIDEO_PAL ? PROJECT_VIDEO_PAL : PROJECT_VIDEO_NTSC;
}

extern "C" void ProjectClock_ApplyStartupVideoMode(void)
{
	g_videoMode = g_preferredVideoMode;
}

extern "C" int ProjectClock_GetPresentationFps(void) { return g_presentationFps; }

extern "C" void ProjectClock_SetPresentationFps(int fps)
{
	fps = fps < 15 ? 15 : (fps > 360 ? 360 : fps);
	if (fps == g_presentationFps)
		return;
	g_presentationFps = fps;
	g_presentationDeadline = 0;
	g_presentationRemainder = 0;
}

extern "C" void ProjectClock_WaitForPresentation(void)
{
	const unsigned long long frequency = SDL_GetPerformanceFrequency();
	if (!frequency)
		return;
	unsigned long long now = SDL_GetPerformanceCounter();
	if (!g_presentationDeadline || now > g_presentationDeadline + frequency)
	{
		g_presentationDeadline = now;
		g_presentationRemainder = 0;
		return;
	}

	// Carry the division remainder so the cap stays exact on counters whose
	// frequency is not divisible by FPS. VSync can already have spent this
	// interval: a missed deadline does not cause an additional full wait.
	g_presentationRemainder += frequency % (unsigned int)g_presentationFps;
	g_presentationDeadline += frequency / (unsigned int)g_presentationFps +
		g_presentationRemainder / (unsigned int)g_presentationFps;
	g_presentationRemainder %= (unsigned int)g_presentationFps;
	if (now > g_presentationDeadline)
	{
		// Keep small overruns on the original schedule. Rebasing every late
		// frame turns timer/driver jitter into a permanently lower FPS cap.
		if (now - g_presentationDeadline > frequency / (unsigned int)g_presentationFps)
		{
			g_presentationDeadline = now;
			g_presentationRemainder = 0;
		}
		return;
	}
	while (now < g_presentationDeadline)
	{
		const unsigned long long milliseconds = (g_presentationDeadline - now) * 1000ULL / frequency;
		SDL_Delay(milliseconds > 1 ? (Uint32)(milliseconds - 1) : 0);
		now = SDL_GetPerformanceCounter();
	}
}

extern "C" void ProjectClock_Init(void)
{
	g_frequency = SDL_GetPerformanceFrequency();
	if (g_frequency == 0)
		g_frequency = kMicrosPerSecond;

	g_tickRate = ProjectClock_GetTickRate();
	g_tickPeriodMicros = (int)(kMicrosPerSecond / (unsigned int)g_tickRate);

	g_lastCounter = SDL_GetPerformanceCounter();
	g_accumulatorPhase = 0;
	g_elapsedCounts = 0;
	g_paused = 0;
	g_tickCount = 0;
	g_droppedTicks = 0;
	g_firstFramePending = 1;

	// Debug fixture: RED2_CLOCKJUMP_MS arms a one-shot synthetic stall so the
	// bounded backlog policy can be exercised without real frame hitching.
	g_pendingJumpCounts = 0;
	const char* jumpMs = getenv("RED2_CLOCKJUMP_MS");
	if (jumpMs != NULL)
	{
		long long ms = atoll(jumpMs);
		if (ms > 0)
			g_pendingJumpCounts = (g_frequency * (unsigned long long)ms) / 1000ULL;
	}
}

extern "C" void ProjectClock_Reset(void)
{
	g_lastCounter = SDL_GetPerformanceCounter();
	g_accumulatorPhase = 0;
	g_elapsedCounts = 0;
	g_pendingJumpCounts = 0;
	g_firstFramePending = 0;
}

extern "C" void ProjectClock_SetPaused(int paused)
{
	if (paused && !g_paused)
		g_accumulatorPhase = 0;

	// On resume, re-base the counter so the paused interval is not replayed as a
	// backlog burst if BeginFrame was skipped while paused.
	if (!paused && g_paused)
		g_lastCounter = SDL_GetPerformanceCounter();

	g_paused = paused;
}

extern "C" void ProjectClock_InjectElapsedMicros(unsigned long long micros)
{
	if (g_frequency == 0)
		return;

	g_pendingJumpCounts += (g_frequency * micros) / kMicrosPerSecond;
}

extern "C" int ProjectClock_BeginFrame(void)
{
	const unsigned long long now = SDL_GetPerformanceCounter();

	// Skip the load interval between Init and the first presented frame: re-base
	// so world setup time is not replayed as a catch-up backlog.
	if (g_firstFramePending)
	{
		g_firstFramePending = 0;
		g_lastCounter = now;
	}

	const unsigned long long delta = now - g_lastCounter;
	g_lastCounter = now;
	g_elapsedCounts += delta;

	if (g_paused)
	{
		g_accumulatorPhase = 0;
		return 0;
	}

	g_accumulatorPhase += delta * (unsigned long long)g_tickRate;

	// Apply any armed one-shot stall so it goes through the same bounded path.
	if (g_pendingJumpCounts != 0)
	{
		g_accumulatorPhase += g_pendingJumpCounts * (unsigned long long)g_tickRate;
		g_pendingJumpCounts = 0;
	}

	unsigned long long ticks = g_accumulatorPhase / g_frequency;
	// Keep the fractional phase even when whole overdue ticks are discarded.
	// Resetting it to zero makes interpolation jump backwards after every hitch.
	g_accumulatorPhase %= g_frequency;

	if (ticks > (unsigned long long)kMaxCatchUpTicks)
	{
		/* Bounded backlog: drop the excess instead of an unbounded catch-up. */
		g_droppedTicks += (unsigned int)(ticks - (unsigned long long)kMaxCatchUpTicks);
		ticks = (unsigned long long)kMaxCatchUpTicks;
	}

	g_tickCount += (unsigned int)ticks;
	return (int)ticks;
}

extern "C" unsigned long long ProjectClock_ElapsedMicros(void)
{
	if (g_frequency == 0)
		return 0;
	return (g_elapsedCounts * kMicrosPerSecond) / g_frequency;
}

extern "C" int ProjectClock_GetInterpolationAlpha(void)
{
	if (g_frequency == 0)
		return 256;

	unsigned long long alpha = (g_accumulatorPhase * 256ULL) / g_frequency;
	if (alpha > 256ULL)
		alpha = 256ULL;

	return (int)alpha;
}

extern "C" unsigned int ProjectClock_GetTickCount(void)
{
	return g_tickCount;
}

extern "C" unsigned int ProjectClock_GetDroppedTicks(void)
{
	return g_droppedTicks;
}

extern "C" int ProjectClock_GetTickPeriodMicros(void)
{
	return g_tickPeriodMicros;
}
