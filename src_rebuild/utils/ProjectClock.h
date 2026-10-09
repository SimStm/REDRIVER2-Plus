#ifndef PROJECT_CLOCK_H
#define PROJECT_CLOCK_H

/*
 * Project-owned monotonic clock and fixed simulation-tick scheduler (E01).
 *
 * Replaces the emulated vblank gate (`VSync(-1)` in `FilterFrameTime`): the
 * simulation advances by whole fixed ticks derived from real elapsed time, so
 * its cadence does not depend on the presentation frame rate, the swap
 * interval, the emulated vblank thread or CPU speed.
 *
 * The tick period preserves the original regional cadence: 30 Hz on
 * NTSC, 25 Hz on PAL, selected at startup (build region supplies the default).
 * Catch-up is bounded; excess backlog is
 * dropped and counted rather than replayed in an unbounded loop. While paused
 * no time accumulates, so resuming cannot burst.
 */

#ifdef __cplusplus
extern "C" {
#endif

enum ProjectVideoMode
{
	PROJECT_VIDEO_NTSC = 0,
	PROJECT_VIDEO_PAL = 1
};

/* The preferred region is persisted by the panel. Activate it once at startup,
   before initializing game services; changing a preference never retimes a
   running mission or replay. */
int ProjectClock_GetVideoMode(void);
int ProjectClock_GetPreferredVideoMode(void);
void ProjectClock_SetPreferredVideoMode(int mode);
void ProjectClock_ApplyStartupVideoMode(void);
int ProjectClock_GetTickRate(void);

/* Presentation only: 15..360 FPS. Changing the cap does not reset simulation
   time. The desktop state loop waits once per presented game/frontend frame. */
int ProjectClock_GetPresentationFps(void);
void ProjectClock_SetPresentationFps(int fps);
void ProjectClock_WaitForPresentation(void);

/* Initialises the monotonic clock and the region tick period. Safe to call
   again to reinitialise a session. */
void ProjectClock_Init(void);

/* Discards accumulated time and re-bases the monotonic clock (level load or
   other discontinuity) so no stale backlog is applied. */
void ProjectClock_Reset(void);

/* Paused: elapsed time does not accumulate toward ticks. Resuming re-bases the
   counter so a paused interval is not replayed as a backlog burst. */
void ProjectClock_SetPaused(int paused);

/* Test/debug fixture: arms a one-shot synthetic stall of `micros` that is
   applied through the normal bounded catch-up path on the next BeginFrame. */
void ProjectClock_InjectElapsedMicros(unsigned long long micros);

/* Returns the number of simulation ticks due for this presentation frame,
   bounded by the catch-up cap. */
int ProjectClock_BeginFrame(void);

/* Monotonic microseconds since the last Init/Reset (not affected by pause). */
unsigned long long ProjectClock_ElapsedMicros(void);

/* Fraction of the current tick period already elapsed, in Q8 (0..256). The
   presentation layer interpolates between the previous and current tick poses
   with this value; it never feeds back into simulation state. */
int ProjectClock_GetInterpolationAlpha(void);

/* Diagnostics. The integer microsecond period is rounded down for existing
   replay metadata; the scheduler uses the exact regional rate instead. */
unsigned int ProjectClock_GetTickCount(void);
unsigned int ProjectClock_GetDroppedTicks(void);
int ProjectClock_GetTickPeriodMicros(void);

#ifdef __cplusplus
}
#endif

#endif
