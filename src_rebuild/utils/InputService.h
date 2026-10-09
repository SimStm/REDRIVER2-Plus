#ifndef INPUT_SERVICE_H
#define INPUT_SERVICE_H

/*
 * Project-owned input service (E01).
 *
 * Owns the SDL2 keyboard/game-controller polling and the active input mapping,
 * and produces the raw PSX pad bytes that Game/C/pad.c maps into Pads[]. This
 * moves input ownership out of PsyCross (PsyX_pad.cpp); PsyCross's own pad
 * system is kept only as a compatibility entry point while callers migrate.
 *
 * The mapping types are project-owned copies of the former PsyX_public.h
 * structs so this service has no PsyCross include/link dependency.
 */

#define INPUT_CONTROLLER_MAP_FLAG_AXIS		0x4000
#define INPUT_CONTROLLER_MAP_FLAG_INVERSE	0x8000

#define INPUT_CAPTURE_KEYBOARD	0x1
#define INPUT_CAPTURE_GAMEPAD	0x2

typedef struct
{
	int id;

	int kc_square, kc_circle, kc_triangle, kc_cross;

	int kc_l1, kc_l2, kc_l3;
	int kc_r1, kc_r2, kc_r3;

	int kc_start, kc_select;

	int kc_dpad_left, kc_dpad_right, kc_dpad_up, kc_dpad_down;
} InputKeyboardMapping;

typedef struct
{
	int id;

	// axis bindings carry INPUT_CONTROLLER_MAP_FLAG_AXIS
	int gc_square, gc_circle, gc_triangle, gc_cross;

	int gc_l1, gc_l2, gc_l3;
	int gc_r1, gc_r2, gc_r3;

	int gc_start, gc_select;

	int gc_dpad_left, gc_dpad_right, gc_dpad_up, gc_dpad_down;

	int gc_axis_left_x, gc_axis_left_y;
	int gc_axis_right_x, gc_axis_right_y;
} InputControllerMapping;

#ifdef __cplusplus
extern "C" {
#endif

/* The mapping the game is currently using (game table or menu table). */
extern InputKeyboardMapping g_inputKeyboardMapping;
extern InputControllerMapping g_inputControllerMapping;

/* Initialises SDL game controllers and binds the two raw pad buffers
   (equivalent to the former PadInitDirect). Returns 1 on success. */
int InputService_Init(unsigned char* pad1, unsigned char* pad2);

/* Closes controllers and releases the pad buffers. */
void InputService_Shutdown(void);

/* Blocks game input while a developer UI has focus (keyboard/gamepad flags). */
void InputService_SetCapture(int captureFlags);
int InputService_GetCapture(void);

/* Polls SDL and fills both raw pad buffers in the PSX PADRAW layout. */
void InputService_UpdateRaw(void);

/* DualShock contract used by pad.c's state machine, per port (0/1). */
int InputService_GetPadState(int port);
int InputService_IsAnalogCapable(int port);
int InputService_InitActuators(int port);
void InputService_Rumble(int port, const unsigned char* motors, int len);

#ifdef __cplusplus
}
#endif

#endif
