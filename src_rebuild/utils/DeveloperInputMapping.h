#ifndef DEVELOPER_INPUT_MAPPING_H
#define DEVELOPER_INPUT_MAPPING_H

#include "PsyX/PsyX_public.h"

// The four binding tables the game applies through SwitchMappings(). They are
// defined next to the config.ini parser in redriver2_psxpc.cpp, which stays the
// single source of truth; the developer panel edits them here.
extern PsyXKeyboardMapping g_kbGameMappings;
extern PsyXKeyboardMapping g_kbMenuMappings;
extern PsyXControllerMapping g_gcGameMappings;
extern PsyXControllerMapping g_gcMenuMappings;

enum class DeveloperInputTable { Game, Menu };
enum class DeveloperInputDevice { Keyboard, Controller };

/* Bindable actions in table order. The controller device has four extra
   stick-axis actions; every other action name is shared by both devices so the
   panel and the game describe a binding the same way. */
int DeveloperInputMapping_ActionCount(DeveloperInputDevice device);
const char* DeveloperInputMapping_ActionName(DeveloperInputDevice device, int action);

int DeveloperInputMapping_Get(DeveloperInputTable table, DeveloperInputDevice device, int action);
void DeveloperInputMapping_Set(DeveloperInputTable table, DeveloperInputDevice device, int action, int value);

/* True when a binding is deliberately empty: no scancode for the keyboard, an
   invalid button/axis for the controller. */
int DeveloperInputMapping_IsUnbound(DeveloperInputDevice device, int value);

/* Records the bindings that were active when the game started (the config.ini
   values) as the defaults every reset returns to. Idempotent. */
void DeveloperInputMapping_CaptureDefaults();
int DeveloperInputMapping_GetDefault(DeveloperInputTable table, DeveloperInputDevice device, int action);
void DeveloperInputMapping_ResetAction(DeveloperInputTable table, DeveloperInputDevice device, int action);
void DeveloperInputMapping_ResetTable(DeveloperInputTable table, DeveloperInputDevice device);

/* Human-readable binding: the SDL scancode name, or the SDL controller
   button/axis name with a '-' prefix for an inverted axis. Unbound values are
   reported as "unbound". */
void DeveloperInputMapping_Format(DeveloperInputDevice device, int value, char* buffer, int capacity);

/* True when a value could have come from a binding: a scancode, or a controller
   button/axis with valid flag bits. */
int DeveloperInputMapping_IsValidValue(DeveloperInputDevice device, int value);

/* Persistence for developer_input.ini, the developer overrides that sit on top
   of the config.ini bindings. Only bindings that differ from the config.ini
   defaults are written, so the shipped defaults keep applying when the file or
   a single key is removed, and reset-to-default removes an override. Both
   return the number of bindings written/loaded, or -1 when the file could not
   be read or written. */
int DeveloperInputMapping_SaveDefaultFile(void);
int DeveloperInputMapping_LoadDefaultFile(void);

/* Other actions in the same table that already use `value`. Returns how many
   there are, writing at most `capacity` of their action indices. */
int DeveloperInputMapping_Conflicts(DeveloperInputTable table, DeveloperInputDevice device, int value, int* actions, int capacity);

/* Tells the module which table the game is currently using. The game calls
   SwitchMappings when it enters or leaves a menu, so an edit only reaches the
   live mapping when it belongs to the active table. */
void DeveloperInputMapping_NotifyActive(DeveloperInputTable table);
DeveloperInputTable DeveloperInputMapping_GetActive(void);

/* Pushes an edited table into the live mapping when that table is the active
   one. The game picks up the other table the next time it switches, so editing
   gameplay bindings never changes what the menu currently responds to. */
void DeveloperInputMapping_Apply(DeveloperInputTable table);

#endif // DEVELOPER_INPUT_MAPPING_H
