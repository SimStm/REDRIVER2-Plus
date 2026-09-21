#include "DeveloperInputMapping.h"
#include "DeveloperSettingsFile.h"

#include <SDL_scancode.h>
#include <SDL_gamecontroller.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

namespace
{
struct KeyboardAction
{
	const char* name;
	const char* key;
	int PsyXKeyboardMapping::*field;
};

// Order is the storage order the panel and the defaults table share. `key` is
// the stable name used by developer_input.ini and must not change.
const KeyboardAction kKeyboardActions[] =
{
	{ "Cross", "cross", &PsyXKeyboardMapping::kc_cross },
	{ "Circle", "circle", &PsyXKeyboardMapping::kc_circle },
	{ "Triangle", "triangle", &PsyXKeyboardMapping::kc_triangle },
	{ "Square", "square", &PsyXKeyboardMapping::kc_square },
	{ "L1", "l1", &PsyXKeyboardMapping::kc_l1 },
	{ "L2", "l2", &PsyXKeyboardMapping::kc_l2 },
	{ "L3", "l3", &PsyXKeyboardMapping::kc_l3 },
	{ "R1", "r1", &PsyXKeyboardMapping::kc_r1 },
	{ "R2", "r2", &PsyXKeyboardMapping::kc_r2 },
	{ "R3", "r3", &PsyXKeyboardMapping::kc_r3 },
	{ "Up", "dpadup", &PsyXKeyboardMapping::kc_dpad_up },
	{ "Down", "dpaddown", &PsyXKeyboardMapping::kc_dpad_down },
	{ "Left", "dpadleft", &PsyXKeyboardMapping::kc_dpad_left },
	{ "Right", "dpadright", &PsyXKeyboardMapping::kc_dpad_right },
	{ "Select", "select", &PsyXKeyboardMapping::kc_select },
	{ "Start", "start", &PsyXKeyboardMapping::kc_start },
};

struct ControllerAction
{
	const char* name;
	const char* key;
	int PsyXControllerMapping::*field;
};

const ControllerAction kControllerActions[] =
{
	{ "Cross", "cross", &PsyXControllerMapping::gc_cross },
	{ "Circle", "circle", &PsyXControllerMapping::gc_circle },
	{ "Triangle", "triangle", &PsyXControllerMapping::gc_triangle },
	{ "Square", "square", &PsyXControllerMapping::gc_square },
	{ "L1", "l1", &PsyXControllerMapping::gc_l1 },
	{ "L2", "l2", &PsyXControllerMapping::gc_l2 },
	{ "L3", "l3", &PsyXControllerMapping::gc_l3 },
	{ "R1", "r1", &PsyXControllerMapping::gc_r1 },
	{ "R2", "r2", &PsyXControllerMapping::gc_r2 },
	{ "R3", "r3", &PsyXControllerMapping::gc_r3 },
	{ "Up", "dpadup", &PsyXControllerMapping::gc_dpad_up },
	{ "Down", "dpaddown", &PsyXControllerMapping::gc_dpad_down },
	{ "Left", "dpadleft", &PsyXControllerMapping::gc_dpad_left },
	{ "Right", "dpadright", &PsyXControllerMapping::gc_dpad_right },
	{ "Select", "select", &PsyXControllerMapping::gc_select },
	{ "Start", "start", &PsyXControllerMapping::gc_start },
	{ "Left stick X", "axisleftx", &PsyXControllerMapping::gc_axis_left_x },
	{ "Left stick Y", "axislefty", &PsyXControllerMapping::gc_axis_left_y },
	{ "Right stick X", "axisrightx", &PsyXControllerMapping::gc_axis_right_x },
	{ "Right stick Y", "axisrighty", &PsyXControllerMapping::gc_axis_right_y },
};

const int kKeyboardActionCount = (int)(sizeof(kKeyboardActions) / sizeof(kKeyboardActions[0]));
const int kControllerActionCount = (int)(sizeof(kControllerActions) / sizeof(kControllerActions[0]));

PsyXKeyboardMapping& KeyboardTable(DeveloperInputTable table)
{
	return table == DeveloperInputTable::Game ? g_kbGameMappings : g_kbMenuMappings;
}

PsyXControllerMapping& ControllerTable(DeveloperInputTable table)
{
	return table == DeveloperInputTable::Game ? g_gcGameMappings : g_gcMenuMappings;
}

int* KeyboardField(DeveloperInputTable table, int action)
{
	if (action < 0 || action >= kKeyboardActionCount)
		return NULL;
	return &(KeyboardTable(table).*(kKeyboardActions[action].field));
}

int* ControllerField(DeveloperInputTable table, int action)
{
	if (action < 0 || action >= kControllerActionCount)
		return NULL;
	return &(ControllerTable(table).*(kControllerActions[action].field));
}

// The bindings the game started with, which reset restores.
PsyXKeyboardMapping kDefaultKbGame;
PsyXKeyboardMapping kDefaultKbMenu;
PsyXControllerMapping kDefaultGcGame;
PsyXControllerMapping kDefaultGcMenu;
int kDefaultsCaptured = 0;

DeveloperInputTable kActiveTable = DeveloperInputTable::Game;

const char* const kBindingsFilename = "developer_input.ini";
const int kMaxBindingKeys = 1 + 4 * 20;		// schemaVersion + every action of both devices

const char* ActionKey(DeveloperInputDevice device, int action)
{
	return device == DeveloperInputDevice::Keyboard ? kKeyboardActions[action].key : kControllerActions[action].key;
}

// `<device>_<context>_<action>`, e.g. `keyboard_game_cross`.
void BuildBindingName(char* buffer, int capacity, DeveloperInputTable table, DeveloperInputDevice device, int action)
{
	snprintf(buffer, capacity, "%s_%s_%s",
		device == DeveloperInputDevice::Keyboard ? "keyboard" : "controller",
		table == DeveloperInputTable::Game ? "game" : "menu",
		ActionKey(device, action));
}

// Reverses BuildBindingName. Returns 0 for anything this module does not own.
int ParseBindingName(const char* name, DeveloperInputTable* table, DeveloperInputDevice* device, int* action)
{
	const char* separator = strchr(name, '_');
	if (!separator)
		return 0;

	const size_t deviceLength = (size_t)(separator - name);
	if (deviceLength == 8 && strncmp(name, "keyboard", 8) == 0)
		*device = DeveloperInputDevice::Keyboard;
	else if (deviceLength == 10 && strncmp(name, "controller", 10) == 0)
		*device = DeveloperInputDevice::Controller;
	else
		return 0;

	const char* context = separator + 1;
	const char* actionName = strchr(context, '_');
	if (!actionName)
		return 0;
	actionName++;

	const size_t contextLength = (size_t)(actionName - context - 1);
	if (contextLength == 4 && strncmp(context, "game", 4) == 0)
		*table = DeveloperInputTable::Game;
	else if (contextLength == 4 && strncmp(context, "menu", 4) == 0)
		*table = DeveloperInputTable::Menu;
	else
		return 0;

	const int actionCount = DeveloperInputMapping_ActionCount(*device);
	for (int i = 0; i < actionCount; i++)
	{
		if (strcmp(ActionKey(*device, i), actionName) == 0)
		{
			*action = i;
			return 1;
		}
	}

	return 0;
}
}

int DeveloperInputMapping_ActionCount(DeveloperInputDevice device)
{
	return device == DeveloperInputDevice::Keyboard ? kKeyboardActionCount : kControllerActionCount;
}

const char* DeveloperInputMapping_ActionName(DeveloperInputDevice device, int action)
{
	if (action < 0 || action >= DeveloperInputMapping_ActionCount(device))
		return "";
	return device == DeveloperInputDevice::Keyboard ? kKeyboardActions[action].name : kControllerActions[action].name;
}

int DeveloperInputMapping_Get(DeveloperInputTable table, DeveloperInputDevice device, int action)
{
	const int* field = device == DeveloperInputDevice::Keyboard ? KeyboardField(table, action) : ControllerField(table, action);
	return field ? *field : 0;
}

void DeveloperInputMapping_Set(DeveloperInputTable table, DeveloperInputDevice device, int action, int value)
{
	int* field = device == DeveloperInputDevice::Keyboard ? KeyboardField(table, action) : ControllerField(table, action);
	if (field)
		*field = value;
}

int DeveloperInputMapping_IsUnbound(DeveloperInputDevice device, int value)
{
	if (device == DeveloperInputDevice::Keyboard)
		return value == SDL_SCANCODE_UNKNOWN;
	return value == SDL_CONTROLLER_BUTTON_INVALID || value == SDL_CONTROLLER_AXIS_INVALID;
}

void DeveloperInputMapping_CaptureDefaults()
{
	if (kDefaultsCaptured)
		return;

	kDefaultKbGame = g_kbGameMappings;
	kDefaultKbMenu = g_kbMenuMappings;
	kDefaultGcGame = g_gcGameMappings;
	kDefaultGcMenu = g_gcMenuMappings;
	kDefaultsCaptured = 1;
}

int DeveloperInputMapping_GetDefault(DeveloperInputTable table, DeveloperInputDevice device, int action)
{
	DeveloperInputMapping_CaptureDefaults();

	if (action < 0 || action >= DeveloperInputMapping_ActionCount(device))
		return 0;

	if (device == DeveloperInputDevice::Keyboard)
	{
		const PsyXKeyboardMapping& mapping = table == DeveloperInputTable::Game ? kDefaultKbGame : kDefaultKbMenu;
		return mapping.*(kKeyboardActions[action].field);
	}

	const PsyXControllerMapping& mapping = table == DeveloperInputTable::Game ? kDefaultGcGame : kDefaultGcMenu;
	return mapping.*(kControllerActions[action].field);
}

void DeveloperInputMapping_ResetAction(DeveloperInputTable table, DeveloperInputDevice device, int action)
{
	DeveloperInputMapping_Set(table, device, action, DeveloperInputMapping_GetDefault(table, device, action));
}

void DeveloperInputMapping_ResetTable(DeveloperInputTable table, DeveloperInputDevice device)
{
	const int count = DeveloperInputMapping_ActionCount(device);
	for (int action = 0; action < count; action++)
		DeveloperInputMapping_ResetAction(table, device, action);
}

void DeveloperInputMapping_Format(DeveloperInputDevice device, int value, char* buffer, int capacity)
{
	if (!buffer || capacity <= 0)
		return;

	buffer[0] = '\0';

	if (DeveloperInputMapping_IsUnbound(device, value))
	{
		snprintf(buffer, capacity, "unbound");
		return;
	}

	if (device == DeveloperInputDevice::Keyboard)
	{
		const char* name = SDL_GetScancodeName((SDL_Scancode)value);
		snprintf(buffer, capacity, "%s", (name && name[0]) ? name : "unbound");
		return;
	}

	const char* inverse = (value & CONTROLLER_MAP_FLAG_INVERSE) ? "-" : "";
	if (value & CONTROLLER_MAP_FLAG_AXIS)
	{
		const char* name = SDL_GameControllerGetStringForAxis((SDL_GameControllerAxis)(value & ~(CONTROLLER_MAP_FLAG_AXIS | CONTROLLER_MAP_FLAG_INVERSE)));
		snprintf(buffer, capacity, "%s%s", inverse, (name && name[0]) ? name : "unbound");
	}
	else
	{
		const char* name = SDL_GameControllerGetStringForButton((SDL_GameControllerButton)value);
		snprintf(buffer, capacity, "%s", (name && name[0]) ? name : "unbound");
	}
}

int DeveloperInputMapping_Conflicts(DeveloperInputTable table, DeveloperInputDevice device, int value, int* actions, int capacity)
{
	if (DeveloperInputMapping_IsUnbound(device, value))
		return 0;

	const int count = DeveloperInputMapping_ActionCount(device);
	int found = 0;

	for (int action = 0; action < count; action++)
	{
		if (DeveloperInputMapping_Get(table, device, action) != value)
			continue;

		if (actions && found < capacity)
			actions[found] = action;
		found++;
	}

	return found;
}

void DeveloperInputMapping_NotifyActive(DeveloperInputTable table)
{
	kActiveTable = table;
}

DeveloperInputTable DeveloperInputMapping_GetActive(void)
{
	return kActiveTable;
}

void DeveloperInputMapping_Apply(DeveloperInputTable table)
{
	DeveloperInputMapping_CaptureDefaults();

	if (table != kActiveTable)
		return;

	if (table == DeveloperInputTable::Game)
	{
		g_cfg_keyboardMapping = g_kbGameMappings;
		g_cfg_controllerMapping = g_gcGameMappings;
	}
	else
	{
		g_cfg_keyboardMapping = g_kbMenuMappings;
		g_cfg_controllerMapping = g_gcMenuMappings;
	}
}

int DeveloperInputMapping_IsValidValue(DeveloperInputDevice device, int value)
{
	if (device == DeveloperInputDevice::Keyboard)
		return value >= 0 && value < SDL_NUM_SCANCODES;

	if (value == SDL_CONTROLLER_BUTTON_INVALID || value == SDL_CONTROLLER_AXIS_INVALID)
		return 1;

	if (value & CONTROLLER_MAP_FLAG_AXIS)
	{
		const int axis = value & ~(CONTROLLER_MAP_FLAG_AXIS | CONTROLLER_MAP_FLAG_INVERSE);
		return axis >= 0 && axis < SDL_CONTROLLER_AXIS_MAX;
	}

	return value >= 0 && value < SDL_CONTROLLER_BUTTON_MAX;
}

int DeveloperInputMapping_SaveDefaultFile(void)
{
	DeveloperInputMapping_CaptureDefaults();

	static const char* const comments[] =
	{
		"# REDRIVER2 developer input bindings",
		"# Only the bindings that differ from config.ini are listed; deleting a",
		"# line returns that binding to the config.ini value.",
	};

	// Every key this module owns, and the subset that should exist in the file.
	// The key strings must outlive the write, and the vectors must not reallocate
	// while the key arrays point into them, hence the reserve before the first
	// push.
	std::vector<std::string> ownedNames;
	std::vector<std::string> overrideNames;
	ownedNames.reserve(kMaxBindingKeys);
	overrideNames.reserve(kMaxBindingKeys);

	const char* knownKeys[kMaxBindingKeys];
	const char* keys[kMaxBindingKeys];
	int values[kMaxBindingKeys];

	knownKeys[0] = "schemaVersion";
	keys[0] = "schemaVersion";
	values[0] = 1;
	int knownCount = 1;
	int count = 1;

	const DeveloperInputTable tables[] = { DeveloperInputTable::Game, DeveloperInputTable::Menu };
	const DeveloperInputDevice devices[] = { DeveloperInputDevice::Keyboard, DeveloperInputDevice::Controller };

	for (int t = 0; t < 2; t++)
	{
		for (int d = 0; d < 2; d++)
		{
			const int actionCount = DeveloperInputMapping_ActionCount(devices[d]);
			for (int action = 0; action < actionCount; action++)
			{
				char name[64];
				BuildBindingName(name, sizeof(name), tables[t], devices[d], action);
				ownedNames.push_back(std::string(name));
				knownKeys[knownCount++] = ownedNames.back().c_str();

				const int value = DeveloperInputMapping_Get(tables[t], devices[d], action);
				if (value == DeveloperInputMapping_GetDefault(tables[t], devices[d], action))
					continue;

				overrideNames.push_back(std::string(name));
				keys[count] = overrideNames.back().c_str();
				values[count] = value;
				count++;
			}
		}
	}

	if (!DeveloperSettingsFile_WriteKeys(kBindingsFilename, comments, 3,
		knownKeys, knownCount, keys, values, count))
		return -1;

	return count - 1;
}

int DeveloperInputMapping_LoadDefaultFile(void)
{
	DeveloperInputMapping_CaptureDefaults();

	FILE* file = fopen(kBindingsFilename, "rb");
	if (!file)
		return 0;

	int loaded = 0;
	char line[512];

	while (fgets(line, sizeof(line), file))
	{
		size_t length = strlen(line);
		while (length > 0 && (line[length - 1] == '\n' || line[length - 1] == '\r'))
			line[--length] = '\0';

		if (line[0] == '#' || line[0] == '\0')
			continue;

		char* separator = strchr(line, '=');
		if (!separator)
			continue;
		*separator = '\0';

		const char* key = line;
		const char* valueText = separator + 1;
		if (*valueText == '\0')
			continue;

		const int value = atoi(valueText);

		if (strcmp(key, "schemaVersion") == 0)
			continue;

		DeveloperInputTable table = DeveloperInputTable::Game;
		DeveloperInputDevice device = DeveloperInputDevice::Keyboard;
		int action = -1;
		if (!ParseBindingName(key, &table, &device, &action))
			continue;

		if (!DeveloperInputMapping_IsValidValue(device, value))
			continue;

		DeveloperInputMapping_Set(table, device, action, value);
		loaded++;
	}

	fclose(file);
	return loaded;
}
