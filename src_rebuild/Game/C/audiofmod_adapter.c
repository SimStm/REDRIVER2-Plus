#include "audiofmod_adapter.h"
#include "audiocontract.h"

#include <stdlib.h>

// E04 A1 SDK-free adapter. See audiofmod_adapter.h for the contract; the
// FMOD service itself is never referenced here.

#define AUDIO_FMOD_FE_SOUND_BANK	1
#define AUDIO_FMOD_PITCH_UNIT		4096.0f
#define AUDIO_FMOD_PITCH_MAX_FIXED	16383.0f

static AudioConsumerMode g_consumerMode = AUDIO_CONSUMER_SPU_ONLY;
static int g_consumerModeResolved = 0;

AudioConsumerMode AudioFmod_ParseConsumerMode(const char* value)
{
	if (value && value[0] == '1' && value[1] == '\0')
		return AUDIO_CONSUMER_FMOD_ONLY;

	return AUDIO_CONSUMER_SPU_ONLY;
}

AudioConsumerMode AudioFmod_GetConsumerMode(void)
{
	if (!g_consumerModeResolved)
	{
		g_consumerMode = AudioFmod_ParseConsumerMode(getenv(AUDIO_FMOD_MODE_ENV));
		g_consumerModeResolved = 1;
	}

	return g_consumerMode;
}

void AudioFmod_SetConsumerMode(AudioConsumerMode mode)
{
	g_consumerMode = (mode == AUDIO_CONSUMER_FMOD_ONLY) ? AUDIO_CONSUMER_FMOD_ONLY : AUDIO_CONSUMER_SPU_ONLY;
	g_consumerModeResolved = 1;
}

float AudioFmod_MasterVolume(int masterVolume)
{
	float volume = (10000.0f + (float)masterVolume) / 10000.0f;

	if (volume < 0.0f)
		volume = 0.0f;
	else if (volume > 1.0f)
		volume = 1.0f;

	return volume;
}

float AudioFmod_PitchFromGame(int gamePitch)
{
	const float maxPitch = AUDIO_FMOD_PITCH_MAX_FIXED / AUDIO_FMOD_PITCH_UNIT;
	float pitch = (float)gamePitch / AUDIO_FMOD_PITCH_UNIT;

	if (pitch > maxPitch)
		pitch = maxPitch;

	return pitch;
}

unsigned int AudioFmod_MakeFESoundEventId(int sample)
{
	return AudioContract_MakeEventId(AUDIO_KIND_SFX, AUDIO_FMOD_FE_SOUND_BANK, sample);
}
