// Deterministic presentation regressions. SDL's clock is replaced here only;
// the production scheduler and snapshot implementation are compiled unchanged.
#include <SDL.h>
#include "driver2.h"
#include "C/camera.h"
#include "C/cars.h"
#include "C/main.h"
#include "C/players.h"
#include "C/event.h"
#include "C/draw.h"
#include "C/glaunch.h"
#include "C/mission.h"
#include "../utils/ProjectClock.h"
#include "../utils/VisualSnapshot.h"
#include "../utils/PresentationInterpolation.h"
#include <string.h>
#include <stdlib.h>

CAR_DATA car_data[MAX_CARS + 2] = {};
PLAYER player[MAX_PLAYERS] = {};
EventGlobal events = {};
VECTOR camera_position = {};
SVECTOR camera_angle = {};
MATRIX camera_matrix = {}, inv_camera_matrix = {}, face_camera = {}, face_camera_work = {};
int CameraCnt = 0, FrameCnt = 0, CurrentPlayerView = 0, scr_z = 256;
int switch_detail_distance = 10000, CameraCar = 0, pauseflag = 0, FastForward = 0;
int AttractMode = 0;
char NoPlayerControl = 0, camera_change = 0;
unsigned char NumPlayers = 1;

static Uint64 counter = 0, frequency = 1000003;
static bool coarseDelay = false;
static int checks = 0, failures = 0, cameraUpdates = 0;
extern "C" Uint64 SDLCALL SDL_GetPerformanceCounter(void) { return counter; }
extern "C" Uint64 SDLCALL SDL_GetPerformanceFrequency(void) { return frequency; }
extern "C" void SDLCALL SDL_Delay(Uint32 milliseconds)
{
	if (milliseconds && coarseDelay)
		counter += frequency * ((milliseconds + 15) / 16) * 16 / 1000;
	else
		counter += milliseconds ? frequency * milliseconds / 1000 : 10;
}
void SetGeomScreen(int) {}

static void Check(bool result, const char* description)
{
	++checks;
	if (!result) { ++failures; printf("FAIL: %s\n", description); }
}

static void Identity(MATRIX& m)
{
	memset(&m, 0, sizeof(m));
	for (int i = 0; i < 3; ++i) m.m[i][i] = 4096;
}

// Stand-in for the stateful original follow-camera controller. Each invocation
// advances head/angle state, so a presentation-rate leak is directly observable.
void InitCamera(PLAYER* p)
{
	++cameraUpdates;
	++p->headPos;
	const CAR_DATA& car = car_data[p->cameraCarId];
	p->cameraPos.vx = car.hd.where.t[0];
	p->cameraPos.vy = -car.hd.where.t[1] - 200;
	p->cameraPos.vz = car.hd.where.t[2] - 1000;
	camera_position = p->cameraPos;
	camera_angle.vx = camera_angle.vy = camera_angle.vz = 0;
	Identity(inv_camera_matrix);
#ifdef PAL_VERSION
	inv_camera_matrix.m[1][1] = 4300;
#endif
	Identity(camera_matrix); Identity(face_camera); Identity(face_camera_work);
	CameraCar = p->cameraCarId;
}

static void Reset()
{
	counter = 0;
	CameraCnt = FrameCnt = cameraUpdates = pauseflag = FastForward = 0;
	NumPlayers = 1; NoPlayerControl = 0; camera_change = 0; AttractMode = 0;
	memset(car_data, 0, sizeof(car_data));
	memset(player, 0, sizeof(player));
	memset(&events, 0, sizeof(events));
	for (int i = 0; i < 2; ++i)
	{
		car_data[i].controlType = CONTROL_TYPE_PLAYER;
		Identity(car_data[i].hd.where);
		player[i].cameraCarId = i;
	}
	ProjectClock_Init();
	Check(ProjectClock_BeginFrame() == 0, "first frame skips setup time");
	VisualSnapshot_Reset();
	VisualSnapshot_Capture();
	VisualSnapshot_UpdateCameras();
}

static const int rate =
#ifdef PAL_VERSION
	25;
#else
	30;
#endif

static void RunSchedule(int fps, bool irregular)
{
	Reset();
	const int seconds = 20;
	Uint64 elapsed = 0;
	long long state = 0;
	int lastRendered = -10000;
	for (int frame = 1; elapsed < frequency * seconds; ++frame)
	{
		Uint64 next = irregular ? elapsed + frequency * (frame % 5 + 1) / 220 :
			(Uint64)frame * frequency / fps;
		if (next > frequency * seconds) next = frequency * seconds;
		counter = next;
		elapsed = next;
		const int due = ProjectClock_BeginFrame();
		for (int i = 0; i < due; ++i)
		{
			VisualSnapshot_RestoreCamera();
			++CameraCnt;
			car_data[0].hd.where.t[0] += 80;
			car_data[0].hd.where.t[2] += 120;
			state += (long long)CameraCnt * car_data[0].hd.where.t[2];
			VisualSnapshot_Capture();
			VisualSnapshot_UpdateCameras();
		}
		const CAR_DATA before = car_data[0];
		const PLAYER beforePlayer = player[0];
		VisualSnapshot_ApplyCamera(0);
		int position[3];
		Check(VisualSnapshot_GetPosition(0, VisualSnapshot_GetAlpha(), position) != 0, "active car has a pose");
		Check(position[0] == camera_position.vx && position[2] - camera_position.vz == 1000,
			"camera and moving car share one time interval (no tick sawtooth)");
		Check(position[2] >= lastRendered, "constant forward motion never goes backwards at a tick boundary");
		lastRendered = position[2];
		Check(memcmp(&before, &car_data[0], sizeof(before)) == 0 &&
			memcmp(&beforePlayer, &player[0], sizeof(beforePlayer)) == 0,
			"render interpolation leaves car and player state unchanged");
		VisualSnapshot_RestoreCamera();
		Check(camera_position.vx == car_data[0].hd.where.t[0], "tick camera restored before simulation");
	}
	Check(CameraCnt == seconds * rate, "exact regional cadence with indivisible counter frequency");
	Check(cameraUpdates == CameraCnt + 1, "camera controller runs once per tick, independent of display frames");
	Check(ProjectClock_GetDroppedTicks() == 0, "normal schedules never drop ticks");
	long long expected = 0;
	for (int i = 1; i <= seconds * rate; ++i) expected += (long long)i * i * 120;
	Check(state == expected, "tick-input state is identical across presentation schedules");
	printf("PASS: %s schedule %d FPS, %d ticks\n", irregular ? "irregular" : "regular", fps, CameraCnt);
}

static void TestDiscontinuities()
{
	Reset();
	counter += frequency / rate + 1;
	ProjectClock_BeginFrame();
	++CameraCnt; car_data[0].hd.where.t[2] = 100;
	VisualSnapshot_Capture(); VisualSnapshot_UpdateCameras();
	const CAR_DATA before = car_data[0];
	pauseflag = 1;
	const int pausedUpdates = cameraUpdates;
	for (int i = 0; i < 20; ++i)
	{
		counter += frequency / 144;
		ProjectClock_BeginFrame();
		VisualSnapshot_Capture();
		VisualSnapshot_UpdateCameras();
		int p[3]; VisualSnapshot_GetPosition(0, VisualSnapshot_GetAlpha(), p);
		Check(p[2] == 100, "paused car does not cycle between snapshots");
	}
	Check(cameraUpdates == pausedUpdates, "paused gameplay does not advance the camera controller");
	Check(memcmp(&before, &car_data[0], sizeof(before)) == 0, "pause interpolation leaves physics unchanged");
	pauseflag = 0;
	++CameraCnt; car_data[0].hd.where.t[2] = 1000000;
	VisualSnapshot_Capture(); VisualSnapshot_UpdateCameras();
	int p[3]; VisualSnapshot_GetPosition(0, 128, p);
	Check(p[2] == 1000000, "teleport collapses car history");
	++CameraCnt; car_data[0].ap.model = 1; car_data[0].hd.where.t[2] += 10;
	VisualSnapshot_Capture(); VisualSnapshot_GetPosition(0, 0, p);
	Check(p[2] == 1000010, "replacement in a car slot collapses history");
	VisualSnapshot_Reset();
	Check(!VisualSnapshot_GetPosition(0, 128, p), "session reset invalidates old poses even at the same tick number");
	const PLAYER resetPlayer = player[0];
	const int resetUpdates = cameraUpdates;
	VisualSnapshot_ApplyCamera(0);
	Check(cameraUpdates == resetUpdates && memcmp(&resetPlayer, &player[0], sizeof(resetPlayer)) == 0,
		"rendering an invalid snapshot never initializes or advances gameplay camera state");
	Reset();
	NumPlayers = 2;
	car_data[1].hd.where.t[0] = 2000;
	++CameraCnt;
	VisualSnapshot_UpdateCameras();
	VisualSnapshot_ApplyCamera(0); Check(camera_position.vx == 0, "split-screen first camera remains independent");
	VisualSnapshot_ApplyCamera(1); Check(camera_position.vx == 2000, "split-screen second camera remains independent");
	Reset();
	++CameraCnt; car_data[0].hd.where.t[0] = 100;
	camera_change = 1;
	VisualSnapshot_Capture(); VisualSnapshot_UpdateCameras(); VisualSnapshot_ApplyCamera(0);
	Check(camera_position.vx == 100, "camera cut collapses history at the cut tick");
	++CameraCnt; car_data[0].hd.where.t[0] = 200; camera_change = 0;
	VisualSnapshot_Capture(); VisualSnapshot_UpdateCameras(); VisualSnapshot_ApplyCamera(0);
	Check(camera_position.vx < 200, "normal interpolation resumes immediately after the cut");
	pauseflag = 1; NoPlayerControl = 1;
	VisualSnapshot_UpdateCameras();
	Check(cameraUpdates == 4, "paused replay editor still evaluates explicit camera changes");
}

static void TestClockStalls()
{
	Reset();
	counter = frequency * 2 + frequency / (rate * 2);
	Check(ProjectClock_BeginFrame() == 4, "two-second hitch applies bounded catch-up");
	Check(ProjectClock_GetDroppedTicks() == (unsigned)(2 * rate - 4), "hitch reports exact discarded whole ticks");
	Check(abs(ProjectClock_GetInterpolationAlpha() - 128) <= 1, "hitch preserves its fractional phase");
	ProjectClock_SetPaused(1);
	counter += frequency * 10;
	Check(ProjectClock_BeginFrame() == 0, "paused clock accumulates no ticks");
	ProjectClock_SetPaused(0);
	Check(ProjectClock_BeginFrame() == 0, "resume rebases instead of catching up paused time");
	ProjectClock_InjectElapsedMicros(2000000);
	ProjectClock_Reset();
	Check(ProjectClock_BeginFrame() == 0, "reset clears pending elapsed-time fixture");
}

static void TestClockFreeze()
{
	// (a) The freeze clamps bounded catch-up, then the remaining room to the
	// armed tick, then holds the simulation at exactly that tick.
	Reset();
	ProjectClock_SetFreezeTick(5);
	ProjectClock_InjectElapsedMicros(1000000);
	Check(ProjectClock_BeginFrame() == 4, "freeze still applies bounded catch-up");
	ProjectClock_InjectElapsedMicros(1000000);
	Check(ProjectClock_BeginFrame() == 1, "freeze never steps past the armed tick");
	Check(ProjectClock_BeginFrame() == 0, "freeze holds the simulation at the armed tick");
	Check(ProjectClock_GetTickCount() == 5, "frozen tick count equals the armed tick");
	Check(ProjectClock_GetDroppedTicks() == (unsigned)(2 * (rate - 4)),
		"freeze-clamped-away ticks are an intentional freeze, not dropped ticks");
	ProjectClock_ClearFreezeTick();
	ProjectClock_InjectElapsedMicros(1000000);
	Check(ProjectClock_BeginFrame() > 0, "clearing the freeze resumes simulation time");

	// (b) A fresh session clears an armed freeze, so one capture cannot leak its
	// frozen tick into the next.
	ProjectClock_SetFreezeTick(3);
	ProjectClock_Init();
	Check(ProjectClock_BeginFrame() == 0, "first frame after Init skips setup time");
	ProjectClock_InjectElapsedMicros(1000000);
	Check(ProjectClock_BeginFrame() == 4, "Init clears an armed freeze");
	ProjectClock_SetFreezeTick(1);
	ProjectClock_Reset();
	ProjectClock_InjectElapsedMicros(1000000);
	Check(ProjectClock_BeginFrame() == 4, "Reset clears an armed freeze");
}

static void TestRotations()
{
	MATRIX a, b, out;
	Identity(a); Identity(b); Identity(out);
	// Across the 0/4096 angle seam, +10 and -10 degrees blend through identity.
	const double angle = 10 * 3.141592653589793 / 180;
	a.m[0][0] = a.m[2][2] = b.m[0][0] = b.m[2][2] = (short)(cos(angle) * 4096);
	a.m[0][2] = b.m[2][0] = (short)(sin(angle) * 4096);
	a.m[2][0] = b.m[0][2] = -a.m[0][2];
	PresentationInterpolation::Matrix(a.m, b.m, 128, out.m);
	Check(abs(out.m[0][0] - 4096) <= 2 && abs(out.m[0][2]) <= 1, "rotation takes the shortest arc across angle wrap");
	PresentationInterpolation::Matrix(a.m, b.m, 0, out.m);
	Check(memcmp(a.m, out.m, sizeof(a.m)) == 0, "previous rotation endpoint is exact");
	PresentationInterpolation::Matrix(a.m, b.m, 256, out.m);
	Check(memcmp(b.m, out.m, sizeof(b.m)) == 0, "current rotation endpoint is exact");
	Check(PresentationInterpolation::Angle(4090, 6, 128) == 0, "camera angle uses shortest wrapped arc");
	Check(PresentationInterpolation::Lerp(INT_MIN, INT_MAX, 128) == -1, "position arithmetic widens before subtracting");
	// Normalized quaternion blending preserves size; element-wise matrix lerp
	// would shrink the body halfway between these rotations.
	Identity(a); Identity(b);
	b.m[0][0] = b.m[2][2] = 0; b.m[0][2] = 4096; b.m[2][0] = -4096;
	a.m[1][1] = b.m[1][1] = 4300;
	PresentationInterpolation::Matrix(a.m, b.m, 128, out.m);
	const double rowLength = sqrt((double)out.m[0][0] * out.m[0][0] + (double)out.m[0][2] * out.m[0][2]);
	Check(fabs(rowLength - 4096) <= 1, "rotation blend preserves car size");
	Check(out.m[1][1] == 4300, "camera rotation blend preserves PAL vertical aspect scale");
}

static void TestPresentationControls()
{
	const int originalMode = ProjectClock_GetVideoMode();
	ProjectClock_SetPreferredVideoMode(originalMode == PROJECT_VIDEO_PAL ? PROJECT_VIDEO_NTSC : PROJECT_VIDEO_PAL);
	Check(ProjectClock_GetVideoMode() == originalMode, "editing region preference preserves running session");
	ProjectClock_ApplyStartupVideoMode();
	ProjectClock_Init(); ProjectClock_BeginFrame();
	Check(ProjectClock_GetTickRate() == (rate == 30 ? 25 : 30), "startup activates the selected regional rate");
	const Uint64 start = counter;
	counter += frequency;
	Check(ProjectClock_BeginFrame() == 4, "selected region still uses bounded catch-up");
	Check(ProjectClock_GetDroppedTicks() == (unsigned)(ProjectClock_GetTickRate() - 4), "runtime region changes exact scheduled tick count");
	ProjectClock_SetPreferredVideoMode(originalMode);
	ProjectClock_ApplyStartupVideoMode();

	const int caps[] = {25, 30, 60, 120, 144, 360};
	for (int i = 0; i < 6; ++i)
	{
		counter = start + frequency * (i + 2);
		ProjectClock_SetPresentationFps(caps[i] == 25 ? 30 : 25);
		ProjectClock_SetPresentationFps(caps[i]);
		ProjectClock_Init(); ProjectClock_BeginFrame();
		ProjectClock_WaitForPresentation();
		const Uint64 firstFrame = counter;
		for (int f = 0; f < caps[i] * 20; ++f)
		{
			ProjectClock_WaitForPresentation();
			ProjectClock_BeginFrame();
		}
		Check(counter >= firstFrame + frequency * 20 && counter < firstFrame + frequency * 20 + 10,
			"FPS pacer keeps exact deadlines without accumulating counter rounding drift");
		Check(ProjectClock_GetTickCount() == (unsigned)(rate * 20), "presentation pacing preserves original simulation cadence");
		const unsigned int ticks = ProjectClock_GetTickCount();
		const int alpha = ProjectClock_GetInterpolationAlpha();
		ProjectClock_SetPresentationFps(90);
		Check(ProjectClock_GetTickCount() == ticks && ProjectClock_GetInterpolationAlpha() == alpha,
			"live FPS edit does not reset simulation time or interpolation phase");
		ProjectClock_WaitForPresentation();
		counter += frequency / 10;
		const Uint64 afterVsync = counter;
		ProjectClock_WaitForPresentation();
		Check(counter == afterVsync, "an interval already spent presenting does not cause an additional wait");
	}
	counter += frequency * 2;
	ProjectClock_SetPresentationFps(30); ProjectClock_SetPresentationFps(60);
	ProjectClock_WaitForPresentation();
	const Uint64 coarseStart = counter;
	coarseDelay = true;
	for (int f = 0; f < 1200; ++f)
	{
		counter += frequency * 6 / 1000; // GPU/CPU work between paced frames
		ProjectClock_WaitForPresentation();
	}
	coarseDelay = false;
	Check(counter >= coarseStart + frequency * 20 && counter < coarseStart + frequency * 20 + frequency * 16 / 1000,
		"coarse timer wakeups do not accumulate into a permanently lower FPS rate");
	ProjectClock_SetPresentationFps(0);
	Check(ProjectClock_GetPresentationFps() == 15, "invalid FPS is clamped to supported minimum");
	ProjectClock_SetPresentationFps(9999);
	Check(ProjectClock_GetPresentationFps() == 360, "invalid FPS is clamped to supported maximum");
}

int main()
{
	const int fps[] = {25, 30, 60, 120, 144, 1000};
	for (int i = 0; i < 6; ++i) RunSchedule(fps[i], false);
	RunSchedule(0, true);
	TestClockStalls(); TestClockFreeze(); TestDiscontinuities(); TestRotations();
	TestPresentationControls();
	printf("%d checks, %d failures (%d Hz)\n", checks, failures, rate);
	return failures ? 1 : 0;
}
