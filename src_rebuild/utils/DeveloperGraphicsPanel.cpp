#include "DeveloperGraphicsPanel.h"

#include "DeveloperDebugStart.h"
#include "DeveloperGraphicsSettings.h"
#include "HdTextureOverrides.h"

#if defined(_WIN32) || defined(__linux__)

#include <SDL.h>
#include <stdio.h>

#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl2.h"

#include "driver2.h"
#include "C/assetcatalog.h"
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
bool g_inspectorPickMode = false;
char g_persistenceStatus[96] = "Session settings only";
char g_inspectorExportModId[48] = "inspector-export";
char g_inspectorExportStatus[192] = "Select a texture to export it into a new or existing mod directory.";

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

const char* AssetSourceLabel(AssetCatalogSource source)
{
	switch (source)
	{
		case ASSET_CATALOG_SOURCE_VERIFIED: return "verified";
		case ASSET_CATALOG_SOURCE_DECLARED: return "declared";
		default: return "unknown";
	}
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

	if (ImGui::CollapsingHeader("Reproduce this state (launch snapshot)", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::TextWrapped("Capture the current mission, vehicle and position, then start a Debug or Release_dev build directly in it without the frontend or intro.");
		ImGui::SameLine();
		HelpMarker("The generated command uses -mission, -playercar, -startpos, -players and -chase, which require a Debug or Release_dev build. A replay or attract demo is reproduced with -replay instead. developer_graphics.ini and installed mods still load normally.");

		static char command[512] = "";
		static char snapshotStatus[256] = "";

		if (ImGui::Button("Refresh launch command"))
		{
			if (!DeveloperDebugStart_BuildCommandLine(command, sizeof(command)))
				snprintf(command, sizeof(command), "No reproducible session yet; start a mission or a replay first.");
		}
		ImGui::SameLine();
		if (ImGui::Button("Copy launch command"))
		{
			if (DeveloperDebugStart_BuildCommandLine(command, sizeof(command)))
				ImGui::SetClipboardText(command);
			else
				snprintf(command, sizeof(command), "No reproducible session yet; start a mission or a replay first.");
		}
		ImGui::SameLine();
		if (ImGui::Button("Save as debug start"))
		{
			DeveloperDebugStart_SaveSnapshot(snapshotStatus, sizeof(snapshotStatus));
			DeveloperDebugStart_BuildCommandLine(command, sizeof(command));
		}

		if (command[0] != '\0')
			ImGui::TextWrapped("Command: %s", command);

		if (ImGui::Button("Enable debug start"))
			DeveloperDebugStart_SetEnabled(1, snapshotStatus, sizeof(snapshotStatus));
		ImGui::SameLine();
		if (ImGui::Button("Disable debug start"))
			DeveloperDebugStart_SetEnabled(0, snapshotStatus, sizeof(snapshotStatus));
		ImGui::SameLine();
		if (ImGui::Button("Delete snapshot"))
			DeveloperDebugStart_Clear(snapshotStatus, sizeof(snapshotStatus));

		if (snapshotStatus[0] == '\0')
			DeveloperDebugStart_GetStatus(snapshotStatus, sizeof(snapshotStatus));

		ImGui::TextWrapped("%s", snapshotStatus);
		ImGui::TextDisabled("%s is applied at startup only when no -mission or -replay argument is present.", DeveloperDebugStart_GetFilePath());
	}
}

void DrawThreeDDebugTab()
{
	static bool showHighlight = true;
	static bool showLabel = true;
	static float highlightColour[4] = { 0.1f, 0.85f, 1.0f, 0.25f };

	ImGui::TextUnformatted("Render inspector");
	ImGui::SameLine();
	HelpMarker("Click picking resolves the visible PSX primitive, its page, CLUT, UV region, and render provenance. Texture mod manifests, active mods and the declared override list live in the Mods tab.");

	if (ImGui::CollapsingHeader("Asset catalog", ImGuiTreeNodeFlags_DefaultOpen))
	{
		AssetCatalogContext context = {};
		int modelCount = 0, textureCount = 0, carCount = 0, materialRefs = 0;
		int modelCap = 0, textureCap = 0, carCap = 0, materialCap = 0;
		AssetCatalog_GetStats(&modelCount, &textureCount, &carCount, &materialRefs);
		AssetCatalog_GetCapacities(&modelCap, &textureCap, &carCap, &materialCap);

		if (AssetCatalog_GetContext(&context))
		{
			ImGui::Text("Context: level %d, variant %d, %s", context.level, context.variant,
				context.multiplayer ? "multiplayer" : "single player");
			ImGui::Text("Generation: %u", AssetCatalog_GetGeneration());
		}
		else
		{
			ImGui::TextDisabled("Catalog inactive (no level loaded).");
		}

		ImGui::Text("Models %d/%d | textures %d/%d | cars %d/%d", modelCount, modelCap,
			textureCount, textureCap, carCount, carCap);
		ImGui::Text("Material links %d/%d", materialRefs, materialCap);
		ImGui::Text("Static footprint: %d bytes", AssetCatalog_GetCapacityBytes());
		ImGui::SameLine();
		HelpMarker("Fixed size, populated during level load and region streaming. The catalog performs no per-frame allocation, so its cost does not scale with the frame.");
	}

	ImGui::Separator();
	ImGui::Checkbox("Pick visible primitive", &g_inspectorPickMode);
	ImGui::SameLine();
	HelpMarker("With this enabled, left-click a visible game primitive outside the ImGui windows. The selection is resolved from the completed PSX draw stream on the next frame.");
	ImGui::Checkbox("Highlight selected draw source", &showHighlight);
	ImGui::Checkbox("Show selection label", &showLabel);
	ImGui::ColorEdit4("Highlight colour", highlightColour);
	if (ImGui::Button("Clear selection")) PsyX_Inspector_ClearSelection();
	if (ImGui::Button("Reload mod manifests and images"))
		HdTextureOverrides_Reload();
	ImGui::SameLine();
	HelpMarker("Reloads JSON manifests and PNG files, then re-registers texture regions already loaded by the current level. It never reloads or edits original game data.");
	PsyXInspectorSelection selection = {};
	const bool hasSelection = PsyX_Inspector_GetSelection(&selection) != 0;
	HdTextureInspectorInfo textureInfo = {};
	const bool hasTexture = hasSelection && HdTextureOverrides_FindTextureInfo(selection.tpage, selection.clut,
		selection.sourceU, selection.sourceV, selection.sourceWidth, selection.sourceHeight, &textureInfo);
	int selectedCarId = -1;
	const bool hasCar = hasSelection && sscanf(selection.provenance, "Car #%d", &selectedCarId) == 1 &&
		selectedCarId >= 0 && selectedCarId < MAX_CARS;
	if (hasSelection)
	{
		ImGui::Text("Selected primitive: %d", selection.primitiveIndex);
		ImGui::Text("Texture page: %u | CLUT: %u", selection.tpage, selection.clut);
		ImGui::Text("UV: (%u, %u), (%u, %u), (%u, %u)", selection.u[0], selection.v[0],
			selection.u[1], selection.v[1], selection.u[2], selection.v[2]);
		ImGui::Text("Source region: %u, %u — %u x %u%s", selection.sourceU, selection.sourceV,
			selection.sourceWidth, selection.sourceHeight, selection.textureOverridden ? " (override source)" : "");
		if (hasTexture)
		{
			ImGui::Text("Texture: %s | level page %d | index %d", textureInfo.textureName,
				textureInfo.texturePage, textureInfo.textureIndex);
			if (textureInfo.hasOverride)
				ImGui::Text("Override: %s :: %s", textureInfo.modId, textureInfo.overridePath);
			ImGui::Text("Full texture: %u x %u at UV %u, %u", textureInfo.width, textureInfo.height, textureInfo.u, textureInfo.v);
			if (textureInfo.previewTextureId)
			{
				ImGui::TextUnformatted("Loaded override preview (not the original VRAM texture)");
				const float size = ImGui::GetContentRegionAvail().x < 192.0f ? ImGui::GetContentRegionAvail().x : 192.0f;
				ImGui::Image((ImTextureID)textureInfo.previewTextureId, ImVec2(size, size));
			}
		}
		else if (selection.textured)
		{
			ImGui::TextDisabled("Texture name is not registered by the current level texture set.");
		}
		ImGui::TextWrapped("Object: %s", selection.provenance[0] ? selection.provenance : "Unlabelled draw source");
		if (selection.object.key[0])
		{
			ImGui::Text("Object key: %s", selection.object.key);
			ImGui::Text("Model name: %s | source slot: %d", selection.object.modelName, selection.object.modelIndex);
			ImGui::Text("Rendered model: %d polygons | %d vertices (0 = not reported)", selection.object.polygonCount, selection.object.vertexCount);
			ImGui::Text("World position: %d, %d, %d", selection.object.position[0], selection.object.position[1], selection.object.position[2]);
			ImGui::TextDisabled("Object highlighting groups submitted triangles across textures and LOD changes. The texture above belongs to the clicked face.");
		}
		if (hasCar)
		{
			const int* position = car_data[selectedCarId].hd.where.t;
			ImGui::Text("Live car position (game units): %d, %d, %d", position[0], position[1], position[2]);
			ImGui::Text("Live model slot: %d | speed: %d", car_data[selectedCarId].ap.model, car_data[selectedCarId].hd.speed);
			ImGui::TextDisabled("Tracking uses the current car slot; reselect after changing mission or spawning cars.");
		}
	}
	else
	{
		ImGui::TextDisabled("No primitive selected. Enable picking and click a visible element outside this panel.");
	}

	if (hasSelection && selection.object.key[0] && !hasCar)
	{
		const int modelRecord = AssetCatalog_FindModel(selection.object.modelIndex);
		if (modelRecord >= 0)
		{
			char catalogId[ASSET_CATALOG_ID_CAPACITY] = {};
			char catalogName[ASSET_CATALOG_NAME_CAPACITY] = {};
			int lodParent = -1, highDetail = -1;
			AssetCatalogSource source = ASSET_CATALOG_SOURCE_UNKNOWN;
			AssetCatalog_MakeModelId(selection.object.modelIndex, catalogId, sizeof(catalogId));
			AssetCatalog_GetModel(modelRecord, NULL, catalogName, sizeof(catalogName), &source, &lodParent, &highDetail);

			ImGui::SeparatorText("Catalog record");
			ImGui::Text("Stable id: %s", catalogId);
			ImGui::Text("Source: %s | name: %s", AssetSourceLabel(source), catalogName[0] ? catalogName : "(unnamed)");
			ImGui::Text("LOD: high-detail %d | parent %d (-1 = none)", highDetail, lodParent);

			int materials[192];
			const int materialCount = AssetCatalog_EnumerateModelTextures(modelRecord, materials, 192);
			if (ImGui::CollapsingHeader("Source materials (catalog, includes hidden faces)", ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::Text("%d material link(s)", materialCount);
				for (int i = 0; i < materialCount && i < 192; ++i)
				{
					char textureName[ASSET_CATALOG_NAME_CAPACITY] = {};
					int page = -1, index = -1, set = -1;
					AssetCatalogSource textureSource = ASSET_CATALOG_SOURCE_UNKNOWN;
					AssetCatalog_GetTexture(materials[i], textureName, sizeof(textureName), &page, &index, &set, &textureSource);
					ImGui::BulletText("%s | page %d / index %d (%s) [set %d]", textureName, page, index,
						AssetSourceLabel(textureSource), set);
				}
				if (materialCount == 0)
					ImGui::TextDisabled("No materials linked for this model in the catalog.");
			}
		}
		else
		{
			ImGui::TextDisabled("Model slot %d is not in the catalog (freed/streamed slot or unlabelled draw source).",
				selection.object.modelIndex);
		}
	}

	ImGui::SeparatorText("Export");
	ImGui::InputText("Export mod id", g_inspectorExportModId, sizeof(g_inspectorExportModId));
	char modsDirectory[512];
	HdTextureOverrides_GetModsDirectory(modsDirectory, sizeof(modsDirectory));
	ImGui::TextWrapped("Mods root: %s", modsDirectory);
	ImGui::TextWrapped("Texture destination: %s/assets/inspector/\nModel destination: %s/assets/models/", g_inspectorExportModId, g_inspectorExportModId);
#ifdef _WIN32
	const bool exportSupported = true;
#else
	const bool exportSupported = false;
	ImGui::TextDisabled("PNG and OBJ export currently require Windows.");
#endif
	ImGui::BeginDisabled(!hasTexture || !exportSupported);
	if (ImGui::Button("Export original full texture (PNG)"))
		HdTextureOverrides_ExportTexture(selection.tpage, selection.clut, textureInfo.u, textureInfo.v,
			textureInfo.width, textureInfo.height, textureInfo.textureName, textureInfo.texturePage,
			textureInfo.textureIndex, g_inspectorExportModId, g_inspectorExportStatus, sizeof(g_inspectorExportStatus));
	ImGui::EndDisabled();
	if (!hasTexture) ImGui::TextDisabled("PNG unavailable: select a primitive inside a registered texture region.");
	ImGui::BeginDisabled(!hasCar || !exportSupported);
	if (ImGui::Button("Export selected car geometry (OBJ)"))
		Cars_ExportInspectorModel(selectedCarId, g_inspectorExportModId, g_inspectorExportStatus, sizeof(g_inspectorExportStatus));
	ImGui::EndDisabled();
	if (!hasCar) ImGui::TextDisabled("OBJ unavailable: only labelled car bodies currently support model export.");
	ImGui::BeginDisabled(!hasSelection);
	char report[2048];
	snprintf(report, sizeof(report),
		"REDRIVER2-Plus inspector selection\nObject: %s\nPrimitive: %d\nGPU page: %u\nCLUT: %u\n"
		"UV: %u,%u / %u,%u / %u,%u\nTexture: %s\nLevel page/index: %d/%d\n"
		"Full region: %u,%u %ux%u\nLoaded override mod: %s\nOverride path: %s\nMods root: %s\n"
		"Object key: %s\nModel name: %s\nModel slot: %d\nVertices/polygons: %d/%d\nWorld position: %d,%d,%d\n"
		"Model archive filename: not recorded by this draw source\nPicking: draw-stream approximation, not depth-tested\n",
		selection.provenance[0] ? selection.provenance : "Unlabelled", selection.primitiveIndex, selection.tpage, selection.clut,
		selection.u[0], selection.v[0], selection.u[1], selection.v[1], selection.u[2], selection.v[2],
		hasTexture ? textureInfo.textureName : "Unregistered", hasTexture ? textureInfo.texturePage : -1,
		hasTexture ? textureInfo.textureIndex : -1, textureInfo.u, textureInfo.v, textureInfo.width, textureInfo.height,
		textureInfo.hasOverride ? textureInfo.modId : "None", textureInfo.overridePath, modsDirectory,
		selection.object.key, selection.object.modelName, selection.object.modelIndex, selection.object.vertexCount,
		selection.object.polygonCount, selection.object.position[0], selection.object.position[1], selection.object.position[2]);
	if (ImGui::Button("Copy selection details")) ImGui::SetClipboardText(report);
	ImGui::BeginDisabled(!exportSupported);
	if (ImGui::Button("Export selection details (TXT)"))
		HdTextureOverrides_ExportInspectorReport(g_inspectorExportModId, report, g_inspectorExportStatus, sizeof(g_inspectorExportStatus));
	ImGui::EndDisabled();
	ImGui::EndDisabled();
	ImGui::TextWrapped("%s", g_inspectorExportStatus);
	ImGui::TextDisabled("Each successful export replaces that file. Failed writes preserve the previous export. PNG exports original VRAM pixels; edit the PNG, declare it in manifest.json, enable the mod and reload above. OBJ is geometry-only; model re-import is not implemented.");
	ImGui::TextDisabled("Names are runtime texture names and model slots, not inferred archive filenames. Picking/highlighting is a diagnostic draw-stream approximation, not a depth-tested editor selection.");

	PsyXInspectorTriangle triangle;
	int triangleCount = 0;
	const ImVec2 display = ImGui::GetIO().DisplaySize;
	ImDrawList* overlay = ImGui::GetBackgroundDrawList();
	const ImU32 fill = ImGui::ColorConvertFloat4ToU32(ImVec4(highlightColour[0], highlightColour[1], highlightColour[2], highlightColour[3]));
	const ImU32 edge = ImGui::ColorConvertFloat4ToU32(ImVec4(highlightColour[0], highlightColour[1], highlightColour[2], 1.0f));
	while (PsyX_Inspector_GetTriangle(triangleCount, &triangle))
	{
		ImVec2 points[3];
		for (int i = 0; i < 3; ++i) points[i] = ImVec2(triangle.x[i] * display.x, triangle.y[i] * display.y);
		if (showHighlight)
		{
			overlay->AddTriangleFilled(points[0], points[1], points[2], fill);
			overlay->AddTriangle(points[0], points[1], points[2], edge);
		}
		if (showLabel && triangleCount == 0)
			overlay->AddText(points[0], edge, selection.provenance[0] ? selection.provenance : "Selected triangle");
		++triangleCount;
	}
	ImGui::Text("Highlighted triangles this frame: %d (limit 4096)", triangleCount);
	if (hasSelection && !triangleCount) ImGui::TextDisabled("Selected draw source not found this frame. Unlabelled primitives require another click; labelled sources track while rendered.");

	if (hasSelection && ImGui::CollapsingHeader("Textures on the selected object", ImGuiTreeNodeFlags_DefaultOpen))
	{
		struct ObjectTexture { HdTextureInspectorInfo info; unsigned short page, clut; };
		static ObjectTexture textures[128];
		static int textureCount = 0;
		static char cachedKey[128] = {};
		static double nextRefresh = 0;
		const char* key = selection.object.key[0] ? selection.object.key : selection.provenance;
		if (!triangleCount || strcmp(cachedKey, key) || ImGui::GetTime() >= nextRefresh)
		{
			snprintf(cachedKey, sizeof(cachedKey), "%s", key);
			nextRefresh = ImGui::GetTime() + 0.25;
			textureCount = 0;
			for (int i = 0; i < triangleCount && textureCount < 128; ++i)
			{
				PsyX_Inspector_GetTriangle(i, &triangle);
				bool found = false;
				for (int j = 0; j < textureCount; ++j)
				{
					const ObjectTexture& item = textures[j];
					if (item.page == triangle.tpage && item.clut == triangle.clut && triangle.u >= item.info.u && triangle.v >= item.info.v &&
						triangle.u + triangle.width <= item.info.u + item.info.width && triangle.v + triangle.height <= item.info.v + item.info.height)
					{ found = true; break; }
				}
				if (found) continue;
				ObjectTexture item = {};
				if (!HdTextureOverrides_FindTextureInfo(triangle.tpage, triangle.clut, triangle.u, triangle.v, triangle.width, triangle.height, &item.info)) continue;
				item.page = triangle.tpage; item.clut = triangle.clut;
				textures[textureCount++] = item;
			}
		}
		ImGui::Text("%d registered texture/palette bindings (limit 128)", textureCount);
		ImGui::TextDisabled("Uses submitted geometry, not all materials in the source archive. Unregistered regions are omitted.");
		for (int i = 0; i < textureCount; ++i)
		{
			const ObjectTexture& item = textures[i];
			ImGui::PushID(i);
			ImGui::Text("%s | page %d / index %d | %u x %u | CLUT %u", item.info.textureName, item.info.texturePage,
				item.info.textureIndex, item.info.width, item.info.height, item.clut);
			ImGui::BeginDisabled(!exportSupported);
			if (ImGui::Button("Export this texture"))
				HdTextureOverrides_ExportTexture(item.page, item.clut, item.info.u, item.info.v, item.info.width, item.info.height,
					item.info.textureName, item.info.texturePage, item.info.textureIndex, g_inspectorExportModId, g_inspectorExportStatus, sizeof(g_inspectorExportStatus));
			ImGui::EndDisabled();
			ImGui::PopID();
		}
	}

}

void DrawModsTab()
{
	HdTextureOverrideDiagnostics diagnostics = {};
	HdTextureOverrides_GetDiagnostics(&diagnostics);

	ImGui::TextUnformatted("Texture mods");
	ImGui::SameLine();
	HelpMarker("Active texture mods replace registered level textures. Later enabled mods win when two declare the same texture; a disabled override is inert, so the original VRAM data is always available.");

	if (diagnostics.supported)
	{
		bool enabled = diagnostics.enabled != 0;
		if (ImGui::Checkbox("Enable HD texture overrides", &enabled))
			HdTextureOverrides_SetEnabled(enabled);
		ImGui::SameLine();
		HelpMarker("Takes effect on the next primitive without reloading the level. Manifest or PNG edits require a reload or a level reload.");
	}
	else
	{
		ImGui::TextDisabled("HD texture PNG loading is not available on this platform.");
	}

	ImGui::Text("Mods: %d discovered | %d enabled | %d texture entries", diagnostics.discoveredMods,
		diagnostics.activeMods, diagnostics.manifestEntries);
	ImGui::Text("Loaded RGBA images: %d | active renderer mappings: %d", diagnostics.loadedImages,
		diagnostics.registeredOverrides);
	ImGui::TextWrapped("%s", diagnostics.status);

	char modsDirectory[512];
	HdTextureOverrides_GetModsDirectory(modsDirectory, sizeof(modsDirectory));
	ImGui::TextWrapped("Mods root: %s", modsDirectory);
	ImGui::TextDisabled("Each mod is a folder with a manifest.json under this root.");

	if (ImGui::Button("Reload mod manifests and images"))
		HdTextureOverrides_Reload();
	ImGui::SameLine();
	HelpMarker("Reloads JSON manifests and PNG files, then re-registers texture regions already loaded by the current level. It never reloads or edits original game data.");

	if (ImGui::CollapsingHeader("Enabled mod order", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::TextDisabled("Later enabled mods have higher texture-override priority.");
		for (int i = 0; i < HdTextureOverrides_GetModCount(); ++i)
		{
			HdTextureOverrideModInfo mod = {};
			if (!HdTextureOverrides_GetModInfo(i, &mod))
				continue;
			ImGui::BulletText("%s%s — %s (%d texture entries)", mod.enabled ? "" : "[disabled] ",
				mod.id, mod.name, mod.textureEntries);
			if (mod.description[0] != '\0')
				ImGui::TextDisabled("    %s", mod.description);
		}
	}

	if (ImGui::CollapsingHeader("Declared texture overrides", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::TextDisabled("Entries are resolved by texture name and can be narrowed with texturePage and textureIndex in manifest.json.");
		ImGui::BeginChild("ModTextureEntries", ImVec2(0.0f, 220.0f), true);
		for (int i = 0; i < HdTextureOverrides_GetEntryCount(); ++i)
		{
			HdTextureOverrideEntryInfo entry = {};
			if (!HdTextureOverrides_GetEntryInfo(i, &entry))
				continue;
			ImGui::Text("%s :: %s", entry.modId, entry.textureName);
			ImGui::TextDisabled("%s%s", entry.active ? "loaded — " : "pending — ", entry.assetPath);
			if (entry.texturePage >= 0 || entry.textureIndex >= 0)
				ImGui::TextDisabled("    page: %d  index: %d", entry.texturePage, entry.textureIndex);
		}
		ImGui::EndChild();
	}
}

void DrawAboutTab()
{
	ImGui::TextUnformatted("REDRIVER2-Plus modifications implemented by Lucas Sims.");
	ImGui::TextLinkOpenURL("github.com/SimStm", "https://github.com/SimStm");
	ImGui::TextLinkOpenURL("REDRIVER2-Plus repository", "https://github.com/SimStm/REDRIVER2-Plus");

	ImGui::Separator();
	ImGui::TextUnformatted("Original project and credits");
	ImGui::TextLinkOpenURL("OpenDriver2/REDRIVER2", "https://github.com/OpenDriver2/REDRIVER2");
	ImGui::TextLinkOpenURL("OpenDriver2/PsyCross (Psy-X)", "https://github.com/OpenDriver2/PsyCross");
	ImGui::BulletText("SoapyMan — lead reverse engineer and programmer");
	ImGui::BulletText("Fireboyd78 — refactoring and improvements");
	ImGui::BulletText("Krishty and someone972 — early format decoding");
	ImGui::BulletText("Gh0stBlade — PsyCross original code base");
	ImGui::BulletText("Ben Lincoln — TDR utility");
	ImGui::BulletText("Stohrendorf — Symdump utility");
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

	if (g_visible && g_inspectorPickMode && event->type == SDL_MOUSEBUTTONDOWN &&
		event->button.button == SDL_BUTTON_LEFT && !ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow))
	{
		PsyX_Inspector_RequestPick(event->button.x, event->button.y);
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

	const ImVec2 displaySize = ImGui::GetIO().DisplaySize;
	ImGui::SetNextWindowSizeConstraints(ImVec2(320.0f, 240.0f), ImVec2(displaySize.x, displaySize.y * 0.95f));
	ImGui::SetNextWindowSize(ImVec2(620.0f, displaySize.y * 0.85f), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("Developer Graphics Panel", &g_visible))
	{
		ImGui::PushTextWrapPos(0.0f);
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
				ImGui::TextDisabled("Texture overrides and active mods are on the Mods tab.");
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

			if (ImGui::BeginTabItem("3D Debug"))
			{
				DrawThreeDDebugTab();
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Mods"))
			{
				DrawModsTab();
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("About"))
			{
				DrawAboutTab();
				ImGui::EndTabItem();
			}
			ImGui::EndTabBar();
		}
		ImGui::PopTextWrapPos();
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
