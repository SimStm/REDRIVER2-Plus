---
type: Rule
title: Own input mapping and SDL polling in the project, not PsyCross
description: How the project input service owns pad polling and mapping while PsyCross stays a compatibility entry point.
tags: [okf, engine, input, e01, psycross]
---

# Own input mapping and SDL polling in the project, not PsyCross

**When** touching player input, **then** keep the mapping data and the SDL2
polling in the project (`utils/InputService.*`), not in PsyCross.

- The mapping types and the four binding tables
  (`g_kbGameMappings`/`g_kbMenuMappings`/`g_gcGameMappings`/`g_gcMenuMappings`)
  are project-owned in `utils/InputService.h`; `redriver2_psxpc.cpp` parses
  `config.ini` into them and applies the active table to
  `g_inputKeyboardMapping`/`g_inputControllerMapping` via `SwitchMappings`.
  `DeveloperInputMapping` edits the tables; it must not reintroduce the former
  `PsyXKeyboardMapping`/`PsyXControllerMapping` types.
- `Game/C/pad.c` keeps the `Pads[]` and per-tick-sampling contract but calls
  `InputService_Init`/`InputService_UpdateRaw`/`InputService_GetPadState`/
  `InputService_IsAnalogCapable`/`InputService_InitActuators`/`InputService_Rumble`
  instead of the libpad equivalents. The raw buffers stay in the PSX PADRAW
  layout (`status`, `id`, `buttons[2]`, `analog[4]`).
- `InputService` must not include PsyCross headers or link PsyCross symbols; it
  is a project service. PsyX's pad backend is disabled in the project fork
  (`PsyX_main.cpp` no longer calls `PsyX_Pad_InitSystem`,
  `PsyX_Pad_InternalPadUpdates` or the controller event dispatch), so
  `InputService` is the single controller owner.
- The game still calls `PsyX_UpdateInput()` for the SDL event pump (it now polls
  events only); do not drop it without first giving the project an event-pump
  owner. `PsyX_Pad_*` remains compiled for the libpad compatibility surface but
  is not driven by the game.

See [engine-migration-boundaries.md](engine-migration-boundaries.md) for the
broader authority rules.
