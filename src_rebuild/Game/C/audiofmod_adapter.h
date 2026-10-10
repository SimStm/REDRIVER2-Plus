#ifndef AUDIO_FMOD_ADAPTER_H
#define AUDIO_FMOD_ADAPTER_H

// E04 A1 SDK-free routing helpers for the FESound -> FMOD migration.
//
// This module is deliberately free of FMOD, PsyCross and game-state includes so
// a translation unit can compile and test it without the SDK or an audio
// device. sound.c and the startup seam use these helpers to decide whether the
// frontend one-shots route to the project-owned audio service and to build the
// request fields from the game's fixed-point values. Nothing here touches the
// SPU path, the voice table, the channel array or the service's lifetime.

#ifdef __cplusplus
extern "C" {
#endif

// Runtime consumer mode. The compile-time RED2_AUDIO_FMOD still decides whether
// the FMOD service exists at all; this selects between the existing SPU path
// (default) and the FMOD route for the migrated FESound slice.
typedef enum
{
	AUDIO_CONSUMER_SPU_ONLY = 0,
	AUDIO_CONSUMER_FMOD_ONLY = 1,
} AudioConsumerMode;

// Environment variable that selects the consumer mode ("1" => FMOD_ONLY).
#define AUDIO_FMOD_MODE_ENV "RED2_FMOD_AUDIO"

// Pure parse of a mode string: FMOD_ONLY only for the exact value "1"; every
// other value (NULL, empty, "0", anything else) is SPU_ONLY.
AudioConsumerMode AudioFmod_ParseConsumerMode(const char* value);

// Reads AUDIO_FMOD_MODE_ENV once and caches the result. Later calls return the
// cached mode without reading the environment again.
AudioConsumerMode AudioFmod_GetConsumerMode(void);

// Overrides the cached mode. Startup uses it to stay on SPU when the FMOD
// service fails to initialize (whole-mode fallback, never a per-call fallback).
void AudioFmod_SetConsumerMode(AudioConsumerMode mode);

// Linear master gain from the game's signed master volume:
// clamp((10000 + masterVolume) / 10000, 0, 1). Endpoints: -10000 => 0,
// 0 => 1; monotone in between.
float AudioFmod_MasterVolume(int masterVolume);

// Playback pitch multiplier from the game's fixed-point pitch:
// gamePitch / 4096, clamped above to 16383 / 4096 (mirrors the SPU path's
// MIN(..., 16383)). FESound's fixed pitch of 2048 maps to 0.5.
// Precondition: gamePitch > 0 - the contract rejects non-positive pitch and
// this helper does not clamp below. FESound always passes 2048; any future
// non-FESound caller must validate or clamp before calling.
float AudioFmod_PitchFromGame(int gamePitch);

// Canonical FESound event id: SFX(bank 1, sample) through the audio contract's
// packing. FESound always addresses SOUND_BANK_SFX (bank 1), so primary is 1.
unsigned int AudioFmod_MakeFESoundEventId(int sample);

#ifdef __cplusplus
}
#endif

#endif
