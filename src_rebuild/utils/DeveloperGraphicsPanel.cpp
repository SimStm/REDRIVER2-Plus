#include "DeveloperGraphicsPanel.h"

#include "DeveloperDebugStart.h"
#include "DeveloperGraphicsSettings.h"
#include "DeveloperInputMapping.h"
#include "DeveloperModernMesh.h"
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
#include "C/loadview.h"
#include "C/mission.h"
#include "C/overlay.h"
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

// Legacy content options owned by config.ini [game]. The game declares these
// inside the functions that read them, so they are declared here the same way;
// inside the panel's anonymous namespace they would have internal linkage.
extern int gContentOverride;
extern int gUserLanguage;
extern int gDriver1Music;

// config.ini freeCamera, kept so the panel can show the startup value of a
// restart-only option it cannot change at runtime.
extern int gFreeCameraConfiguration;

namespace
{
bool g_initialised = false;
bool g_visible = false;
bool g_captureGameInput = true;
bool g_inspectorPickMode = false;
char g_persistenceStatus[96] = "Session settings only";
char g_inspectorExportModId[48] = "inspector-export";
char g_inspectorExportStatus[512] = "Select a texture to export it into a new or existing mod directory.";

// A display mode change can leave the window unusable, so it is provisional
// until the user confirms it. The countdown runs even while the panel is
// closed, so a change can never strand the window: with no way to press Keep,
// it reverts.
const float kDisplayRevertSeconds = 15.0f;

int g_displayRevertArmed = 0;
int g_displayRevertFullscreen = 0;
int g_displayRevertWidth = 0;
int g_displayRevertHeight = 0;
float g_displayRevertRemaining = 0.0f;
char g_displayStatus[128] = "";

// Defined below, once the binding-capture state it also reads is declared.
void UpdateOverlayInputState();

int ChaseDisplayMode(int fullscreen, int width, int height)
{
	int appliedWidth = 0;
	int appliedHeight = 0;
	const int applied = PsyX_ApplyWindowMode(fullscreen, width, height, &appliedWidth, &appliedHeight);
	snprintf(g_displayStatus, sizeof(g_displayStatus), applied
		? "Applied %s %d x %d"
		: "Window manager refused %s %d x %d; kept the previous mode",
		fullscreen ? "fullscreen" : "windowed", appliedWidth, appliedHeight);
	return applied;
}

// Starts a provisional mode change from the state that is on screen now.
void BeginDisplayChange(int fullscreen, int width, int height)
{
	SDL_Window* window = PsyX_GetSDLWindow();
	if (window)
	{
		g_displayRevertFullscreen = (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) != 0;
		SDL_GetWindowSize(window, &g_displayRevertWidth, &g_displayRevertHeight);
	}
	else
	{
		g_displayRevertFullscreen = 0;
		g_displayRevertWidth = width;
		g_displayRevertHeight = height;
	}

	if (!ChaseDisplayMode(fullscreen, width, height))
		return;

	g_displayRevertArmed = 1;
	g_displayRevertRemaining = kDisplayRevertSeconds;
	UpdateOverlayInputState();
}

void RevertDisplayMode()
{
	ChaseDisplayMode(g_displayRevertFullscreen, g_displayRevertWidth, g_displayRevertHeight);
	g_displayRevertArmed = 0;
	g_displayRevertRemaining = 0.0f;
	UpdateOverlayInputState();
}

// Accepts the provisional mode and persists it. Shared by the panel button and
// the display tests so both exercise the same path.
int ConfirmDisplayMode()
{
	g_displayRevertArmed = 0;
	g_displayRevertRemaining = 0.0f;
	UpdateOverlayInputState();
	return DeveloperGraphicsSettings_SaveRuntime() ? 1 : 0;
}

// Runs every frame the overlay is drawn, visible or not, so the countdown is
// never paused by closing the panel.
void UpdateDisplayRevert()
{
	if (!g_displayRevertArmed)
		return;

	g_displayRevertRemaining -= ImGui::GetIO().DeltaTime;
	if (g_displayRevertRemaining <= 0.0f)
		RevertDisplayMode();
}

// The display confirmation is its own window rather than part of the panel: a
// mode change can hide, clip or shrink the panel, so the buttons that accept or
// reject it must not depend on it. The position is anchored to the applied
// display size every frame, which keeps the window on screen after the very
// resolution change it is asking about.
void DrawDisplayConfirmWindow()
{
	if (!g_displayRevertArmed)
		return;

	const ImGuiIO& io = ImGui::GetIO();
	const ImVec2 margin(16.0f, 16.0f);
	ImGui::SetNextWindowPos(ImVec2(margin.x, io.DisplaySize.y - margin.y), ImGuiCond_Always, ImVec2(0.0f, 1.0f));
	ImGui::SetNextWindowBgAlpha(0.94f);

	const ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize;

	if (ImGui::Begin("Display mode", NULL, flags))
	{
		ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), "Display mode not confirmed");
		ImGui::Text("Reverting in %.0f s.", g_displayRevertRemaining);
		ImGui::Separator();
		if (ImGui::Button("Keep this display mode", ImVec2(190.0f, 0.0f)))
		{
			snprintf(g_persistenceStatus, sizeof(g_persistenceStatus), "%s",
				ConfirmDisplayMode() ? "Saved developer_graphics.ini" : "Save failed; display mode is not saved");
		}
		ImGui::SameLine();
		if (ImGui::Button("Revert now", ImVec2(120.0f, 0.0f)))
			RevertDisplayMode();
	}
	ImGui::End();
}

// Binding capture: while a binding is being captured the panel consumes the
// next matching event, so the key or button being bound can never reach the
// game. A capture also expires on its own, so a lost focus or a forgotten
// dialog cannot leave input held by the panel forever.
struct BindingCapture
{
	int active;
	DeveloperInputTable table;
	DeveloperInputDevice device;
	int action;
	float remaining;
};

BindingCapture g_bindingCapture = { 0, DeveloperInputTable::Game, DeveloperInputDevice::Keyboard, -1, 0.0f };
const float kBindingCaptureSeconds = 10.0f;
char g_bindingStatus[160] = "Overrides live in developer_input.ini; config.ini is not modified.";

// The panel, a binding capture and the provisional display mode all need the
// mouse cursor and all must keep their clicks out of the game. The display
// confirmation can be on screen with the panel closed, so it is part of both
// decisions instead of relying on g_visible.
void UpdateOverlayInputState()
{
	const int showCursor = g_visible || g_displayRevertArmed;
	PsyX_SetCursorRelative(0);
	SDL_ShowCursor(showCursor ? SDL_ENABLE : SDL_DISABLE);

	const int capture = (g_visible && g_captureGameInput) || g_bindingCapture.active || g_displayRevertArmed;
	PsyX_SetInputCapture(capture ? PSYX_INPUT_CAPTURE_KEYBOARD | PSYX_INPUT_CAPTURE_GAMEPAD : 0);
}

void BeginBindingCapture(DeveloperInputTable table, DeveloperInputDevice device, int action)
{
	g_bindingCapture.active = 1;
	g_bindingCapture.table = table;
	g_bindingCapture.device = device;
	g_bindingCapture.action = action;
	g_bindingCapture.remaining = kBindingCaptureSeconds;
	UpdateOverlayInputState();
}

void EndBindingCapture()
{
	g_bindingCapture.active = 0;
	g_bindingCapture.action = -1;
	g_bindingCapture.remaining = 0.0f;
	UpdateOverlayInputState();
}

void CancelBindingCapture()
{
	if (!g_bindingCapture.active)
		return;

	snprintf(g_bindingStatus, sizeof(g_bindingStatus), "Cancelled %s capture",
		DeveloperInputMapping_ActionName(g_bindingCapture.device, g_bindingCapture.action));
	EndBindingCapture();
}

void CommitBinding(DeveloperInputDevice device, int value)
{
	if (!g_bindingCapture.active || g_bindingCapture.device != device)
		return;

	char binding[64];
	char status[160];
	DeveloperInputMapping_Format(device, value, binding, sizeof(binding));
	DeveloperInputMapping_Set(g_bindingCapture.table, device, g_bindingCapture.action, value);
	DeveloperInputMapping_Apply(g_bindingCapture.table);
	const int saved = DeveloperInputMapping_SaveDefaultFile();

	snprintf(status, sizeof(status), "%s -> %s (%s), %s",
		DeveloperInputMapping_ActionName(device, g_bindingCapture.action), binding,
		g_bindingCapture.table == DeveloperInputTable::Game ? "game" : "menu",
		saved >= 0 ? "saved" : "save failed; session only");
	snprintf(g_bindingStatus, sizeof(g_bindingStatus), "%s", status);
	EndBindingCapture();
}

// Consumes the events a capture is waiting for. A capture only accepts its own
// device, so binding a key never silently rebinds a controller action.
int HandleBindingCaptureEvent(const SDL_Event* event)
{
	if (!g_bindingCapture.active)
		return 0;

	if (g_bindingCapture.device == DeveloperInputDevice::Keyboard)
	{
		switch (event->type)
		{
		case SDL_KEYDOWN:
			if (event->key.repeat == 0)
			{
				if (event->key.keysym.scancode == SDL_SCANCODE_ESCAPE)
					CancelBindingCapture();
				else
					CommitBinding(DeveloperInputDevice::Keyboard, event->key.keysym.scancode);
			}
			return 1;
		default:
			break;
		}
	}
	else
	{
		switch (event->type)
		{
		case SDL_CONTROLLERBUTTONDOWN:
			CommitBinding(DeveloperInputDevice::Controller, event->cbutton.button);
			return 1;
		case SDL_CONTROLLERAXISMOTION:
			// A resting stick reports a small offset; only a deliberate push
			// counts as a binding.
			if (event->caxis.value > 16000 || event->caxis.value < -16000)
			{
				int value = event->caxis.axis | CONTROLLER_MAP_FLAG_AXIS;
				if (event->caxis.value < 0)
					value |= CONTROLLER_MAP_FLAG_INVERSE;
				CommitBinding(DeveloperInputDevice::Controller, value);
			}
			return 1;
		default:
			break;
		}
	}

	// Right-click cancels; left clicks must still reach the panel's own widgets.
	if (event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_RIGHT)
	{
		CancelBindingCapture();
		return 1;
	}

	return 0;
}

void UpdateBindingCapture()
{
	if (!g_bindingCapture.active)
		return;

	g_bindingCapture.remaining -= ImGui::GetIO().DeltaTime;
	if (g_bindingCapture.remaining <= 0.0f)
	{
		snprintf(g_bindingStatus, sizeof(g_bindingStatus), "Capture timed out; nothing changed");
		EndBindingCapture();
	}
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

// Width of the checkbox as ImGui lays it out: the square, the inner spacing and
// the label. Used to decide whether a trailing help marker still fits.
float CheckboxControlWidth(const char* label)
{
	return ImGui::GetFrameHeight() + ImGui::GetStyle().ItemInnerSpacing.x + ImGui::CalcTextSize(label).x;
}

// Help marker at the end of the control that was just submitted. The marker
// stays on the control's line when the line has room and moves to its own line
// otherwise, so a long label is never clipped by the marker.
void HelpMarkerAfterControl(const char* description, float controlWidth)
{
	const float markerWidth = ImGui::CalcTextSize("(?)").x + ImGui::GetStyle().ItemSpacing.x;
	if (controlWidth + markerWidth <= ImGui::GetContentRegionAvail().x)
		ImGui::SameLine();
	HelpMarker(description);
}

// Checkbox with its explanation at the end of the label instead of below it.
bool CheckboxWithHelp(const char* label, bool* value, const char* description)
{
	const float controlWidth = CheckboxControlWidth(label);
	const bool changed = ImGui::Checkbox(label, value);
	HelpMarkerAfterControl(description, controlWidth);
	return changed;
}

// Slider with the same trailing-marker treatment as CheckboxWithHelp.
bool SliderFloatWithHelp(const char* label, float* value, float minimum, float maximum,
	const char* format, const char* description)
{
	const float controlWidth = ImGui::CalcItemWidth() + ImGui::GetStyle().ItemInnerSpacing.x + ImGui::CalcTextSize(label).x;
	const bool changed = ImGui::SliderFloat(label, value, minimum, maximum, format);
	HelpMarkerAfterControl(description, controlWidth);
	return changed;
}

// Integer slider with the same trailing-marker treatment as CheckboxWithHelp.
bool SliderIntWithHelp(const char* label, int* value, int minimum, int maximum,
	const char* format, const char* description)
{
	const float controlWidth = ImGui::CalcItemWidth() + ImGui::GetStyle().ItemInnerSpacing.x + ImGui::CalcTextSize(label).x;
	const bool changed = ImGui::SliderInt(label, value, minimum, maximum, format);
	HelpMarkerAfterControl(description, controlWidth);
	return changed;
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

// Builds the readable filename suffix and the informational model references
// for one identified texture from verified catalog metadata. The suffix falls
// back to the model slot, and a missing catalog model record is stated
// explicitly instead of inventing a name. Neither value changes the override
// key: identity stays the (name, texturePage, textureIndex) triple.
void FillBatchTextureItem(HdTextureOverrideBatchItem* item, int selectedModelIndex,
	const char* textureName, int texturePage, int textureIndex,
	unsigned short tpage, unsigned short clut,
	unsigned short u, unsigned short v, unsigned short width, unsigned short height)
{
	memset(item, 0, sizeof(*item));
	item->tpage = tpage;
	item->clut = clut;
	item->u = u;
	item->v = v;
	item->width = width;
	item->height = height;
	snprintf(item->textureName, sizeof(item->textureName), "%s", textureName ? textureName : "");
	item->texturePage = texturePage;
	item->textureIndex = textureIndex;

	if (selectedModelIndex >= 0)
	{
		char modelName[ASSET_CATALOG_NAME_CAPACITY] = {};
		const int modelRecord = AssetCatalog_FindModel(selectedModelIndex);
		if (modelRecord >= 0)
			AssetCatalog_GetModel(modelRecord, NULL, modelName, sizeof(modelName), NULL, NULL, NULL);
		if (!HdTextureOverrides_MakeSafeNameToken(modelName, item->filenameSuffix, sizeof(item->filenameSuffix)))
			snprintf(item->filenameSuffix, sizeof(item->filenameSuffix), "slot%d", selectedModelIndex);
	}

	const int textureRecord = AssetCatalog_FindTexture(item->textureName, texturePage, textureIndex);
	if (textureRecord < 0)
		return;

	int modelRecords[HD_TEXTURE_MODEL_REFERENCES_MAX];
	const int modelCount = AssetCatalog_EnumerateTextureModels(textureRecord, modelRecords,
		HD_TEXTURE_MODEL_REFERENCES_MAX);
	for (int i = 0; i < modelCount && item->modelReferenceCount < HD_TEXTURE_MODEL_REFERENCES_MAX; ++i)
	{
		int slot = -1;
		if (!AssetCatalog_GetModel(modelRecords[i], &slot, NULL, 0, NULL, NULL, NULL) || slot < 0)
			continue;

		char* reference = item->modelReferences[item->modelReferenceCount];
		if (!AssetCatalog_MakeModelId(slot, reference, HD_TEXTURE_MODEL_REFERENCE_CAPACITY))
			snprintf(reference, HD_TEXTURE_MODEL_REFERENCE_CAPACITY, "unknown:model-slot:%d", slot);
		item->modelReferenceCount++;
	}
}

// Stamps the selection's object type and the current level onto an item, so the
// manifest describes what each exported texture belongs to.
void StampBatchItemContext(HdTextureOverrideBatchItem& item, const char* objectKey)
{
	snprintf(item.objectType, sizeof(item.objectType), "%s", HdTextureOverrides_ObjectTypeFromKey(objectKey));
	snprintf(item.levelName, sizeof(item.levelName), "%s", LevelNames[GameLevel]);
}



// Cooperative export state. The item array and job are static so a batch can
// keep running across frames while the panel shows progress and a cancel
// control. Files are published one at a time; cancelling keeps what was written.
HdTextureOverrideBatchItem g_batchItems[HD_TEXTURE_BATCH_MAX_ITEMS];
HdTextureOverrideBatchJob g_batchJob;
bool g_batchJobVisible = false;
char g_batchScope[192] = {};

void StartBatchJob(const char* modId, int itemCount, const char* scope)
{
	memset(&g_batchJob, 0, sizeof(g_batchJob));
	snprintf(g_batchScope, sizeof(g_batchScope), "%s", scope ? scope : "");
	g_batchJobVisible = true;
	HdTextureOverrides_BeginBatchJob(&g_batchJob, modId, g_batchItems, itemCount);
}

// Fills g_batchItems with every catalog material of the selected source model
// (including hidden faces) and its high-detail LOD sibling. Textures whose VRAM
// region is not registered are counted and named instead of being dropped,
// because the catalog does not model palette variants or cross-model children.
int BuildSourceModelBatchItems(int modelIndex, char* scope, int scopeCapacity)
{
	if (scopeCapacity > 0) scope[0] = '\0';
	if (modelIndex < 0)
	{
		snprintf(scope, scopeCapacity, "Source-model export needs a labelled object; this draw source has no model slot.");
		return 0;
	}

	const int modelRecord = AssetCatalog_FindModel(modelIndex);
	if (modelRecord < 0)
	{
		snprintf(scope, scopeCapacity, "Model slot %d is not in the catalog (streamed, unlabelled, or a car pack); source-model export is unavailable.", modelIndex);
		return 0;
	}

	int modelRecords[2];
	const int modelRecordCount = AssetCatalog_CollectExportModels(modelRecord, modelRecords, 2);

	int itemCount = 0;
	int missingRegions = 0;
	char missingNames[160] = {};

	for (int m = 0; m < modelRecordCount; ++m)
	{
		int textureRecords[HD_TEXTURE_BATCH_MAX_ITEMS];
		const int textureCount = AssetCatalog_EnumerateModelTextures(modelRecords[m], textureRecords, HD_TEXTURE_BATCH_MAX_ITEMS);
		for (int t = 0; t < textureCount; ++t)
		{
			char textureName[ASSET_CATALOG_NAME_CAPACITY] = {};
			int page = -1, index = -1;
			AssetCatalog_GetTexture(textureRecords[t], textureName, sizeof(textureName), &page, &index, NULL, NULL);

			unsigned short tpage = 0, clut = 0, u = 0, v = 0, width = 0, height = 0;
			if (!HdTextureOverrides_GetKnownTextureRegion(page, index, &tpage, &clut, &u, &v, &width, &height, NULL, 0))
			{
				++missingRegions;
				if (missingNames[0] == '\0')
					snprintf(missingNames, sizeof(missingNames), "%s (page %d/index %d)", textureName, page, index);
				continue;
			}
			if (itemCount >= HD_TEXTURE_BATCH_MAX_ITEMS)
				break;

			FillBatchTextureItem(&g_batchItems[itemCount], modelIndex, textureName, page, index, tpage, clut, u, v, width, height);
			++itemCount;
		}
	}

	snprintf(scope, scopeCapacity,
		"Source model %d: %d model(s) incl. high-detail LOD, %d material(s) incl. hidden faces. Missing VRAM region: %d%s%s. Palette variants and cross-model child parts are not enumerated.",
		modelIndex, modelRecordCount, itemCount, missingRegions,
		missingNames[0] != '\0' ? ", e.g. " : "", missingNames);
	return itemCount;
}

void DrawBatchJobUi()
{
	if (!g_batchJobVisible)
		return;

	if (HdTextureOverrides_BatchJobActive(&g_batchJob))
		HdTextureOverrides_StepBatchJob(&g_batchJob, 2);

	const bool active = HdTextureOverrides_BatchJobActive(&g_batchJob);
	const int total = g_batchJob.itemCount < HD_TEXTURE_BATCH_MAX_ITEMS ? g_batchJob.itemCount : HD_TEXTURE_BATCH_MAX_ITEMS;
	const int done = g_batchJob.exported + g_batchJob.duplicates + g_batchJob.failed;
	const float fraction = total > 0 ? (float)done / (float)total : 0.0f;

	ImGui::SeparatorText("Texture export batch");
	if (g_batchScope[0]) ImGui::TextWrapped("%s", g_batchScope);

	char overlay[48];
	snprintf(overlay, sizeof(overlay), "%d/%d", done, total);
	ImGui::ProgressBar(fraction, ImVec2(-1.0f, 0.0f), overlay);

	char status[320];
	HdTextureOverrides_GetBatchJobStatus(&g_batchJob, status, sizeof(status));
	ImGui::TextWrapped("%s", status);

	if (active)
	{
		if (ImGui::Button("Cancel batch")) HdTextureOverrides_CancelBatchJob(&g_batchJob);
	}
	else
	{
		if (g_batchJob.failed > 0 && ImGui::Button("Retry failed")) HdTextureOverrides_RetryFailedBatchJob(&g_batchJob);
		if (g_batchJob.failed > 0) ImGui::SameLine();
		if (ImGui::Button("Dismiss")) g_batchJobVisible = false;
	}

	if (ImGui::CollapsingHeader("Per-resource results", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::BeginChild("TextureBatchResults", ImVec2(0.0f, 150.0f), true);
		for (int i = 0; i < total; ++i)
		{
			const char* outcome;
			switch (g_batchJob.outcomes[i])
			{
				case HD_TEXTURE_BATCH_OUTCOME_DUPLICATE: outcome = "duplicate omitted"; break;
				case HD_TEXTURE_BATCH_OUTCOME_EXPORTED: outcome = "exported"; break;
				case HD_TEXTURE_BATCH_OUTCOME_FAILED: outcome = "failed"; break;
				default: outcome = active ? "pending" : "not processed"; break;
			}
			ImGui::Text("%s | page %d / index %d | %s", g_batchJob.items[i].textureName,
				g_batchJob.items[i].texturePage, g_batchJob.items[i].textureIndex, outcome);
			if (g_batchJob.messages[i][0] != '\0')
				ImGui::TextDisabled("    %s", g_batchJob.messages[i]);
		}
		if (total == 0)
			ImGui::TextDisabled("No resources in this batch.");
		ImGui::EndChild();
	}
}

void DrawLegacyContentSection();

void DrawGameDebugTab()
{
	ImGui::TextUnformatted("Live values behind the legacy in-game debug overlay.");
	ImGui::SameLine();
	HelpMarker("These values are read from the running game state. They help diagnose streaming, traffic, mission, vehicle, and road behaviour.");

	bool showLegacyStats = gDisplayDrawStats != 0;
	if (CheckboxWithHelp("Show legacy in-game stats", &showLegacyStats,
		"Draws the original text-only statistics directly into the PlayStation-style game frame. The ImGui view below is easier to inspect and does not require this option."))
		gDisplayDrawStats = showLegacyStats;

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

	ImGui::Separator();
	DrawLegacyContentSection();
}

// Legacy content options that live in config.ini [game]. They are session
// values here: the panel never writes config.ini, and each one states when the
// running game picks the change up.
void DrawLegacyContentSection()
{
	ImGui::TextUnformatted("Content and language");
	ImGui::SameLine();
	HelpMarker("These are config.ini [game] options. Changing them here affects the running session only; config.ini is the shipped owner and is never modified by the panel.");

	bool contentOverride = gContentOverride != 0;
	if (CheckboxWithHelp("Modded content override", &contentOverride,
		"config.ini overrideContent. Uses modded car models and cosmetic/denting resources when present. Car availability and the frontend read it while loading, so the change applies from the next frontend or level load."))
		gContentOverride = contentOverride ? 1 : 0;

	bool disableBridges = gDisableChicagoBridges != 0;
	if (CheckboxWithHelp("Disable Chicago bridges", &disableBridges,
		"config.ini disableChicagoBridges (experimental: also activate AI roads). Read when a level's roads are set up, so it applies from the next level load."))
		gDisableChicagoBridges = disableBridges ? 1 : 0;

	static const char* const kLanguageNames[] = { "English", "Italian", "German", "French", "Spanish" };
	int language = gUserLanguage;
	if (ImGui::Combo("Language", &language, kLanguageNames, (int)(sizeof(kLanguageNames) / sizeof(kLanguageNames[0]))))
		gUserLanguage = language;
	HelpMarkerAfterControl("config.ini languageId. The *_GAME.LTXT and *_MISSION.LTXT files are loaded once at startup, so a change applies after a restart.",
		ImGui::CalcItemWidth() + ImGui::GetStyle().ItemInnerSpacing.x + ImGui::CalcTextSize("Language").x);

	bool driver1Music = gDriver1Music != 0;
	if (CheckboxWithHelp("Driver 1 music", &driver1Music,
		"config.ini driver1music. Plays D1MUSIC.BIN from the DRIVER2\\SOUND folder when present. Read when music starts, so it applies from the next music change."))
		gDriver1Music = driver1Music ? 1 : 0;

	// Restart-only options are shown disabled with the reason rather than hidden,
	// so the panel does not look like it is missing them.
	bool freeCamera = gFreeCameraConfiguration != 0;
	ImGui::BeginDisabled();
	ImGui::Checkbox("Free camera (restart only)", &freeCamera);
	ImGui::EndDisabled();
	ImGui::SameLine();
	HelpMarker("config.ini freeCamera installs the debug camera key and mouse handlers while the game initialises, so it cannot be toggled in a running session. Set it in config.ini and restart.");
}

void DrawThreeDDebugTab()
{
	static bool showHighlight = true;
	static bool showLabel = true;
	static float highlightColour[4] = { 0.1f, 0.85f, 1.0f, 0.25f };

	// Logical scopes the same pick can be read at. The picker is unchanged; the
	// scope only decides which identity the panel presents and how it is named.
	enum SelectionScope { SCOPE_FACE = 0, SCOPE_MATERIAL, SCOPE_COMPONENT, SCOPE_OBJECT };
	static int selectionScope = SCOPE_COMPONENT;
	static AssetCatalogAnchor selectionAnchor = {};
	static char lastPickSignature[256] = {};
	static int anchorCarSlot = -1;
	static int anchorCarModel = -1;

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
	CheckboxWithHelp("Pick visible primitive", &g_inspectorPickMode,
		"With this enabled, left-click a visible game primitive outside the ImGui windows. The selection is resolved from the completed PSX draw stream on the next frame.");
	ImGui::Checkbox("Highlight selected draw source", &showHighlight);
	ImGui::Checkbox("Show selection label", &showLabel);
	ImGui::ColorEdit4("Highlight colour", highlightColour);
	if (ImGui::Button("Clear selection")) PsyX_Inspector_ClearSelection();
	if (ImGui::Button("Reload mod manifests and images"))
		HdTextureOverrides_Reload();
	ImGui::SameLine();
	HelpMarker("Reloads JSON manifests and PNG files, then re-registers texture regions already loaded by the current level. It never reloads or edits original game data.");

	ImGui::SeparatorText("Selection scope");
	ImGui::RadioButton("Face", &selectionScope, SCOPE_FACE);
	ImGui::SameLine();
	ImGui::RadioButton("Material", &selectionScope, SCOPE_MATERIAL);
	ImGui::SameLine();
	ImGui::RadioButton("Component", &selectionScope, SCOPE_COMPONENT);
	ImGui::SameLine();
	ImGui::RadioButton("Logical object", &selectionScope, SCOPE_OBJECT);
	ImGui::SameLine();
	HelpMarker("Every scope comes from the same pick: Face = the clicked primitive, Material = its texture, Component = a wheel/bone part, Logical object = the parent instance or model the part belongs to. No geometry or packet changes.");

	PsyXInspectorSelection selection = {};
	const bool hasSelection = PsyX_Inspector_GetSelection(&selection) != 0;
	HdTextureInspectorInfo textureInfo = {};
	const bool hasTexture = hasSelection && HdTextureOverrides_FindTextureInfo(selection.tpage, selection.clut,
		selection.sourceU, selection.sourceV, selection.sourceWidth, selection.sourceHeight, &textureInfo);
	int selectedCarId = -1;
	const bool hasCar = hasSelection && sscanf(selection.provenance, "Car #%d", &selectedCarId) == 1 &&
		selectedCarId >= 0 && selectedCarId < MAX_CARS;

	// A new pick captures a retained anchor. Model-backed keys get a catalog
	// handle; car keys additionally record the live slot so a reused car is
	// detected. Nothing is re-anchored while the same primitive stays selected.
	if (!hasSelection)
	{
		lastPickSignature[0] = '\0';
		selectionAnchor = AssetCatalogAnchor();
		anchorCarSlot = -1;
		anchorCarModel = -1;
	}
	else
	{
		char pickSignature[256];
		snprintf(pickSignature, sizeof(pickSignature), "%d|%s|%s", selection.primitiveIndex,
			selection.object.key, selection.provenance);
		if (strcmp(pickSignature, lastPickSignature) != 0)
		{
			snprintf(lastPickSignature, sizeof(lastPickSignature), "%s", pickSignature);
			AssetCatalog_CaptureAnchor(selection.object.key, &selectionAnchor);

			// Refresh the lookup explanation with the new selection, so its numbers
			// never describe a previous pick. The button stays for manual refresh.
			HdTextureOverrides_DebugRegion(selection.tpage, selection.clut, selection.sourceU,
				selection.sourceV, selection.sourceWidth, selection.sourceHeight);

			anchorCarSlot = -1;
			anchorCarModel = -1;
			char carKey[ASSET_CATALOG_ID_CAPACITY] = {};
			if (AssetCatalog_IsComponentKey(selection.object.key))
				AssetCatalog_ParseComponentKey(selection.object.key, carKey, sizeof(carKey), NULL, 0, NULL);
			else
				snprintf(carKey, sizeof(carKey), "%s", selection.object.key);
			int carLevel = -1, carVariant = -1, carSlot = -1, carModel = -1, carModelNumber = -1;
			if (sscanf(carKey, "car:%d:%d:%d:%d:%d", &carLevel, &carVariant, &carSlot, &carModel, &carModelNumber) == 5 &&
				carSlot >= 0 && carSlot < MAX_CARS)
			{
				anchorCarSlot = carSlot;
				anchorCarModel = carModel;
			}
		}
	}

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
				const unsigned long long overlayTexture = PsyX_GetOverlayTextureId(textureInfo.previewTextureId);
				if (overlayTexture)
				{
					int previewWidth = 0, previewHeight = 0;
					PsyX_GetRGBATextureSize(textureInfo.previewTextureId, &previewWidth, &previewHeight);

					const float maxWidth = ImGui::GetContentRegionAvail().x < 192.0f ? ImGui::GetContentRegionAvail().x : 192.0f;
					float width = maxWidth;
					float height = maxWidth;
					if (previewWidth > 0 && previewHeight > 0)
					{
						height = width * (float)previewHeight / (float)previewWidth;
						if (height > 256.0f)
						{
							height = 256.0f;
							width = height * (float)previewWidth / (float)previewHeight;
						}
					}
					ImGui::Image((ImTextureID)overlayTexture, ImVec2(width, height));
				}
				else
				{
					ImGui::TextDisabled("Preview is unavailable on this backend.");
				}
			}
		}
		else if (selection.textured)
		{
			ImGui::TextDisabled("Texture name is not registered by the current level texture set.");
		}
		ImGui::TextWrapped("Object: %s", selection.provenance[0] ? selection.provenance : "Unlabelled draw source");
		if (selection.wholeObject)
			ImGui::TextDisabled("Whole object: double-click expanded the pick to the parent instance; every part is highlighted and exports together.");
		if (selection.object.key[0])
		{
			ImGui::Text("Object key: %s", selection.object.key);
			ImGui::Text("Model name: %s | source slot: %d", selection.object.modelName, selection.object.modelIndex);
			ImGui::Text("Rendered model: %d polygons | %d vertices (0 = not reported)", selection.object.polygonCount, selection.object.vertexCount);
			ImGui::Text("World position: %d, %d, %d", selection.object.position[0], selection.object.position[1], selection.object.position[2]);
			if (AssetCatalog_IsComponentKey(selection.object.key))
			{
				char parentKey[ASSET_CATALOG_ID_CAPACITY] = {};
				char componentKind[ASSET_CATALOG_NAME_CAPACITY] = {};
				int componentIndex = -1;
				if (AssetCatalog_ParseComponentKey(selection.object.key, parentKey, sizeof(parentKey),
					componentKind, sizeof(componentKind), &componentIndex))
					ImGui::Text("Component: %s #%d of %s", componentKind, componentIndex, parentKey);
			}
			ImGui::TextDisabled("Object highlighting groups submitted triangles across textures and LOD changes. The texture above belongs to the clicked face.");
		}
		if (hasCar)
		{
			const int* position = car_data[selectedCarId].hd.where.t;
			ImGui::Text("Live car position (game units): %d, %d, %d", position[0], position[1], position[2]);
			ImGui::Text("Live model slot: %d | speed: %d", car_data[selectedCarId].ap.model, car_data[selectedCarId].hd.speed);
			ImGui::TextDisabled("Tracking uses the current car slot; reselect after changing mission or spawning cars.");
		}

		const AssetCatalogSelectionState anchorState = AssetCatalog_CheckAnchor(&selectionAnchor);
		ImGui::SeparatorText("Selected scope");
		switch (selectionScope)
		{
		case SCOPE_FACE:
			ImGui::Text("Face: primitive %d", selection.primitiveIndex);
			break;
		case SCOPE_MATERIAL:
			if (hasTexture)
				ImGui::Text("Material: %s | page %d / index %d", textureInfo.textureName,
					textureInfo.texturePage, textureInfo.textureIndex);
			else
				ImGui::TextDisabled("Material: no registered texture for this primitive.");
			break;
		case SCOPE_COMPONENT:
			if (AssetCatalog_IsComponentKey(selection.object.key))
			{
				char parentKey[ASSET_CATALOG_ID_CAPACITY] = {};
				char componentKind[ASSET_CATALOG_NAME_CAPACITY] = {};
				int componentIndex = -1;
				AssetCatalog_ParseComponentKey(selection.object.key, parentKey, sizeof(parentKey),
					componentKind, sizeof(componentKind), &componentIndex);
				ImGui::Text("Component: %s #%d of %s", componentKind, componentIndex, parentKey);
			}
			else
			{
				ImGui::TextDisabled("Component: this primitive has no component identity.");
			}
			break;
		case SCOPE_OBJECT:
		default:
			{
				char objectKey[ASSET_CATALOG_ID_CAPACITY] = {};
				if (AssetCatalog_IsComponentKey(selection.object.key))
					AssetCatalog_ParseComponentKey(selection.object.key, objectKey, sizeof(objectKey), NULL, 0, NULL);
				else
					snprintf(objectKey, sizeof(objectKey), "%s",
						selection.object.key[0] ? selection.object.key : selection.provenance);
				ImGui::Text("Logical object: %s", objectKey[0] ? objectKey : "(none)");
			}
			break;
		}

		const char* anchorLabel = "none";
		if (anchorState == ASSET_CATALOG_SELECTION_VALID) anchorLabel = "valid";
		else if (anchorState == ASSET_CATALOG_SELECTION_STALE) anchorLabel = "stale";
		else if (anchorState == ASSET_CATALOG_SELECTION_UNKNOWN) anchorLabel = "not model-backed";
		ImGui::Text("Lifetime: %s (anchor generation %u)", anchorLabel, selectionAnchor.generation);
		if (anchorState == ASSET_CATALOG_SELECTION_STALE)
			ImGui::TextDisabled("The anchored resource was reused or the level changed; re-pick to refresh.");
		if (anchorCarSlot >= 0)
		{
			if (car_data[anchorCarSlot].ap.model != anchorCarModel)
				ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f),
					"Car slot %d now holds model %d (anchor %d): stale",
					anchorCarSlot, car_data[anchorCarSlot].ap.model, anchorCarModel);
			else
				ImGui::Text("Car instance: slot %d | model %d (live)", anchorCarSlot, anchorCarModel);
		}

		const int selectionRange = PsyX_Inspector_GetSelectionRangeIndex();
		if (selectionRange >= 0)
		{
			PsyX_Inspector_GetRangeBounds(selectionRange);
			if (g_inspectorRangeBoundsValid)
				ImGui::Text("Source range %d screen bounds: (%.3f, %.3f) - (%.3f, %.3f)",
					selectionRange, g_inspectorRangeBounds[0], g_inspectorRangeBounds[1],
					g_inspectorRangeBounds[2], g_inspectorRangeBounds[3]);
		}

		if (!selection.object.key[0])
		{
			ImGui::SeparatorText("Diagnostics");
			ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f),
				"Unsupported source: no producer registered a draw-source range for this primitive.");
			ImGui::TextDisabled("Only the render label is available; identity, lifecycle checks and export are not.");
		}
		else if (selection.textureOverridden)
		{
			ImGui::SeparatorText("Diagnostics");
			if (selection.cutoutSampled)
				ImGui::TextDisabled("Cutout alpha sampled: texels the override discards are skipped when picking.");
			else
				ImGui::TextDisabled("Cutout alpha not sampled for this primitive (no CPU mask retained); the selection is geometric.");
		}
	}
	else
	{
		ImGui::TextDisabled("No primitive selected. Enable picking and click a visible element outside this panel.");
	}

	if (hasSelection && selection.object.key[0] && !hasCar && !AssetCatalog_IsComponentKey(selection.object.key))
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

	if (ImGui::CollapsingHeader("Pick index (frame-local registered sources)"))
	{
		int rangeCount = 0;
		for (int i = 0; i < 4096; ++i)
		{
			char rangeLabel[PSYX_INSPECTOR_LABEL_LENGTH] = {};
			int rangeVertices = 0;
			if (!PsyX_Inspector_GetRangeInfo(i, rangeLabel, sizeof(rangeLabel), &rangeVertices))
				break;
			++rangeCount;
		}

		int unreachable = 0;
		for (int i = 0; i < rangeCount; ++i)
		{
			char rangeLabel[PSYX_INSPECTOR_LABEL_LENGTH] = {};
			int rangeVertices = 0;
			PsyX_Inspector_GetRangeInfo(i, rangeLabel, sizeof(rangeLabel), &rangeVertices);
			if (rangeVertices == 0)
				++unreachable;
		}

		ImGui::Text("Ranges: %d | registered but unreachable: %d", rangeCount, unreachable);
		ImGui::SameLine();
		HelpMarker("A range with zero vertices was registered by a producer but no parsed primitive mapped to it this frame, so it cannot be picked. This is the diagnostic fallback for sources that would otherwise fail silently.");
		ImGui::Text("Last pick cost: %.3f ms (CPU)", PsyX_Inspector_GetPickCostMicros() / 1000.0f);
		if (ImGui::Button("Pick first reachable component"))
		{
			PsyX_Inspector_FindComponentPixel();
			if (g_inspectorComponentRange >= 0)
				PsyX_Inspector_RequestPick(g_inspectorComponentPixel[0], g_inspectorComponentPixel[1]);
		}
		ImGui::SameLine();
		if (g_inspectorComponentRange >= 0)
			ImGui::Text("range %d at (%d, %d)", g_inspectorComponentRange,
				g_inspectorComponentPixel[0], g_inspectorComponentPixel[1]);
		else
			ImGui::TextDisabled("No component is reachable in this frame.");
		if (ImGui::Button("Locate pickable pixel"))
		{
			const int locateRange = PsyX_Inspector_GetSelectionRangeIndex();
			if (locateRange >= 0)
			{
				PsyX_Inspector_FindRangePixel(locateRange);
				if (g_inspectorRangePixelValid)
					PsyX_Inspector_RequestPick(g_inspectorRangePixel[0], g_inspectorRangePixel[1]);
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Pick first reachable wheel"))
		{
			PsyX_Inspector_FindComponentPixelMatching("/component:wheel:");
			if (g_inspectorComponentRange >= 0)
				PsyX_Inspector_RequestPick(g_inspectorComponentPixel[0], g_inspectorComponentPixel[1]);
		}
		ImGui::SameLine();
		ImGui::TextDisabled("Find a pixel that resolves to the selected range and pick it.");
		ImGui::Checkbox("Skip cut-out texels when picking", (bool*)&g_inspectorCutoutPickingEnabled);
		ImGui::SameLine();
		if (ImGui::Button("Locate a cut-out texel"))
			PsyX_Inspector_FindCutoutSample();
		ImGui::SameLine();
		if (ImGui::Button("Locate a cut-out fall-through"))
			PsyX_Inspector_FindCutoutFallThrough();
		ImGui::SameLine();
		if (g_inspectorCutoutFallThroughValid)
			ImGui::Text("fall-through at (%.3f, %.3f)", g_inspectorCutoutFallThrough[0], g_inspectorCutoutFallThrough[1]);
		else
			ImGui::TextDisabled("none");
		ImGui::SameLine();
		if (g_inspectorCutoutCount > 0)
			ImGui::Text("%d cut-out point(s); first at (%.3f, %.3f) owned by range %d",
				g_inspectorCutoutCount, g_inspectorCutoutPoint[0], g_inspectorCutoutPoint[1], g_inspectorCutoutRange);
		else
			ImGui::Text("%d masked override primitive(s); no cut-out texel on screen.", g_inspectorCutoutMaskedCount);
		ImGui::TextDisabled("Picking accounts for ordering-table depth, the display/viewport area, PGXP projection and override cutout coverage; the PSX clip rect is not modelled.");
		if (ImGui::BeginChild("inspector_ranges", ImVec2(0, 150)))
		{
			for (int i = 0; i < rangeCount; ++i)
			{
				char rangeLabel[PSYX_INSPECTOR_LABEL_LENGTH] = {};
				int rangeVertices = 0;
				PsyX_Inspector_GetRangeInfo(i, rangeLabel, sizeof(rangeLabel), &rangeVertices);
				if (rangeVertices == 0)
					ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "%d: unreachable | %s", i, rangeLabel);
				else
					ImGui::Text("%d: %d verts | %s", i, rangeVertices, rangeLabel);
			}
		}
		ImGui::EndChild();
	}

	ImGui::SeparatorText("Export");
	if (ImGui::Button("Explain texture lookup") && hasSelection)
		HdTextureOverrides_DebugRegion(selection.tpage, selection.clut, selection.sourceU,
			selection.sourceV, selection.sourceWidth, selection.sourceHeight);
	ImGui::SameLine();
	ImGui::TextDisabled("page/CLUT matches: %d | containing the region: %d | best entry: %d",
		g_hdDebugPageClutMatches, g_hdDebugRegionMatches, g_hdDebugBestIndex);
	if (g_hdDebugBestIndex >= 0)
		ImGui::TextDisabled("Resolved name: %s", g_hdDebugBestName);
	else if (g_hdDebugPageMatches > 0)
		ImGui::TextDisabled("Resolved by palette fallback: %d entries on the page contain the region but with another CLUT (first %s, CLUT %d)",
			g_hdDebugPaletteFallbackMatches, g_hdDebugPageFirstName, g_hdDebugPageFirstClut);
	else if (g_hdDebugXYMatches > 0)
		ImGui::TextDisabled("Same page position, different depth: %d entries, first %s (CLUT %d)",
			g_hdDebugXYMatches, g_hdDebugXYFirstName, g_hdDebugXYFirstClut);
	else
		ImGui::TextDisabled("No named texture shares this page at all.");
	ImGui::InputText("Export mod id", g_inspectorExportModId, sizeof(g_inspectorExportModId));
	char modsDirectory[512];
	HdTextureOverrides_GetModsDirectory(modsDirectory, sizeof(modsDirectory));
	ImGui::TextWrapped("Mods root: %s", modsDirectory);
	ImGui::TextWrapped("Texture destination: %s/assets/inspector/\nModel destination: %s/assets/models/", g_inspectorExportModId, g_inspectorExportModId);

	// Export organization is a developer preference, persisted with the panel.
	bool organizedExport = HdTextureOverrides_IsOrganizedExportEnabled() != 0;
	if (ImGui::Checkbox("Organize exports by type and level", &organizedExport))
	{
		HdTextureOverrides_SetOrganizedExport(organizedExport ? 1 : 0);
		DeveloperGraphicsSettings_SaveRuntime();
	}
	ImGui::TextDisabled(organizedExport
		? "Textures go to assets/inspector/<type>/<level>/; the manifest records that path."
		: "Textures go to assets/inspector/; the manifest records type and level as metadata.");

	// Palette choice at export time. Cars and pedestrians are drawn through
	// runtime palettes, so this decides which colours the PNG carries.
	bool baseColourExport = HdTextureOverrides_IsBaseColourExportEnabled() != 0;
	if (CheckboxWithHelp("Export textures in base colours", &baseColourExport,
		"Enabled: the PNG uses the palette the level registered for the texture, the original artwork colours. This is the right choice for recolouring mods, because the game still selects a runtime palette (CLUT) for cars and pedestrians, and the modded image is re-mapped through it.\n\nDisabled: the PNG uses the palette of the clicked primitive, so a car or pedestrian is exported with the colours that instance currently shows. The same texture can then produce a different PNG per instance."))
	{
		HdTextureOverrides_SetBaseColourExport(baseColourExport ? 1 : 0);
		DeveloperGraphicsSettings_SaveRuntime();
	}
	ImGui::TextDisabled(baseColourExport
		? "Base palette: original colours, keeps CLUT recolouring working."
		: "Primitive palette: the colours this instance currently shows.");

	// Alpha convention visibility: 0000h is always a cutout, while STP=1 only
	// becomes half alpha when the draw that consumes the texture actually
	// blends. On an opaque draw PSX ignores STP, so the export stays opaque.
	if (hasTexture)
	{
		ImGui::TextDisabled(selection.semiTransparent
			? "Exported alpha: 0000h -> cutout, STP=1 -> 128 (semi-transparent draw)."
			: "Exported alpha: 0000h -> cutout, STP=1 -> 255 (opaque draw).");
	}

#ifdef _WIN32
	const bool exportSupported = true;
#else
	const bool exportSupported = false;
	ImGui::TextDisabled("PNG and OBJ export currently require Windows.");
#endif
	ImGui::BeginDisabled(!hasTexture || !exportSupported);
	if (ImGui::Button("Export original full texture (PNG)"))
	{
		HdTextureExportContext context = {};
		context.objectType = HdTextureOverrides_ObjectTypeFromKey(selection.object.key);
		context.levelName = LevelNames[GameLevel];
		context.blendModeKnown = 1;
		context.semiTransparent = selection.semiTransparent;
		HdTextureOverrides_ExportTextureWithContext(selection.tpage, selection.clut, textureInfo.u, textureInfo.v,
			textureInfo.width, textureInfo.height, textureInfo.textureName, textureInfo.texturePage,
			textureInfo.textureIndex, g_inspectorExportModId, &context, g_inspectorExportStatus, sizeof(g_inspectorExportStatus));
	}
	ImGui::EndDisabled();
	if (!hasTexture) ImGui::TextDisabled("PNG unavailable: select a primitive inside a registered texture region.");

	// Palette variants: cars and pedestrians share one (name, page, index)
	// identity across every runtime palette, so export each palette as its own
	// PNG and manifest entry. civ_clut is indexed by car palette block, texture
	// id and palette; pedestrians use block 0.
	if (hasTexture && exportSupported)
	{
		const char* variantType = HdTextureOverrides_ObjectTypeFromKey(selection.object.key);
		const bool isCar = strcmp(variantType, "cars") == 0;
		const bool isPedestrian = strcmp(variantType, "pedestrians") == 0;
		const int carPalette = isCar ? GetCarPalIndex(selection.tpage) : 0;
		if ((isCar || isPedestrian) && textureInfo.textureIndex >= 0 && textureInfo.textureIndex < 32)
		{
			int variantCluts[6];
			int variantCount = 0;
			for (int pal = 0; pal < 6; ++pal)
			{
				const int variantClut = civ_clut[carPalette][textureInfo.textureIndex][pal];
				if (variantClut == 0) continue;
				bool duplicate = false;
				for (int v = 0; v < variantCount; ++v) if (variantCluts[v] == variantClut) { duplicate = true; break; }
				if (!duplicate) variantCluts[variantCount++] = variantClut;
			}
			ImGui::BeginDisabled(variantCount == 0);
			if (ImGui::Button("Export all palette variants (PNG)"))
			{
				HdTextureExportContext variantContext = {};
				variantContext.objectType = variantType;
				variantContext.levelName = LevelNames[GameLevel];
				variantContext.blendModeKnown = 1;
				variantContext.semiTransparent = selection.semiTransparent;
				HdTextureOverrides_ExportPaletteVariants(selection.tpage, textureInfo.u, textureInfo.v,
					textureInfo.width, textureInfo.height, textureInfo.textureName, textureInfo.texturePage,
					textureInfo.textureIndex, g_inspectorExportModId, variantCluts, variantCount,
					&variantContext, g_inspectorExportStatus, sizeof(g_inspectorExportStatus));
			}
			ImGui::EndDisabled();
			ImGui::SameLine();
			ImGui::TextDisabled("Palette variants: %d (block %d)", variantCount, carPalette);
		}
	}

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
	ImGui::TextDisabled("The identified-texture batch adds a readable model suffix and a modelReferences list. These are metadata; the override key stays the (name, page, index) triple and a shared texture is exported once.");

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
		ImGui::BeginDisabled(!exportSupported || textureCount == 0 || HdTextureOverrides_BatchJobActive(&g_batchJob));
		if (ImGui::Button("Export all identified textures (batch)"))
		{
			int batchCount = 0;
			for (int i = 0; i < textureCount && batchCount < HD_TEXTURE_BATCH_MAX_ITEMS; ++i)
			{
				const ObjectTexture& item = textures[i];
				FillBatchTextureItem(&g_batchItems[batchCount], selection.object.modelIndex,
					item.info.textureName, item.info.texturePage, item.info.textureIndex,
					item.page, item.clut, item.info.u, item.info.v, item.info.width, item.info.height);
				StampBatchItemContext(g_batchItems[batchCount], selection.object.key);
				++batchCount;
			}
			StartBatchJob(g_inspectorExportModId, batchCount, "Identified visible textures (submitted geometry).");
			snprintf(g_inspectorExportStatus, sizeof(g_inspectorExportStatus), "Batch started; progress and per-resource results are below.");
		}
		ImGui::EndDisabled();
		ImGui::SameLine();
		HelpMarker("Exports every identified texture once, deduplicated by the (name, page, index) identity. Model references and the filename suffix come from the asset catalog; they are metadata only and never change the override key.");
		if (!exportSupported) ImGui::TextDisabled("Batch export currently requires Windows.");
		else if (textureCount == 0) ImGui::TextDisabled("No identified textures to export for this selection.");

		const bool sourceModelAvailable = selection.object.modelIndex >= 0 &&
			AssetCatalog_FindModel(selection.object.modelIndex) >= 0;
		ImGui::BeginDisabled(!exportSupported || !sourceModelAvailable || HdTextureOverrides_BatchJobActive(&g_batchJob));
		if (ImGui::Button("Export all source-model textures (catalog)"))
		{
			char scope[192] = {};
			const int batchCount = BuildSourceModelBatchItems(selection.object.modelIndex, scope, sizeof(scope));
			for (int i = 0; i < batchCount && i < HD_TEXTURE_BATCH_MAX_ITEMS; ++i)
				StampBatchItemContext(g_batchItems[i], selection.object.key);
			if (batchCount > 0)
			{
				StartBatchJob(g_inspectorExportModId, batchCount, scope);
				snprintf(g_inspectorExportStatus, sizeof(g_inspectorExportStatus), "Source-model batch started; progress and per-resource results are below.");
			}
			else
			{
				snprintf(g_inspectorExportStatus, sizeof(g_inspectorExportStatus), "%s", scope);
			}
		}
		ImGui::EndDisabled();
		ImGui::SameLine();
		HelpMarker("Catalog scope, not the submitted geometry: every material linked to the selected source model (including hidden faces) and its high-detail LOD sibling. Palette variants and cross-model child parts are not enumerated, and a material with no registered VRAM region is reported as a missing adapter.");
		if (!sourceModelAvailable)
			ImGui::TextDisabled("Source-model export needs a labelled building or tile in the catalog; cars are a separate pack.");
		for (int i = 0; i < textureCount; ++i)
		{
			const ObjectTexture& item = textures[i];
			ImGui::PushID(i);
			ImGui::Text("%s | page %d / index %d | %u x %u | CLUT %u", item.info.textureName, item.info.texturePage,
				item.info.textureIndex, item.info.width, item.info.height, item.clut);
			ImGui::BeginDisabled(!exportSupported);
			if (ImGui::Button("Export this texture"))
			{
				HdTextureOverrideBatchItem single = {};
				FillBatchTextureItem(&single, selection.object.modelIndex, item.info.textureName,
					item.info.texturePage, item.info.textureIndex, item.page, item.clut,
					item.info.u, item.info.v, item.info.width, item.info.height);
				HdTextureOverrideBatchResult one = {};
				HdTextureOverrides_ExportTextureBatch(g_inspectorExportModId, &single, 1, &one);
				snprintf(g_inspectorExportStatus, sizeof(g_inspectorExportStatus), "%s", one.status);
			}
			ImGui::EndDisabled();
			ImGui::PopID();
		}
	}

	DrawBatchJobUi();

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
		if (CheckboxWithHelp("Enable HD texture overrides", &enabled,
			"Takes effect on the next primitive without reloading the level. Manifest or PNG edits require a reload or a level reload."))
			HdTextureOverrides_SetEnabled(enabled);

		bool proportionalAlpha = g_cfg_overrideProportionalAlpha != 0;
		if (CheckboxWithHelp("Proportional override alpha", &proportionalAlpha,
			"Off (default): an override texel below 0.5 alpha is a hole, the behaviour existing mods were authored against.\n\nOn: on BM_AVERAGE draws the imported alpha blends proportionally, so 0/64/128/192/255 give five steps, while alpha 0 still cuts out and 128 still reproduces the original STP=1 blend. Additive, subtractive and opaque draws keep the binary cutout so rendered results and picking agree."))
		{
			g_cfg_overrideProportionalAlpha = proportionalAlpha ? 1 : 0;
			DeveloperGraphicsSettings_SaveRuntime();
		}
		ImGui::TextDisabled(proportionalAlpha
			? "Override alpha: proportional on BM_AVERAGE; 0 cuts out, 128 matches the original STP=1 blend."
			: "Override alpha: binary 0.5 cutout (compatibility default).");
	}
	else
	{
		ImGui::TextDisabled("HD texture PNG loading is not available on this platform.");
	}

	ImGui::Text("Mods: %d discovered | %d enabled | %d texture entries", diagnostics.discoveredMods,
		diagnostics.activeMods, diagnostics.manifestEntries);
	ImGui::Text("Loaded RGBA images: %d | active renderer mappings: %d", diagnostics.loadedImages,
		diagnostics.registeredOverrides);
	if (diagnostics.pageRegistrationCount > 0)
	{
		const double averageMicros = (double)diagnostics.pageRegistrationMicros / diagnostics.pageRegistrationCount;
		ImGui::Text("Page registration: %.3f ms over %d pages (%d textures, avg %.1f us/page)",
			diagnostics.pageRegistrationMicros / 1000.0, diagnostics.pageRegistrationCount,
			diagnostics.pageRegistrationTextures, averageMicros);
	}
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

// Binding editor. The game and frontend use separate tables, so the tab keeps
// them distinct and shows which one is live.
void DrawInputTab()
{
	static int contextIndex = 0;
	const DeveloperInputTable table = contextIndex == 0 ? DeveloperInputTable::Game : DeveloperInputTable::Menu;

	ImGui::TextWrapped("Bindings apply to the running game immediately and are saved to developer_input.ini as overrides: only bindings that differ from config.ini are listed, so resetting one returns it to the config.ini value.");
	HelpMarker("Each row starts with the binding on its button. Click it, then press the key or controller button to bind; Esc, a right click, or 10 seconds without input cancels. Editing never changes which table is active, so a gameplay binding cannot make the menu respond differently until the game switches context on its own.");

	if (g_bindingCapture.active)
	{
		ImGui::Separator();
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.85f, 0.3f, 1.0f));
		ImGui::TextWrapped("Listening for %s on %s (%s): %.0f s left.",
			g_bindingCapture.device == DeveloperInputDevice::Keyboard ? "a key" : "a button or stick push",
			DeveloperInputMapping_ActionName(g_bindingCapture.device, g_bindingCapture.action),
			g_bindingCapture.table == DeveloperInputTable::Game ? "game" : "menu",
			g_bindingCapture.remaining);
		ImGui::PopStyleColor();
		if (ImGui::Button("Cancel capture", ImVec2(160.0f, 0.0f)))
			CancelBindingCapture();
		ImGui::Separator();
	}

	ImGui::RadioButton("Game controls", &contextIndex, 0);
	ImGui::SameLine();
	ImGui::RadioButton("Menu controls", &contextIndex, 1);
	ImGui::SameLine();
	ImGui::TextDisabled("active: %s", DeveloperInputMapping_GetActive() == DeveloperInputTable::Game ? "game" : "menu");
	HelpMarker("config.ini [kbcontrols_game]/[controls_game] and [kbcontrols_menu]/[controls_menu] are separate tables; the game swaps them when a menu opens, so the same key may appear in both.");

	for (int deviceIndex = 0; deviceIndex < 2; deviceIndex++)
	{
		const DeveloperInputDevice device = deviceIndex == 0 ? DeveloperInputDevice::Keyboard : DeveloperInputDevice::Controller;
		const int actionCount = DeveloperInputMapping_ActionCount(device);

		ImGui::Separator();
		ImGui::TextUnformatted(deviceIndex == 0 ? "Keyboard" : "Controller");
		ImGui::SameLine();
		ImGui::PushID(deviceIndex);
		if (ImGui::Button("Reset all to defaults", ImVec2(170.0f, 0.0f)))
		{
			DeveloperInputMapping_ResetTable(table, device);
			DeveloperInputMapping_Apply(table);
			const int saved = DeveloperInputMapping_SaveDefaultFile();
			snprintf(g_bindingStatus, sizeof(g_bindingStatus), "Reset the %s %s bindings, %s",
				contextIndex == 0 ? "game" : "menu", deviceIndex == 0 ? "keyboard" : "controller",
				saved >= 0 ? "saved" : "save failed; session only");
		}
		ImGui::PopID();
		HelpMarkerAfterControl(deviceIndex == 0
			? "Defaults are the bindings config.ini provided at startup for this table."
			: "Controller defaults use the SDL game-controller names, including the four stick axes, which the keyboard has no equivalent for.",
			ImGui::GetItemRectSize().x);

		// The action column fits the longest action name; the conflict note has
		// its own stretch column, so a long note wraps inside the table instead
		// of widening the binding column until the table leaves the panel.
		float actionWidth = 0.0f;
		for (int action = 0; action < actionCount; action++)
		{
			const float width = ImGui::CalcTextSize(DeveloperInputMapping_ActionName(device, action)).x;
			if (width > actionWidth)
				actionWidth = width;
		}
		actionWidth += ImGui::GetStyle().CellPadding.x * 2.0f;

		if (ImGui::BeginTable(deviceIndex == 0 ? "keyboardBindings" : "controllerBindings", 4,
			ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings))
		{
			ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, actionWidth);
			ImGui::TableSetupColumn("Binding", ImGuiTableColumnFlags_WidthFixed, 150.0f);
			ImGui::TableSetupColumn("Also bound to", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed);
			ImGui::TableHeadersRow();

			for (int action = 0; action < actionCount; action++)
			{
				ImGui::TableNextRow();
				ImGui::PushID(action);

				ImGui::TableSetColumnIndex(0);
				ImGui::AlignTextToFramePadding();
				ImGui::TextUnformatted(DeveloperInputMapping_ActionName(device, action));

				ImGui::TableSetColumnIndex(1);
				char binding[64];
				const int value = DeveloperInputMapping_Get(table, device, action);
				DeveloperInputMapping_Format(device, value, binding, sizeof(binding));
				const int capturing = g_bindingCapture.active && g_bindingCapture.table == table &&
					g_bindingCapture.device == device && g_bindingCapture.action == action;
				if (ImGui::Button(capturing ? "press..." : binding, ImVec2(150.0f, 0.0f)))
					BeginBindingCapture(table, device, action);

				ImGui::TableSetColumnIndex(2);
				int conflicts[8];
				const int conflictCount = DeveloperInputMapping_Conflicts(table, device, value, conflicts, 8);
				if (conflictCount > 1)
				{
					char shared[128] = "";
					for (int i = 0; i < conflictCount && i < 8; i++)
					{
						if (conflicts[i] == action)
							continue;
						char part[40];
						snprintf(part, sizeof(part), "%s%s", shared[0] ? ", " : "",
							DeveloperInputMapping_ActionName(device, conflicts[i]));
						strncat(shared, part, sizeof(shared) - strlen(shared) - 1);
					}
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.65f, 0.3f, 1.0f));
					ImGui::TextWrapped("also %s", shared);
					ImGui::PopStyleColor();
				}
				else
				{
					ImGui::AlignTextToFramePadding();
					ImGui::TextDisabled("-");
				}

				ImGui::TableSetColumnIndex(3);
				if (ImGui::Button("Reset"))
				{
					DeveloperInputMapping_ResetAction(table, device, action);
					DeveloperInputMapping_Apply(table);
					const int saved = DeveloperInputMapping_SaveDefaultFile();
					snprintf(g_bindingStatus, sizeof(g_bindingStatus), "Reset %s (%s), %s",
						DeveloperInputMapping_ActionName(device, action), contextIndex == 0 ? "game" : "menu",
						saved >= 0 ? "saved" : "save failed; session only");
				}
				ImGui::PopID();
			}

			ImGui::EndTable();
		}
	}

	ImGui::Separator();
	ImGui::TextWrapped("%s", g_bindingStatus);
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
	if (!visible)
		CancelBindingCapture();

	g_visible = visible;
	UpdateOverlayInputState();
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

	// A pending capture owns the next matching event, before the game and before
	// the panel's other input paths.
	if (HandleBindingCaptureEvent(event))
		return 1;

	if (g_visible && g_inspectorPickMode && event->type == SDL_MOUSEBUTTONDOWN &&
		event->button.button == SDL_BUTTON_LEFT && !ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow))
	{
		// A single click selects the part under the cursor; a double click keeps
		// the parent instance instead, so every part of it highlights and exports
		// together. SDL reports the click count on the event itself.
		PsyX_Inspector_SetWholeObjectPick(event->button.clicks >= 2);
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

// The panel body. The display confirmation is deliberately not part of it:
// a mode change can hide or clip the panel, so that window must be drawn
// whether or not this one is, and after it so it is never covered.
void DrawDeveloperPanel()
{
	const ImVec2 displaySize = ImGui::GetIO().DisplaySize;
	ImGui::SetNextWindowSizeConstraints(ImVec2(320.0f, 240.0f), ImVec2(displaySize.x, displaySize.y * 0.95f));
	ImGui::SetNextWindowSize(ImVec2(620.0f, displaySize.y * 0.85f), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("Developer Graphics Panel", &g_visible))
	{
		ImGui::PushTextWrapPos(0.0f);
		ImGui::TextUnformatted("F11 closes this panel. Settings are live.");
		if (CheckboxWithHelp("Capture game input while panel is open", &g_captureGameInput,
			"Enabled: keyboard, controller, and mouse events are held by the panel so changing a setting cannot also control the game. Disabled: the panel remains visible while the game continues to receive its normal inputs. UI interactions may then also affect the game."))
			UpdateOverlayInputState();

		if (ImGui::BeginTabBar("DeveloperPanelTabs"))
		{
			if (ImGui::BeginTabItem("Graphics"))
			{
				bool bilinear = g_cfg_bilinearFiltering != 0;
				bool pgxpTextureMapping = g_cfg_pgxpTextureCorrection != 0;
				bool pgxpZBuffer = g_cfg_pgxpZBuffer != 0;
				bool vsync = g_cfg_swapInterval != 0;
				if (ImGui::Checkbox("Bilinear filtering", &bilinear)) g_cfg_bilinearFiltering = bilinear;
				if (CheckboxWithHelp("PGXP texture mapping", &pgxpTextureMapping,
					"Perspective-correct textures and sub-pixel vertex positions for the emulated PSX geometry. It also changes the depth the legacy renderer stores, which the modern shadow and lighting reconstruction reads."))
					g_cfg_pgxpTextureCorrection = pgxpTextureMapping;
				if (CheckboxWithHelp("PGXP Z-buffer", &pgxpZBuffer,
					"Depth-tests the emulated PSX geometry with PGXP's interpolated Z. With it off the legacy scene is drawn without depth testing and writes no depth, so the modern shadow and lighting passes have nothing to reconstruct from."))
					g_cfg_pgxpZBuffer = pgxpZBuffer;
				if (ImGui::Checkbox("VSync", &vsync)) g_cfg_swapInterval = vsync;

				ImGui::Separator();
				bool enhancedRenderer = DeveloperModernMesh_GetEnabled() != 0;
				if (CheckboxWithHelp("Enhanced renderer (modern meshes/PBR)", &enhancedRenderer,
					"Switches between the classic PSX renderer and the experimental modern path (imported glTF meshes, PBR materials, dynamic lights). F10 toggles it at any time. Requires developer_modern_mesh.ini."))
					DeveloperModernMesh_SetEnabled(enhancedRenderer);

				bool modernShadows = DeveloperModernMesh_GetShadows() != 0;
				if (CheckboxWithHelp("Modern shadows", &modernShadows,
					"Directional shadow map. Modern meshes cast; legacy geometry receives the projected shadow. Legacy geometry does not cast yet."))
					DeveloperModernMesh_SetShadows(modernShadows);

				bool modernAO = DeveloperModernMesh_GetAmbientOcclusion() != 0;
				if (CheckboxWithHelp("Modern ambient occlusion", &modernAO,
					"Normal-based ambient-occlusion approximation on the modern path; a true depth-based screen-space AO is a follow-up."))
					DeveloperModernMesh_SetAmbientOcclusion(modernAO);

				bool legacyLighting = DeveloperModernMesh_GetLegacyLighting() != 0;
				if (CheckboxWithHelp("Legacy geometry receives modern lighting", &legacyLighting,
					"Adds the modern sun to the already-rendered legacy scene (buildings, vehicles, pedestrians, trees) using a normal reconstructed from the depth buffer. The legacy shading itself is preserved; the strength slider scales the added light. F9 toggles it. Requires the enhanced renderer."))
					DeveloperModernMesh_SetLegacyLighting(legacyLighting);

				// Every developer_modern_mesh.ini light and look value in one
				// place, so the lighting can be checked at different angles
				// without editing the file or restarting. The F-keys remain as
				// a quick in-game alternative.
				if (ImGui::CollapsingHeader("Modern sun and look", ImGuiTreeNodeFlags_DefaultOpen))
				{
					float sunAzimuth = 0.0f;
					float sunElevation = 0.0f;
					DeveloperModernMesh_GetSunAngles(&sunAzimuth, &sunElevation);
					if (SliderFloatWithHelp("Sun azimuth", &sunAzimuth, 0.0f, 360.0f, "%.0f deg",
						"Compass direction the sunlight travels from. [ and ] rotate the sun in game."))
						DeveloperModernMesh_SetSunAngles(sunAzimuth, sunElevation);
					if (SliderFloatWithHelp("Sun height", &sunElevation, 3.0f, 88.0f, "%.0f deg",
						"Elevation above the horizon. Lower values lengthen the shadows; ; and ' tilt the sun in game."))
						DeveloperModernMesh_SetSunAngles(sunAzimuth, sunElevation);

					float lightIntensity = DeveloperModernMesh_GetLightIntensity();
					if (SliderFloatWithHelp("Sun intensity", &lightIntensity, 0.0f, 4.0f, "%.2f",
						"Modern sun radiance for the modern meshes and the legacy receptivity term. config.ini has no equivalent; it is stored in developer_modern_mesh.ini as lightintensity."))
						DeveloperModernMesh_SetLightIntensity(lightIntensity);

					float ambient = DeveloperModernMesh_GetAmbient();
					if (SliderFloatWithHelp("Modern ambient", &ambient, 0.0f, 1.0f, "%.2f",
						"Ambient term of the modern mesh shading. It does not change the legacy scene, which keeps its own shading."))
						DeveloperModernMesh_SetAmbient(ambient);

					float exposure = DeveloperModernMesh_GetExposure();
					if (SliderFloatWithHelp("Modern exposure", &exposure, 0.1f, 4.0f, "%.2f",
						"Output multiplier of the modern mesh shading. - and = change it in game."))
						DeveloperModernMesh_SetExposure(exposure);

					float shadowExtent = DeveloperModernMesh_GetShadowExtent();
					if (SliderFloatWithHelp("Shadow volume size", &shadowExtent, 500.0f, 8000.0f, "%.0f",
						"Half-size of the orthographic shadow volume in world units. Larger values cover more ground at a lower shadow resolution."))
						DeveloperModernMesh_SetShadowExtent(shadowExtent);

					if (legacyLighting)
					{
						float receptivity = DeveloperModernMesh_GetLegacyLightReceptivity();
						if (SliderFloatWithHelp("Legacy light strength", &receptivity, 0.0f, 1.0f, "%.2f",
							"Diffuse sun multiplier applied to legacy pixels that face the sun. 0 is the shipped look; higher values brighten sun-facing surfaces more."))
							DeveloperModernMesh_SetLegacyLightReceptivity(receptivity);
					}

					int shadowDebug = DeveloperModernMesh_GetShadowDebug();
					if (SliderIntWithHelp("Shadow debug view", &shadowDebug, 0, 10, "%d",
						"Replaces the frame with a diagnostic view: 1 scene depth, 2 shadow frustum, 3 shadow map, 4 projected vs stored depth, 5 sun term, 6 reconstructed normal, 7 light scale, 8 two-channel depth, 9 two-channel distance, 10 sun coverage. 0 is the normal frame. The 7 key cycles 0-4 in game."))
						DeveloperModernMesh_SetShadowDebugMode(shadowDebug);
				}

				ImGui::SliderInt("Draw distance", &gDrawDistance, 441, 1800);
				int fieldOfView = gCameraDefaultScrZ;
				if (ImGui::SliderInt("Field of view", &fieldOfView, 128, 384))
					gCameraDefaultScrZ = (short)fieldOfView;

				ImGui::Separator();
				// Game options that the classic renderer reads every frame or
				// every loading screen, so a change is visible without a
				// restart. They persist here instead of in config.ini, which
				// stays the shipped default.
				bool dynamicLights = gEnableDlights != 0;
				if (CheckboxWithHelp("Dynamic vehicle lights", &dynamicLights,
					"Lit vehicle polygons (headlights, brake and indicator glow). Read per car draw, so it applies immediately. Equivalent to config.ini [game] dynamicLights."))
					gEnableDlights = dynamicLights;

				bool widescreenOverlays = gWidescreenOverlayAlign != 0;
				if (CheckboxWithHelp("Widescreen overlay alignment", &widescreenOverlays,
					"Aligns the map, damage bars and stats to the screen corners on wide displays. Read every overlay draw. Equivalent to config.ini [game] widescreenOverlays."))
					gWidescreenOverlayAlign = widescreenOverlays;

				bool fastLoadingScreens = gFastLoadingScreens != 0;
				if (CheckboxWithHelp("Fast loading screens", &fastLoadingScreens,
					"Skips vsync waits and delays in the loading sequence. Applies from the next loading screen. Equivalent to config.ini [game] fastLoadingScreens."))
					gFastLoadingScreens = fastLoadingScreens;

				ImGui::Separator();
				// Display mode. A change resets the render device immediately, so
				// the viewport, aspect and picking follow the new window size;
				// it stays provisional until confirmed (see the banner above).
				SDL_Window* window = PsyX_GetSDLWindow();
				int windowWidth = 0;
				int windowHeight = 0;
				bool fullscreen = false;
				if (window)
				{
					fullscreen = (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) != 0;
					SDL_GetWindowSize(window, &windowWidth, &windowHeight);
				}

				ImGui::TextUnformatted("Display mode");
				ImGui::SameLine();
				HelpMarker("Applies immediately and resets the render device, so the viewport, aspect and primitive picking all follow the new window size. The change is provisional for 15 seconds: press Keep to save it to developer_graphics.ini, or let it revert. Fullscreen uses the desktop resolution; Alt+Enter toggles fullscreen without saving.");

				if (ImGui::Checkbox("Fullscreen (desktop)", &fullscreen))
					BeginDisplayChange(fullscreen ? 1 : 0, windowWidth, windowHeight);

				if (!fullscreen)
				{
					// The list always offers the size that is on screen now, so
					// a mode applied by Alt+Enter or the window manager can be
					// kept or returned to.
					static const int kPresets[][2] = { { 1280, 720 }, { 1600, 900 }, { 1920, 1080 } };
					char current[32];
					snprintf(current, sizeof(current), "Current (%d x %d)", windowWidth, windowHeight);
					if (ImGui::BeginCombo("Window size", current))
					{
						for (int i = 0; i < (int)(sizeof(kPresets) / sizeof(kPresets[0])); i++)
						{
							char label[32];
							snprintf(label, sizeof(label), "%d x %d", kPresets[i][0], kPresets[i][1]);
							if (ImGui::Selectable(label) &&
								(kPresets[i][0] != windowWidth || kPresets[i][1] != windowHeight))
								BeginDisplayChange(0, kPresets[i][0], kPresets[i][1]);
						}
						ImGui::EndCombo();
					}
					HelpMarker("Windowed sizes. The current size is always listed so the combo reports what is on screen after a manual resize.");
				}

				if (g_displayStatus[0] != '\0')
					ImGui::TextDisabled("%s", g_displayStatus);

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

			if (ImGui::BeginTabItem("Input"))
			{
				DrawInputTab();
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
}

// Builds the panel's ImGui windows. The Vulkan backend owns the ImGui frame
// and calls this between ImGui::NewFrame and ImGui::Render.
// Reconstructed region (lost to a bad line-range edit; see the runtime-settings
// change record). Rewrites DrawDeveloperPanel + BuildOverlayWidgets and
// RenderOverlay from the HEAD version plus the session's additions, then the
// anonymous-namespace close.
void BuildOverlayWidgets()
{
	// The display revert countdown must run whether or not the panel is open,
	// and its confirmation window must be visible even when the panel is not.
	UpdateDisplayRevert();
	UpdateBindingCapture();

	if (g_visible)
		DrawDeveloperPanel();

	// Drawn after the panel so the confirmation is never covered by it.
	DrawDisplayConfirmWindow();
}

void RenderOverlay()
{
	if (PsyX_GetRenderBackend() == PSYX_BACKEND_VULKAN)
	{
		// The Vulkan backend drives the frame and its renderer; only the
		// widgets are contributed here.
		BuildOverlayWidgets();
		return;
	}

	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplSDL2_NewFrame();
	ImGui::NewFrame();
	BuildOverlayWidgets();
	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}
}

void DeveloperGraphicsPanel_Initialise()
{
	if (g_initialised || !PsyX_GetSDLWindow())
		return;

	if (PsyX_GetRenderBackend() == PSYX_BACKEND_VULKAN)
	{
		// The Vulkan backend already created the ImGui context and its SDL2 and
		// Vulkan backends; the panel only adds its windows and input handling.
		g_initialised = true;
		DeveloperGraphicsSettings_LoadAndApply();
		PsyX_SetSDLEventHandler(HandleSDLEvent);
		PsyX_SetRenderOverlayHandler(RenderOverlay);
		return;
	}

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

	if (PsyX_GetRenderBackend() != PSYX_BACKEND_VULKAN)
	{
		// The Vulkan backend tears its ImGui backends down in PsyX_Vk_Shutdown.
		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplSDL2_Shutdown();
		ImGui::DestroyContext();
	}

	g_initialised = false;
}

#else

void DeveloperGraphicsPanel_Initialise() {}
void DeveloperGraphicsPanel_Shutdown() {}

#endif
