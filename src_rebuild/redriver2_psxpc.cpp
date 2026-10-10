// redriver2_psxpc.cpp
//

#include <SDL_keyboard.h>

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "platform.h"

#include "driver2.h"
#include "C/main.h"
#include "C/assetcatalog_game.h"
#include "C/system.h"
#include "C/gamesnd.h"
#include "C/camera.h"
#include "C/cars.h"
#include "C/civ_ai.h"
#include "C/mission.h"
#include "C/cutscene.h"
#include "C/glaunch.h"
#include "C/loadview.h"
#include "C/overlay.h"
#include "C/players.h"
#include "C/time.h"
#include "C/draw.h"

#include "utils/ini.h"
#include "utils/DeveloperDebugStart.h"
#include "utils/DeveloperGraphicsPanel.h"
#include "utils/DeveloperInputMapping.h"
#include "utils/DeveloperModernMesh.h"
#include "utils/DeveloperVkFixture.h"
#include "utils/NativeCameraView.h"
#include "utils/NativeInstanceTransform.h"
#include "utils/DeterministicBaseline.h"
#include "utils/HdTextureOverrides.h"
#include "utils/InputService.h"
#include "utils/ProjectClock.h"

#ifdef RED2_AUDIO_FMOD
#include "Game/C/audiofmod_adapter.h"
#include "utils/AudioService.h"
#endif

#include <SDL_scancode.h>
#include <SDL_gamecontroller.h>
#include <SDL_messagebox.h>
#include <SDL_surface.h>
#include <SDL_timer.h>

#ifdef _WIN32
#include <SDL_syswm.h>
#include "NativeRenderer.h"
#endif

#include "PsyX/PsyX_globals.h"

// The live GTE control registers (OFX/OFY), so the native projection can apply
// the game's SetGeomOffset exactly like the legacy GTE screen mapping. PsyCross
// exposes them as the `gteRegs` control-register block; the game headers do not
// pull it in, so the bridge includes it directly.
#include "psx/gtereg.h"


int(*GPU_printf)(const char *fmt, ...);

bool CtrlModifier;
bool ShiftModifier;
bool AltModifier;

void CheckModifierKeys(int nKey, bool down)
{
	switch (nKey)
	{
	case SDL_SCANCODE_LCTRL:
		CtrlModifier = down;
		break;
	case SDL_SCANCODE_LSHIFT:
		ShiftModifier = down;
		break;
	case SDL_SCANCODE_LALT:
		AltModifier = down;
		break;
	}
}

extern int gDrawDistance;
extern int g_FreeCameraEnabled;

void FreeCameraMouseHandler(int x, int y, int dx, int dy)
{
	PsyX_SetCursorRelative(g_FreeCameraEnabled);
	if (g_FreeCameraEnabled)
	{
		extern SVECTOR g_FreeCameraRotation;

		int width, height;
		PsyX_GetScreenSize(&width, &height);

		PsyX_SetCursorPosition(width / 2, height / 2);

		g_FreeCameraRotation.vy -= dx;
		g_FreeCameraRotation.vx += dy;
	}
}

void FreeCameraKeyboardHandler(int nKey, char down)
{
	if (g_FreeCameraEnabled)
	{
		extern int g_FreeCameraControls;
		if (nKey == SDL_SCANCODE_W)
		{
			if (down)
				g_FreeCameraControls |= 0x1;
			else
				g_FreeCameraControls &= ~0x1;
		}
		else if (nKey == SDL_SCANCODE_S)
		{
			if (down)
				g_FreeCameraControls |= 0x2;
			else
				g_FreeCameraControls &= ~0x2;
		}
		else if (nKey == SDL_SCANCODE_A)
		{
			if (down)
				g_FreeCameraControls |= 0x4;
			else
				g_FreeCameraControls &= ~0x4;
		}
		else if (nKey == SDL_SCANCODE_D)
		{
			if (down)
				g_FreeCameraControls |= 0x8;
			else
				g_FreeCameraControls &= ~0x8;
		}
	}

	if(!down)
		return;

	if (nKey == SDL_SCANCODE_F7)
	{
		g_FreeCameraEnabled ^= 1;
		printf("Free camera: %s\n", g_FreeCameraEnabled ? "ON" : "OFF");
	}
}

int gShowCollisionDebug = 0;

#if defined(DEBUG_OPTIONS) || defined(_DEBUG)

extern int gDisplayPosition;
extern int gDisplayDrawStats;
int gStopCivCars = 0;

extern void FunkUpDaBGMTunez(int funk);

void GameDebugKeys(int nKey, char down)
{
	CheckModifierKeys(nKey, down);
	FreeCameraKeyboardHandler(nKey, down);

	if (!down)
		return;

	if (nKey == SDL_SCANCODE_1)
	{
		gDisplayPosition ^= 1;
		printf("Position display %s\n", gDisplayPosition ? "ON" : "OFF");
	}
	else if (nKey == SDL_SCANCODE_2)
	{
		gShowCollisionDebug++;
		gShowCollisionDebug %= 5;

		printf("Collision debug: %d\n", gShowCollisionDebug);
	}
	else if (nKey == SDL_SCANCODE_3)
	{
		gDisplayDrawStats ^= 1;
		printf("Stats %s\n", gDisplayDrawStats ? "ON" : "OFF");
	}
	else if (nKey == SDL_SCANCODE_BACKSPACE)
	{
		extern int FastForward;
		FastForward = 1;
	}
#ifdef _DEBUG
	else if (nKey == SDL_SCANCODE_0)
	{
		gStopCivCars ^= 1;
		printf("Civ cars stop %s\n", gStopCivCars ? "ON" : "OFF");
	}
	else if (nKey == SDL_SCANCODE_PAGEUP)
	{
		MainPlayer.cameraCarId++;

		if (MainPlayer.cameraCarId > MAX_CARS - 1)
			MainPlayer.cameraCarId = MAX_CARS - 1;

		printf("Car on camera: %d\n", MainPlayer.cameraCarId);
	}
	else if (nKey == SDL_SCANCODE_PAGEDOWN)
	{
		MainPlayer.cameraCarId--;

		if (MainPlayer.cameraCarId < 0)
			MainPlayer.cameraCarId = 0;

		printf("Car on camera: %d\n", MainPlayer.cameraCarId);
	}
	else if (nKey == SDL_SCANCODE_KP_DIVIDE)
	{
		FunkUpDaBGMTunez(0);
	}
	else if (nKey == SDL_SCANCODE_KP_MULTIPLY)
	{
		FunkUpDaBGMTunez(1);
		extern void CreateRoadblock();
		CreateRoadblock();
	}
	else if (nKey == SDL_SCANCODE_KP_PLUS)
	{
		extern LEAD_PARAMETERS LeadValues;

		static bool already = false;
		static bool leadReady = false;
		static int lastmodel = 1;

		/*
			---- LEAD VALUES ----
			500 1000 428 21 885 10 487 2
			700 350 1731 30 320 7 225 3
		*/

		// Find the clue
		LeadValues.tEnd			= 500;	LeadValues.tAvelLimit	= 1000;
		LeadValues.tDist		= 428;	LeadValues.tDistMul		= 21;
		LeadValues.tWidth		= 885;	LeadValues.tWidthMul	= 10;
		LeadValues.tWidth80		= 487;	LeadValues.tWidth80Mul	= 2;

		LeadValues.hEnd			= 700;	LeadValues.dEnd			= 350;
		LeadValues.hDist		= 1731; LeadValues.hDistMul		= 30;
		LeadValues.hWidth		= 320;	LeadValues.hWidthMul	= 7;
		LeadValues.hWidth80		= 225;	LeadValues.hWidth80Mul	= 3;

		CAR_DATA *pCar = &car_data[MainPlayer.playerCarId];

		LONGVECTOR4 startpos = {
			pCar->hd.where.t[0],
			pCar->hd.where.t[1],
			pCar->hd.where.t[2],
		};

		int dir = pCar->hd.direction;
		int offset = 1000 + (pCar->hd.speed * 36);
		int heading = (((dir + 512) & 0xfff) >> 10) + 2 & 3;

		int which = (heading & 1) ? 0 : 2;

		int model = lastmodel;
		int palette = 1;

		if (ShiftModifier)
		{
			// force a special car
			model = residentCarModels[SPECIAL_CAR_SLOT];
			palette = 0;
			lastmodel = model;
		}
		else
		{
			for (int i = 0; i < 5; i++)
			{
				// horrible randomness
				int newmodel = residentCarModels[i];

				if (newmodel == 0 || newmodel == lastmodel)
					continue;

				if (newmodel != model)
				{
					int tmp = CameraCnt % (newmodel + 1);

					if (tmp & 4)
					{
						// rare chance of spawning special car
						model = residentCarModels[SPECIAL_CAR_SLOT];
						palette = 0;
						lastmodel = model;
						break;
					}
					else if (tmp & 3)
					{
						model = newmodel;
						palette = CameraCnt % 6;
						lastmodel = model;
						break;
					}
				}
			}
		}

		// add a little more room
		if (model > 4)
			offset += 250;

		// spawn it in front of us
		if (ABS(dir) >= 750)
		{
			if ((heading >> 1) != 1)
				offset = -offset;
		}

		startpos[which] += offset;

		int slot = CreateStationaryCivCar(dir, 0, 0, &startpos, model, palette, 0);

		if (slot == -1)
		{
			slot = PingInCivCar(15900);
			slot--;
		}

		if (slot != -1)
		{
			char playerid = 0xff;

			InitPlayer((PLAYER *)(player + 1), &car_data[slot], 4, dir, &startpos, model, palette, &playerid);
			already = true;
		}
		else
		{
			requestStationaryCivCar = 1;
		}
	}
	else if (nKey == SDL_SCANCODE_KP_MINUS)
	{
		PingOutAllCivCarsAndCopCars();
	}
#endif
}
#endif

#if !USE_CRT_MALLOC
char g_Overlay_buffer[0x50000];		// 0x1C0000
char g_Frontend_buffer[0x60000];	// 0xFB400
char g_Other_buffer[0x50000];		// 0xF3000
char g_Other_buffer2[0x50000];		// 0xE7000
OTTYPE g_OT1[OTSIZE];				// 0xF3000
OTTYPE g_OT2[OTSIZE];				// 0xF7200
char g_PrimTab1[PRIMTAB_SIZE];		// 0xFB400
char g_PrimTab2[PRIMTAB_SIZE];		// 0x119400
char g_SBank_buffer[0x50000];		// 0x180000
char g_Replay_buffer[0x50000];		// 0x1fABBC
#endif

void ParseKeyboardMappings(ini_t* config, char* section, InputKeyboardMapping& outMapping)
{
	// since it is configured by default
	const PsyXKeyboardMapping& defaultMapping = g_cfg_keyboardMapping;

	const char* str;

	str = ini_get(config, section, "square");
	outMapping.kc_square = PsyX_LookupKeyboardMapping(str, defaultMapping.kc_square);

	str = ini_get(config, section, "circle");
	outMapping.kc_circle = PsyX_LookupKeyboardMapping(str, defaultMapping.kc_circle);

	str = ini_get(config, section, "triangle");
	outMapping.kc_triangle = PsyX_LookupKeyboardMapping(str, defaultMapping.kc_triangle);

	str = ini_get(config, section, "cross");
	outMapping.kc_cross = PsyX_LookupKeyboardMapping(str, defaultMapping.kc_cross);

	str = ini_get(config, section, "l1");
	outMapping.kc_l1 = PsyX_LookupKeyboardMapping(str, defaultMapping.kc_l1);

	str = ini_get(config, section, "l2");
	outMapping.kc_l2 = PsyX_LookupKeyboardMapping(str, defaultMapping.kc_l2);

	str = ini_get(config, section, "l3");
	outMapping.kc_l3 = PsyX_LookupKeyboardMapping(str, defaultMapping.kc_l3);

	str = ini_get(config, section, "r1");
	outMapping.kc_r1 = PsyX_LookupKeyboardMapping(str, defaultMapping.kc_r1);

	str = ini_get(config, section, "r2");
	outMapping.kc_r2 = PsyX_LookupKeyboardMapping(str, defaultMapping.kc_r2);

	str = ini_get(config, section, "r3");
	outMapping.kc_r3 = PsyX_LookupKeyboardMapping(str, defaultMapping.kc_r3);

	str = ini_get(config, section, "up");
	outMapping.kc_dpad_up = PsyX_LookupKeyboardMapping(str, defaultMapping.kc_dpad_up);

	str = ini_get(config, section, "down");
	outMapping.kc_dpad_down = PsyX_LookupKeyboardMapping(str, defaultMapping.kc_dpad_down);

	str = ini_get(config, section, "left");
	outMapping.kc_dpad_left = PsyX_LookupKeyboardMapping(str, defaultMapping.kc_dpad_left);

	str = ini_get(config, section, "right");
	outMapping.kc_dpad_right = PsyX_LookupKeyboardMapping(str, defaultMapping.kc_dpad_right);

	str = ini_get(config, section, "select");
	outMapping.kc_select = PsyX_LookupKeyboardMapping(str, defaultMapping.kc_select);

	str = ini_get(config, section, "start");
	outMapping.kc_start = PsyX_LookupKeyboardMapping(str, defaultMapping.kc_start);
}

void ParseControllerMappings(ini_t* config, char* section, InputControllerMapping& outMapping)
{
	const PsyXControllerMapping& defaultMapping = g_cfg_controllerMapping;

	const char* str;

	str = ini_get(config, section, "square");
	outMapping.gc_square = PsyX_LookupGameControllerMapping(str, defaultMapping.gc_square);

	str = ini_get(config, section, "circle");
	outMapping.gc_circle = PsyX_LookupGameControllerMapping(str, defaultMapping.gc_circle);

	str = ini_get(config, section, "triangle");
	outMapping.gc_triangle = PsyX_LookupGameControllerMapping(str, defaultMapping.gc_triangle);

	str = ini_get(config, section, "cross");
	outMapping.gc_cross = PsyX_LookupGameControllerMapping(str, defaultMapping.gc_cross);

	str = ini_get(config, section, "l1");
	outMapping.gc_l1 = PsyX_LookupGameControllerMapping(str, defaultMapping.gc_l1);

	str = ini_get(config, section, "l2");
	outMapping.gc_l2 = PsyX_LookupGameControllerMapping(str, defaultMapping.gc_l2);

	str = ini_get(config, section, "l3");
	outMapping.gc_l3 = PsyX_LookupGameControllerMapping(str, defaultMapping.gc_l3);

	str = ini_get(config, section, "r1");
	outMapping.gc_r1 = PsyX_LookupGameControllerMapping(str, defaultMapping.gc_r1);

	str = ini_get(config, section, "r2");
	outMapping.gc_r2 = PsyX_LookupGameControllerMapping(str, defaultMapping.gc_r2);

	str = ini_get(config, section, "r3");
	outMapping.gc_r3 = PsyX_LookupGameControllerMapping(str, defaultMapping.gc_r3);

	str = ini_get(config, section, "up");
	outMapping.gc_dpad_up = PsyX_LookupGameControllerMapping(str, defaultMapping.gc_dpad_up);

	str = ini_get(config, section, "down");
	outMapping.gc_dpad_down = PsyX_LookupGameControllerMapping(str, defaultMapping.gc_dpad_down);

	str = ini_get(config, section, "left");
	outMapping.gc_dpad_left = PsyX_LookupGameControllerMapping(str, defaultMapping.gc_dpad_left);

	str = ini_get(config, section, "right");
	outMapping.gc_dpad_right = PsyX_LookupGameControllerMapping(str, defaultMapping.gc_dpad_right);

	str = ini_get(config, section, "select");
	outMapping.gc_select = PsyX_LookupGameControllerMapping(str, defaultMapping.gc_select);

	str = ini_get(config, section, "start");
	outMapping.gc_start = PsyX_LookupGameControllerMapping(str, defaultMapping.gc_start);

	str = ini_get(config, section, "axis_left_x");
	outMapping.gc_axis_left_x = PsyX_LookupGameControllerMapping(str, defaultMapping.gc_axis_left_x);

	str = ini_get(config, section, "axis_left_y");
	outMapping.gc_axis_left_y = PsyX_LookupGameControllerMapping(str, defaultMapping.gc_axis_left_y);

	str = ini_get(config, section, "axis_right_x");
	outMapping.gc_axis_right_x = PsyX_LookupGameControllerMapping(str, defaultMapping.gc_axis_right_x);

	str = ini_get(config, section, "axis_right_y");
	outMapping.gc_axis_right_y = PsyX_LookupGameControllerMapping(str, defaultMapping.gc_axis_right_y);
}

// config.ini freeCamera installs the debug camera handlers while the game
// initialises. The value is kept here so the developer panel can show it as a
// disabled, restart-only option.
int gFreeCameraConfiguration = 0;

InputKeyboardMapping g_kbGameMappings = { 0x123 };
InputKeyboardMapping g_kbMenuMappings = { 0x456 };

InputControllerMapping g_gcGameMappings = { 0x321 };
InputControllerMapping g_gcMenuMappings = { 0x654 };

void SwitchMappings(int menu)
{
	if(menu)
	{
		g_inputKeyboardMapping = g_kbMenuMappings;
		g_inputControllerMapping = g_gcMenuMappings;
	}
	else
	{
		g_inputKeyboardMapping = g_kbGameMappings;
		g_inputControllerMapping = g_gcGameMappings;
	}

	// The developer panel edits these tables; it must know which one the game is
	// using so an edit only reaches the mapping that is live.
	DeveloperInputMapping_NotifyActive(menu ? DeveloperInputTable::Menu : DeveloperInputTable::Game);
}

#ifdef _WIN32
// E03: with RED2_NATIVE_RENDERER set the game runs PsyCross headless (no
// swapchain) and the native renderer owns the window's presentation. Its scene
// is drawn from a post-frame handler with the game's own camera, so the frame
// stays in step with the simulation. The default PsyCross path is untouched.
// The view matrix is the game's raw-unit world->camera matrix, composed by the
// pure NativeCameraView helper from inv_camera_matrix and camera_position.
static int s_nativeRendererActive = 0;
static int s_nativeRendererFrames = 0;
static int s_nativeRendererFailureStreak = 0;
static int s_nativeRendererWidth = 0;
static int s_nativeRendererHeight = 0;

// Bounded per-instance native draw driven by `-drawmodel <slot> [count]`. The
// resource is registered once from the published-resource registry and the
// collected placements are submitted each frame; a missing resource or an empty
// placement set leaves the existing content/placeholder path untouched.
static int s_nativeDrawRegistered = 0;
static char s_nativeDrawRegisteredKey[CONVERSION_CACHE_KEY_CAPACITY];
static int s_nativeDrawInstancesSet = 0;
static int s_nativeDrawReported = 0;
static int s_nativeDrawWaitingLogged = 0;
static int s_nativeDrawValidFrames = 0;

// Full-region native draw driven by `-drawregion`: every converted resource is
// registered once and every resident placement is submitted as one instance with
// the game camera. The instance buffer is sized to the renderer's bound and is
// static (BSS) because a full 8192-entry NativeInstance buffer (~1.9 MB) would
// overflow the stack; the per-frame copies stay on the stack.
static_assert(ASSET_CATALOG_GAME_MAX_REGION_PLACEMENTS <= NATIVE_MAX_INSTANCES,
	"the region placement store must fit the native instance bound");

static int s_regionDrawRegistered = 0;
static int s_regionDrawRegisteredCount = 0;
static char s_regionDrawRegisteredKeys[ASSET_CATALOG_GAME_MAX_MODEL_RESOURCES][CONVERSION_CACHE_KEY_CAPACITY];
static int s_regionDrawInstancesSet = 0;
static int s_regionDrawReported = 0;
static int s_regionDrawWaitingLogged = 0;
static int s_regionDrawValidFrames = 0;
static int s_regionDrawSubmittedCount = 0;
static NativeInstance s_regionDrawInstances[NATIVE_MAX_INSTANCES];

// E03 visual-parity P9 LOD tier: both resource keys per instance (the detail
// model and its distinct LOD variant, when the placement has one) plus the last
// per-instance selection. The instance transforms are built once, because they
// are camera-independent world placements; only the selected resourceKey
// changes, and only when the per-frame camera-space Z crosses DRAW_LOD_DIST_LOW
// for at least one placement. The selection is the legacy world-draw rule
// (draw.c:1041 buildings, tile.c:304 tiles).
//
// E03 visual-parity P10a high-detail tier: a TILE-classified placement also
// carries its high-detail sibling key (Low2HighDetailTable, tile.c:290-295) and
// selects it while camera-space Z <= DRAW_LOD_DIST_HIGH. Buildings never use
// this tier, so their high-detail key is never built.
static char s_regionDrawDetailKeys[NATIVE_MAX_INSTANCES][NATIVE_RESOURCE_KEY_CAPACITY];
static char s_regionDrawLodKeys[NATIVE_MAX_INSTANCES][NATIVE_RESOURCE_KEY_CAPACITY];
static char s_regionDrawHighDetailKeys[NATIVE_MAX_INSTANCES][NATIVE_RESOURCE_KEY_CAPACITY];
static unsigned char s_regionDrawHasLod[NATIVE_MAX_INSTANCES];
static unsigned char s_regionDrawHasHighDetail[NATIVE_MAX_INSTANCES];
static unsigned char s_regionDrawLodSelected[NATIVE_MAX_INSTANCES];
static unsigned char s_regionDrawHighDetailSelected[NATIVE_MAX_INSTANCES];
static int s_regionDrawKeysBuilt = 0;
static int s_regionDrawSelectionValid = 0;
static int s_regionDrawLodSelectedCount = 0;
static int s_regionDrawHighDetailSelectedCount = 0;

// Last level generation each dev-draw mode observed. The registry can be
// repopulated in the same frame as the level boundary (BeginLevel ->
// SpoolSYNC -> Prepare*), so the bridge resets on a generation change rather
// than relying on the registry being transiently empty.
static unsigned int s_regionDrawGeneration = 0;
static unsigned int s_nativeDrawGeneration = 0;

// E1 diagnosis: RED2_NATIVE_REGION_DELAY_MS defers the region prepare out of
// SpoolSYNC (main.c skips it) and runs it here once, that many milliseconds
// after the bridge's first post-frame callback. The clock starts on the first
// callback, the same frame the captureAfterSeconds timer starts, so a delay
// just below the capture delay bakes the region shortly before the capture.
// Absent/0 keeps the immediate SpoolSYNC prepare and changes nothing.
static int s_regionDrawDelayMs = -1; // -1 until parsed
static int s_regionDrawDelayArmed = 0;
static int s_regionDrawDelayPrepared = 0;
static unsigned int s_regionDrawDelayStartTicks = 0;
static unsigned int s_regionDrawDelayGeneration = 0;

// Raw clip planes shared by SetClipPlanes and the visibility selection.
static const float kNativeDrawClipNear = 100.0f;
static const float kNativeDrawClipFar = 125000.0f;

// The live GTE screen distance h the native camera uses (SetGeomScreen(scr_z),
// camera.c). `scr_z` is `extern int scr_z` (Game/C/main.h:23, defined
// `int scr_z = 0` in main.c:148); the camera/director update it each frame, but
// it can still be 0 before the first scene build, so fall back to
// gCameraDefaultScrZ (`extern short`, Game/C/camera.h:31, default 256).
static float NativeDrawScreenDistance(void)
{
	const int live = scr_z;
	return live > 0 ? (float)live : (float)gCameraDefaultScrZ;
}

// The documented game default SetGeomOffset(160,128) (Game/C/main.c:2084), used
// only while the live GTE registers are still at the InitGeom() zero state,
// i.e. before the game's SetGeomOffset has run.
static const float kNativeDrawDefaultOffsetX = 160.0f;
static const float kNativeDrawDefaultOffsetY = 128.0f;

// The live GTE screen offset in PSX 320x240 pixels, the same OFX/OFY the legacy
// GTE screen mapping uses (x_screen = ofx + x*h/z). SetGeomOffset stores them as
// 16.16 fixed point (C2_OFX = ofx << 16, LIBGTE.C:53-57), so the integer screen
// offset is the register shifted right by 16. The game sets (160,128) at init
// (main.c:2084) and fmvplay does too, motion_c temporarily uses (32,60) then
// restores it, and system.c uses (320/2, height/2) only for split-screen, so the
// live registers are the single source of truth. InitGeom() zeroes both
// (LIBGTE.C:49-50); that exact pre-SetGeomOffset state is treated as the
// documented default rather than an offset of (0,0), which would shift the whole
// image off-centre.
static void NativeDrawScreenOffset(float* offsetX, float* offsetY)
{
	const int ofx = gteRegs.CP2C.n.ofx >> 16;
	const int ofy = gteRegs.CP2C.n.ofy >> 16;
	const int unset = (ofx == 0 && ofy == 0);
	*offsetX = unset ? kNativeDrawDefaultOffsetX : (float)ofx;
	*offsetY = unset ? kNativeDrawDefaultOffsetY : (float)ofy;
}

// The offscreen dump size. The visibility ranking uses the dump's aspect: the
// native horizontal scale follows the render target aspect, so a square dump
// narrows the horizontal field of view relative to the dump's vertical one.
static const int kNativeDrawDumpWidth = 640;
static const int kNativeDrawDumpHeight = 360;
static const float kNativeDrawDumpAspect = (float)kNativeDrawDumpWidth / (float)kNativeDrawDumpHeight;

// The one-shot report waits this many valid-camera frames before it fires, so a
// transient early camera state cannot produce a meaningless report or dump.
static const int kNativeDrawReportDelayFrames = 3;

// The submitted order (nearest/visible first) and its camera-space z evidence,
// captured by DevDrawSubmit and read by the one-shot report.
static int s_nativeDrawOrder[ASSET_CATALOG_GAME_MAX_DRAW_PLACEMENTS];
static int s_nativeDrawSubmittedCount = 0;
static float s_nativeDrawFirstZ = 0.0f;
static float s_nativeDrawMinZ = 0.0f;
static float s_nativeDrawMaxZ = 0.0f;
static int s_nativeDrawInFrustum = 0;
static int s_nativeDrawLaterallyVisible = 0;

// Drops the full-region draw's GPU registrations and per-mode report state so a
// new level re-registers and re-reports. Instance transforms are world-space,
// so the previous level's list is cleared before it can be drawn under the new
// camera.
static void NativeRenderer_ResetRegionDrawState(void)
{
	if (s_regionDrawInstancesSet)
	{
		NativeRenderer_ClearInstances();
		s_regionDrawInstancesSet = 0;
	}
	for (int i = 0; i < s_regionDrawRegisteredCount; ++i)
		NativeRenderer_UnregisterResource(s_regionDrawRegisteredKeys[i]);
	s_regionDrawRegistered = 0;
	s_regionDrawRegisteredCount = 0;
	s_regionDrawReported = 0;
	s_regionDrawWaitingLogged = 0;
	s_regionDrawValidFrames = 0;
	s_regionDrawSubmittedCount = 0;
	s_regionDrawKeysBuilt = 0;
	s_regionDrawSelectionValid = 0;
	s_regionDrawLodSelectedCount = 0;
	s_regionDrawHighDetailSelectedCount = 0;
}

// Drops the bounded `-drawmodel` draw's registration and per-mode report state.
static void NativeRenderer_ResetNativeDrawState(void)
{
	if (s_nativeDrawInstancesSet)
	{
		NativeRenderer_ClearInstances();
		s_nativeDrawInstancesSet = 0;
	}
	if (s_nativeDrawRegistered)
	{
		NativeRenderer_UnregisterResource(s_nativeDrawRegisteredKey);
		s_nativeDrawRegistered = 0;
		s_nativeDrawRegisteredKey[0] = '\0';
	}
	s_nativeDrawReported = 0;
	s_nativeDrawWaitingLogged = 0;
	s_nativeDrawValidFrames = 0;
	s_nativeDrawSubmittedCount = 0;
}

// Raw camera-space position of a placement under the composed view, using the
// same inputs the bridge feeds the camera (view = [R | -R*cam]).
static void CameraSpace(const float view[16], int x, int y, int z, float out[3])
{
	const float fx = (float)x, fy = (float)y, fz = (float)z;
	out[0] = view[0] * fx + view[4] * fy + view[8] * fz + view[12];
	out[1] = view[1] * fx + view[5] * fy + view[9] * fz + view[13];
	out[2] = view[2] * fx + view[6] * fy + view[10] * fz + view[14];
}

// Integer camera-space Z exactly as the legacy world draw computes it for the
// LOD threshold (Game/ASM/rndrasm.c Apply_InvCameraMatrixSetTrans):
//   local = (short)(pos - camera_position)
//   vec   = FIXED(inv_camera_matrix * local)
//   Z     = vec.vx >> 1 < 0 ? vec.vz - vec.vx : vec.vz + vec.vx
// The matrix and camera position are the same globals the live view is composed
// from (NativeRenderer_ComposeLiveView), so the LOD selection and the drawn
// camera always agree. The legacy uses this Z at draw.c:1041 (buildings) and
// tile.c:304 (tiles) to choose pLodModels over modelpointers.
static int LegacyPlacementZ(const AssetCatalogGameDrawPlacement& placement)
{
	const MATRIX& lc = inv_camera_matrix;
	const short lx = (short)(placement.x - camera_position.vx);
	const short ly = (short)(placement.y - camera_position.vy);
	const short lz = (short)(placement.z - camera_position.vz);

	const int vx = FIXED(lc.m[0][0] * lx + lc.m[0][1] * ly + lc.m[0][2] * lz);
	const int vz = FIXED(lc.m[2][0] * lx + lc.m[2][1] * ly + lc.m[2][2] * lz);

	return (vx >> 1 < 0) ? vz - vx : vz + vx;
}

// True when a camera-space point is inside the dumped frame: in front of the
// camera, within the clip range and inside the vertical/horizontal field of
// view for the dump aspect. `zInRange` optionally reports the depth-range test
// alone, so the log can separate "in depth range" from "actually on screen".
// The native vertical focal is 2h/NATIVE_PSX_DISPLAY_HEIGHT, so tanHalfY is
// NATIVE_PSX_DISPLAY_HEIGHT/(2h); the horizontal follows the dump aspect.
static int PlacementOnScreen(const float cam[3], int* zInRange)
{
	const int inRange = cam[2] >= kNativeDrawClipNear && cam[2] <= kNativeDrawClipFar;
	if (zInRange)
		*zInRange = inRange;

	if (!inRange)
		return 0;

	const float h = NativeDrawScreenDistance();
	const float tanHalfY = NATIVE_PSX_DISPLAY_HEIGHT / (2.0f * h);
	const float tanHalfX = tanHalfY * kNativeDrawDumpAspect;
	return fabsf(cam[0]) <= tanHalfX * cam[2] && fabsf(cam[1]) <= tanHalfY * cam[2];
}

// Ranks the collected placements so the truly visible ones come first, then the
// nearest camera-space ones, and returns how many to submit (the bounded K, at
// least one for any non-empty input). The selection is an explicit O(n^2) pick
// because the bound is small. Camera-space z evidence and the in-range /
// on-screen counts are reported for the submitted set.
static int RankPlacements(const AssetCatalogGameDrawPlacement* placements, int count,
	const float view[16], int bound, int order[ASSET_CATALOG_GAME_MAX_DRAW_PLACEMENTS],
	float* firstZ, float* minZ, float* maxZ, int* inFrustum, int* laterallyVisible)
{
	const int submitCount = count < bound ? count : bound;
	if (submitCount <= 0)
		return 0;

	unsigned char used[ASSET_CATALOG_GAME_MAX_DRAW_PLACEMENTS];
	memset(used, 0, sizeof(used));

	for (int slot = 0; slot < submitCount; ++slot)
	{
		int best = -1;
		int bestOnScreen = 0;
		float bestDistSq = 0.0f;

		for (int i = 0; i < count; ++i)
		{
			if (used[i])
				continue;

			float cam[3];
			CameraSpace(view, placements[i].x, placements[i].y, placements[i].z, cam);
			const int onScreen = PlacementOnScreen(cam, NULL);
			const float distSq = cam[0] * cam[0] + cam[1] * cam[1] + cam[2] * cam[2];

			// Prefer truly visible placements, then the nearest one.
			const int better = best < 0 ||
				(onScreen && !bestOnScreen) ||
				(onScreen == bestOnScreen && distSq < bestDistSq);
			if (better)
			{
				best = i;
				bestOnScreen = onScreen;
				bestDistSq = distSq;
			}
		}

		used[best] = 1;
		order[slot] = best;
	}

	float zFirst = 0.0f, zMin = 0.0f, zMax = 0.0f;
	int inRangeCount = 0;
	int onScreenCount = 0;
	for (int slot = 0; slot < submitCount; ++slot)
	{
		float cam[3];
		const AssetCatalogGameDrawPlacement& p = placements[order[slot]];
		CameraSpace(view, p.x, p.y, p.z, cam);

		if (slot == 0)
		{
			zFirst = cam[2];
			zMin = cam[2];
			zMax = cam[2];
		}
		else
		{
			if (cam[2] < zMin) zMin = cam[2];
			if (cam[2] > zMax) zMax = cam[2];
		}

		int zInRange = 0;
		if (PlacementOnScreen(cam, &zInRange))
			onScreenCount++;
		if (zInRange)
			inRangeCount++;
	}

	*firstZ = zFirst;
	*minZ = zMin;
	*maxZ = zMax;
	*inFrustum = inRangeCount;
	*laterallyVisible = onScreenCount;
	return submitCount;
}

// E1 diagnosis: parses RED2_NATIVE_REGION_DELAY_MS once. A non-positive or
// malformed value is 0, which selects the immediate SpoolSYNC prepare, so a
// normal run is unchanged.
static int NativeRenderer_RegionDrawDelayMs(void)
{
	if (s_regionDrawDelayMs < 0)
	{
		const char* value = getenv("RED2_NATIVE_REGION_DELAY_MS");
		int parsed = 0;

		if (value && value[0])
		{
			parsed = atoi(value);
			if (parsed < 0)
				parsed = 0;
		}

		s_regionDrawDelayMs = parsed;
	}

	return s_regionDrawDelayMs;
}

// E1 diagnosis: runs the deferred region prepare once the configured delay has
// elapsed since the bridge's first frame. Returns 1 while the caller must not
// submit (the prepare is still pending, or it just ran this frame and the next
// frame will register the fresh resources) and 0 when the immediate-prepare
// path applies (no delay configured). Warn-only: a failed prepare leaves a
// named diagnostic and never interrupts the game.
static int NativeRenderer_RegionDrawPrepareDeferred(void)
{
	const int delayMs = NativeRenderer_RegionDrawDelayMs();
	if (delayMs <= 0)
		return 0;

	// A level boundary clears the placement store; re-prepare so a reload after
	// the delay has elapsed bakes again on its first frame instead of drawing an
	// empty region. The clock is not restarted, so the original frame-relative
	// timing is preserved.
	const unsigned int generation = AssetCatalogGame_LevelGeneration();
	if (generation != s_regionDrawDelayGeneration)
	{
		s_regionDrawDelayPrepared = 0;
		s_regionDrawDelayGeneration = generation;
	}

	if (s_regionDrawDelayPrepared)
		return 0;

	const unsigned int now = SDL_GetTicks();

	if (!s_regionDrawDelayArmed)
	{
		s_regionDrawDelayArmed = 1;
		s_regionDrawDelayStartTicks = now;
		PsyX_Log_Info("native region draw: deferred prepare armed for %d ms after the first frame\n",
			delayMs);
		return 1;
	}

	const unsigned int elapsed = now - s_regionDrawDelayStartTicks;
	if (elapsed < (unsigned int)delayMs)
		return 1;

	s_regionDrawDelayPrepared = 1;

	char status[512];
	if (AssetCatalogGame_PrepareRegionDraw(status, sizeof(status)))
		PsyX_Log_Info("native region draw: deferred prepare at %u ms: %s\n", elapsed, status);
	else
		PsyX_Log_Warning("native region draw: deferred prepare at %u ms: %s\n", elapsed, status);

	return 1;
}

// Full-region submit: register every converted resource once, build the full
// instance list once from the prepared region placements, and re-submit it only
// when the per-frame LOD selection changes. The placements are camera-independent
// world transforms, so they are never re-collected; only the selected resourceKey
// changes (E03 visual-parity P9). A level-change generation bump drops the stale
// list and registrations.
static int NativeRenderer_DevRegionDrawSubmit(const float view[16])
{
	(void)view;

	// E1 diagnosis: when main.c deferred the region prepare out of SpoolSYNC,
	// run it here once the delay elapses. Until then there is nothing to submit.
	if (NativeRenderer_RegionDrawPrepareDeferred())
		return 0;

	// A level change bumps the generation. Reset before re-registering so the
	// previous level's instances and resources never draw under the new camera,
	// even though the registry is already repopulated by the same frame's
	// preparation. The registry-empty guard stays as a fallback for a failed
	// preparation.
	const unsigned int generation = AssetCatalogGame_LevelGeneration();
	if (generation != s_regionDrawGeneration)
	{
		NativeRenderer_ResetRegionDrawState();
		s_regionDrawGeneration = generation;
	}

	if (AssetCatalogGame_GetModelResourceCount() <= 0)
	{
		NativeRenderer_ResetRegionDrawState();
		return 0;
	}

	if (!s_regionDrawRegistered)
	{
		const int resourceCount = AssetCatalogGame_GetModelResourceCount();
		char error[256];

		for (int i = 0; i < resourceCount; ++i)
		{
			char key[CONVERSION_CACHE_KEY_CAPACITY];
			char glbPath[CONVERSION_CACHE_PATH_CAPACITY];
			int slot = -1;

			if (!AssetCatalogGame_GetModelResourceAt(i, key, sizeof(key), glbPath, sizeof(glbPath), &slot))
				continue;

			if (!NativeRenderer_RegisterResource(key, glbPath, error, sizeof(error)))
			{
				PsyX_Log_Warning("native region draw: register resource for slot %d failed: %s\n", slot, error);
				continue;
			}

			if (s_regionDrawRegisteredCount < ASSET_CATALOG_GAME_MAX_MODEL_RESOURCES)
				snprintf(s_regionDrawRegisteredKeys[s_regionDrawRegisteredCount++],
					CONVERSION_CACHE_KEY_CAPACITY, "%s", key);
		}

		s_regionDrawRegistered = 1;
		PsyX_Log_Info("native region draw: registered %d/%d resources, %d LOD variants, %d high-detail slots\n",
			s_regionDrawRegisteredCount, resourceCount, AssetCatalogGame_RegionDrawLodSlotCount(),
			AssetCatalogGame_RegionDrawHighDetailSlotCount());
	}

	// Build the transforms and both resource keys once. The transforms are
	// camera-independent world placements, so they never need rebuilding.
	if (!s_regionDrawKeysBuilt)
	{
		const int placementCount = AssetCatalogGame_GetRegionDrawPlacementCount();
		if (placementCount <= 0)
			return 0;

		const int count = placementCount < NATIVE_MAX_INSTANCES ? placementCount : NATIVE_MAX_INSTANCES;

		for (int i = 0; i < count; ++i)
		{
			AssetCatalogGameDrawPlacement placement;

			if (!AssetCatalogGame_GetRegionDrawPlacement(i, &placement))
				continue;

			NativeInstance& instance = s_regionDrawInstances[i];
			memset(&instance, 0, sizeof(instance));

			// The renderer resolves the selected slot's resource key. A slot
			// whose conversion failed keeps an empty key and is counted as
			// skippedUnresolved rather than falling back to a placeholder.
			AssetCatalogGame_GetModelResource(placement.modelSlot, s_regionDrawDetailKeys[i],
				NATIVE_RESOURCE_KEY_CAPACITY, NULL, 0, NULL, 0, NULL);

			s_regionDrawHasLod[i] = 0;
			s_regionDrawLodKeys[i][0] = '\0';

			// The LOD key exists only for a placement with a distinct, converted
			// LOD variant; otherwise the placement draws its detail model at
			// every distance (the fallback was already named during prepare).
			if (placement.lodModelSlot >= 0 &&
				AssetCatalogGame_GetModelResource(placement.lodModelSlot, s_regionDrawLodKeys[i],
					NATIVE_RESOURCE_KEY_CAPACITY, NULL, 0, NULL, 0, NULL))
			{
				s_regionDrawHasLod[i] = 1;
			}

			s_regionDrawHasHighDetail[i] = 0;
			s_regionDrawHighDetailKeys[i][0] = '\0';

			// The high-detail key exists only for a TILE-classified placement
			// with a distinct, converted sibling (the resolver already returned
			// -1 for non-tile models and named the fallback during prepare).
			if (placement.highDetailModelSlot >= 0 &&
				AssetCatalogGame_GetModelResource(placement.highDetailModelSlot, s_regionDrawHighDetailKeys[i],
					NATIVE_RESOURCE_KEY_CAPACITY, NULL, 0, NULL, 0, NULL))
			{
				s_regionDrawHasHighDetail[i] = 1;
			}

			const int pos[3] = { placement.x, placement.y, placement.z };
			NativeInstance_ComposeTransform(pos, (unsigned int)placement.rotation, instance.transform);

			instance.level = GameLevel;
			instance.variant = (int)GetCityType();
			instance.sourceObjectIndex = placement.sourceObjectIndex;
			instance.modelSlot = placement.modelSlot;
			instance.modelRevision = (int)placement.modelRevision;
		}

		s_regionDrawSubmittedCount = count;
		s_regionDrawKeysBuilt = 1;
	}

	const int count = s_regionDrawSubmittedCount;
	if (count <= 0)
		return 0;

	// Per-frame tier selection, exactly like the legacy world draw:
	//   * a TILE-classified placement with camera-space Z <= DRAW_LOD_DIST_HIGH
	//     draws its high-detail sibling (tile.c:290-295);
	//   * otherwise, Z > DRAW_LOD_DIST_LOW swaps the detail key for the LOD key
	//     (draw.c:1041, tile.c:304);
	//   * otherwise the detail key is drawn.
	// Buildings never take the high-detail tier. Re-submit only when at least
	// one selection changed, which keeps a settled frame from re-uploading the
	// whole instance array.
	int changed = !s_regionDrawSelectionValid;
	int lodSelectedCount = 0;
	int highDetailSelectedCount = 0;

	for (int i = 0; i < count; ++i)
	{
		AssetCatalogGameDrawPlacement placement;

		if (!AssetCatalogGame_GetRegionDrawPlacement(i, &placement))
			continue;

		const int z = LegacyPlacementZ(placement);
		const int wantHighDetail = s_regionDrawHasHighDetail[i] && z <= DRAW_LOD_DIST_HIGH;
		const int wantLod = !wantHighDetail && s_regionDrawHasLod[i] && z > DRAW_LOD_DIST_LOW;

		if ((int)s_regionDrawHighDetailSelected[i] != wantHighDetail)
		{
			changed = 1;
			s_regionDrawHighDetailSelected[i] = (unsigned char)wantHighDetail;
		}

		if ((int)s_regionDrawLodSelected[i] != wantLod)
		{
			changed = 1;
			s_regionDrawLodSelected[i] = (unsigned char)wantLod;
		}

		lodSelectedCount += wantLod;
		highDetailSelectedCount += wantHighDetail;
	}

	s_regionDrawLodSelectedCount = lodSelectedCount;
	s_regionDrawHighDetailSelectedCount = highDetailSelectedCount;

	if (!changed)
		return count;

	for (int i = 0; i < count; ++i)
	{
		NativeInstance& instance = s_regionDrawInstances[i];
		const char* key = s_regionDrawHighDetailSelected[i]
			? s_regionDrawHighDetailKeys[i]
			: (s_regionDrawLodSelected[i] ? s_regionDrawLodKeys[i] : s_regionDrawDetailKeys[i]);
		snprintf(instance.resourceKey, sizeof(instance.resourceKey), "%s", key);
	}

	char error[256];
	if (!NativeRenderer_SetInstances(s_regionDrawInstances, count, error, sizeof(error)))
	{
		PsyX_Log_Warning("native region draw: set instances failed: %s\n", error);
		s_regionDrawInstancesSet = 1; // do not retry every frame
		s_regionDrawSubmittedCount = 0;
		return 0;
	}

	s_regionDrawInstancesSet = 1;
	s_regionDrawSelectionValid = 1;
	return count;
}

static int NativeRenderer_DevDrawSubmit(const float view[16])
{
	if (AssetCatalogGame_RegionDrawRequested())
		return NativeRenderer_DevRegionDrawSubmit(view);

	if (!AssetCatalogGame_NativeDrawRequested())
		return 0;

	// A level change bumps the generation. Reset before re-registering so the
	// previous level's instance/resource state never leaks into the new level,
	// even though the registry is already repopulated by the same frame's
	// preparation. The registry-miss guard below stays as a fallback.
	const unsigned int generation = AssetCatalogGame_LevelGeneration();
	if (generation != s_nativeDrawGeneration)
	{
		NativeRenderer_ResetNativeDrawState();
		s_nativeDrawGeneration = generation;
	}

	const int slot = AssetCatalogGame_NativeDrawSlot();
	char key[CONVERSION_CACHE_KEY_CAPACITY];
	char glbPath[CONVERSION_CACHE_PATH_CAPACITY];

	if (!AssetCatalogGame_GetModelResource(slot, key, sizeof(key), glbPath, sizeof(glbPath),
		NULL, 0, NULL))
	{
		// The registry miss (for example after a failed preparation) must not
		// leave the previous level's instances drawn under the new camera.
		NativeRenderer_ResetNativeDrawState();
		return 0;
	}

	if (!s_nativeDrawRegistered)
	{
		char error[256];
		if (!NativeRenderer_RegisterResource(key, glbPath, error, sizeof(error)))
		{
			if (!s_nativeDrawReported)
			{
				PsyX_Log_Warning("native draw: register resource failed: %s\n", error);
				s_nativeDrawReported = 1;
			}
			return 0;
		}
		s_nativeDrawRegistered = 1;
		snprintf(s_nativeDrawRegisteredKey, sizeof(s_nativeDrawRegisteredKey), "%s", key);
		PsyX_Log_Info("native draw: registered resource %s\n", key);
	}

	AssetCatalogGameDrawPlacement placements[ASSET_CATALOG_GAME_MAX_DRAW_PLACEMENTS];
	const int count = AssetCatalogGame_GetDrawPlacements(placements, ASSET_CATALOG_GAME_MAX_DRAW_PLACEMENTS);

	if (count <= 0)
	{
		// No instances: drop a previously submitted list so the renderer returns
		// to its content/placeholder mode instead of drawing a stale set.
		if (s_nativeDrawInstancesSet)
		{
			NativeRenderer_ClearInstances();
			s_nativeDrawInstancesSet = 0;
		}
		s_nativeDrawSubmittedCount = 0;
		return 0;
	}

	// Submit the nearest placements to the current camera, preferring those in
	// the clip range; when none are in range the nearest are submitted anyway so
	// the diagnostics still describe the real data.
	const int submitCount = RankPlacements(placements, count, view, AssetCatalogGame_NativeDrawCount(),
		s_nativeDrawOrder, &s_nativeDrawFirstZ, &s_nativeDrawMinZ, &s_nativeDrawMaxZ,
		&s_nativeDrawInFrustum, &s_nativeDrawLaterallyVisible);

	if (submitCount <= 0)
		return 0;

	NativeInstance instances[ASSET_CATALOG_GAME_MAX_DRAW_PLACEMENTS];
	for (int slotIndex = 0; slotIndex < submitCount; ++slotIndex)
	{
		const AssetCatalogGameDrawPlacement& placement = placements[s_nativeDrawOrder[slotIndex]];

		NativeInstance& instance = instances[slotIndex];
		memset(&instance, 0, sizeof(instance));
		strncpy(instance.resourceKey, key, sizeof(instance.resourceKey) - 1);

		const int pos[3] = { placement.x, placement.y, placement.z };
		NativeInstance_ComposeTransform(pos, (unsigned int)placement.rotation, instance.transform);

		instance.level = GameLevel;
		instance.variant = (int)GetCityType();
		instance.sourceObjectIndex = placement.sourceObjectIndex;
		instance.modelSlot = placement.modelSlot;
		instance.modelRevision = (int)placement.modelRevision;
	}

	char error[256];
	if (!NativeRenderer_SetInstances(instances, submitCount, error, sizeof(error)))
	{
		if (!s_nativeDrawReported)
		{
			PsyX_Log_Warning("native draw: set instances failed: %s\n", error);
			s_nativeDrawReported = 1;
		}
		return 0;
	}

	s_nativeDrawInstancesSet = 1;
	s_nativeDrawSubmittedCount = submitCount;
	return submitCount;
}

static unsigned long long HashBytes(const unsigned char* bytes, int length)
{
	unsigned long long hash = 1469598103934665603ULL; // FNV-1a 64 offset basis
	for (int i = 0; i < length; ++i)
	{
		hash ^= bytes[i];
		hash *= 1099511628211ULL;
	}
	return hash;
}

// One-shot offscreen frame for a dev draw: the same instance scene through the
// public readback path, written to `path` next to the log with a content hash.
static void NativeRenderer_DevDrawCaptureFrame(const char* path)
{
	// Dump at the window aspect (16:9), not square, so a placement ranked as
	// on-screen is inside the dumped frame's horizontal field of view.
	const int width = kNativeDrawDumpWidth;
	const int height = kNativeDrawDumpHeight;
	static unsigned char rgba[kNativeDrawDumpWidth * kNativeDrawDumpHeight * 4];

	char error[256];
	if (!NativeRenderer_RenderSceneToRGBA(rgba, width, height, error, sizeof(error)))
	{
		PsyX_Log_Warning("native draw: offscreen frame failed: %s\n", error);
		return;
	}

	const unsigned long long hash = HashBytes(rgba, (int)sizeof(rgba));

	FILE* file = fopen(path, "wb");
	if (!file)
	{
		PsyX_Log_Warning("native draw: offscreen frame write failed for %s\n", path);
		return;
	}
	fwrite(rgba, 1, sizeof(rgba), file);
	fclose(file);
	PsyX_Log_Info("native draw: offscreen frame %dx%d -> %s (rgba-hash %016llx)\n",
		width, height, path, hash);
}

// True when the game camera is usable for a meaningful report/dump: the
// composed view rotation is non-degenerate and camera_position is non-zero (the
// first native frames can run before the game camera is initialized, which
// collapses every camera-space point to the origin and dumps a clear frame).
static int CameraValid(const float view[16], const int cameraPosition[3])
{
	const float rotationScale = fabsf(view[0]) + fabsf(view[4]) + fabsf(view[8]) +
		fabsf(view[1]) + fabsf(view[5]) + fabsf(view[9]) +
		fabsf(view[2]) + fabsf(view[6]) + fabsf(view[10]);
	const int rotationNonZero = rotationScale > 1e-3f;

	const int cameraNonZero = cameraPosition[0] != 0 || cameraPosition[1] != 0 ||
		cameraPosition[2] != 0;

	return rotationNonZero && cameraNonZero;
}

// One line per run for the full-region draw: resource/instance stats, the
// capacity truncation flags and camera-space evidence over the submitted
// placements. Gated on a valid game camera like the bounded dev draw, and never
// repeats. The evidence is computed at report time so the gate and the reported
// numbers describe the same camera.
static void NativeRenderer_DevRegionDrawReport(int submitted, const float view[16],
	const int cameraPosition[3])
{
	if (submitted <= 0 || s_regionDrawReported)
		return;

	if (!CameraValid(view, cameraPosition))
	{
		s_regionDrawValidFrames = 0;
		if (!s_regionDrawWaitingLogged)
		{
			PsyX_Log_Info("native region draw: report waiting for a valid game camera\n");
			s_regionDrawWaitingLogged = 1;
		}
		return;
	}

	if (++s_regionDrawValidFrames < kNativeDrawReportDelayFrames)
		return;

	s_regionDrawReported = 1;

	int statsSubmitted = 0, drawn = 0, skipped = 0, resident = 0;
	NativeRenderer_GetInstanceStats(&statsSubmitted, &drawn, &skipped, &resident);

	char diagnostic[256];
	const int hasDiagnostic = NativeRenderer_GetFirstInstanceDiagnostic(diagnostic, sizeof(diagnostic));

	// Camera-space evidence over the submitted placements, with the same
	// raw-unit view the camera uses.
	const int placementCount = AssetCatalogGame_GetRegionDrawPlacementCount();
	float zFirst = 0.0f, zMin = 0.0f, zMax = 0.0f;
	int inFrustum = 0, onScreen = 0, sampled = 0;

	for (int i = 0; i < placementCount && i < NATIVE_MAX_INSTANCES; ++i)
	{
		AssetCatalogGameDrawPlacement placement;

		if (!AssetCatalogGame_GetRegionDrawPlacement(i, &placement))
			continue;

		float cam[3];
		CameraSpace(view, placement.x, placement.y, placement.z, cam);

		if (sampled == 0)
			zFirst = zMin = zMax = cam[2];
		else
		{
			if (cam[2] < zMin) zMin = cam[2];
			if (cam[2] > zMax) zMax = cam[2];
		}
		sampled++;

		int zInRange = 0;
		if (PlacementOnScreen(cam, &zInRange))
			onScreen++;
		if (zInRange)
			inFrustum++;
	}

	char diagText[288];
	diagText[0] = '\0';

	// Identify the first unresolved submitted instance from the bridge's own
	// list so the report names the slot and source object, not just a key; fall
	// back to the renderer's key diagnostic when it found one the bridge scan
	// did not (for example an all-missing submission).
	int unresolvedIndex = -1;
	for (int i = 0; i < s_regionDrawSubmittedCount && i < NATIVE_MAX_INSTANCES; ++i)
	{
		if (!NativeRenderer_ResourceResident(s_regionDrawInstances[i].resourceKey))
		{
			unresolvedIndex = i;
			break;
		}
	}

	if (unresolvedIndex >= 0)
	{
		const NativeInstance& instance = s_regionDrawInstances[unresolvedIndex];

		// Name the unresolved instance's origin. The resource registry already
		// told the bridge this slot has no published key; to tell a failed
		// conversion from a truncated (never attempted) slot, note that the
		// preparation attempts only the first bounded set of distinct
		// referenced slots. A slot inside that set was attempted and failed,
		// while one outside it was truncated. When no truncation was reported
		// every referenced slot was attempted, so the slot simply failed. The
		// attempted set is reconstructed from the bridge's own prepared
		// placement list, in the same walk order the preparation used.
		const char* origin = "failed slot";

		if (AssetCatalogGame_RegionDrawResourcesTruncated())
		{
			int attemptedSlots[ASSET_CATALOG_GAME_MAX_MODEL_RESOURCES];
			int attemptedCount = 0;

			for (int i = 0; i < placementCount && attemptedCount < ASSET_CATALOG_GAME_MAX_MODEL_RESOURCES; ++i)
			{
				AssetCatalogGameDrawPlacement placement;

				if (!AssetCatalogGame_GetRegionDrawPlacement(i, &placement))
					continue;

				bool seen = false;
				for (int s = 0; s < attemptedCount; ++s)
					if (attemptedSlots[s] == placement.modelSlot)
					{
						seen = true;
						break;
					}

				if (!seen)
					attemptedSlots[attemptedCount++] = placement.modelSlot;
			}

			bool attempted = false;
			for (int s = 0; s < attemptedCount; ++s)
				if (attemptedSlots[s] == instance.modelSlot)
				{
					attempted = true;
					break;
				}

			if (!attempted)
				origin = "truncated slot";
		}

		snprintf(diagText, sizeof(diagText),
			"; diag: unresolved instance slot %d src %d (%s) key %s",
			instance.modelSlot, instance.sourceObjectIndex, origin, instance.resourceKey);
	}
	else if (hasDiagnostic)
	{
		snprintf(diagText, sizeof(diagText), "; diag: %s", diagnostic);
	}

	PsyX_Log_Info("native region draw: resources registered %d/%d, instances submitted %d, drawn %d, "
		"skippedUnresolved %d, residentResources %d, placements %d, placement-truncated %d, "
		"resource-truncated %d, lod slots %d, lod-selected %d, hd slots %d, hd-selected %d, "
		"camera-position %d,%d,%d, "
		"view diag %.5f/%.5f/%.5f, camera-z first %.1f, min %.1f, max %.1f, inFrustum %d/%d, "
		"laterallyVisible %d/%d, level %d variant %d level-generation %u%s\n",
		s_regionDrawRegisteredCount, AssetCatalogGame_GetModelResourceCount(),
		statsSubmitted, drawn, skipped, resident, placementCount,
		AssetCatalogGame_RegionDrawPlacementsTruncated(), AssetCatalogGame_RegionDrawResourcesTruncated(),
		AssetCatalogGame_RegionDrawLodSlotCount(), s_regionDrawLodSelectedCount,
		AssetCatalogGame_RegionDrawHighDetailSlotCount(), s_regionDrawHighDetailSelectedCount,
		cameraPosition[0], cameraPosition[1], cameraPosition[2], view[0], view[5], view[10],
		zFirst, zMin, zMax, inFrustum, statsSubmitted, onScreen, statsSubmitted,
		GameLevel, (int)GetCityType(), AssetCatalogGame_LevelGeneration(), diagText);

	NativeRenderer_DevDrawCaptureFrame("native_region_draw.rgba");
}

// One line per run with the instance stats, the resource identity and a few
// submitted instance identities. Never repeats. Gated on a valid game camera so
// an early degenerate frame cannot produce a meaningless report or dump.
static void NativeRenderer_DevDrawReport(int submitted, const float view[16],
	const int cameraPosition[3])
{
	if (AssetCatalogGame_RegionDrawRequested())
	{
		NativeRenderer_DevRegionDrawReport(submitted, view, cameraPosition);
		return;
	}

	if (submitted <= 0 || s_nativeDrawReported)
		return;

	if (!CameraValid(view, cameraPosition))
	{
		// Retry on later frames; log once that the report is held back.
		s_nativeDrawValidFrames = 0;
		if (!s_nativeDrawWaitingLogged)
		{
			PsyX_Log_Info("native draw: report waiting for a valid game camera\n");
			s_nativeDrawWaitingLogged = 1;
		}
		return;
	}

	// Belt-and-braces: require a few consecutive valid frames before firing.
	if (++s_nativeDrawValidFrames < kNativeDrawReportDelayFrames)
		return;

	s_nativeDrawReported = 1;

	const int slot = AssetCatalogGame_NativeDrawSlot();
	char key[CONVERSION_CACHE_KEY_CAPACITY];
	char contentHash[CONVERSION_CACHE_HASH_CAPACITY];
	unsigned long long sourceHash = 0;
	AssetCatalogGame_GetModelResource(slot, key, sizeof(key), NULL, 0, contentHash,
		sizeof(contentHash), &sourceHash);

	int statsSubmitted = 0, drawn = 0, skipped = 0, resident = 0;
	NativeRenderer_GetInstanceStats(&statsSubmitted, &drawn, &skipped, &resident);

	char diagnostic[256];
	const int hasDiagnostic = NativeRenderer_GetFirstInstanceDiagnostic(diagnostic, sizeof(diagnostic));

	AssetCatalogGameDrawPlacement placements[ASSET_CATALOG_GAME_MAX_DRAW_PLACEMENTS];
	AssetCatalogGame_GetDrawPlacements(placements, ASSET_CATALOG_GAME_MAX_DRAW_PLACEMENTS);

	// Sample the submitted (nearest/visible-first) identities, not the raw
	// collection order.
	char sample[192];
	size_t offset = 0;
	sample[0] = '\0';
	const int sampleCount = s_nativeDrawSubmittedCount < 3 ? s_nativeDrawSubmittedCount : 3;
	for (int i = 0; i < sampleCount; ++i)
	{
		const AssetCatalogGameDrawPlacement& placement = placements[s_nativeDrawOrder[i]];
		const int written = snprintf(sample + offset, sizeof(sample) - offset,
			" (src %d pos %d,%d,%d yang %d)", placement.sourceObjectIndex,
			placement.x, placement.y, placement.z, placement.rotation);
		if (written <= 0 || (size_t)written >= sizeof(sample) - offset)
			break;
		offset += (size_t)written;
	}

	char diagText[288];
	diagText[0] = '\0';
	if (hasDiagnostic)
		snprintf(diagText, sizeof(diagText), "; diag: %s", diagnostic);

	PsyX_Log_Info("native draw: slot %d, submitted %d, drawn %d, skippedUnresolved %d, residentResources %d, "
		"camera-position %d,%d,%d, view diag %.5f/%.5f/%.5f, "
		"camera-z first %.1f, min %.1f, max %.1f, inFrustum %d/%d, laterallyVisible %d/%d, "
		"source-hash %016llx, content-hash %s, level %d variant %d, key %s%s%s\n",
		slot, statsSubmitted, drawn, skipped, resident,
		cameraPosition[0], cameraPosition[1], cameraPosition[2], view[0], view[5], view[10],
		s_nativeDrawFirstZ, s_nativeDrawMinZ, s_nativeDrawMaxZ, s_nativeDrawInFrustum, statsSubmitted,
		s_nativeDrawLaterallyVisible, statsSubmitted,
		sourceHash, contentHash, GameLevel, (int)GetCityType(), key, sample, diagText);

	char dumpPath[96];
	snprintf(dumpPath, sizeof(dumpPath), "native_draw_slot_%d.rgba", slot);
	NativeRenderer_DevDrawCaptureFrame(dumpPath);
}

// Composes the live game camera into the raw-unit view the native scene draws
// with (view = [R | -R*cam], R the game's inverse camera rotation, 4096 = 1.0).
// Shared by the per-frame presentation and the timed screenshot so both read the
// same camera at the same instant.
static void NativeRenderer_ComposeLiveView(float view[16], int cameraPosition[3])
{
	const int rotation[9] = {
		inv_camera_matrix.m[0][0], inv_camera_matrix.m[0][1], inv_camera_matrix.m[0][2],
		inv_camera_matrix.m[1][0], inv_camera_matrix.m[1][1], inv_camera_matrix.m[1][2],
		inv_camera_matrix.m[2][0], inv_camera_matrix.m[2][1], inv_camera_matrix.m[2][2],
	};

	cameraPosition[0] = camera_position.vx;
	cameraPosition[1] = camera_position.vy;
	cameraPosition[2] = camera_position.vz;

	NativeCameraView_Compose(rotation, cameraPosition, view);
}

// Opt-in, one aggregate per second. Simulation diagnostics stay in the game
// bridge; the renderer reports only its own CPU work and submitted draws.
static void NativeRenderer_ReportPerformance(double submitMilliseconds)
{
	static const int enabled = getenv("RED2_NATIVE_PERF") &&
		strcmp(getenv("RED2_NATIVE_PERF"), "1") == 0;
	if (!enabled)
		return;
	static Uint64 start = 0;
	static unsigned int frames = 0, tickStart = 0, dropStart = 0;
	static double submitSum = 0, drawSum = 0, flushSum = 0, presentSum = 0;
	const Uint64 now = SDL_GetPerformanceCounter();
	if (!start)
	{
		start = now;
		tickStart = ProjectClock_GetTickCount();
		dropStart = ProjectClock_GetDroppedTicks();
	}
	NativeFrameStats stats;
	NativeRenderer_GetFrameStats(&stats);
	++frames;
	submitSum += submitMilliseconds;
	drawSum += stats.drawMilliseconds;
	flushSum += stats.flushMilliseconds;
	presentSum += stats.presentMilliseconds;
	const double elapsed = (double)(now - start) / SDL_GetPerformanceFrequency();
	if (elapsed < 1.0)
		return;
	FILE* log = fopen("native_performance.log", "a");
	if (log)
	{
		fprintf(log, "native-perf fps=%.2f submit=%.3fms draw=%.3fms flush=%.3fms present=%.3fms draws=%u instances=%d culled=%u ticks=%u dropped=%u cap=%d vsync=%d tick=%u\n",
			frames / elapsed, submitSum / frames, drawSum / frames, flushSum / frames,
			presentSum / frames, stats.drawCalls, s_regionDrawSubmittedCount, stats.culledInstances,
			ProjectClock_GetTickCount() - tickStart, ProjectClock_GetDroppedTicks() - dropStart,
			ProjectClock_GetPresentationFps(), g_cfg_swapInterval, ProjectClock_GetTickCount());
		fclose(log);
	}
	start = now;
	frames = 0;
	tickStart = ProjectClock_GetTickCount();
	dropStart = ProjectClock_GetDroppedTicks();
	submitSum = drawSum = flushSum = presentSum = 0;
}

static void NativeRenderer_Frame()
{
	if (!s_nativeRendererActive)
		return;
	NativeRenderer_SetVSync(g_cfg_swapInterval != 0);

	// Forward game-window size changes to the native swap chain. A minimized
	// window reports a zero size; keep the last valid buffers in that case.
	SDL_Window* window = PsyX_GetSDLWindow();
	if (window)
	{
		int width = 0, height = 0;
		SDL_GetWindowSize(window, &width, &height);
		if (width > 0 && height > 0 &&
			(width != s_nativeRendererWidth || height != s_nativeRendererHeight))
		{
			NativeRenderer_Resize(width, height);
			s_nativeRendererWidth = width;
			s_nativeRendererHeight = height;
		}
	}

	// Game camera in raw game units. The native projection is +Z-forward
	// (Option B), so inv_camera_matrix is fed directly with no reflection, and a
	// raw-unit scene needs raw-unit clip planes (the game's far clip limit is
	// 125000; draw.c).
	int cameraPosition[3];
	float view[16];
	NativeRenderer_ComposeLiveView(view, cameraPosition);

	NativeRenderer_SetClipPlanes(kNativeDrawClipNear, kNativeDrawClipFar);
	NativeRenderer_SetCamera(view, NativeDrawScreenDistance());

	// The live GTE screen offset (SetGeomOffset), so the native framing matches
	// the legacy GTE screen mapping; the game's (160,128) sits 8 PSX lines below
	// the display centre.
	float screenOffsetX = 0.0f, screenOffsetY = 0.0f;
	NativeDrawScreenOffset(&screenOffsetX, &screenOffsetY);
	NativeRenderer_SetScreenOffset(screenOffsetX, screenOffsetY);

	const Uint64 submitStart = SDL_GetPerformanceCounter();
	const int devDrawInstances = NativeRenderer_DevDrawSubmit(view);
	const double submitMilliseconds = (double)(SDL_GetPerformanceCounter() - submitStart) *
		1000.0 / SDL_GetPerformanceFrequency();

	char error[256];
	if (!NativeRenderer_RenderScene(error, sizeof(error)))
	{
		// Edge-triggered: log the first failure of a streak only, so a
		// persistent failure does not flood the log; recovery reports the
		// streak length.
		if (s_nativeRendererFailureStreak == 0)
			PsyX_Log_Warning("native renderer scene failed: %s\n", error);
		++s_nativeRendererFailureStreak;
		return;
	}
	if (s_nativeRendererFailureStreak > 0)
	{
		PsyX_Log_Info("native renderer scene recovered after %d failed frames\n",
			s_nativeRendererFailureStreak);
		s_nativeRendererFailureStreak = 0;
	}
	if (s_nativeRendererFrames == 0)
		PsyX_Log_Info("native renderer drawing the game window with the game camera\n");
	++s_nativeRendererFrames;
	NativeRenderer_ReportPerformance(submitMilliseconds);

	NativeRenderer_DevDrawReport(devDrawInstances, view, cameraPosition);
}

// Timed native screenshot: re-renders the live scene (current camera and instance
// submission) into an offscreen RGBA target at the window size and writes it as
// SCREENSHOT.BMP. Called from DeveloperDebugStart_Tick, before the post-frame
// present, so the camera is the one the current frame is about to present and the
// image is comparable to the legacy SCREENSHOT.BMP. Warn-only: a failure leaves
// the game running with no screenshot.
static int NativeRenderer_CaptureScreenshot(const char* path)
{
	if (!path || !path[0])
		return 0;

	SDL_Window* window = PsyX_GetSDLWindow();
	int width = 0, height = 0;
	if (window)
		SDL_GetWindowSize(window, &width, &height);
	if (width <= 0 || height <= 0)
	{
		PsyX_Log_Warning("native screenshot: the game window size is unavailable\n");
		return 0;
	}

	// Take the live camera and the live instance submission at this tick, not the
	// earlier one-shot report snapshot.
	int cameraPosition[3];
	float view[16];
	NativeRenderer_ComposeLiveView(view, cameraPosition);
	NativeRenderer_SetClipPlanes(kNativeDrawClipNear, kNativeDrawClipFar);
	NativeRenderer_SetCamera(view, NativeDrawScreenDistance());

	// Match the presented frame: apply the live GTE screen offset to the
	// screenshot too, so it is comparable to the legacy capture.
	float screenOffsetX = 0.0f, screenOffsetY = 0.0f;
	NativeDrawScreenOffset(&screenOffsetX, &screenOffsetY);
	NativeRenderer_SetScreenOffset(screenOffsetX, screenOffsetY);

	NativeRenderer_DevDrawSubmit(view);

	const size_t pixelBytes = (size_t)width * (size_t)height * 4u;
	unsigned char* rgba = (unsigned char*)malloc(pixelBytes);
	if (!rgba)
	{
		PsyX_Log_Warning("native screenshot: could not allocate %zu bytes\n", pixelBytes);
		return 0;
	}

	char error[256];
	if (!NativeRenderer_RenderSceneToRGBA(rgba, width, height, error, sizeof(error)))
	{
		PsyX_Log_Warning("native screenshot: offscreen render failed: %s\n", error);
		free(rgba);
		return 0;
	}

	// The offscreen target is RGBA8 and read back top-down, matching the legacy
	// Vulkan readback, so SDL gets the same channel masks and no row flip.
	SDL_Surface* surface = SDL_CreateRGBSurfaceFrom(rgba, width, height, 32, width * 4,
		0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000);
	if (!surface)
	{
		PsyX_Log_Warning("native screenshot: could not create the BMP surface\n");
		free(rgba);
		return 0;
	}

	const int saved = SDL_SaveBMP(surface, path);
	SDL_FreeSurface(surface);
	free(rgba);

	if (saved != 0)
	{
		PsyX_Log_Warning("native screenshot: SDL_SaveBMP failed for %s: %s\n", path, SDL_GetError());
		return 0;
	}

	PsyX_Log_Info("native screenshot: %dx%d -> %s\n", width, height, path);
	return 1;
}

static void NativeRenderer_AttachGameWindow()
{
	const char* enabled = getenv("RED2_NATIVE_RENDERER");
	if (!enabled || !enabled[0] || enabled[0] == '0')
		return;

	SDL_Window* window = PsyX_GetSDLWindow();
	if (!window)
	{
		PsyX_Log_Warning("native renderer: the game window is unavailable\n");
		return;
	}

	SDL_SysWMinfo info;
	SDL_VERSION(&info.version);
	if (!SDL_GetWindowWMInfo(window, &info) || info.subsystem != SDL_SYSWM_WINDOWS)
	{
		PsyX_Log_Warning("native renderer: the window handle is unavailable\n");
		return;
	}

	int width = 0, height = 0;
	SDL_GetWindowSize(window, &width, &height);

	char error[256];
	if (!NativeRenderer_Init(info.info.win.window, width, height, error, sizeof(error)))
	{
		PsyX_Log_Warning("native renderer init failed: %s\n", error);
		return;
	}

	s_nativeRendererActive = 1;
	s_nativeRendererWidth = width;
	s_nativeRendererHeight = height;
	PsyX_SetPostFrameHandler(NativeRenderer_Frame);
	PsyX_Log_Info("native renderer active on the game window (%dx%d)\n", width, height);

	// Optional converted content for the scene. A load failure must not abort
	// the native path; the placeholder scene keeps rendering instead.
	const char* contentPath = getenv("RED2_NATIVE_CONTENT");
	if (contentPath && contentPath[0])
	{
		if (NativeRenderer_LoadContent(contentPath, error, sizeof(error)))
			PsyX_Log_Info("native renderer loaded content %s\n", contentPath);
		else
			PsyX_Log_Warning("native renderer content load failed: %s\n", error);
	}
}
#endif

// Capture entry point called by DeveloperDebugStart_Tick. The native path owns
// the window, so PsyCross cannot read its swap chain back; re-render the live
// scene offscreen instead. Every other path keeps the legacy PsyCross readback.
// Warn-only on failure; the capture never interrupts play.
void DeveloperDebugStart_RequestScreenshot(void)
{
#if !defined(__EMSCRIPTEN__) && !defined(__ANDROID__)
#ifdef _WIN32
	if (NativeRenderer_IsActive())
	{
		if (!NativeRenderer_CaptureScreenshot("SCREENSHOT.BMP"))
			PsyX_Log_Warning("native screenshot: capture failed; no SCREENSHOT.BMP was written\n");
		return;
	}
#endif
	PsyX_TakeScreenshot();
#endif
}

int main(int argc, char** argv)
{
	ini_t* config;

#if USE_CRT_MALLOC
	_overlay_buffer = (char*)malloc(0x50000);			// 0x1C0000
	_frontend_buffer = (char*)malloc(0x60000);			// 0xFB400
	_other_buffer = (char*)malloc(0x50000);				// 0xF3000
	_other_buffer2 = (char*)malloc(0x50000);			// 0xE7000
	_OT1 = (OTTYPE*)malloc(OTSIZE * sizeof(OTTYPE));	// 0xF3000
	_OT2 = (OTTYPE*)malloc(OTSIZE * sizeof(OTTYPE));	// 0xF7200
	_primTab1 = (char*)malloc(PRIMTAB_SIZE);			// 0xFB400
	_primTab2 = (char*)malloc(PRIMTAB_SIZE);			// 0x119400
	_sbank_buffer = (char*)malloc(0x80000);				// 0x180000
	_replay_buffer = (char*)malloc(0x50000);			// 0x1fabbc
	
#else
	_overlay_buffer = g_Overlay_buffer;		// 0x1C0000
	_frontend_buffer = g_Frontend_buffer;	// 0xFB400
	_other_buffer = g_Other_buffer;			// 0xF3000
	_other_buffer2 = g_Other_buffer2;		// 0xE7000
	_OT1 = g_OT1;							// 0xF3000
	_OT2 = g_OT2;							// 0xF7200
	_primTab1 = g_PrimTab1;					// 0xFB400
	_primTab2 = g_PrimTab2;					// 0x119400
	_sbank_buffer = g_SBank_buffer;			// 0x180000
	_replay_buffer = g_Replay_buffer;		// 0x1fABBC
#endif

#if defined(DEBUG_OPTIONS) || defined(_DEBUG)
	GPU_printf = printf;
#endif // _DEBUG

	
#if 0 // defined(__EMSCRIPTEN__)
	// mount the current folder as a NODEFS instance
	// inside of emscripten
	EM_ASM(
		FS.mkdir('/working');
		FS.mount(NODEFS, {}, '/working1');
	);
#endif

	const char* configFilename = "config.ini";
	const char* cdImageFileName = NULL;
	// Vulkan is the default game backend now that it has reached parity with
	// OpenGL (same image, resize, readback, stencil, offscreen and VRAM paths,
	// 0 validation errors, measured 30 FPS equality). OpenGL is kept as a
	// selectable option with -opengl so the two paths can be compared, and
	// -vulkan stays accepted for compatibility. Platforms without the native
	// backend (PSX, Android, Emscripten) keep OpenGL as their default.
#if !defined(PSX) && !defined(__ANDROID__) && !defined(__EMSCRIPTEN__)
	int useVulkan = 1;
#else
	int useVulkan = 0;
#endif
	int useOpenGL = 0;

	for (int i = 1; i < argc; i++)
	{
		if (!strcmp(argv[i], "-ini"))
		{
			i++;
			configFilename = argv[i];
		}
		else if(!strcmp(argv[i], "-cdimage"))
		{
			i++;
			cdImageFileName = argv[i];
		}
		else if (!strcmp(argv[i], "-vulkan"))
		{
			// Kept for compatibility; Vulkan is already the default.
			useVulkan = 1;
			useOpenGL = 0;
		}
		else if (!strcmp(argv[i], "-opengl"))
		{
			// Force the original OpenGL renderer. Any driver without a usable
			// Vulkan device also falls back to it automatically.
			useOpenGL = 1;
			useVulkan = 0;
		}
	}

	if (useOpenGL)
		PsyX_SetRenderBackend(PSYX_BACKEND_OPENGL);
	else if (useVulkan)
		PsyX_SetRenderBackend(PSYX_BACKEND_VULKAN);

	// Developer Vulkan fixture (renderer roadmap R7): a standalone modern-scene
	// window with its own swapchain. It must run before any OpenGL or game
	// initialisation because it owns the SDL window for its lifetime.
	if (DeveloperVkFixture_HandleCommandLine(argc, argv))
		return 0;

	config = ini_load(configFilename);

	// best distance
	gDrawDistance = 600;

	int windowWidth = 800;
	int windowHeight = 600;
	int screenWidth = -1;
	int screenHeight = -1;
	int fullScreen = 0;
	int vsync = 0;
	int enableFreecamera = 0;
	int textureOverrides = 1;
	int textureOverridesSpecified = 0;
	int captureAfterSeconds = 0;
	int captureAfterTicks = 0;

	DeveloperDebugStart_SetProgramName(argv[0]);

	extern int gUserLanguage;

	if (config)
	{
		extern int gDisableChicagoBridges;
		extern int gContentOverride;
		int newScrZ = gCameraDefaultScrZ;
		const char* dataFolderStr = ini_get(config, "fs", "dataFolder");

		if(!cdImageFileName)
			cdImageFileName = ini_get(config, "cdfs", "image");
		
		// configure Psy-X pads
		ini_sget(config, "pad", "pad1device", "%d", &g_cfg_controllerToSlotMapping[0]);
		ini_sget(config, "pad", "pad2device", "%d", &g_cfg_controllerToSlotMapping[1]);

		// configure Psy-X renderer
		ini_sget(config, "render", "windowWidth", "%d", &windowWidth);
		ini_sget(config, "render", "windowHeight", "%d", &windowHeight);

		ini_sget(config, "render", "screenWidth", "%d", &screenWidth);
		ini_sget(config, "render", "screenHeight", "%d", &screenHeight);

		ini_sget(config, "render", "fullscreen", "%d", &fullScreen);
		ini_sget(config, "render", "vsync", "%d", &g_cfg_swapInterval);
		ini_sget(config, "render", "pgxpTextureMapping", "%d", &g_cfg_pgxpTextureCorrection);
		ini_sget(config, "render", "pgxpZbuffer", "%d", &g_cfg_pgxpZBuffer);
		ini_sget(config, "render", "bilinearFiltering", "%d", &g_cfg_bilinearFiltering);
		textureOverridesSpecified = ini_sget(config, "render", "textureOverrides", "%d", &textureOverrides);

		// configure host game
		ini_sget(config, "game", "drawDistance", "%d", &gDrawDistance);
#ifdef DYNAMIC_LIGHTING
		ini_sget(config, "game", "dynamicLights", "%d", &gEnableDlights);
#endif
		ini_sget(config, "game", "disableChicagoBridges", "%d", &gDisableChicagoBridges);
		ini_sget(config, "game", "fieldOfView", "%d", &newScrZ);
		ini_sget(config, "game", "freeCamera", "%d", &enableFreecamera);
		ini_sget(config, "game", "driver1music", "%d", &gDriver1Music);
		ini_sget(config, "game", "widescreenOverlays", "%d", &gWidescreenOverlayAlign);
		ini_sget(config, "game", "fastLoadingScreens", "%d", &gFastLoadingScreens);
		ini_sget(config, "game", "languageId", "%d", &gUserLanguage);
		ini_sget(config, "game", "overrideContent", "%d", &gContentOverride);
		ini_sget(config, "game", "captureAfterSeconds", "%d", &captureAfterSeconds);
		ini_sget(config, "game", "captureAfterTicks", "%d", &captureAfterTicks);
		
	
		gCameraDefaultScrZ = MAX(MIN(newScrZ, 384), 128);
		
		if (dataFolderStr)
		{
			strcpy(gDataFolder, dataFolderStr);

			int len = strlen(gDataFolder);
			if (gDataFolder[len - 1] != '\\')
			{
				gDataFolder[len] = '\\';
				gDataFolder[len + 1] = '\0';
			}
		}

#if defined(DEBUG_OPTIONS)
		int unlockAll = 0;
		ini_sget(config, "game", "unlockAll", "%d", &unlockAll);

		if (unlockAll)
		{
			gFurthestMission = 40;
			AvailableCheats.cheat5 = 1;
			AvailableCheats.cheat6 = 1;
			AvailableCheats.cheat7 = 1;
			AvailableCheats.cheat8 = 1;
			AvailableCheats.cheat2 = 1;
			AvailableCheats.cheat11 = 1;
			AvailableCheats.cheat1 = 1;
			AvailableCheats.cheat3 = 1;
			AvailableCheats.cheat4 = 1;
		}
#endif
	}

	gFreeCameraConfiguration = enableFreecamera;

#ifndef _DEBUG
	if (enableFreecamera)
	{
		g_dbg_gameDebugKeys = FreeCameraKeyboardHandler;
		g_dbg_gameDebugMouse = FreeCameraMouseHandler;
	}
#else

	g_dbg_gameDebugKeys = GameDebugKeys;
	g_dbg_gameDebugMouse = FreeCameraMouseHandler;

#endif

#ifdef _WIN32
	// Diligent owns the only GPU renderer; PsyCross retains its SDL window,
	// CPU VRAM and compatibility services until their consumers migrate.
	const char* nativeRenderer = getenv("RED2_NATIVE_RENDERER");
	if (nativeRenderer && nativeRenderer[0] && nativeRenderer[0] != '0')
		PsyX_SetHeadless(PSYX_HEADLESS_WINDOW_ONLY);
#endif

	PsyX_Initialise("REDRIVER2", fullScreen ? screenWidth : windowWidth, fullScreen ? screenHeight : windowHeight, fullScreen);

#ifdef _WIN32
	// E03: optional native-renderer ownership of the game's own window.
	NativeRenderer_AttachGameWindow();
	if (PsyX_GetHeadless() == PSYX_HEADLESS_WINDOW_ONLY && !NativeRenderer_IsActive())
	{
		PsyX_Log_Error("Native renderer failed; external presentation cannot continue\n");
		PsyX_Shutdown();
		return 1;
	}
#endif

	char version_info[32];
	GetTimeStamp(version_info);
	PsyX_Log_Info("%s %s (%s)\n", GAME_VERSION, version_info, BUILD_CONFIGURATION_STRING);

	// configure Psy-X CD image reader
	if (cdImageFileName)
		PsyX_CDFS_Init(cdImageFileName, 0, 0);
	
	if (config)
	{
		ParseKeyboardMappings(config, "kbcontrols_game", g_kbGameMappings);
		ParseKeyboardMappings(config, "kbcontrols_menu", g_kbMenuMappings);

		ParseControllerMappings(config, "controls_game", g_gcGameMappings);
		ParseControllerMappings(config, "controls_menu", g_gcMenuMappings);
		
		ini_free(config);
	}

	// start with menu mapping
	SwitchMappings(1);
	// The bindings parsed from config.ini are the defaults the developer panel
	// resets to, so they must be recorded before the developer overrides are
	// loaded on top of them.
	DeveloperInputMapping_CaptureDefaults();
	DeveloperInputMapping_LoadDefaultFile();
	// The modern-mesh module reads its asset/light config first; the developer
	// Graphics settings then own the authoritative classic/enhanced toggle.
	DeveloperModernMesh_Initialise();
	DeveloperGraphicsPanel_Initialise();
	ProjectClock_ApplyStartupVideoMode();

	// An explicit config.ini key is applied after the panel so it can force
	// texture overrides off for reproducible captures. When the key is absent,
	// keep what the panel loaded from developer_graphics.ini.
	if (textureOverridesSpecified)
		HdTextureOverrides_SetEnabled(textureOverrides);
	DeveloperDebugStart_ConfigureCapture(captureAfterSeconds);
	if (captureAfterTicks > 0)
		DeveloperDebugStart_ConfigureCaptureTicks(captureAfterTicks);

	// E00/E01 determinism checks: optional canonical per-tick state dump.
	DeterministicBaseline_ConfigureFromCommandLine(argc, argv);
	// E02: optional level inventory report once the world regions are resident.
	AssetCatalogGame_ConfigureFromCommandLine(argc, argv);

#ifdef RED2_AUDIO_FMOD
	// E04 A1: initialize the FMOD service before the frontend bank load (inside
	// redriver2_main) so the menu registration seam sees an active service, and
	// read the consumer mode once. If initialization fails, log once and stay in
	// SPU mode for the whole run; there is no per-call fallback.
	if (AudioFmod_GetConsumerMode() == AUDIO_CONSUMER_FMOD_ONLY)
	{
		char audioError[256];
		if (AudioService_Init(audioError, sizeof(audioError)))
			PsyX_Log_Info("audio: FMOD service active (RED2_FMOD_AUDIO=1)\n");
		else
		{
			PsyX_Log_Warning("audio: FMOD service init failed (%s); staying on SPU\n", audioError);
			AudioFmod_SetConsumerMode(AUDIO_CONSUMER_SPU_ONLY);
		}
	}
#endif

	redriver2_main(argc, argv);

#ifdef RED2_AUDIO_FMOD
	AudioService_Shutdown();
#endif

	DeterministicBaseline_Shutdown();
	DeinitStringMng();
	DeveloperModernMesh_Shutdown();
	DeveloperGraphicsPanel_Shutdown();

#ifdef _WIN32
	// E03: the Diligent device and swap chain must release while the game's
	// window still exists, so the native renderer never outlives its HWND.
	if (s_nativeRendererActive)
	{
		NativeRenderer_Shutdown();
		s_nativeRendererActive = 0;
		s_nativeRendererFrames = 0;
	}
#endif

	PsyX_Shutdown();

	return 0;
}
