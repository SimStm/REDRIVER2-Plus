// redriver2_psxpc.cpp
//

#include <SDL_keyboard.h>

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
#include "utils/DeterministicBaseline.h"
#include "utils/HdTextureOverrides.h"
#include "utils/InputService.h"

#ifdef RED2_AUDIO_FMOD
#include "Game/C/audiofmod_adapter.h"
#include "utils/AudioService.h"
#endif

#include <SDL_scancode.h>
#include <SDL_gamecontroller.h>
#include <SDL_messagebox.h>

#ifdef _WIN32
#include <SDL_syswm.h>
#include "NativeRenderer.h"
#endif

#include "PsyX/PsyX_globals.h"


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
// The view rotation is derived from the game's inv_camera_matrix exactly like
// the modern-mesh path (rotation-only, 1/4096-scaled).
static int s_nativeRendererActive = 0;
static int s_nativeRendererFrames = 0;
static int s_nativeRendererFailureStreak = 0;
static int s_nativeRendererWidth = 0;
static int s_nativeRendererHeight = 0;

static void NativeRenderer_Frame()
{
	if (!s_nativeRendererActive)
		return;

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

	const float scale = 1.0f / 4096.0f;
	float rotation[16];
	rotation[0] = inv_camera_matrix.m[0][0] * scale; rotation[4] = inv_camera_matrix.m[0][1] * scale; rotation[8] = inv_camera_matrix.m[0][2] * scale; rotation[12] = 0.0f;
	rotation[1] = inv_camera_matrix.m[1][0] * scale; rotation[5] = inv_camera_matrix.m[1][1] * scale; rotation[9] = inv_camera_matrix.m[1][2] * scale; rotation[13] = 0.0f;
	rotation[2] = inv_camera_matrix.m[2][0] * scale; rotation[6] = inv_camera_matrix.m[2][1] * scale; rotation[10] = inv_camera_matrix.m[2][2] * scale; rotation[14] = 0.0f;
	rotation[3] = 0.0f; rotation[7] = 0.0f; rotation[11] = 0.0f; rotation[15] = 1.0f;

	// Full world->camera matrix: rotation plus the camera translation, in the
	// same raw game units the game's modern-mesh path uses.
	float view[16];
	for (int i = 0; i < 16; ++i)
		view[i] = rotation[i];
	const float px = (float)camera_position.vx;
	const float py = (float)camera_position.vy;
	const float pz = (float)camera_position.vz;
	view[12] = -(rotation[0] * px + rotation[4] * py + rotation[8] * pz);
	view[13] = -(rotation[1] * px + rotation[5] * py + rotation[9] * pz);
	view[14] = -(rotation[2] * px + rotation[6] * py + rotation[10] * pz);

	NativeRenderer_SetCamera(view, 1.0472f); // 60 degrees

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
	// E03: with the native-renderer probe enabled PsyCross must not own the
	// window's presentation, so it creates the window without a swapchain
	// (Vulkan rejects a second swapchain on the same window with
	// VK_ERROR_NATIVE_WINDOW_IN_USE_KHR).
	const char* nativeRenderer = getenv("RED2_NATIVE_RENDERER");
	if (nativeRenderer && nativeRenderer[0] && nativeRenderer[0] != '0')
		PsyX_SetHeadless(1);
#endif

	PsyX_Initialise("REDRIVER2", fullScreen ? screenWidth : windowWidth, fullScreen ? screenHeight : windowHeight, fullScreen);

#ifdef _WIN32
	// E03: optional native-renderer ownership of the game's own window.
	NativeRenderer_AttachGameWindow();
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
