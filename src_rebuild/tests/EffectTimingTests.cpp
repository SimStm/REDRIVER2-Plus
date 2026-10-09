// Keep external dependencies in the global namespace, and compile the actual
// pool/fade/palette code with internal linkage; unused render functions are omitted.
// Counter, wind, camera/hubcap and palette VRAM dependencies are substitutes.
// Live OpenGL checks exercise actual rendering separately.
#include "driver2.h"
#include <SDL.h>
short rcossin_tbl[8192] = {};
#include "../Game/C/motion_c.h"
#include "../Game/C/texture.h"
#include "../Game/C/pres.h"
#include "../Game/C/director.h"
#include "../Game/C/pause.h"
#include "../Game/C/sound.h"
#include "../Game/C/main.h"
#include "../Game/C/system.h"
#include "../Game/C/cars.h"
#include "../Game/C/camera.h"
#include "../Game/C/dr2roads.h"
#include "../Game/C/convert.h"
#include "../Game/C/map.h"
#include "../Game/C/mission.h"
#include "../Game/C/draw.h"
#include "../Game/C/models.h"
#include "../Game/C/players.h"
#include "../Game/C/shadow.h"
#include "../Game/C/cosmetic.h"
#include "../Game/C/denting.h"
#include "../Game/C/gamesnd.h"
#include "../Game/C/cell.h"
#include "../Game/C/E3stuff.h"
#include "../Game/C/glaunch.h"
#include "../Game/C/cutscene.h"
#include "../Game/C/spool.h"
#include "../Game/C/assetcatalog.h"
#include "../Game/C/civ_ai.h"
#include "../Game/C/cop_ai.h"
#include "../Game/C/felony.h"
#include "../utils/VisualSnapshot.h"
#include "../utils/ProjectClock.h"
#include "../utils/PresentationInterpolation.h"
int CameraCnt = 0, pauseflag = 0, GameLevel = 0;
int gStopPadReads = 0, bMissionTitleFade = 0, gShowMap = 0;
int gInGameCutsceneActive = 0;
char NoPlayerControl = 0;
VECTOR camera_position = {};
int LoadingArea = 0, gTimeOfDay = 0;
unsigned char NumPlayers = 1;
unsigned short texture_cluts[128][32] = {};
unsigned char tpageloaded[128] = {};
unsigned short PGXP_GetIndex(int) { return 0; }
int StoreImage(RECT16*, u_long* data) { for (int i = 0; i < 8; ++i) data[i] = i; return 0; }
void SetDrawLoad(DR_LOAD* packet, RECT16* RECT16) { setDrawLoad(packet, RECT16); }
DB* current = NULL;
void PsyX_Log_Error(const char*, ...) {}
void StepHubcap() {}
namespace {
#include "../Game/C/debris.c"
#include "../Game/C/loadview.c"
#include "../Game/C/objanim.c"
#include <string.h>
static Uint64 counter = 0;
static int checks = 0, failures = 0;
extern "C" Uint64 SDLCALL SDL_GetPerformanceCounter() { return counter; }
extern "C" Uint64 SDLCALL SDL_GetPerformanceFrequency() { return 1000000; }
extern "C" void SDLCALL SDL_Delay(Uint32) {}
void Apply_Inv_CameraMatrix(VECTOR*) {}

static void Check(bool condition, const char* message)
{
	++checks;
	if (!condition) { ++failures; printf("FAIL: %s\n", message); }
}

static void Reset()
{
	memset(smoke, 0, sizeof(smoke));
	memset(debris, 0, sizeof(debris));
	memset(leaf, 0, sizeof(leaf));
	memset(birds, 0, sizeof(birds));
	memset(previousSmoke, 0, sizeof(previousSmoke));
	CameraCnt = pauseflag = counter = 0;
	gNextRainDrop = gNumBirds = next_leaf = 0;
	next_smoke = next_debris = 1;
	main_cop_light_pos = 0;
	gEffectsTimer = 41;
	SMOKE& fume = smoke[MAX_SMOKE - 1];
	fume.flags = 6; fume.life = 90; fume.halflife = 90;
	fume.transparency = 255; fume.t_step = 1;
	fume.drift.vx = 3; fume.step = 2;
	DEBRIS& fragment = debris[MAX_DEBRIS - 1];
	fragment.flags = 2; fragment.life = 90;
	fragment.position.vy = -30000;
	fragment.direction.vx = 5;
	// Active slots after holes must still age; allocation count is not a
	// dense prefix of the fixed pool.
	bWantFade = 1; fadeVal = 255; bMissionTitleFade = 0;
	gStopPadReads = 1; gInGameCutsceneActive = 0;
	ProjectClock_Init(); ProjectClock_BeginFrame();
}

static unsigned int RunSchedule(int fps)
{
	Reset();
	const int rate = ProjectClock_GetTickRate();
	for (int frame = 1; frame <= fps * 2; ++frame)
	{
		counter = (Uint64)frame * 1000000 / fps;
		const int ticks = ProjectClock_BeginFrame();
		for (int tick = 0; tick < ticks; ++tick)
		{
			++CameraCnt; StepDebris(); StepMissionTitle();
			Check(smoke[MAX_SMOKE - 1].life == 90 - CameraCnt, "smoke lifetime follows ticks through pool holes");
			Check(debris[MAX_DEBRIS - 1].life == 90 - CameraCnt, "debris lifetime follows ticks through pool holes");
			Check(smoke[MAX_SMOKE - 1].position.vx == CameraCnt * 3, "smoke displacement follows ticks");
			Check(debris[MAX_DEBRIS - 1].position.vx == CameraCnt * 5, "debris displacement follows ticks");
		}
		SMOKE pose = smoke[MAX_SMOKE - 1];
		const unsigned int before = Debris_GetTimingDigest();
		InterpolateEffectPosition(pose.position, previousSmoke[MAX_SMOKE - 1], pose, ProjectClock_GetInterpolationAlpha());
		Check(before == Debris_GetTimingDigest(), "presentation interpolation preserves the pool");
	}
	Check(CameraCnt == rate * 2, "all regional ticks execute at low and high display rates");
	Check(gStopPadReads == 0 && bMissionTitleFade == 0, "mission fade releases input at its fixed tick");
	const unsigned int digest = Debris_GetTimingDigest();
	pauseflag = 1;
	for (int frame = 0; frame < 100; ++frame) { StepDebris(); StepMissionTitle(); }
	Check(Debris_GetTimingDigest() == digest, "paused scheduled updates preserve effects");
	return digest;
}

static void CheckPaletteCatchUp()
{
	alignas(DR_LOAD) static char primitiveBuffer[PRIMTAB_SIZE];
	OTTYPE orderingTable = {};
	DB buffer = {};
	buffer.primtab = buffer.primptr = primitiveBuffer;
	buffer.ot = &orderingTable;
	current = &buffer;
	num_cycle_obj = 1; cycle_phase = 0; cycle_timer = 0;
	cycle_tex[0].texture_page = cycle_tex[0].texture_number = 0;
	tpageloaded[0] = 1; LoadingArea = 0; GameLevel = 0;
	for (CameraCnt = 2; CameraCnt <= 8; CameraCnt += 2) ColourCycle();
	Check(orderingTable.addr == 0 && buffer.primptr == primitiveBuffer,
		"catch-up ticks never link palette packets into the draw list");
	DR_LOAD palette = cyclecluts[0];
	DrawCyclingPals(); DrawCyclingPals();
	DR_LOAD* first = (DR_LOAD*)primitiveBuffer;
	DR_LOAD* second = first + 1;
	Check(orderingTable.addr == (uintptr_t)second && second->addr == (uintptr_t)first && first->addr == 0,
		"repeated draws own distinct palette packets without a list cycle");
	Check(memcmp(&palette, &cyclecluts[0], sizeof(palette)) == 0,
		"palette drawing preserves tick-owned colour data");
	LoadingArea = 1; ColourCycle();
	char* previousEnd = buffer.primptr;
	DrawCyclingPals();
	Check(buffer.primptr == previousEnd, "area loading invalidates palette presentation history");
	LoadingArea = 0;
}

}

int main()
{
	CheckPaletteCatchUp();
	const unsigned int reference = RunSchedule(30);
	const int rates[] = { 15, 25, 60, 120, 360 };
	for (int fps : rates) Check(RunSchedule(fps) == reference, "effect state matches across frame schedules");
	// Reusing a pool slot must collapse its prior interpolation history.
	Reset(); smoke_alloc[1] = MAX_SMOKE - 1;
	previousSmoke[MAX_SMOKE - 1].flags = 6;
	Check(AllocateSmoke() == MAX_SMOKE - 1 && previousSmoke[MAX_SMOKE - 1].flags == 0, "smoke slot reuse invalidates old pose");
	printf("EffectTimingTests: %d checks, %d failures (%d Hz)\n", checks, failures, ProjectClock_GetTickRate());
	return failures ? 1 : 0;
}
