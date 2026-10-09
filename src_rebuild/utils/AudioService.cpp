#ifdef _WIN32

#include "AudioService.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "fmod.hpp"
#include "fmod_errors.h"

/*
 * E04 FMOD Core audio service. See AudioService.h for the boundaries.
 *
 * FMOD types stay in this translation unit. No callbacks are registered, so
 * the service is confined to the calling (game) thread; the only asynchronous
 * actor is FMOD's own mixer inside the DLL. System::update() is driven by
 * AudioService_Update, once per rendered frame, and it is also where voices
 * that finished naturally are reclaimed, so a one-shot never leaks its slot.
 */

namespace
{
	struct SoundEntry
	{
		unsigned int eventId;
		int inUse;
		int streaming;	// registered from a stream (diagnostics; memory ownership differs)
		int hasLoop;	// loop metadata applied, AUDIO_PLAY_LOOP is legal
		FMOD::Sound* sound;
		void* streamMemory;
	};

	FMOD::System* g_system = nullptr;
	FMOD::ChannelGroup* g_masterGroup = nullptr;
	int g_active = 0;

	AudioVoiceTable g_voices;
	FMOD::Channel* g_channels[AUDIO_VOICE_MAX];
	unsigned int g_voiceEvent[AUDIO_VOICE_MAX];
	AudioListener g_listener;						// last accepted listener pose

	SoundEntry g_sounds[AUDIO_SERVICE_SOUND_MAX];

	void SetError(char* error, int errorCapacity, const char* format, ...)
	{
		if (!error || errorCapacity <= 0)
			return;
		va_list args;
		va_start(args, format);
		vsnprintf(error, (size_t)errorCapacity, format, args);
		va_end(args);
	}

	void SetFmodError(char* error, int errorCapacity, const char* context, FMOD_RESULT result)
	{
		SetError(error, errorCapacity, "%s: %s (FMOD %d)", context, FMOD_ErrorString(result), (int)result);
	}

	int CheckFmod(FMOD_RESULT result, const char* context)
	{
		if (result == FMOD_OK) return 1;
		fprintf(stderr, "AudioService %s: %s (FMOD %d)\n", context, FMOD_ErrorString(result), (int)result);
		return 0;
	}

	void StopChannel(FMOD::Channel* channel)
	{
		const FMOD_RESULT result = channel->stop();
		if (result != FMOD_ERR_INVALID_HANDLE && result != FMOD_ERR_CHANNEL_STOLEN)
			CheckFmod(result, "stop channel");
	}

	// Output-device failures deserve a distinct message: the E04 migration must
	// be able to tell "no device" from "the SDK rejected us".
	int IsOutputDeviceError(FMOD_RESULT result)
	{
		return result == FMOD_ERR_OUTPUT_INIT ||
			result == FMOD_ERR_OUTPUT_NODRIVERS ||
			result == FMOD_ERR_OUTPUT_CREATEBUFFER ||
			result == FMOD_ERR_OUTPUT_DRIVERCALL ||
			result == FMOD_ERR_OUTPUT_FORMAT;
	}

	int IsValidEventId(unsigned int eventId)
	{
		return eventId != 0 && AudioContract_EventKind(eventId) != AUDIO_KIND_INVALID;
	}

	int FindSoundSlot(unsigned int eventId)
	{
		for (int i = 0; i < AUDIO_SERVICE_SOUND_MAX; ++i)
		{
			if (g_sounds[i].inUse && g_sounds[i].eventId == eventId)
				return i;
		}
		return -1;
	}

	int FindFreeSoundSlot()
	{
		for (int i = 0; i < AUDIO_SERVICE_SOUND_MAX; ++i)
		{
			if (!g_sounds[i].inUse)
				return i;
		}
		return -1;
	}

	// A re-registration always fits; a new id needs a free entry. Checked
	// before loading so an overflow does not waste an FMOD load.
	int CheckRegistryRoom(unsigned int eventId, char* error, int errorCapacity)
	{
		if (FindSoundSlot(eventId) >= 0 || FindFreeSoundSlot() >= 0)
			return 1;
		SetError(error, errorCapacity,
			"the sound registry is full (%d entries); unregister sounds before adding more",
			AUDIO_SERVICE_SOUND_MAX);
		return 0;
	}

	// Releases a voice slot that the table still owns, so the handle generation
	// advances and every held copy of the handle goes stale.
	int ReleaseVoiceSlot(int index)
	{
		AudioVoiceHandle handle;
		handle.index = (unsigned short)index;
		handle.generation = g_voices.generation[index];
		g_channels[index] = nullptr;
		g_voiceEvent[index] = 0;
		return AudioVoiceTable_Release(&g_voices, handle);
	}

	void StopHandle(AudioVoiceHandle handle)
	{
		if (!AudioVoiceTable_IsValid(&g_voices, handle))
			return;
		const int index = handle.index;
		if (g_channels[index])
			StopChannel(g_channels[index]);
		ReleaseVoiceSlot(index);
	}

	void StopVoicesForEvent(unsigned int eventId)
	{
		for (int i = 0; i < AUDIO_VOICE_MAX; ++i)
		{
			if (!g_voices.inUse[i] || g_voiceEvent[i] != eventId)
				continue;
			if (g_channels[i])
				StopChannel(g_channels[i]);
			ReleaseVoiceSlot(i);
		}
	}

	// First live voice for the event; writes its handle and returns the slot.
	int FindLiveVoiceForEvent(unsigned int eventId, AudioVoiceHandle* out)
	{
		for (int i = 0; i < AUDIO_VOICE_MAX; ++i)
		{
			if (!g_voices.inUse[i] || !g_channels[i] || g_voiceEvent[i] != eventId)
				continue;
			out->index = (unsigned short)i;
			out->generation = g_voices.generation[i];
			return i;
		}
		return -1;
	}

	// Reclaims slots whose channels stopped (finished, stopped or stolen).
	// FMOD only guarantees that isPlaying fails or reports false for those.
	void ReapFinishedVoices()
	{
		for (int i = 0; i < AUDIO_VOICE_MAX; ++i)
		{
			if (!g_voices.inUse[i])
				continue;
			bool playing = false;
			const FMOD_RESULT result = g_channels[i] ? g_channels[i]->isPlaying(&playing) : FMOD_OK;
			if (result != FMOD_OK && result != FMOD_ERR_INVALID_HANDLE && result != FMOD_ERR_CHANNEL_STOLEN)
			{
				fprintf(stderr, "AudioService voice query: %s\n", FMOD_ErrorString(result));
				continue; // unexpected errors are not evidence of completion
			}
			if (result != FMOD_OK || !playing)
				ReleaseVoiceSlot(i);
		}
	}

	FMOD_RESULT ApplyListener(const AudioListener& listener)
	{
		const FMOD_VECTOR position = { listener.position[0], listener.position[1], listener.position[2] };
		const FMOD_VECTOR velocity = { listener.velocity[0], listener.velocity[1], listener.velocity[2] };
		const FMOD_VECTOR forward = { listener.forward[0], listener.forward[1], listener.forward[2] };
		const FMOD_VECTOR up = { listener.up[0], listener.up[1], listener.up[2] };
		return g_system->set3DListenerAttributes(0, &position, &velocity, &forward, &up);
	}

	int RegisterSoundEntry(unsigned int eventId, FMOD::Sound* sound, int streaming, int hasLoop,
		char* error, int errorCapacity)
	{
		if (!CheckRegistryRoom(eventId, error, errorCapacity))
			return 0;

		const int existing = FindSoundSlot(eventId);
		const int slot = existing >= 0 ? existing : FindFreeSoundSlot();
		if (existing >= 0)
		{
			// Re-registration replaces the sound: stop its live voices first so
			// none is left pointing at a released Sound.
			StopVoicesForEvent(eventId);
			if (g_sounds[slot].sound)
				CheckFmod(g_sounds[slot].sound->release(), "release replaced sound");
			free(g_sounds[slot].streamMemory);
		}

		g_sounds[slot].eventId = eventId;
		g_sounds[slot].inUse = 1;
		g_sounds[slot].streaming = streaming;
		g_sounds[slot].hasLoop = hasLoop;
		g_sounds[slot].sound = sound;
		g_sounds[slot].streamMemory = nullptr;
		return 1;
	}

	// Validates converted loop metadata for an in-memory sample and returns the
	// inclusive FMOD end through `outInclusiveEnd` when it can be applied.
	int ValidateMemoryLoop(int frameCount, int loopStart, int loopEnd, int* outInclusiveEnd,
		char* error, int errorCapacity)
	{
		if (loopStart == -1 && loopEnd == -1)
			return 0;	// no loop metadata, a valid registration
		if (loopStart < 0 || loopEnd < 0)
		{
			SetError(error, errorCapacity,
				"the loop metadata must provide both loopStart and loopEnd, or -1 for both");
			return -1;
		}

		char loopError[AUDIO_CONTRACT_ERROR_CAPACITY];
		if (AudioContract_ValidateLoop(frameCount, loopStart, loopEnd, loopError, sizeof(loopError)) != AUDIO_CONTRACT_OK)
		{
			SetError(error, errorCapacity, "the loop metadata is invalid: %s", loopError);
			return -1;
		}

		if (loopStart == loopEnd)
		{
			SetError(error, errorCapacity,
				"FMOD does not support equal loop endpoints; one-frame loops are rejected, not lengthened");
			return -1;
		}

		*outInclusiveEnd = loopEnd;
		return 1;
	}
}

extern "C" {

int AudioService_Init(char* error, int errorCapacity)
{
	if (error && errorCapacity > 0)
		error[0] = '\0';

	if (g_active)
	{
		SetError(error, errorCapacity, "the audio service is already active");
		return 0;
	}

	FMOD::System* system = nullptr;
	FMOD_RESULT result = FMOD::System_Create(&system);
	if (result != FMOD_OK || !system)
	{
		SetFmodError(error, errorCapacity, "could not create the FMOD system", result);
		return 0;
	}

	int drivers = 0;
	result = system->getNumDrivers(&drivers);
	if (result != FMOD_OK || drivers == 0)
	{
		SetFmodError(error, errorCapacity, "no audio output device is available", result == FMOD_OK ? FMOD_ERR_OUTPUT_NODRIVERS : result);
		system->release();
		return 0;
	}
	result = system->init(AUDIO_VOICE_MAX, FMOD_INIT_NORMAL, nullptr);
	if (result != FMOD_OK)
	{
		if (IsOutputDeviceError(result))
		{
			SetError(error, errorCapacity,
				"no audio output device is available, so FMOD could not initialize (FMOD: %s)",
				FMOD_ErrorString(result));
		}
		else
		{
			SetFmodError(error, errorCapacity, "could not initialize the FMOD system", result);
		}
		system->release();
		return 0;
	}

	FMOD::ChannelGroup* masterGroup = nullptr;
	result = system->getMasterChannelGroup(&masterGroup);
	if (result != FMOD_OK || !masterGroup)
	{
		SetFmodError(error, errorCapacity, "could not get the FMOD master channel group", result);
		system->close();
		system->release();
		return 0;
	}

	memset(g_channels, 0, sizeof(g_channels));
	memset(g_voiceEvent, 0, sizeof(g_voiceEvent));
	memset(g_sounds, 0, sizeof(g_sounds));
	memset(&g_listener, 0, sizeof(g_listener));
	// Static table is zero-initialized once, never reset across service eras.
	result = system->set3DSettings(0.0f, 1.0f, 1.0f);
	if (result != FMOD_OK)
	{
		SetFmodError(error, errorCapacity, "could not disable Doppler (raw-unit/metre mapping deferred)", result);
		system->close(); system->release(); return 0;
	}
	fprintf(stderr, "AudioService: Doppler disabled; raw-unit/metre mapping is deferred.\n");

	g_system = system;
	g_masterGroup = masterGroup;
	g_active = 1;
	return 1;
}

int AudioService_IsActive(void)
{
	return g_active;
}

void AudioService_Update(void)
{
	if (!g_active)
		return;

	const FMOD_RESULT result = g_system->update();
	if (result != FMOD_OK)
	{
		// The API is void: there is no caller diagnostic to carry this, but a
		// failed update is loud rather than silently ignored.
		fprintf(stderr, "AudioService_Update: FMOD System::update failed: %s (FMOD %d)\n",
			FMOD_ErrorString(result), (int)result);
	}
	ReapFinishedVoices();
}

void AudioService_Shutdown(void)
{
	if (!g_active)
		return;

	for (int i = 0; i < AUDIO_VOICE_MAX; ++i)
	{
		if (!g_voices.inUse[i])
			continue;
		if (g_channels[i])
			StopChannel(g_channels[i]);
		ReleaseVoiceSlot(i);
	}

	for (int i = 0; i < AUDIO_SERVICE_SOUND_MAX; ++i)
	{
		if (g_sounds[i].sound)
			CheckFmod(g_sounds[i].sound->release(), "release sound on shutdown");
		free(g_sounds[i].streamMemory);
		memset(&g_sounds[i], 0, sizeof(g_sounds[i]));
	}

	CheckFmod(g_system->close(), "close system");
	CheckFmod(g_system->release(), "release system");
	g_system = nullptr;
	g_masterGroup = nullptr;
	g_active = 0;
}

int AudioService_SetListener(const AudioListener* listener, char* error, int cap)
{
	if (!g_active)
	{
		SetError(error, cap, "the audio service is not active"); return 0;
	}

	if (AudioContract_ValidateListener(listener, error, cap) != AUDIO_CONTRACT_OK)
		return 0;

	AudioListener accepted = *listener;
	double forward[3], up[3], forwardLength = 0, upLength = 0;
	for (int i = 0; i < 3; ++i)
	{
		forward[i] = listener->forward[i]; up[i] = listener->up[i];
		forwardLength += forward[i] * forward[i]; upLength += up[i] * up[i];
	}
	double dot = 0;
	for (int i = 0; i < 3; ++i)
	{
		forward[i] /= sqrt(forwardLength); up[i] /= sqrt(upLength);
		dot += forward[i] * up[i];
	}
	upLength = 0;
	for (int i = 0; i < 3; ++i)
	{
		up[i] -= dot * forward[i]; upLength += up[i] * up[i];
	}
	for (int i = 0; i < 3; ++i)
	{
		accepted.forward[i] = (float)forward[i]; accepted.up[i] = (float)(up[i] / sqrt(upLength));
	}
	const FMOD_RESULT result = ApplyListener(accepted);
	if (result != FMOD_OK)
	{
		SetFmodError(error, cap, "could not set listener", result); return 0;
	}
	g_listener = accepted;
	return 1;
}

int AudioService_RegisterSound(unsigned int eventId, const char* wavPath, int streaming,
	char* error, int errorCapacity)
{
	if (!g_active)
	{
		SetError(error, errorCapacity, "the audio service is not active");
		return 0;
	}
	if (!IsValidEventId(eventId))
	{
		SetError(error, errorCapacity, "the event id is invalid (zero or an unknown kind)");
		return 0;
	}
	if (!wavPath || wavPath[0] == '\0')
	{
		SetError(error, errorCapacity, "the sound path is empty");
		return 0;
	}
	if (!CheckRegistryRoom(eventId, error, errorCapacity))
		return 0;

	// Loop metadata is never inferred from the container: a file sound plays
	// once, and loops are registered explicitly through the memory entry point.
	const FMOD_MODE mode = FMOD_3D | FMOD_3D_LINEARROLLOFF | FMOD_LOOP_OFF |
		(streaming ? FMOD_CREATESTREAM : 0);

	FMOD::Sound* sound = nullptr;
	const FMOD_RESULT result = g_system->createSound(wavPath, mode, nullptr, &sound);
	if (result != FMOD_OK || !sound)
	{
		SetFmodError(error, errorCapacity, "could not load the sound file", result);
		return 0;
	}

	if (!RegisterSoundEntry(eventId, sound, streaming != 0, 0, error, errorCapacity))
	{
		sound->release();
		return 0;
	}
	return 1;
}

int AudioService_RegisterSoundMemory(unsigned int eventId, const void* pcm16, int bytes, int channels,
	int sampleRate, int frameCount, int loopStart, int loopEnd, int streaming,
	char* error, int errorCapacity)
{
	if (!g_active)
	{
		SetError(error, errorCapacity, "the audio service is not active");
		return 0;
	}
	if (!IsValidEventId(eventId))
	{
		SetError(error, errorCapacity, "the event id is invalid (zero or an unknown kind)");
		return 0;
	}
	if (!pcm16 || bytes <= 0)
	{
		SetError(error, errorCapacity, "the PCM16 buffer is empty");
		return 0;
	}
	if (channels != 1 && channels != 2)
	{
		SetError(error, errorCapacity, "the channel count must be 1 (mono) or 2 (stereo), got %d", channels);
		return 0;
	}
	if (sampleRate <= 0)
	{
		SetError(error, errorCapacity, "the sample rate must be positive, got %d", sampleRate);
		return 0;
	}
	if (frameCount <= 0)
	{
		SetError(error, errorCapacity, "the frame count must be positive, got %d", frameCount);
		return 0;
	}

	const long long expectedBytes = (long long)frameCount * channels * 2;
	if (expectedBytes != bytes)
	{
		SetError(error, errorCapacity,
			"the PCM16 buffer is %d bytes but %d frames x %d channel(s) x 2 bytes is %lld",
			bytes, frameCount, channels, expectedBytes);
		return 0;
	}

	int inclusiveLoopEnd = 0;
	const int loopState = ValidateMemoryLoop(frameCount, loopStart, loopEnd, &inclusiveLoopEnd, error, errorCapacity);
	if (loopState < 0)
		return 0;
	const int hasLoop = loopState > 0;
	if (streaming && hasLoop)
	{
		SetError(error, errorCapacity, "stream loops are not supported until prebuffer timing is verified"); return 0;
	}

	if (!CheckRegistryRoom(eventId, error, errorCapacity))
		return 0;

	FMOD_CREATESOUNDEXINFO exinfo;
	memset(&exinfo, 0, sizeof(exinfo));
	exinfo.cbsize = sizeof(exinfo);
	exinfo.length = (unsigned int)bytes;
	exinfo.numchannels = channels;
	exinfo.defaultfrequency = sampleRate;
	exinfo.format = FMOD_SOUND_FORMAT_PCM16;

	// Sound defaults are overridden per channel while playback is paused.
	const FMOD_MODE mode = FMOD_3D | FMOD_3D_LINEARROLLOFF |
		(hasLoop ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF) |
		FMOD_OPENMEMORY | FMOD_OPENRAW |
		(streaming ? FMOD_CREATESTREAM : 0);

	FMOD::Sound* sound = nullptr;
	void* streamMemory = nullptr;
	if (streaming)
	{
		streamMemory = malloc((size_t)bytes);
		if (!streamMemory) { SetError(error, errorCapacity, "could not allocate stream PCM storage"); return 0; }
		memcpy(streamMemory, pcm16, (size_t)bytes);
	}
	FMOD_RESULT result = g_system->createSound((const char*)(streaming ? streamMemory : pcm16), mode, &exinfo, &sound);
	if (result != FMOD_OK || !sound)
	{
		SetFmodError(error, errorCapacity, "could not create the PCM16 sound", result);
		free(streamMemory);
		return 0;
	}

	// Pins the PCM-vs-frame semantics: converted loop points and frame counts
	// are meaningless if FMOD's PCM length disagrees with the record.
	unsigned int pcmFrames = 0;
	result = sound->getLength(&pcmFrames, FMOD_TIMEUNIT_PCM);
	if (result != FMOD_OK)
	{
		SetFmodError(error, errorCapacity, "could not read the sound's PCM length", result);
		sound->release();
		free(streamMemory);
		return 0;
	}
	if (pcmFrames != (unsigned int)frameCount)
	{
		SetError(error, errorCapacity,
			"FMOD reports %u PCM frames but the record declares %d", pcmFrames, frameCount);
		sound->release();
		free(streamMemory);
		return 0;
	}

	if (hasLoop)
	{
		result = sound->setLoopPoints((unsigned int)loopStart, FMOD_TIMEUNIT_PCM,
			(unsigned int)inclusiveLoopEnd, FMOD_TIMEUNIT_PCM);
		if (result != FMOD_OK)
		{
			SetFmodError(error, errorCapacity, "could not apply the loop points", result);
			sound->release();
			free(streamMemory);
			return 0;
		}
	}

	if (!RegisterSoundEntry(eventId, sound, streaming != 0, hasLoop, error, errorCapacity))
	{
		sound->release();
		free(streamMemory);
		return 0;
	}
	g_sounds[FindSoundSlot(eventId)].streamMemory = streamMemory;
	return 1;
}

int AudioService_UnregisterSound(unsigned int eventId)
{
	if (!g_active)
		return 0;

	const int slot = FindSoundSlot(eventId);
	if (slot < 0)
		return 0;

	StopVoicesForEvent(eventId);
	if (g_sounds[slot].sound)
		CheckFmod(g_sounds[slot].sound->release(), "unregister sound");
	free(g_sounds[slot].streamMemory);
	memset(&g_sounds[slot], 0, sizeof(g_sounds[slot]));
	return 1;
}

int AudioService_Play(const AudioPlayRequest* request, AudioVoiceHandle* out,
	char* error, int errorCapacity)
{
	if (out)
	{
		out->index = 0;
		out->generation = 0;
	}

	if (!g_active)
	{
		SetError(error, errorCapacity, "the audio service is not active");
		return 0;
	}

	// The contract is the single validator; its diagnostic is the caller's.
	if (AudioContract_ValidateRequest(request, error, errorCapacity) != AUDIO_CONTRACT_OK)
		return 0;

	// Reap before allocating so a finished channel cannot be recycled under a
	// slot this call is still tracking.
	ReapFinishedVoices();

	const int soundSlot = FindSoundSlot(request->eventId);
	if (soundSlot < 0)
	{
		SetError(error, errorCapacity, "no sound is registered for event id 0x%08X", request->eventId);
		return 0;
	}
	const SoundEntry& entry = g_sounds[soundSlot];

	const int loopRequested = (request->flags & AUDIO_PLAY_LOOP) != 0;
	if (loopRequested && !entry.hasLoop)
	{
		SetError(error, errorCapacity, "the sound for event id 0x%08X has no loop metadata", request->eventId);
		return 0;
	}

	if (request->flags & AUDIO_PLAY_RESTART)
	{
		SetError(error, errorCapacity, "RESTART needs an explicit voice identity; use Stop(handle) + Play"); return 0;
	}
	AudioVoiceHandle existing;
	if (entry.streaming && FindLiveVoiceForEvent(request->eventId, &existing) >= 0)
	{
		SetError(error, errorCapacity, "the stream already has a live cursor; concurrent stream voices are unsupported"); return 0;
	}

	AudioVoiceHandle acquired;
	if (AudioVoiceTable_Acquire(&g_voices, &acquired) != 0)
	{
		SetError(error, errorCapacity, "no voice slot is free (%d voices)", AUDIO_VOICE_MAX);
		return 0;
	}
	const int index = acquired.index;

	const bool startPaused = (request->flags & AUDIO_PLAY_PAUSED) != 0;
	FMOD::Channel* channel = nullptr;
	FMOD_RESULT result = g_system->playSound(entry.sound, g_masterGroup, true, &channel);
	if (result != FMOD_OK || !channel)
	{
		ReleaseVoiceSlot(index);
		SetFmodError(error, errorCapacity, "could not play the sound", result);
		return 0;
	}

	g_channels[index] = channel;
	g_voiceEvent[index] = request->eventId;

	result = channel->setMode(((request->flags & AUDIO_PLAY_3D) ? (FMOD_3D | FMOD_3D_LINEARROLLOFF) : FMOD_2D) |
		(loopRequested ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF));
	if (result == FMOD_OK)
		result = channel->setVolume(request->volume);
	if (result == FMOD_OK)
		result = channel->setPitch(request->pitch);
	if (result == FMOD_OK && loopRequested)
		result = channel->setLoopCount(-1);	// loop forever over the stored loop points
	if (result == FMOD_OK && !loopRequested && entry.hasLoop)
		result = channel->setLoopCount(0);	// a looped sample requested as a one-shot
	if (result != FMOD_OK)
	{
		StopChannel(channel);
		ReleaseVoiceSlot(index);
		SetFmodError(error, errorCapacity, "could not configure the voice", result);
		return 0;
	}

	if ((request->flags & AUDIO_PLAY_3D) != 0)
	{
		const FMOD_VECTOR position = { request->position[0], request->position[1], request->position[2] };
		const FMOD_VECTOR velocity = { request->velocity[0], request->velocity[1], request->velocity[2] };
		result = channel->set3DAttributes(&position, &velocity);
		if (result == FMOD_OK)
			result = channel->set3DMinMaxDistance(request->minDistance, request->maxDistance);
		if (result != FMOD_OK)
		{
			StopChannel(channel);
			ReleaseVoiceSlot(index);
			SetFmodError(error, errorCapacity, "could not spatialize the voice", result);
			return 0;
		}
	}
	result = channel->setPaused(startPaused); // LAST: no output before complete configuration
	if (result != FMOD_OK)
	{
		StopChannel(channel); ReleaseVoiceSlot(index);
		SetFmodError(error, errorCapacity, "could not apply final voice pause state", result); return 0;
	}

	if (out)
		*out = acquired;
	return 1;
}

int AudioService_Stop(AudioVoiceHandle handle)
{
	if (!g_active || !AudioVoiceTable_IsValid(&g_voices, handle))
		return 0;
	StopHandle(handle);
	return 1;
}

int AudioService_Pause(AudioVoiceHandle handle, int paused)
{
	if (!g_active || !AudioVoiceTable_IsValid(&g_voices, handle) || !g_channels[handle.index])
		return 0;
	return CheckFmod(g_channels[handle.index]->setPaused(paused != 0), "pause voice");
}

int AudioService_SetVoicePitch(AudioVoiceHandle handle, float pitch)
{
	if (!g_active || !AudioVoiceTable_IsValid(&g_voices, handle) || !g_channels[handle.index])
		return 0;
	if (!(pitch > 0.0f) || !(pitch <= AUDIO_PITCH_MAX))
		return 0;
	return CheckFmod(g_channels[handle.index]->setPitch(pitch), "set pitch");
}

int AudioService_SetVoiceVolume(AudioVoiceHandle handle, float volume)
{
	if (!g_active || !AudioVoiceTable_IsValid(&g_voices, handle) || !g_channels[handle.index])
		return 0;
	if (!(volume >= 0.0f && volume <= 1.0f))
		return 0;
	return CheckFmod(g_channels[handle.index]->setVolume(volume), "set volume");
}

int AudioService_SetVoicePosition(AudioVoiceHandle handle, const float position[3], const float velocity[3])
{
	if (!g_active || !AudioVoiceTable_IsValid(&g_voices, handle) || !g_channels[handle.index])
		return 0;
	if (!position || !velocity)
		return 0;
	for (int i = 0; i < 3; ++i)
	{
		if (!(position[i] >= -AUDIO_COORDINATE_MAX && position[i] <= AUDIO_COORDINATE_MAX))
			return 0;
		if (!(velocity[i] >= -AUDIO_COORDINATE_MAX && velocity[i] <= AUDIO_COORDINATE_MAX))
			return 0;
	}

	const FMOD_VECTOR fmodPosition = { position[0], position[1], position[2] };
	const FMOD_VECTOR fmodVelocity = { velocity[0], velocity[1], velocity[2] };
	if (!CheckFmod(g_channels[handle.index]->setMode(FMOD_3D | FMOD_3D_LINEARROLLOFF), "transition voice to 3D"))
		return 0;
	if (!CheckFmod(g_channels[handle.index]->set3DAttributes(&fmodPosition, &fmodVelocity), "set position"))
		return 0;
	return 1;
}

int AudioService_IsVoicePlaying(AudioVoiceHandle handle)
{
	if (!g_active || !AudioVoiceTable_IsValid(&g_voices, handle) || !g_channels[handle.index])
		return 0;
	bool playing = false;
	const FMOD_RESULT result = g_channels[handle.index]->isPlaying(&playing);
	if (result != FMOD_OK)
	{
		if (result != FMOD_ERR_INVALID_HANDLE && result != FMOD_ERR_CHANNEL_STOLEN)
			CheckFmod(result, "query playing status");
		return 0;
	}
	return playing ? 1 : 0;
}

int AudioService_VoiceCount(void)
{
	return g_active ? AudioVoiceTable_Count(&g_voices) : 0;
}

int AudioService_FormatVersion(unsigned int packed, char* out, int cap)
{
	if (!out || cap < 10) return 0;
	for (int shift = 0; shift < 32; shift += 4)
		if (((packed >> shift) & 15) > 9) return 0;
	snprintf(out, cap, "%X.%02X.%02X", packed >> 16, (packed >> 8) & 255, packed & 255);
	return 1;
}

const char* AudioService_Version(void)
{
	static char version[32];
	static int built = 0;
	if (!built)
	{
		// 0xaaaabbcc: aaaa = product version, bb = major, cc = minor.
		AudioService_FormatVersion(FMOD_VERSION, version, sizeof(version));
		built = 1;
	}
	return version;
}

int AudioService_QuerySound(unsigned int eventId, unsigned int* frames, unsigned int* start, unsigned int* end)
{
	const int slot = FindSoundSlot(eventId);
	if (!g_active || slot < 0 || !frames || !start || !end) return 0;
	return g_sounds[slot].sound->getLength(frames, FMOD_TIMEUNIT_PCM) == FMOD_OK &&
		g_sounds[slot].sound->getLoopPoints(start, FMOD_TIMEUNIT_PCM, end, FMOD_TIMEUNIT_PCM) == FMOD_OK;
}

int AudioService_QueryVoice(AudioVoiceHandle handle, int* spatialized, int* paused, float position[3])
{
	if (!g_active || !AudioVoiceTable_IsValid(&g_voices, handle) || !spatialized || !paused || !position) return 0;
	FMOD::Channel* channel = g_channels[handle.index];
	FMOD_MODE mode = 0; bool isPaused = false; FMOD_VECTOR pos = {}, vel = {};
	if (channel->getMode(&mode) != FMOD_OK || channel->getPaused(&isPaused) != FMOD_OK) return 0;
	*spatialized = (mode & FMOD_3D) != 0; *paused = isPaused;
	if (*spatialized && channel->get3DAttributes(&pos, &vel) != FMOD_OK) return 0;
	position[0] = pos.x; position[1] = pos.y; position[2] = pos.z;
	return 1;
}

int AudioService_QueryListener(AudioListener* listener)
{
	if (!g_active || !listener) return 0;
	FMOD_VECTOR p, v, f, u;
	if (g_system->get3DListenerAttributes(0, &p, &v, &f, &u) != FMOD_OK) return 0;
	const FMOD_VECTOR vectors[4] = { p, v, f, u };
	float* outputs[4] = { listener->position, listener->velocity, listener->forward, listener->up };
	for (int i = 0; i < 4; ++i) { outputs[i][0] = vectors[i].x; outputs[i][1] = vectors[i].y; outputs[i][2] = vectors[i].z; }
	return 1;
}

} // extern "C"

#else

// The service is Windows-only in this block; a non-Windows translation unit
// stays empty so the file can be listed unconditionally.
typedef int AudioServiceWindowsOnlyTranslationUnit;

#endif // _WIN32
