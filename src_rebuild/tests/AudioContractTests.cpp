// Standalone regression tests for the E04 audio contract.
//
// The contract is pure data (no FMOD, PsyCross, GPU or game state), so stable
// event id packing, play flags, request/loop validation and the generational
// voice table can be checked in isolation. Build and run instructions are in
// tests/README.md.

#include "../Game/C/audiocontract.h"

#include <stdio.h>
#include <string.h>

static int g_checks = 0;
static int g_failures = 0;

#define CHECK(condition) \
	do { \
		g_checks++; \
		if (!(condition)) { \
			g_failures++; \
			printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
		} \
	} while (0)

static AudioPlayRequest MakeValidRequest(unsigned int eventId, int kind)
{
	AudioPlayRequest request;
	AudioContract_InitPlayRequest(&request);
	request.eventId = eventId;
	request.kind = kind;
	return request;
}

// Build non-finite floats from their bit patterns so the tests do not depend on
// compiler constant-folding of divisions by zero.
static float MakeFloatFromBits(unsigned int bits)
{
	float value;
	memcpy(&value, &bits, sizeof(value));
	return value;
}

static float MakeNaN() { return MakeFloatFromBits(0x7FC00000u); }
static float MakeInfinity() { return MakeFloatFromBits(0x7F800000u); }
static float MakeNegativeInfinity() { return MakeFloatFromBits(0xFF800000u); }

static void TestEventIdPacking()
{
	const unsigned int sfx = AudioContract_MakeEventId(AUDIO_KIND_SFX, 3, 7);
	CHECK(sfx != 0);
	CHECK(AudioContract_EventKind(sfx) == AUDIO_KIND_SFX);
	CHECK(AudioContract_EventPrimary(sfx) == 3);
	CHECK(AudioContract_EventSecondary(sfx) == 7);

	// The packing is pinned: kind in the high nibble, primary in bits 27..16,
	// secondary in bits 15..0.
	CHECK(sfx == (((unsigned int)AUDIO_KIND_SFX << AUDIO_EVENT_KIND_SHIFT) |
		(3u << AUDIO_EVENT_PRIMARY_SHIFT) | 7u));

	// Kind composition: the same primary/secondary in another kind is another id.
	CHECK(AudioContract_MakeEventId(AUDIO_KIND_MUSIC, 3, 7) != sfx);
	CHECK(AudioContract_MakeEventId(AUDIO_KIND_DIALOGUE, 3, 7) != sfx);
	CHECK(AudioContract_MakeEventId(AUDIO_KIND_SFX, 3, 8) != sfx);
	CHECK(AudioContract_MakeEventId(AUDIO_KIND_SFX, 4, 7) != sfx);

	const unsigned int music = AudioContract_MakeEventId(AUDIO_KIND_MUSIC, 2, 9);
	CHECK(AudioContract_EventKind(music) == AUDIO_KIND_MUSIC);
	CHECK(AudioContract_EventPrimary(music) == 2);
	CHECK(AudioContract_EventSecondary(music) == 9);

	// Field limits are inclusive.
	CHECK(AudioContract_MakeEventId(AUDIO_KIND_DIALOGUE, 0, 0) != 0);
	const unsigned int limit = AudioContract_MakeEventId(AUDIO_KIND_SFX, AUDIO_EVENT_PRIMARY_MAX, AUDIO_EVENT_SECONDARY_MAX);
	CHECK(limit != 0);
	CHECK(limit != AudioContract_MakeEventId(AUDIO_KIND_SFX, 0, 0));
	CHECK(AudioContract_EventPrimary(limit) == AUDIO_EVENT_PRIMARY_MAX);
	CHECK(AudioContract_EventSecondary(limit) == AUDIO_EVENT_SECONDARY_MAX);

	// Invalid kinds and out-of-range fields produce the reserved id 0.
	CHECK(AudioContract_MakeEventId(0, 1, 1) == 0);
	CHECK(AudioContract_MakeEventId(-1, 1, 1) == 0);
	CHECK(AudioContract_MakeEventId(4, 1, 1) == 0);
	CHECK(AudioContract_MakeEventId(15, 1, 1) == 0);
	CHECK(AudioContract_MakeEventId(AUDIO_KIND_SFX, -1, 1) == 0);
	CHECK(AudioContract_MakeEventId(AUDIO_KIND_SFX, AUDIO_EVENT_PRIMARY_MAX + 1, 1) == 0);
	CHECK(AudioContract_MakeEventId(AUDIO_KIND_SFX, 1, -1) == 0);
	CHECK(AudioContract_MakeEventId(AUDIO_KIND_SFX, 1, AUDIO_EVENT_SECONDARY_MAX + 1) == 0);

	// Id 0 is invalid: no kind and no unpacked fields.
	CHECK(AudioContract_EventKind(0) == AUDIO_KIND_INVALID);
	CHECK(AudioContract_EventPrimary(0) == -1);
	CHECK(AudioContract_EventSecondary(0) == -1);

	// Ids whose kind field is not an AudioKind are invalid too.
	CHECK(AudioContract_EventKind(4u << AUDIO_EVENT_KIND_SHIFT | 1u) == AUDIO_KIND_INVALID);
	CHECK(AudioContract_EventKind(5u << AUDIO_EVENT_KIND_SHIFT) == AUDIO_KIND_INVALID);
}

static void TestKindAndResultNames()
{
	CHECK(strcmp(AudioContract_KindName(AUDIO_KIND_INVALID), "invalid") == 0);
	CHECK(strcmp(AudioContract_KindName(AUDIO_KIND_SFX), "sfx") == 0);
	CHECK(strcmp(AudioContract_KindName(AUDIO_KIND_MUSIC), "music") == 0);
	CHECK(strcmp(AudioContract_KindName(AUDIO_KIND_DIALOGUE), "dialogue") == 0);
	CHECK(strcmp(AudioContract_KindName(42), "unknown") == 0);

	CHECK(strcmp(AudioContract_ResultName(AUDIO_CONTRACT_OK), "ok") == 0);
	CHECK(strcmp(AudioContract_ResultName(AUDIO_CONTRACT_ERROR_VOLUME), "volume") == 0);
	CHECK(strcmp(AudioContract_ResultName(AUDIO_CONTRACT_ERROR_POSITION), "position") == 0);
	CHECK(strcmp(AudioContract_ResultName(AUDIO_CONTRACT_ERROR_LISTENER), "listener") == 0);
	CHECK(strcmp(AudioContract_ResultName(AUDIO_CONTRACT_ERROR_LISTENER_ORIENTATION), "listener-orientation") == 0);
	CHECK(strcmp(AudioContract_ResultName(AUDIO_CONTRACT_ERROR_LOOP_END), "loop-end") == 0);
	CHECK(strcmp(AudioContract_ResultName(1234), "unknown") == 0);
}

static void TestPlayFlags()
{
	CHECK(AUDIO_PLAY_LOOP == 1);
	CHECK(AUDIO_PLAY_3D == 2);
	CHECK(AUDIO_PLAY_PAUSED == 4);
	CHECK(AUDIO_PLAY_RESTART == 8);

	// The flags are independent bits: no combination overlaps another.
	CHECK((AUDIO_PLAY_LOOP & AUDIO_PLAY_3D) == 0);
	CHECK((AUDIO_PLAY_LOOP & AUDIO_PLAY_PAUSED) == 0);
	CHECK((AUDIO_PLAY_LOOP & AUDIO_PLAY_RESTART) == 0);
	CHECK((AUDIO_PLAY_3D & AUDIO_PLAY_PAUSED) == 0);
	CHECK((AUDIO_PLAY_3D & AUDIO_PLAY_RESTART) == 0);
	CHECK((AUDIO_PLAY_PAUSED & AUDIO_PLAY_RESTART) == 0);
	CHECK((AUDIO_PLAY_LOOP | AUDIO_PLAY_3D) == 3);
	CHECK((AUDIO_PLAY_LOOP | AUDIO_PLAY_3D | AUDIO_PLAY_PAUSED | AUDIO_PLAY_RESTART) == 15);
	CHECK((16u & AUDIO_PLAY_FLAG_MASK) == 0);
}

static void TestRequestValidation()
{
	char error[AUDIO_CONTRACT_ERROR_CAPACITY];
	const unsigned int sfx = AudioContract_MakeEventId(AUDIO_KIND_SFX, 4, 12);

	AudioPlayRequest request = MakeValidRequest(sfx, AUDIO_KIND_SFX);
	CHECK(AudioContract_ValidateRequest(&request, error, sizeof(error)) == AUDIO_CONTRACT_OK);
	CHECK(error[0] == '\0');

	// The reserved/zero event id cannot be played.
	request.eventId = 0;
	CHECK(AudioContract_ValidateRequest(&request, error, sizeof(error)) == AUDIO_CONTRACT_ERROR_EVENT_ID);
	CHECK(error[0] != '\0');

	// An unknown kind and a kind that contradicts the id are distinct errors.
	request = MakeValidRequest(sfx, AUDIO_KIND_SFX);
	request.kind = 7;
	CHECK(AudioContract_ValidateRequest(&request, error, sizeof(error)) == AUDIO_CONTRACT_ERROR_KIND);
	CHECK(error[0] != '\0');

	request = MakeValidRequest(sfx, AUDIO_KIND_SFX);
	request.kind = AUDIO_KIND_MUSIC;
	CHECK(AudioContract_ValidateRequest(&request, error, sizeof(error)) == AUDIO_CONTRACT_ERROR_KIND_MISMATCH);
	CHECK(error[0] != '\0');

	// Unknown flag bits are rejected.
	request = MakeValidRequest(sfx, AUDIO_KIND_SFX);
	request.flags = 16;
	CHECK(AudioContract_ValidateRequest(&request, error, sizeof(error)) == AUDIO_CONTRACT_ERROR_FLAGS);
	CHECK(error[0] != '\0');

	// Volume is a linear gain in [0, 1]; its bounds are inclusive.
	request = MakeValidRequest(sfx, AUDIO_KIND_SFX);
	request.volume = -0.01f;
	CHECK(AudioContract_ValidateRequest(&request, error, sizeof(error)) == AUDIO_CONTRACT_ERROR_VOLUME);
	request.volume = 1.01f;
	CHECK(AudioContract_ValidateRequest(&request, error, sizeof(error)) == AUDIO_CONTRACT_ERROR_VOLUME);
	request.volume = 0.0f;
	CHECK(AudioContract_ValidateRequest(&request, error, sizeof(error)) == AUDIO_CONTRACT_OK);
	request.volume = 1.0f;
	CHECK(AudioContract_ValidateRequest(&request, error, sizeof(error)) == AUDIO_CONTRACT_OK);

	// Pitch is a positive multiplier around the 1.0 reference.
	request = MakeValidRequest(sfx, AUDIO_KIND_SFX);
	request.pitch = 0.0f;
	CHECK(AudioContract_ValidateRequest(&request, error, sizeof(error)) == AUDIO_CONTRACT_ERROR_PITCH);
	request.pitch = -2.0f;
	CHECK(AudioContract_ValidateRequest(&request, error, sizeof(error)) == AUDIO_CONTRACT_ERROR_PITCH);
	request.pitch = 0.5f;
	CHECK(AudioContract_ValidateRequest(&request, error, sizeof(error)) == AUDIO_CONTRACT_OK);
	request.pitch = 4.0f;
	CHECK(AudioContract_ValidateRequest(&request, error, sizeof(error)) == AUDIO_CONTRACT_OK);

	// A spatialized request needs positive, ordered distances.
	request = MakeValidRequest(sfx, AUDIO_KIND_SFX);
	request.flags = AUDIO_PLAY_3D;
	CHECK(AudioContract_ValidateRequest(&request, error, sizeof(error)) == AUDIO_CONTRACT_ERROR_DISTANCE);
	request.minDistance = 1.0f;
	CHECK(AudioContract_ValidateRequest(&request, error, sizeof(error)) == AUDIO_CONTRACT_ERROR_DISTANCE);
	request.maxDistance = 50.0f;
	CHECK(AudioContract_ValidateRequest(&request, error, sizeof(error)) == AUDIO_CONTRACT_OK);
	request.maxDistance = 0.5f;
	CHECK(AudioContract_ValidateRequest(&request, error, sizeof(error)) == AUDIO_CONTRACT_ERROR_DISTANCE);

	// Distances are ignored for non-spatialized requests.
	request = MakeValidRequest(sfx, AUDIO_KIND_SFX);
	request.flags = AUDIO_PLAY_LOOP | AUDIO_PLAY_PAUSED | AUDIO_PLAY_RESTART;
	CHECK(AudioContract_ValidateRequest(&request, error, sizeof(error)) == AUDIO_CONTRACT_OK);

	// Every flag together is still a valid request.
	request = MakeValidRequest(sfx, AUDIO_KIND_SFX);
	request.flags = AUDIO_PLAY_LOOP | AUDIO_PLAY_3D | AUDIO_PLAY_PAUSED | AUDIO_PLAY_RESTART;
	request.minDistance = 2.0f;
	request.maxDistance = 20.0f;
	CHECK(AudioContract_ValidateRequest(&request, error, sizeof(error)) == AUDIO_CONTRACT_OK);

	// A missing request is reported, not dereferenced.
	CHECK(AudioContract_ValidateRequest(NULL, error, sizeof(error)) == AUDIO_CONTRACT_ERROR_NULL_REQUEST);
	CHECK(error[0] != '\0');

	// A missing diagnostic buffer is tolerated.
	request = MakeValidRequest(sfx, AUDIO_KIND_SFX);
	CHECK(AudioContract_ValidateRequest(&request, NULL, 0) == AUDIO_CONTRACT_OK);
	request.volume = 2.0f;
	CHECK(AudioContract_ValidateRequest(&request, NULL, 0) == AUDIO_CONTRACT_ERROR_VOLUME);
}

static void TestNonFiniteRejection()
{
	char error[AUDIO_CONTRACT_ERROR_CAPACITY];
	const unsigned int sfx = AudioContract_MakeEventId(AUDIO_KIND_SFX, 4, 12);

	// Volume rejects NaN and both infinities.
	AudioPlayRequest request = MakeValidRequest(sfx, AUDIO_KIND_SFX);
	request.volume = MakeNaN();
	CHECK(AudioContract_ValidateRequest(&request, error, sizeof(error)) == AUDIO_CONTRACT_ERROR_VOLUME);
	request.volume = MakeInfinity();
	CHECK(AudioContract_ValidateRequest(&request, error, sizeof(error)) == AUDIO_CONTRACT_ERROR_VOLUME);
	request.volume = MakeNegativeInfinity();
	CHECK(AudioContract_ValidateRequest(&request, error, sizeof(error)) == AUDIO_CONTRACT_ERROR_VOLUME);

	// Pitch rejects NaN, both infinities and values above AUDIO_PITCH_MAX.
	request = MakeValidRequest(sfx, AUDIO_KIND_SFX);
	request.pitch = MakeNaN();
	CHECK(AudioContract_ValidateRequest(&request, error, sizeof(error)) == AUDIO_CONTRACT_ERROR_PITCH);
	request.pitch = MakeInfinity();
	CHECK(AudioContract_ValidateRequest(&request, error, sizeof(error)) == AUDIO_CONTRACT_ERROR_PITCH);
	request.pitch = MakeNegativeInfinity();
	CHECK(AudioContract_ValidateRequest(&request, error, sizeof(error)) == AUDIO_CONTRACT_ERROR_PITCH);
	request.pitch = AUDIO_PITCH_MAX + 1.0f;
	CHECK(AudioContract_ValidateRequest(&request, error, sizeof(error)) == AUDIO_CONTRACT_ERROR_PITCH);
	request.pitch = AUDIO_PITCH_MAX;
	CHECK(AudioContract_ValidateRequest(&request, error, sizeof(error)) == AUDIO_CONTRACT_OK);
	CHECK(AUDIO_PITCH_MAX == 8.0f);

	// Distance bounds reject NaN, both infinities and above-bound values.
	request = MakeValidRequest(sfx, AUDIO_KIND_SFX);
	request.flags = AUDIO_PLAY_3D;
	request.minDistance = MakeNaN();
	CHECK(AudioContract_ValidateRequest(&request, error, sizeof(error)) == AUDIO_CONTRACT_ERROR_DISTANCE);
	request.minDistance = MakeInfinity();
	CHECK(AudioContract_ValidateRequest(&request, error, sizeof(error)) == AUDIO_CONTRACT_ERROR_DISTANCE);
	request.minDistance = MakeNegativeInfinity();
	CHECK(AudioContract_ValidateRequest(&request, error, sizeof(error)) == AUDIO_CONTRACT_ERROR_DISTANCE);
	request.minDistance = AUDIO_DISTANCE_MAX * 2.0f;
	CHECK(AudioContract_ValidateRequest(&request, error, sizeof(error)) == AUDIO_CONTRACT_ERROR_DISTANCE);
	request.minDistance = 1.0f;
	request.maxDistance = MakeNaN();
	CHECK(AudioContract_ValidateRequest(&request, error, sizeof(error)) == AUDIO_CONTRACT_ERROR_DISTANCE);
	request.maxDistance = MakeInfinity();
	CHECK(AudioContract_ValidateRequest(&request, error, sizeof(error)) == AUDIO_CONTRACT_ERROR_DISTANCE);
	request.maxDistance = MakeNegativeInfinity();
	CHECK(AudioContract_ValidateRequest(&request, error, sizeof(error)) == AUDIO_CONTRACT_ERROR_DISTANCE);
	request.maxDistance = AUDIO_DISTANCE_MAX * 2.0f;
	CHECK(AudioContract_ValidateRequest(&request, error, sizeof(error)) == AUDIO_CONTRACT_ERROR_DISTANCE);
	request.maxDistance = AUDIO_DISTANCE_MAX;
	CHECK(AudioContract_ValidateRequest(&request, error, sizeof(error)) == AUDIO_CONTRACT_OK);

	// Position and velocity are always checked, even when the request is not
	// spatialized.
	request = MakeValidRequest(sfx, AUDIO_KIND_SFX);
	request.position[0] = MakeNaN();
	CHECK(AudioContract_ValidateRequest(&request, error, sizeof(error)) == AUDIO_CONTRACT_ERROR_POSITION);
	request = MakeValidRequest(sfx, AUDIO_KIND_SFX);
	request.position[1] = MakeInfinity();
	CHECK(AudioContract_ValidateRequest(&request, error, sizeof(error)) == AUDIO_CONTRACT_ERROR_POSITION);
	request.position[1] = MakeNegativeInfinity();
	CHECK(AudioContract_ValidateRequest(&request, error, sizeof(error)) == AUDIO_CONTRACT_ERROR_POSITION);
	request.position[1] = AUDIO_COORDINATE_MAX * 2.0f;
	CHECK(AudioContract_ValidateRequest(&request, error, sizeof(error)) == AUDIO_CONTRACT_ERROR_POSITION);
	request.position[1] = -AUDIO_COORDINATE_MAX;
	CHECK(AudioContract_ValidateRequest(&request, error, sizeof(error)) == AUDIO_CONTRACT_OK);
	request.velocity[0] = MakeNaN();
	CHECK(AudioContract_ValidateRequest(&request, error, sizeof(error)) == AUDIO_CONTRACT_ERROR_POSITION);
	request.velocity[0] = MakeInfinity();
	CHECK(AudioContract_ValidateRequest(&request, error, sizeof(error)) == AUDIO_CONTRACT_ERROR_POSITION);
	request.velocity[0] = MakeNegativeInfinity();
	CHECK(AudioContract_ValidateRequest(&request, error, sizeof(error)) == AUDIO_CONTRACT_ERROR_POSITION);
	request.velocity[0] = AUDIO_COORDINATE_MAX * 2.0f;
	CHECK(AudioContract_ValidateRequest(&request, error, sizeof(error)) == AUDIO_CONTRACT_ERROR_POSITION);
	request.velocity[0] = AUDIO_COORDINATE_MAX;
	CHECK(AudioContract_ValidateRequest(&request, error, sizeof(error)) == AUDIO_CONTRACT_OK);
	CHECK(AUDIO_COORDINATE_MAX == 1.0e8f);
	CHECK(AUDIO_DISTANCE_MAX == 1.0e7f);
}

static void TestInvalidKindNibble()
{
	char error[AUDIO_CONTRACT_ERROR_CAPACITY];
	const unsigned int strangeId = (4u << AUDIO_EVENT_KIND_SHIFT) |
		(5u << AUDIO_EVENT_PRIMARY_SHIFT) | 7u;

	// The kind field is not an AudioKind, so the id is invalid...
	CHECK(AudioContract_EventKind(strangeId) == AUDIO_KIND_INVALID);
	// ...but the packed fields still unpack for diagnostics and logging.
	CHECK(AudioContract_EventPrimary(strangeId) == 5);
	CHECK(AudioContract_EventSecondary(strangeId) == 7);

	// A request for the invalid id cannot agree on a kind.
	AudioPlayRequest request = MakeValidRequest(strangeId, AUDIO_KIND_SFX);
	CHECK(AudioContract_ValidateRequest(&request, error, sizeof(error)) == AUDIO_CONTRACT_ERROR_KIND_MISMATCH);
	CHECK(error[0] != '\0');

	// An invalid request kind is reported as its own field error first.
	request.kind = 4;
	CHECK(AudioContract_ValidateRequest(&request, error, sizeof(error)) == AUDIO_CONTRACT_ERROR_KIND);
}

static void TestDiagnosticBufferGuards()
{
	char error[AUDIO_CONTRACT_ERROR_CAPACITY];
	const unsigned int sfx = AudioContract_MakeEventId(AUDIO_KIND_SFX, 4, 12);

	// A non-NULL buffer with zero capacity is never written, and NULL buffers
	// with any capacity return the same named result instead of crashing.
	AudioPlayRequest request = MakeValidRequest(sfx, AUDIO_KIND_SFX);
	request.volume = 2.0f;
	error[0] = 'x';
	CHECK(AudioContract_ValidateRequest(&request, error, 0) == AUDIO_CONTRACT_ERROR_VOLUME);
	CHECK(error[0] == 'x');
	CHECK(AudioContract_ValidateRequest(&request, NULL, 64) == AUDIO_CONTRACT_ERROR_VOLUME);
	CHECK(AudioContract_ValidateRequest(&request, NULL, 0) == AUDIO_CONTRACT_ERROR_VOLUME);

	CHECK(AudioContract_ValidateLoop(100, 0, 100, error, 0) == AUDIO_CONTRACT_ERROR_LOOP_END);
	CHECK(AudioContract_ValidateLoop(100, 0, 100, NULL, 64) == AUDIO_CONTRACT_ERROR_LOOP_END);
	CHECK(AudioContract_ValidateLoop(100, 0, 100, NULL, 0) == AUDIO_CONTRACT_ERROR_LOOP_END);

	AudioListener listener;
	memset(&listener, 0, sizeof(listener));
	CHECK(AudioContract_ValidateListener(&listener, error, 0) == AUDIO_CONTRACT_ERROR_LISTENER_ORIENTATION);
	CHECK(AudioContract_ValidateListener(&listener, NULL, 64) == AUDIO_CONTRACT_ERROR_LISTENER_ORIENTATION);
	CHECK(AudioContract_ValidateListener(NULL, NULL, 0) == AUDIO_CONTRACT_ERROR_LISTENER);
	CHECK(AudioContract_ValidateListener(NULL, error, sizeof(error)) == AUDIO_CONTRACT_ERROR_LISTENER);
}

static void TestVoiceTable()
{
	AudioVoiceTable table;
	AudioVoiceTable_Reset(&table);
	CHECK(AudioVoiceTable_Count(&table) == 0);

	// A zeroed handle is invalid on a fresh table.
	AudioVoiceHandle zero;
	zero.index = 0;
	zero.generation = 0;
	CHECK(AudioVoiceTable_IsValid(&table, zero) == 0);
	CHECK(AudioVoiceTable_Release(&table, zero) == -1);

	AudioVoiceHandle first;
	CHECK(AudioVoiceTable_Acquire(&table, &first) == 0);
	CHECK(AudioVoiceTable_IsValid(&table, first) == 1);
	CHECK(AudioVoiceTable_Count(&table) == 1);
	CHECK(first.generation != 0);

	// Release stales the handle immediately.
	CHECK(AudioVoiceTable_Release(&table, first) == 0);
	CHECK(AudioVoiceTable_IsValid(&table, first) == 0);
	CHECK(AudioVoiceTable_Release(&table, first) == -1);
	CHECK(AudioVoiceTable_Count(&table) == 0);

	// Reacquiring the slot gives a new generation; the old handle stays stale.
	AudioVoiceHandle second;
	CHECK(AudioVoiceTable_Acquire(&table, &second) == 0);
	CHECK(second.index == first.index);
	CHECK(second.generation != first.generation);
	CHECK(AudioVoiceTable_IsValid(&table, first) == 0);
	CHECK(AudioVoiceTable_IsValid(&table, second) == 1);

	// A forged generation cannot release or validate the live slot.
	AudioVoiceHandle forged = second;
	forged.generation = (unsigned short)(forged.generation + 2);
	CHECK(AudioVoiceTable_IsValid(&table, forged) == 0);
	CHECK(AudioVoiceTable_Release(&table, forged) == -1);
	CHECK(AudioVoiceTable_IsValid(&table, second) == 1);

	// Out-of-range indexes are rejected.
	AudioVoiceHandle outOfRange;
	outOfRange.index = AUDIO_VOICE_MAX;
	outOfRange.generation = 1;
	CHECK(AudioVoiceTable_IsValid(&table, outOfRange) == 0);
	CHECK(AudioVoiceTable_Release(&table, outOfRange) == -1);

	// Fill the table: all 64 slots are distinct and the next acquire fails.
	CHECK(AudioVoiceTable_Release(&table, second) == 0);
	AudioVoiceHandle handles[AUDIO_VOICE_MAX];
	int acquired = 0;
	for (int i = 0; i < AUDIO_VOICE_MAX; ++i)
	{
		if (AudioVoiceTable_Acquire(&table, &handles[i]) != 0)
			break;
		++acquired;
	}
	CHECK(acquired == AUDIO_VOICE_MAX);
	CHECK(AudioVoiceTable_Count(&table) == AUDIO_VOICE_MAX);

	int distinct = 1;
	for (int i = 0; i < AUDIO_VOICE_MAX && distinct; ++i)
	{
		for (int j = i + 1; j < AUDIO_VOICE_MAX; ++j)
		{
			if (handles[i].index == handles[j].index)
				distinct = 0;
		}
	}
	CHECK(distinct == 1);

	AudioVoiceHandle extra;
	CHECK(AudioVoiceTable_Acquire(&table, &extra) == -1);
	CHECK(extra.index == 0 && extra.generation == 0);

	// Reset invalidates every outstanding handle.
	AudioVoiceTable_Reset(&table);
	CHECK(AudioVoiceTable_IsValid(&table, handles[0]) == 0);
	CHECK(AudioVoiceTable_IsValid(&table, handles[AUDIO_VOICE_MAX - 1]) == 0);
	CHECK(AudioVoiceTable_Count(&table) == 0);

	CHECK(AudioVoiceTable_Count(NULL) == 0);
	CHECK(AudioVoiceTable_IsValid(NULL, handles[0]) == 0);
}

static void TestGenerationWraparound()
{
	AudioVoiceTable table;
	AudioVoiceTable_Reset(&table);

	AudioVoiceHandle ancient;
	CHECK(AudioVoiceTable_Acquire(&table, &ancient) == 0);
	CHECK(ancient.generation == 1);
	CHECK(AudioVoiceTable_Release(&table, ancient) == 0);

	// The acquire and release above already advanced the slot generation twice.
	// 32767 more acquire/release pairs (65534 increments) complete one full
	// 16-bit cycle; the stale handle must stay invalid throughout.
	int stayedStale = 1;
	for (int i = 0; i < 32767; ++i)
	{
		AudioVoiceHandle temporary;
		if (AudioVoiceTable_Acquire(&table, &temporary) != 0) { stayedStale = 0; break; }
		if (AudioVoiceTable_IsValid(&table, ancient) != 0) stayedStale = 0;
		if (AudioVoiceTable_Release(&table, temporary) != 0) { stayedStale = 0; break; }
	}
	CHECK(stayedStale == 1);
	CHECK(AudioVoiceTable_Count(&table) == 0);
	CHECK(AudioVoiceTable_IsValid(&table, ancient) == 0);

	// The generation wrapped back to zero, and the next acquire returns
	// generation 1 again: this documented wraparound is the only way a stale
	// handle can become valid again.
	AudioVoiceHandle revived;
	CHECK(AudioVoiceTable_Acquire(&table, &revived) == 0);
	CHECK(revived.index == ancient.index);
	CHECK(revived.generation == ancient.generation);
	CHECK(AudioVoiceTable_IsValid(&table, ancient) == 1);
}

static void TestResetEraCollision()
{
	AudioVoiceTable table;
	AudioVoiceTable_Reset(&table);

	AudioVoiceHandle first, second;
	CHECK(AudioVoiceTable_Acquire(&table, &first) == 0);	// slot 0, generation 1
	CHECK(AudioVoiceTable_Acquire(&table, &second) == 0);	// slot 1
	CHECK(first.index != second.index);
	CHECK(AudioVoiceTable_IsValid(&table, first) == 1);

	// Reset restarts generations at 0: both pre-Reset handles are stale while
	// their slots are free...
	AudioVoiceTable_Reset(&table);
	CHECK(AudioVoiceTable_IsValid(&table, first) == 0);
	CHECK(AudioVoiceTable_IsValid(&table, second) == 0);

	// ...but the first post-Reset acquire returns slot 0 at generation 1 again,
	// exactly the pre-Reset `first` handle. This documented resurrection is why
	// handles are era-scoped and every held handle must be discarded on Reset.
	AudioVoiceHandle reborn;
	CHECK(AudioVoiceTable_Acquire(&table, &reborn) == 0);
	CHECK(reborn.index == first.index);
	CHECK(reborn.generation == first.generation);
	CHECK(AudioVoiceTable_IsValid(&table, first) == 1);

	// The other pre-Reset handle is never re-acquired and stays invalid.
	CHECK(AudioVoiceTable_IsValid(&table, second) == 0);
}

static void TestLoopValidation()
{
	char error[AUDIO_CONTRACT_ERROR_CAPACITY];

	// A valid interior loop; the end frame is inclusive.
	CHECK(AudioContract_ValidateLoop(1000, 100, 500, error, sizeof(error)) == AUDIO_CONTRACT_OK);
	CHECK(error[0] == '\0');

	// The last frame of the sample is the largest valid inclusive end.
	CHECK(AudioContract_ValidateLoop(1000, 0, 999, error, sizeof(error)) == AUDIO_CONTRACT_OK);

	// One-frame loops remain metadata-valid, but the FMOD service rejects
	// equal endpoints explicitly. Both layers retain inclusive end semantics.
	CHECK(AudioContract_ValidateLoop(10, 5, 5, error, sizeof(error)) == AUDIO_CONTRACT_OK);
	CHECK(AudioContract_ValidateLoop(1, 0, 0, error, sizeof(error)) == AUDIO_CONTRACT_OK);

	// end == sampleFrames is outside the sample.
	CHECK(AudioContract_ValidateLoop(1000, 100, 1000, error, sizeof(error)) == AUDIO_CONTRACT_ERROR_LOOP_END);
	CHECK(error[0] != '\0');
	CHECK(AudioContract_ValidateLoop(100, 0, 200, error, sizeof(error)) == AUDIO_CONTRACT_ERROR_LOOP_END);

	// A reversed range is invalid; a negative end is caught as reversed.
	CHECK(AudioContract_ValidateLoop(1000, 500, 100, error, sizeof(error)) == AUDIO_CONTRACT_ERROR_LOOP_ORDER);
	CHECK(AudioContract_ValidateLoop(100, 10, -1, error, sizeof(error)) == AUDIO_CONTRACT_ERROR_LOOP_ORDER);

	// A negative start is its own diagnostic.
	CHECK(AudioContract_ValidateLoop(1000, -1, 500, error, sizeof(error)) == AUDIO_CONTRACT_ERROR_LOOP_START);

	// The sample must have frames at all.
	CHECK(AudioContract_ValidateLoop(0, 0, 0, error, sizeof(error)) == AUDIO_CONTRACT_ERROR_LOOP_FRAMES);
	CHECK(AudioContract_ValidateLoop(-5, 0, 0, error, sizeof(error)) == AUDIO_CONTRACT_ERROR_LOOP_FRAMES);

	// A missing diagnostic buffer is tolerated.
	CHECK(AudioContract_ValidateLoop(100, 0, 50, NULL, 0) == AUDIO_CONTRACT_OK);
	CHECK(AudioContract_ValidateLoop(100, 0, 100, NULL, 0) == AUDIO_CONTRACT_ERROR_LOOP_END);
}

static void TestListenerValidation()
{
	char error[AUDIO_CONTRACT_ERROR_CAPACITY];
	AudioListener listener;
	memset(&listener, 0, sizeof(listener));
	listener.forward[2] = 1.0f;
	listener.up[1] = 1.0f;

	CHECK(AudioContract_ValidateListener(&listener, error, sizeof(error)) == AUDIO_CONTRACT_OK);
	CHECK(error[0] == '\0');

	// Position/velocity may be any finite value; zero velocity means no Doppler.
	listener.position[0] = 100.0f;
	listener.position[1] = -2.0f;
	listener.position[2] = AUDIO_COORDINATE_MAX;
	listener.velocity[0] = 5.0f;
	CHECK(AudioContract_ValidateListener(&listener, error, sizeof(error)) == AUDIO_CONTRACT_OK);

	// A zero forward or up cannot be normalized by the service.
	listener.forward[2] = 0.0f;
	CHECK(AudioContract_ValidateListener(&listener, error, sizeof(error)) == AUDIO_CONTRACT_ERROR_LISTENER_ORIENTATION);
	CHECK(error[0] != '\0');
	listener.forward[2] = 1.0f;
	listener.up[1] = 0.0f;
	CHECK(AudioContract_ValidateListener(&listener, error, sizeof(error)) == AUDIO_CONTRACT_ERROR_LISTENER_ORIENTATION);
	listener.up[1] = 1.0f;

	// Non-finite or above-bound components are rejected wherever they appear.
	listener.position[0] = MakeNaN();
	CHECK(AudioContract_ValidateListener(&listener, error, sizeof(error)) == AUDIO_CONTRACT_ERROR_LISTENER);
	listener.position[0] = 0.0f;
	listener.velocity[0] = MakeInfinity();
	CHECK(AudioContract_ValidateListener(&listener, error, sizeof(error)) == AUDIO_CONTRACT_ERROR_LISTENER);
	listener.velocity[0] = MakeNegativeInfinity();
	CHECK(AudioContract_ValidateListener(&listener, error, sizeof(error)) == AUDIO_CONTRACT_ERROR_LISTENER);
	listener.velocity[0] = 0.0f;
	listener.forward[0] = MakeNegativeInfinity();
	CHECK(AudioContract_ValidateListener(&listener, error, sizeof(error)) == AUDIO_CONTRACT_ERROR_LISTENER);
	listener.forward[0] = 0.0f;
	listener.up[2] = AUDIO_COORDINATE_MAX * 2.0f;
	CHECK(AudioContract_ValidateListener(&listener, error, sizeof(error)) == AUDIO_CONTRACT_ERROR_LISTENER);
	listener.up[2] = 0.0f;

	// A missing listener is reported, not dereferenced.
	CHECK(AudioContract_ValidateListener(NULL, error, sizeof(error)) == AUDIO_CONTRACT_ERROR_LISTENER);
	CHECK(error[0] != '\0');

	// A missing diagnostic buffer is tolerated.
	CHECK(AudioContract_ValidateListener(&listener, NULL, 0) == AUDIO_CONTRACT_OK);
	listener.up[1] = 0; listener.up[2] = 2;
	CHECK(AudioContract_ValidateListener(&listener, error, sizeof(error)) == AUDIO_CONTRACT_ERROR_LISTENER_ORIENTATION);
	CHECK(strstr(error, "parallel") != NULL);
	listener.up[2] = -2;
	CHECK(AudioContract_ValidateListener(&listener, error, sizeof(error)) == AUDIO_CONTRACT_ERROR_LISTENER_ORIENTATION);
	listener.up[1] = 1; listener.up[2] = 0.5f;
	CHECK(AudioContract_ValidateListener(&listener, error, sizeof(error)) == AUDIO_CONTRACT_OK);
	listener.forward[2] = 1e-30f; listener.up[2] = 0; listener.up[1] = 1e-30f;
	CHECK(AudioContract_ValidateListener(&listener, error, sizeof(error)) == AUDIO_CONTRACT_OK);
}

static void TestDeterminism()
{
	// Same inputs, same ids - independent of call order or any pointer identity.
	CHECK(AudioContract_MakeEventId(AUDIO_KIND_SFX, 12, 34) ==
		AudioContract_MakeEventId(AUDIO_KIND_SFX, 12, 34));

	const unsigned int sfx = AudioContract_MakeEventId(AUDIO_KIND_SFX, 1, 2);
	const unsigned int music = AudioContract_MakeEventId(AUDIO_KIND_MUSIC, 1, 2);
	const unsigned int dialogue = AudioContract_MakeEventId(AUDIO_KIND_DIALOGUE, 1, 2);
	const unsigned int swapped = AudioContract_MakeEventId(AUDIO_KIND_SFX, 2, 1);
	CHECK(sfx != music && sfx != dialogue && sfx != swapped);
	CHECK(music != dialogue && music != swapped && dialogue != swapped);
	CHECK(AudioContract_MakeEventId(AUDIO_KIND_MUSIC, 1, 2) == music);
}

int main()
{
	TestEventIdPacking();
	TestKindAndResultNames();
	TestPlayFlags();
	TestRequestValidation();
	TestNonFiniteRejection();
	TestInvalidKindNibble();
	TestDiagnosticBufferGuards();
	TestVoiceTable();
	TestGenerationWraparound();
	TestResetEraCollision();
	TestLoopValidation();
	TestListenerValidation();
	TestDeterminism();

	printf("%d checks, %d failures\n", g_checks, g_failures);
	return g_failures == 0 ? 0 : 1;
}
