#include "InputService.h"

#include <SDL.h>

#include <string.h>
#include <stdlib.h>

/*
 * SDL2 backend for the project input service (E01). Faithful reimplementation
 * of the former PsyCross PsyX_pad.cpp polling, using the project-owned mapping
 * globals. The raw pad buffers use the PSX PADRAW layout consumed by pad.c.
 */

InputKeyboardMapping g_inputKeyboardMapping;
InputControllerMapping g_inputControllerMapping;

namespace
{
struct PADRAW
{
	unsigned char status;
	unsigned char id;
	unsigned char buttons[2];
	unsigned char analog[4];
};

struct InputController
{
	int deviceId;
	SDL_GameController* gc;
	bool switchingAnalog;
};

const int kPorts = 2;

InputController g_controllers[kPorts];
unsigned char* g_padBuffers[kPorts] = { NULL, NULL };
const unsigned char* g_keyboardState = NULL;
int g_capture = 0;
int g_initialised = 0;
int g_padCommEnable = 0;
int g_activeKeyboardControllers = 0x1;

int GetButtonState(SDL_GameController* cont, int buttonOrAxis)
{
	if (buttonOrAxis & INPUT_CONTROLLER_MAP_FLAG_AXIS)
	{
		int value = SDL_GameControllerGetAxis(cont, (SDL_GameControllerAxis)(buttonOrAxis & ~(INPUT_CONTROLLER_MAP_FLAG_AXIS | INPUT_CONTROLLER_MAP_FLAG_INVERSE)));

		if (abs(value) > 500 && (buttonOrAxis & INPUT_CONTROLLER_MAP_FLAG_INVERSE))
			value *= -1;

		return value;
	}

	return SDL_GameControllerGetButton(cont, (SDL_GameControllerButton)buttonOrAxis) * 32767;
}

void OpenController(int deviceId, int slot)
{
	InputController* controller = &g_controllers[slot];

	if (controller->gc)
		return;

	controller->gc = SDL_GameControllerOpen(deviceId);
	controller->switchingAnalog = false;

	if (controller->gc && controller->deviceId == -1)
		controller->deviceId = deviceId;
}

void CloseController(int slot)
{
	InputController* controller = &g_controllers[slot];

	if (controller->gc)
	{
		SDL_GameControllerClose(controller->gc);
		controller->gc = NULL;
	}
}

void HandleControllerAdded(int deviceId)
{
	for (int i = 0; i < kPorts; ++i)
	{
		InputController* controller = &g_controllers[i];

		if (controller->deviceId == -1 || controller->deviceId == deviceId)
		{
			OpenController(deviceId, i);
			break;
		}
	}
}

void HandleControllerRemoved(int deviceId)
{
	for (int i = 0; i < kPorts; ++i)
	{
		if (g_controllers[i].deviceId == deviceId)
			CloseController(i);
	}
}

int SDLCALL EventWatch(void* userdata, SDL_Event* event)
{
	(void)userdata;

	switch (event->type)
	{
	case SDL_CONTROLLERDEVICEADDED:
		HandleControllerAdded(event->cdevice.which);
		break;
	case SDL_CONTROLLERDEVICEREMOVED:
		HandleControllerRemoved(event->cdevice.which);
		break;
	}

	return 0;
}

unsigned short UpdateKeyboardInput(void)
{
	unsigned short ret = 0xFFFF;

	if (g_keyboardState == NULL)
		return ret;

	SDL_PumpEvents();

	const InputKeyboardMapping& mapping = g_inputKeyboardMapping;

	if (g_keyboardState[mapping.kc_square])	ret &= ~0x8000;
	if (g_keyboardState[mapping.kc_circle])	ret &= ~0x2000;
	if (g_keyboardState[mapping.kc_triangle])	ret &= ~0x1000;
	if (g_keyboardState[mapping.kc_cross])	ret &= ~0x4000;
	if (g_keyboardState[mapping.kc_l1])		ret &= ~0x400;
	if (g_keyboardState[mapping.kc_l2])		ret &= ~0x100;
	if (g_keyboardState[mapping.kc_l3])		ret &= ~0x2;
	if (g_keyboardState[mapping.kc_r1])		ret &= ~0x800;
	if (g_keyboardState[mapping.kc_r2])		ret &= ~0x200;
	if (g_keyboardState[mapping.kc_r3])		ret &= ~0x4;
	if (g_keyboardState[mapping.kc_dpad_up])	ret &= ~0x10;
	if (g_keyboardState[mapping.kc_dpad_down])	ret &= ~0x40;
	if (g_keyboardState[mapping.kc_dpad_left])	ret &= ~0x80;
	if (g_keyboardState[mapping.kc_dpad_right])	ret &= ~0x20;
	if (g_keyboardState[mapping.kc_select])	ret &= ~0x1;
	if (g_keyboardState[mapping.kc_start])	ret &= ~0x8;

	return ret;
}

void UpdateGameControllerInput(SDL_GameController* cont, PADRAW* pad)
{
	unsigned short ret = 0xFFFF;

	if (cont == NULL)
	{
		pad->analog[0] = 127;
		pad->analog[1] = 127;
		pad->analog[2] = 127;
		pad->analog[3] = 127;

		*(unsigned short*)pad->buttons = ret;
		return;
	}

	const InputControllerMapping& mapping = g_inputControllerMapping;

	if (GetButtonState(cont, mapping.gc_square) > 16384)	ret &= ~0x8000;
	if (GetButtonState(cont, mapping.gc_circle) > 16384)	ret &= ~0x2000;
	if (GetButtonState(cont, mapping.gc_triangle) > 16384)	ret &= ~0x1000;
	if (GetButtonState(cont, mapping.gc_cross) > 16384)	ret &= ~0x4000;
	if (GetButtonState(cont, mapping.gc_l1) > 16384)		ret &= ~0x400;
	if (GetButtonState(cont, mapping.gc_r1) > 16384)		ret &= ~0x800;
	if (GetButtonState(cont, mapping.gc_l2) > 16384)		ret &= ~0x100;
	if (GetButtonState(cont, mapping.gc_r2) > 16384)		ret &= ~0x200;
	if (GetButtonState(cont, mapping.gc_dpad_up) > 16384)	ret &= ~0x10;
	if (GetButtonState(cont, mapping.gc_dpad_down) > 16384)	ret &= ~0x40;
	if (GetButtonState(cont, mapping.gc_dpad_left) > 16384)	ret &= ~0x80;
	if (GetButtonState(cont, mapping.gc_dpad_right) > 16384)	ret &= ~0x20;
	if (GetButtonState(cont, mapping.gc_l3) > 16384)		ret &= ~0x2;
	if (GetButtonState(cont, mapping.gc_r3) > 16384)		ret &= ~0x4;
	if (GetButtonState(cont, mapping.gc_select) > 16384)	ret &= ~0x1;
	if (GetButtonState(cont, mapping.gc_start) > 16384)		ret &= ~0x8;

	const short leftX = (short)GetButtonState(cont, mapping.gc_axis_left_x);
	const short leftY = (short)GetButtonState(cont, mapping.gc_axis_left_y);
	const short rightX = (short)GetButtonState(cont, mapping.gc_axis_right_x);
	const short rightY = (short)GetButtonState(cont, mapping.gc_axis_right_y);

	*(unsigned short*)pad->buttons = ret;

	pad->analog[0] = (rightX / 256) + 128;
	pad->analog[1] = (rightY / 256) + 128;
	pad->analog[2] = (leftX / 256) + 128;
	pad->analog[3] = (leftY / 256) + 128;
}
}

extern "C" int InputService_Init(unsigned char* pad1, unsigned char* pad2)
{
	if (g_initialised)
		return 1;

	memset(g_controllers, 0, sizeof(g_controllers));
	g_controllers[0].deviceId = -1;
	g_controllers[1].deviceId = -1;

	g_padBuffers[0] = pad1;
	g_padBuffers[1] = pad2;

	// Mirror the former PadInitDirect/PadInitPad defaults: port 0 is the always
	// present keyboard, port 1 starts disconnected.
	for (int i = 0; i < kPorts; ++i)
	{
		PADRAW* pad = (PADRAW*)g_padBuffers[i];

		if (pad == NULL)
			continue;

		pad->id = (i == 0) ? 0x41 : 0xFF;
		pad->buttons[0] = 0xFF;
		pad->buttons[1] = 0xFF;
		pad->analog[0] = 128;
		pad->analog[1] = 128;
		pad->analog[2] = 128;
		pad->analog[3] = 128;
	}

	g_keyboardState = SDL_GetKeyboardState(NULL);

	if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER | SDL_INIT_HAPTIC) < 0)
	{
		SDL_Log("InputService: failed to initialise SDL game controller subsystem: %s", SDL_GetError());
		g_keyboardState = SDL_GetKeyboardState(NULL);
	}

	SDL_GameControllerAddMappingsFromFile("gamecontrollerdb.txt");
	SDL_AddEventWatch(EventWatch, NULL);

	for (int i = 0; i < SDL_NumJoysticks(); ++i)
	{
		if (SDL_IsGameController(i))
			HandleControllerAdded(i);
	}

	g_padCommEnable = 1;
	g_initialised = 1;
	return 1;
}

extern "C" void InputService_Shutdown(void)
{
	for (int i = 0; i < kPorts; ++i)
		CloseController(i);

	g_padBuffers[0] = NULL;
	g_padBuffers[1] = NULL;
	g_padCommEnable = 0;
	g_initialised = 0;
}

extern "C" void InputService_SetCapture(int captureFlags)
{
	g_capture = captureFlags;
}

extern "C" int InputService_GetCapture(void)
{
	return g_capture;
}

extern "C" void InputService_UpdateRaw(void)
{
	if (!g_initialised || g_padCommEnable == 0)
		return;

	const unsigned short kbInputs = (g_capture & INPUT_CAPTURE_KEYBOARD) ? 0xffff : UpdateKeyboardInput();

	for (int i = 0; i < kPorts; ++i)
	{
		PADRAW* pad = (PADRAW*)g_padBuffers[i];

		if (pad == NULL)
			continue;

		SDL_GameController* gc = (g_capture & INPUT_CAPTURE_GAMEPAD) ? NULL : g_controllers[i].gc;

		UpdateGameControllerInput(gc, pad);

		const unsigned short test = *(unsigned short*)pad->buttons;

		// Select + Start toggles analog mode (as the PsyCross backend did).
		if ((test & 0x1) == 0 && (test & 0x8) == 0)
		{
			*(unsigned short*)pad->buttons = 0xffff;

			if (!g_controllers[i].switchingAnalog)
			{
				pad->id = (pad->id == 0x41) ? 0x73 : 0x41;
			}

			g_controllers[i].switchingAnalog = true;
		}
		else
		{
			g_controllers[i].switchingAnalog = false;
		}

		// Keyboard drives the active port(s); force digital while it does.
		if ((g_activeKeyboardControllers & (1 << i)) && kbInputs != 0xffff)
		{
			pad->status = 0;

			if (pad->id != 0x41)
				pad->id = 0x41;

			*(unsigned short*)pad->buttons &= kbInputs;
		}
	}
}

extern "C" int InputService_GetPadState(int port)
{
	const int slot = (port >> 4) & 1;

	if (slot == 0)
		return 6; /* PadStateStable: keyboard is always present */

	return (g_controllers[slot].gc && SDL_GameControllerGetAttached(g_controllers[slot].gc)) ? 6 : 0;
}

extern "C" int InputService_IsAnalogCapable(int port)
{
	(void)port;
	return 1;
}

extern "C" int InputService_InitActuators(int port)
{
	(void)port;
	return 1;
}

extern "C" void InputService_Rumble(int port, const unsigned char* motors, int len)
{
	const int slot = (port >> 4) & 1;

	if (len == 0 || g_controllers[slot].gc == NULL)
		return;

	unsigned short freqHigh = motors[0] * 255;
	unsigned short freqLow = len > 1 ? motors[1] * 255 : 0;

	if (freqLow != 0 && freqLow < 4096)
		freqLow = 4096;

	if (freqHigh != 0 && freqHigh < 4096)
		freqHigh = 4096;

	SDL_GameControllerRumble(g_controllers[slot].gc, freqLow, freqHigh, 200);
}
