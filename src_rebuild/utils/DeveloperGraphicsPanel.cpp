#include "DeveloperGraphicsPanel.h"

#include "DeveloperGraphicsSettings.h"

#if defined(_WIN32) || defined(__linux__)

#include <SDL.h>
#include <stdio.h>

#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl2.h"

#include "driver2.h"
#include "C/camera.h"
#include "C/cars.h"
#include "C/convert.h"
#include "C/dr2roads.h"
#include "C/draw.h"
#include "C/felony.h"
#include "C/glaunch.h"
#include "C/mission.h"
#include "C/players.h"
#include "C/pres.h"
#include "C/spool.h"
#include "C/system.h"
#include "PsyX/PsyX_globals.h"
#include "PsyX/PsyX_public.h"

extern int gDrawDistance;
extern int gDisplayDrawStats;
extern int gDebugPrimtabUsed;
extern volatile int spoolactive;
extern int numActiveCops;
extern int numCopCars;
extern int numCivCars;
extern int numParkedCars;
extern int maxCopCars;
extern int maxCivCars;
extern int current_region;
extern int LoadedArea;

namespace
{
bool g_initialised = false;
bool g_visible = false;
bool g_captureGameInput = true;
char g_persistenceStatus[96] = "Session settings only";

void UpdateInputCapture()
{
	PsyX_SetInputCapture(g_visible && g_captureGameInput ? PSYX_INPUT_CAPTURE_KEYBOARD | PSYX_INPUT_CAPTURE_GAMEPAD : 0);
}

void HelpMarker(const char* description)
{
	ImGui::TextDisabled("(?)");
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
	{
		ImGui::BeginTooltip();
		ImGui::PushTextWrapPos(ImGui::GetFontSize() * 36.0f);
		ImGui::TextUnformatted(description);
		ImGui::PopTextWrapPos();
		ImGui::EndTooltip();
	}
}

void DebugValue(const char* label, int value, const char* description)
{
	ImGui::Text("%s: %d", label, value);
	ImGui::SameLine();
	HelpMarker(description);
}

void DrawGameDebugTab()
{
	ImGui::TextUnformatted("Live values behind the legacy in-game debug overlay.");
	ImGui::SameLine();
	HelpMarker("These values are read from the running game state. They help diagnose streaming, traffic, mission, vehicle, and road behaviour.");

	bool showLegacyStats = gDisplayDrawStats != 0;
	if (ImGui::Checkbox("Show legacy in-game stats", &showLegacyStats))
		gDisplayDrawStats = showLegacyStats;
	ImGui::SameLine();
	HelpMarker("Draws the original text-only statistics directly into the PlayStation-style game frame. The ImGui view below is easier to inspect and does not require this option.");

	if (ImGui::CollapsingHeader("Renderer and primitive table", ImGuiTreeNodeFlags_DefaultOpen))
	{
		DebugValue("Primitive table used", gDebugPrimtabUsed, "Bytes consumed in the PlayStation primitive table for the latest completed game frame. This value is sampled where the legacy overlay runs, before frame buffers are reset.");
		DebugValue("Primitive table capacity", PRIMTAB_SIZE, "Total primitive-table capacity in bytes. The game builds draw primitives in this fixed-size buffer.");
	}

	if (ImGui::CollapsingHeader("Streaming and regions", ImGuiTreeNodeFlags_DefaultOpen))
	{
		DebugValue("Streaming requested", doSpooling, "Non-zero when the game is requesting background region streaming.");
		DebugValue("Special streaming allowed", allowSpecSpooling, "Non-zero when special-region streaming is permitted by the current game state.");
		DebugValue("Streaming active", spoolactive, "Non-zero while the streaming system is actively loading or unloading data.");
		DebugValue("Current region", current_region, "Identifier of the world region associated with the current streaming state.");
		DebugValue("Loaded area", LoadedArea, "Identifier of the area currently available to the game.");

		if (current_region >= 0)
		{
			Spool* spool = (Spool*)(RegionSpoolInfo + spoolinfo_offsets[current_region]);
			DebugValue("Connected area 1", spool->connected_areas[0] & 0x3f, "First region connected to the current region according to the spool table.");
			DebugValue("Connected area 2", spool->connected_areas[1] & 0x3f, "Second region connected to the current region according to the spool table.");
		}
	}

	if (ImGui::CollapsingHeader("Traffic and police", ImGuiTreeNodeFlags_DefaultOpen))
	{
		DebugValue("Civilian cars", numCivCars, "Civilian traffic vehicles currently created by the simulation.");
		DebugValue("Parked cars", numParkedCars, "Civilian vehicles currently allocated as parked traffic.");
		DebugValue("Civilian car limit", maxCivCars, "Maximum number of civilian cars the current scenario may allocate.");
		DebugValue("Police cars", numCopCars, "Police vehicles currently allocated by the simulation.");
		DebugValue("Active police", numActiveCops, "Police vehicles currently active in pursuit or game logic.");
		DebugValue("Police car limit", maxCopCars, "Maximum number of police cars the current scenario may allocate.");
		DebugValue("Police can see player", CopsCanSeePlayer, "Non-zero when the police AI currently has line-of-sight or visibility state for the player.");
	}

	if (ImGui::CollapsingHeader("Mission and player vehicle", ImGuiTreeNodeFlags_DefaultOpen))
	{
		DebugValue("Mission number", gCurrentMissionNumber, "Internal identifier of the active mission or game mode.");
		DebugValue("Random chase", gRandomChase, "Non-zero when the current mission state is using a randomized chase configuration.");

		const int playerCar = MainPlayer.playerCarId;
		DebugValue("Player car ID", playerCar, "Index of the vehicle currently assigned to the player. A negative value means the player is not in a car.");
		if (playerCar >= 0)
		{
			DebugValue("Vehicle speed", car_data[playerCar].hd.speed, "Raw fixed-point vehicle speed used by the game physics. It is not a display speed in km/h or mph.");
			DebugValue("Vehicle direction", car_data[playerCar].hd.direction, "Raw PlayStation angle used by the vehicle heading and physics systems.");
		}
	}

	const int playerCar = MainPlayer.playerCarId;
	if (playerCar >= 0 && ImGui::CollapsingHeader("Player road context", ImGuiTreeNodeFlags_DefaultOpen))
	{
		VECTOR* carPosition = (VECTOR*)car_data[playerCar].hd.where.t;
		DRIVER2_ROAD_INFO roadInfo = {};
		roadInfo.surfId = GetSurfaceIndex(carPosition);
		DebugValue("Surface ID", roadInfo.surfId, "World-surface identifier directly under the player vehicle.");

		if (GetSurfaceRoadInfo(&roadInfo, roadInfo.surfId))
		{
			int dx = carPosition->vx - (roadInfo.straight ? roadInfo.straight->Midx : roadInfo.curve->Midx);
			int dz = carPosition->vz - (roadInfo.straight ? roadInfo.straight->Midz : roadInfo.curve->Midz);
			int segmentLength;
			int distanceAlongSegment;

			if (roadInfo.straight)
			{
				const int theta = roadInfo.straight->angle - ratan2(dx, dz);
				segmentLength = roadInfo.straight->length;
				distanceAlongSegment = (segmentLength / 2) + FIXEDH(RCOS(theta) * SquareRoot0(dx * dx + dz * dz));
			}
			else
			{
				const int theta = ratan2(dx, dz);
				segmentLength = (roadInfo.curve->end - roadInfo.curve->start) & 0xfffU;
				if (roadInfo.curve->inside < 10)
					distanceAlongSegment = ((theta & 0xfffU) - roadInfo.curve->start) & 0xf80;
				else if (roadInfo.curve->inside < 20)
					distanceAlongSegment = ((theta & 0xfffU) - roadInfo.curve->start) & 0xfc0;
				else
					distanceAlongSegment = ((theta & 0xfffU) - roadInfo.curve->start) & 0xfe0;
			}

			const int lane = GetLaneByPositionOnRoad(&roadInfo, carPosition);
			ImGui::Text("Road kind: %s", roadInfo.straight ? "Straight" : "Curve");
			ImGui::SameLine(); HelpMarker("The road primitive type under the vehicle.");
			DebugValue("Road speed limit", ROAD_SPEED_LIMIT(&roadInfo), "Speed-limit value stored in the road data for AI traffic behaviour.");
			DebugValue("Segment length", segmentLength, "Length or angular span of the current road segment in the game's native units.");
			DebugValue("Distance along segment", distanceAlongSegment, "The player's computed progress along the current road segment in native road units.");
			DebugValue("Lane", lane + 1, "One-based lane occupied by the player on this road segment.");
			DebugValue("Lane count", ROAD_WIDTH_IN_LANES(&roadInfo), "Number of lanes represented by this road segment per direction.");
			DebugValue("Lane direction", ROAD_LANE_DIR(&roadInfo, lane), "Direction bit for the occupied lane.");
			DebugValue("AI lane", ROAD_IS_AI_LANE(&roadInfo, lane), "Non-zero when civilian and police AI may drive on the occupied lane.");
		}
		else if (IS_JUNCTION_SURFACE(roadInfo.surfId))
		{
			DRIVER2_JUNCTION* junction = GET_JUNCTION(roadInfo.surfId);
			ImGui::TextUnformatted("Vehicle is on a junction surface.");
			DebugValue("Traffic lights", junction->flags & 1, "Non-zero when this junction has traffic-light behaviour enabled.");
			DebugValue("Yield behaviour", junction->flags & 2, "Non-zero when this junction has yield behaviour enabled.");
		}
		else
		{
			ImGui::TextUnformatted("No road or junction metadata is available for this surface.");
		}
	}
}

void SetVisible(bool visible)
{
	g_visible = visible;
	UpdateInputCapture();
	PsyX_SetCursorRelative(0);
	SDL_ShowCursor(visible ? SDL_ENABLE : SDL_DISABLE);
}

int HandleSDLEvent(const SDL_Event* event)
{
	if (!event)
		return 0;

	ImGui_ImplSDL2_ProcessEvent(event);

	if (event->type == SDL_KEYDOWN && event->key.repeat == 0 &&
		(event->key.keysym.scancode == SDL_SCANCODE_F11 || event->key.keysym.sym == SDLK_F11))
	{
		SetVisible(!g_visible);
		return 1;
	}

	if (!g_visible || !g_captureGameInput)
		return 0;

	switch (event->type)
	{
	case SDL_KEYDOWN:
	case SDL_KEYUP:
	case SDL_TEXTINPUT:
	case SDL_MOUSEMOTION:
	case SDL_MOUSEBUTTONDOWN:
	case SDL_MOUSEBUTTONUP:
	case SDL_MOUSEWHEEL:
		return 1;
	default:
		return 0;
	}
}

void RenderOverlay()
{
	if (!g_visible)
		return;

	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplSDL2_NewFrame();
	ImGui::NewFrame();

	ImGui::SetNextWindowSize(ImVec2(520.0f, 0.0f), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("Developer Graphics Panel", &g_visible))
	{
		ImGui::TextUnformatted("F11 closes this panel. Settings are live.");
		if (ImGui::Checkbox("Capture game input while panel is open", &g_captureGameInput))
			UpdateInputCapture();
		ImGui::SameLine();
		HelpMarker("Enabled: keyboard, controller, and mouse events are held by the panel so changing a setting cannot also control the game. Disabled: the panel remains visible while the game continues to receive its normal inputs. UI interactions may then also affect the game.");
		if (ImGui::BeginTabBar("DeveloperPanelTabs"))
		{
			if (ImGui::BeginTabItem("Graphics"))
			{
				bool bilinear = g_cfg_bilinearFiltering != 0;
				bool pgxpTextureMapping = g_cfg_pgxpTextureCorrection != 0;
				bool pgxpZBuffer = g_cfg_pgxpZBuffer != 0;
				bool vsync = g_cfg_swapInterval != 0;
				if (ImGui::Checkbox("Bilinear filtering", &bilinear)) g_cfg_bilinearFiltering = bilinear;
				if (ImGui::Checkbox("PGXP texture mapping", &pgxpTextureMapping)) g_cfg_pgxpTextureCorrection = pgxpTextureMapping;
				if (ImGui::Checkbox("PGXP Z-buffer", &pgxpZBuffer)) g_cfg_pgxpZBuffer = pgxpZBuffer;
				if (ImGui::Checkbox("VSync", &vsync)) g_cfg_swapInterval = vsync;

				ImGui::SliderInt("Draw distance", &gDrawDistance, 441, 1800);
				int fieldOfView = gCameraDefaultScrZ;
				if (ImGui::SliderInt("Field of view", &fieldOfView, 128, 384))
					gCameraDefaultScrZ = (short)fieldOfView;

				ImGui::Separator();
				PsyXRenderStats renderStats = {};
				PsyX_GetRenderStats(&renderStats);
				ImGui::Text("Frame time: %.2f ms", ImGui::GetIO().DeltaTime * 1000.0f);
				ImGui::Text("Frame rate: %.1f FPS", ImGui::GetIO().Framerate);
				ImGui::Text("Window: %d x %d", g_windowWidth, g_windowHeight);
				ImGui::Text("PSX vertices: %d", renderStats.vertexCount);
				ImGui::Text("Draw splits: %d", renderStats.drawSplitCount);

				ImGui::Separator();
				if (ImGui::Button("Save developer settings"))
					snprintf(g_persistenceStatus, sizeof(g_persistenceStatus), "%s", DeveloperGraphicsSettings_SaveRuntime() ? "Saved developer_graphics.ini" : "Save failed; settings remain in session");
				ImGui::SameLine();
				if (ImGui::Button("Restore defaults"))
				{
					DeveloperGraphicsSettings_RestoreDefaults();
					snprintf(g_persistenceStatus, sizeof(g_persistenceStatus), "%s", "Defaults applied to this session");
				}
				ImGui::TextUnformatted(g_persistenceStatus);
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Game Debug"))
			{
				DrawGameDebugTab();
				ImGui::EndTabItem();
			}
			ImGui::EndTabBar();
		}
	}
	ImGui::End();

	if (!g_visible)
		SetVisible(false);

	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}
}

void DeveloperGraphicsPanel_Initialise()
{
	if (g_initialised || !PsyX_GetSDLWindow())
		return;

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();

	if (!ImGui_ImplSDL2_InitForOpenGL(PsyX_GetSDLWindow(), SDL_GL_GetCurrentContext()) ||
		!ImGui_ImplOpenGL3_Init("#version 130"))
	{
		ImGui::DestroyContext();
		return;
	}

	g_initialised = true;
	DeveloperGraphicsSettings_LoadAndApply();
	PsyX_SetSDLEventHandler(HandleSDLEvent);
	PsyX_SetRenderOverlayHandler(RenderOverlay);
}

void DeveloperGraphicsPanel_Shutdown()
{
	if (!g_initialised)
		return;

	PsyX_SetInputCapture(0);
	PsyX_SetSDLEventHandler(NULL);
	PsyX_SetRenderOverlayHandler(NULL);
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();
	g_initialised = false;
}

#else

void DeveloperGraphicsPanel_Initialise() {}
void DeveloperGraphicsPanel_Shutdown() {}

#endif
