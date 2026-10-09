#include "audiocontract.h"

#include <stdio.h>
#include <string.h>
#include <math.h>

// E04 audio contract. Pure data: no FMOD, PsyCross, GPU or game state.

static int Fail(int result, char* error, int errorCapacity, const char* message)
{
	if (error && errorCapacity > 0)
		snprintf(error, errorCapacity, "%s", message);
	return result;
}

static int IsKind(int kind)
{
	return kind >= AUDIO_KIND_SFX && kind <= AUDIO_KIND_DIALOGUE;
}

unsigned int AudioContract_MakeEventId(int kind, int primary, int secondary)
{
	if (!IsKind(kind))
		return 0;
	if (primary < 0 || primary > AUDIO_EVENT_PRIMARY_MAX)
		return 0;
	if (secondary < 0 || secondary > AUDIO_EVENT_SECONDARY_MAX)
		return 0;

	return ((unsigned int)kind << AUDIO_EVENT_KIND_SHIFT) |
		((unsigned int)primary << AUDIO_EVENT_PRIMARY_SHIFT) |
		((unsigned int)secondary << AUDIO_EVENT_SECONDARY_SHIFT);
}

int AudioContract_EventKind(unsigned int eventId)
{
	const int kind = (int)((eventId >> AUDIO_EVENT_KIND_SHIFT) & 0xF);
	if (eventId == 0 || !IsKind(kind))
		return AUDIO_KIND_INVALID;
	return kind;
}

int AudioContract_EventPrimary(unsigned int eventId)
{
	if (eventId == 0)
		return -1;
	return (int)((eventId >> AUDIO_EVENT_PRIMARY_SHIFT) & AUDIO_EVENT_PRIMARY_MAX);
}

int AudioContract_EventSecondary(unsigned int eventId)
{
	if (eventId == 0)
		return -1;
	return (int)(eventId & AUDIO_EVENT_SECONDARY_MAX);
}

const char* AudioContract_KindName(int kind)
{
	switch (kind)
	{
	case AUDIO_KIND_INVALID: return "invalid";
	case AUDIO_KIND_SFX: return "sfx";
	case AUDIO_KIND_MUSIC: return "music";
	case AUDIO_KIND_DIALOGUE: return "dialogue";
	default: return "unknown";
	}
}

const char* AudioContract_ResultName(int result)
{
	switch (result)
	{
	case AUDIO_CONTRACT_OK: return "ok";
	case AUDIO_CONTRACT_ERROR_NULL_REQUEST: return "null-request";
	case AUDIO_CONTRACT_ERROR_EVENT_ID: return "event-id";
	case AUDIO_CONTRACT_ERROR_KIND: return "kind";
	case AUDIO_CONTRACT_ERROR_KIND_MISMATCH: return "kind-mismatch";
	case AUDIO_CONTRACT_ERROR_FLAGS: return "flags";
	case AUDIO_CONTRACT_ERROR_VOLUME: return "volume";
	case AUDIO_CONTRACT_ERROR_PITCH: return "pitch";
	case AUDIO_CONTRACT_ERROR_POSITION: return "position";
	case AUDIO_CONTRACT_ERROR_DISTANCE: return "distance";
	case AUDIO_CONTRACT_ERROR_LISTENER: return "listener";
	case AUDIO_CONTRACT_ERROR_LISTENER_ORIENTATION: return "listener-orientation";
	case AUDIO_CONTRACT_ERROR_LOOP_FRAMES: return "loop-frames";
	case AUDIO_CONTRACT_ERROR_LOOP_START: return "loop-start";
	case AUDIO_CONTRACT_ERROR_LOOP_END: return "loop-end";
	case AUDIO_CONTRACT_ERROR_LOOP_ORDER: return "loop-order";
	default: return "unknown";
	}
}

void AudioContract_InitPlayRequest(AudioPlayRequest* request)
{
	if (!request)
		return;
	memset(request, 0, sizeof(*request));
	request->volume = 1.0f;
	request->pitch = 1.0f;
}

int AudioContract_ValidateRequest(const AudioPlayRequest* request, char* error, int errorCapacity)
{
	if (error && errorCapacity > 0)
		error[0] = '\0';

	if (!request)
		return Fail(AUDIO_CONTRACT_ERROR_NULL_REQUEST, error, errorCapacity, "no play request");

	if (request->eventId == 0)
		return Fail(AUDIO_CONTRACT_ERROR_EVENT_ID, error, errorCapacity, "the event id is invalid (zero)");

	if (!IsKind(request->kind))
		return Fail(AUDIO_CONTRACT_ERROR_KIND, error, errorCapacity, "the audio kind is invalid");

	if (request->kind != AudioContract_EventKind(request->eventId))
		return Fail(AUDIO_CONTRACT_ERROR_KIND_MISMATCH, error, errorCapacity,
			"the request kind does not match the event id");

	if ((request->flags & ~(unsigned int)AUDIO_PLAY_FLAG_MASK) != 0)
		return Fail(AUDIO_CONTRACT_ERROR_FLAGS, error, errorCapacity,
			"the play flags contain unknown bits");

	// Negated comparisons also reject NaN.
	if (!(request->volume >= 0.0f && request->volume <= 1.0f))
		return Fail(AUDIO_CONTRACT_ERROR_VOLUME, error, errorCapacity,
			"the volume must be between 0 and 1");

	if (!(request->pitch > 0.0f))
		return Fail(AUDIO_CONTRACT_ERROR_PITCH, error, errorCapacity,
			"the pitch multiplier must be positive");
	if (!(request->pitch <= AUDIO_PITCH_MAX))
		return Fail(AUDIO_CONTRACT_ERROR_PITCH, error, errorCapacity,
			"the pitch multiplier must be finite and at most AUDIO_PITCH_MAX");

	// Position/velocity are always checked so a non-finite value never reaches
	// the service, even for a request that ignores them.
	for (int i = 0; i < 3; ++i)
	{
		if (!(request->position[i] >= -AUDIO_COORDINATE_MAX && request->position[i] <= AUDIO_COORDINATE_MAX))
			return Fail(AUDIO_CONTRACT_ERROR_POSITION, error, errorCapacity,
				"every position component must be finite and within +/-AUDIO_COORDINATE_MAX raw game units");
	}
	for (int i = 0; i < 3; ++i)
	{
		if (!(request->velocity[i] >= -AUDIO_COORDINATE_MAX && request->velocity[i] <= AUDIO_COORDINATE_MAX))
			return Fail(AUDIO_CONTRACT_ERROR_POSITION, error, errorCapacity,
				"every velocity component must be finite and within +/-AUDIO_COORDINATE_MAX raw game units per second");
	}

	if ((request->flags & AUDIO_PLAY_3D) != 0)
	{
		if (!(request->minDistance > 0.0f))
			return Fail(AUDIO_CONTRACT_ERROR_DISTANCE, error, errorCapacity,
				"the minimum distance must be positive for a spatialized request");
		if (!(request->minDistance <= AUDIO_DISTANCE_MAX))
			return Fail(AUDIO_CONTRACT_ERROR_DISTANCE, error, errorCapacity,
				"the minimum distance must be finite and at most AUDIO_DISTANCE_MAX raw game units");
		if (!(request->maxDistance > 0.0f))
			return Fail(AUDIO_CONTRACT_ERROR_DISTANCE, error, errorCapacity,
				"the maximum distance must be positive for a spatialized request");
		if (!(request->maxDistance <= AUDIO_DISTANCE_MAX))
			return Fail(AUDIO_CONTRACT_ERROR_DISTANCE, error, errorCapacity,
				"the maximum distance must be finite and at most AUDIO_DISTANCE_MAX raw game units");
		if (request->maxDistance < request->minDistance)
			return Fail(AUDIO_CONTRACT_ERROR_DISTANCE, error, errorCapacity,
				"the maximum distance must not be smaller than the minimum distance");
	}

	return AUDIO_CONTRACT_OK;
}

int AudioContract_ValidateListener(const AudioListener* listener, char* error, int errorCapacity)
{
	if (error && errorCapacity > 0)
		error[0] = '\0';

	if (!listener)
		return Fail(AUDIO_CONTRACT_ERROR_LISTENER, error, errorCapacity, "no listener");

	const float* vectors[4] = { listener->position, listener->velocity, listener->forward, listener->up };
	for (int v = 0; v < 4; ++v)
	{
		for (int i = 0; i < 3; ++i)
		{
			const float value = vectors[v][i];
			if (!(value >= -AUDIO_COORDINATE_MAX && value <= AUDIO_COORDINATE_MAX))
				return Fail(AUDIO_CONTRACT_ERROR_LISTENER, error, errorCapacity,
					"every listener component must be finite and within +/-AUDIO_COORDINATE_MAX raw game units");
		}
	}

	if (listener->forward[0] == 0.0f && listener->forward[1] == 0.0f && listener->forward[2] == 0.0f)
		return Fail(AUDIO_CONTRACT_ERROR_LISTENER_ORIENTATION, error, errorCapacity,
			"the listener forward vector must be non-zero");
	if (listener->up[0] == 0.0f && listener->up[1] == 0.0f && listener->up[2] == 0.0f)
		return Fail(AUDIO_CONTRACT_ERROR_LISTENER_ORIENTATION, error, errorCapacity,
			"the listener up vector must be non-zero");

	double f[3], u[3], fl = 0, ul = 0;
	for (int i = 0; i < 3; ++i)
	{
		f[i] = listener->forward[i]; u[i] = listener->up[i];
		fl += f[i] * f[i]; ul += u[i] * u[i];
	}
	double dot = 0;
	for (int i = 0; i < 3; ++i)
	{
		f[i] /= sqrt(fl); u[i] /= sqrt(ul); dot += f[i] * u[i];
	}
	double residual = 0;
	for (int i = 0; i < 3; ++i)
	{
		const double v = u[i] - dot * f[i]; residual += v * v;
	}
	if (residual <= 1.0e-12)
		return Fail(AUDIO_CONTRACT_ERROR_LISTENER_ORIENTATION, error, errorCapacity,
			"the listener forward/up basis is parallel or degenerate");

	return AUDIO_CONTRACT_OK;
}

// Loop end is inclusive; a one-frame loop (loopEnd == loopStart) is valid and
// the service rejects equal FMOD endpoints rather than changing the loop.
int AudioContract_ValidateLoop(int sampleFrames, int loopStart, int loopEnd, char* error, int errorCapacity)
{
	if (error && errorCapacity > 0)
		error[0] = '\0';

	if (sampleFrames <= 0)
		return Fail(AUDIO_CONTRACT_ERROR_LOOP_FRAMES, error, errorCapacity,
			"the sample length must be positive");

	if (loopStart < 0)
		return Fail(AUDIO_CONTRACT_ERROR_LOOP_START, error, errorCapacity,
			"the loop start must not be negative");

	if (loopEnd >= sampleFrames)
		return Fail(AUDIO_CONTRACT_ERROR_LOOP_END, error, errorCapacity,
			"the loop end must be inside the sample (the end frame is inclusive)");

	if (loopEnd < loopStart)
		return Fail(AUDIO_CONTRACT_ERROR_LOOP_ORDER, error, errorCapacity,
			"the loop end must not be before the loop start");

	return AUDIO_CONTRACT_OK;
}

void AudioVoiceTable_Reset(AudioVoiceTable* table)
{
	if (!table)
		return;
	memset(table, 0, sizeof(*table));
}

int AudioVoiceTable_Acquire(AudioVoiceTable* table, AudioVoiceHandle* out)
{
	if (!table || !out)
		return -1;

	for (int i = 0; i < AUDIO_VOICE_MAX; ++i)
	{
		if (table->inUse[i])
			continue;
		table->generation[i] = (unsigned short)(table->generation[i] + 1);
		table->inUse[i] = 1;
		out->index = (unsigned short)i;
		out->generation = table->generation[i];
		return 0;
	}

	out->index = 0;
	out->generation = 0;
	return -1;
}

static int FindVoiceSlot(const AudioVoiceTable* table, AudioVoiceHandle handle)
{
	if (!table)
		return -1;
	if (handle.index >= AUDIO_VOICE_MAX)
		return -1;
	if (!table->inUse[handle.index])
		return -1;
	if (table->generation[handle.index] != handle.generation)
		return -1;
	return (int)handle.index;
}

int AudioVoiceTable_Release(AudioVoiceTable* table, AudioVoiceHandle handle)
{
	const int slot = FindVoiceSlot(table, handle);
	if (slot < 0)
		return -1;

	table->inUse[slot] = 0;
	table->generation[slot] = (unsigned short)(table->generation[slot] + 1);
	return 0;
}

int AudioVoiceTable_IsValid(const AudioVoiceTable* table, AudioVoiceHandle handle)
{
	return FindVoiceSlot(table, handle) >= 0 ? 1 : 0;
}

int AudioVoiceTable_Count(const AudioVoiceTable* table)
{
	if (!table)
		return 0;

	int count = 0;
	for (int i = 0; i < AUDIO_VOICE_MAX; ++i)
	{
		if (table->inUse[i])
			++count;
	}
	return count;
}
