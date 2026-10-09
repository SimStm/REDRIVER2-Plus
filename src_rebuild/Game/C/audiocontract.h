#ifndef AUDIO_CONTRACT_H
#define AUDIO_CONTRACT_H

// E04 audio contract (pure data module).
//
// The audio contract is the boundary between gameplay code and the future
// project-owned FMOD Core audio service. Gameplay, missions, replays and other
// systems publish semantic playback requests through this contract and receive
// short-lived voice handles; only the audio service translates them into SDK
// objects, channels and mixers. No game system calls FMOD, retains SDK pointers
// or observes the audio device, so this module compiles and tests without the
// SDK (no PsyCross, GPU or game state).
//
// Units:
// - volume is a linear gain in [0, 1]; pitch is a multiplier where 1.0 is the
//   converted sample's reference pitch (see SoundAsset.pitchReferenceHz), must
//   be finite, positive and no larger than AUDIO_PITCH_MAX.
// - position/velocity are raw game simulation units (the same space the game's
//   world coordinates use), not metres; the service maps them to its listener
//   space. Velocity feeds Doppler and may be left zero. Every component must be
//   finite and within +/-AUDIO_COORDINATE_MAX, so a non-finite value is a
//   caller error rather than an SDK surprise.
// - minDistance/maxDistance are attenuation bounds in the same raw units; they
//   are required for spatialized requests and otherwise ignored. Each must be
//   finite and in (0, AUDIO_DISTANCE_MAX].
//
// Event ids are canonical 32-bit values, stable across runs and independent of
// pointers or load order:
//
//   bits 31..28  kind        (1..3, see AudioKind; zero is the invalid id)
//   bits 27..16  primary     (0..AUDIO_EVENT_PRIMARY_MAX)
//   bits 15..0   secondary   (0..AUDIO_EVENT_SECONDARY_MAX)
//
// The kind-specific meaning of primary/secondary is fixed by the conversion
// metadata: SFX uses bank/voice, MUSIC uses bank/sequence (or program) and
// DIALOGUE uses dialogue set/line. Distinct triples always map to distinct ids
// because the packed fields do not overlap, and AudioContract_MakeEventId
// returns the reserved id 0 for every invalid input.
//
// Loop metadata follows the converted sample record (SoundAsset loopStart and
// loopEnd, in frames). The loop end is inclusive: loopEnd is the last frame
// played before wrapping back to loopStart. A valid loop requires a positive
// sample length, loopStart >= 0, loopEnd < sampleFrames and loopEnd >=
// loopStart, so a one-frame loop (loopEnd == loopStart) is contract-valid.
// FMOD 2.03 loop endpoints are also inclusive. Its service adapter rejects
// equal endpoints explicitly; pure metadata remains tolerant of one-frame
// loops, but they must not be silently lengthened when passed to the SDK.
//
// Voice handles carry a slot index and a 16-bit generation. The generation is
// incremented on every successful acquire and release, so a handle is only valid
// while its slot is acquired at the same generation: releasing a voice stales
// its handle immediately, and a stale handle is rejected instead of touching a
// slot that now belongs to another sound. Handles are short-lived by design; the
// only way a stale handle can become valid again within one era is the 16-bit
// generation wrapping around completely (65536 increments). Callers must not
// retain released handles indefinitely: finite generations can wrap.
//
// Handles are era-scoped. AudioVoiceTable_Reset restarts every slot generation
// at 0, so the first acquire after a Reset systematically returns generation 1
// for the first free slot: a live handle from before the Reset is resurrected
// by that acquire. The caller must discard every held handle when it resets the
// table; within one era the wraparound above is the only revival path, while
// across eras Reset plus reacquire can collide systematically. The table is
// single-thread confined: only the simulation/audio-service thread may call
// these functions, never an FMOD callback or mixer thread.

#define AUDIO_CONTRACT_ERROR_CAPACITY	192
#define AUDIO_VOICE_MAX					64

// Validation bounds (see the module comment for units).
#define AUDIO_PITCH_MAX			8.0f	// upper pitch multiplier bound
#define AUDIO_DISTANCE_MAX		1.0e7f	// largest raw-unit min/max distance
#define AUDIO_COORDINATE_MAX	1.0e8f	// largest magnitude of a raw-unit coordinate

// Audio kind. 0 is the invalid kind, so no valid event id has a zero kind.
enum
{
	AUDIO_KIND_INVALID = 0,
	AUDIO_KIND_SFX = 1,			// SPU bank voice / sample effect
	AUDIO_KIND_MUSIC = 2,		// CD-XA / sequenced music
	AUDIO_KIND_DIALOGUE = 3,	// spoken dialogue
};

#define AUDIO_EVENT_KIND_SHIFT		28
#define AUDIO_EVENT_PRIMARY_SHIFT	16
#define AUDIO_EVENT_SECONDARY_SHIFT	0
#define AUDIO_EVENT_PRIMARY_MAX		0x0FFF
#define AUDIO_EVENT_SECONDARY_MAX	0xFFFF

// Playback flags. The service interprets them; this module only validates the
// bitmask.
// - LOOP: loop between the converted sample's loop points (the sample must
//   carry valid loop metadata).
// - 3D: spatialize the voice from position/velocity and the current listener;
//   minDistance/maxDistance must be positive.
// - PAUSED: create the voice paused; it produces no output until resumed.
// - RESTART: reserved for explicit voice/emitter-scoped restart. An event id
//   identifies an asset, not an emitter; the current service rejects this flag
//   until an explicit restart identity is supplied. Stop(handle) + Play works.
enum
{
	AUDIO_PLAY_LOOP = 1 << 0,
	AUDIO_PLAY_3D = 1 << 1,
	AUDIO_PLAY_PAUSED = 1 << 2,
	AUDIO_PLAY_RESTART = 1 << 3,
};

#define AUDIO_PLAY_FLAG_MASK	(AUDIO_PLAY_LOOP | AUDIO_PLAY_3D | AUDIO_PLAY_PAUSED | AUDIO_PLAY_RESTART)

// Named validation results. AUDIO_CONTRACT_OK is 0; every other value names the
// first invalid field found, with a human-readable diagnostic in `error`.
enum
{
	AUDIO_CONTRACT_OK = 0,
	AUDIO_CONTRACT_ERROR_NULL_REQUEST,
	AUDIO_CONTRACT_ERROR_EVENT_ID,
	AUDIO_CONTRACT_ERROR_KIND,
	AUDIO_CONTRACT_ERROR_KIND_MISMATCH,
	AUDIO_CONTRACT_ERROR_FLAGS,
	AUDIO_CONTRACT_ERROR_VOLUME,
	AUDIO_CONTRACT_ERROR_PITCH,
	AUDIO_CONTRACT_ERROR_POSITION,
	AUDIO_CONTRACT_ERROR_DISTANCE,
	AUDIO_CONTRACT_ERROR_LISTENER,
	AUDIO_CONTRACT_ERROR_LISTENER_ORIENTATION,
	AUDIO_CONTRACT_ERROR_LOOP_FRAMES,
	AUDIO_CONTRACT_ERROR_LOOP_START,
	AUDIO_CONTRACT_ERROR_LOOP_END,
	AUDIO_CONTRACT_ERROR_LOOP_ORDER,
};

// A semantic playback request. `kind` is convenience metadata for the service
// and logs; validation requires it to match the kind packed into `eventId`.
typedef struct
{
	unsigned int eventId;
	int kind;
	unsigned int flags;		// AUDIO_PLAY_* bitmask, 0 when none
	float volume;			// linear gain, 0..1
	float pitch;			// multiplier, 1.0 = reference; finite, in (0, AUDIO_PITCH_MAX]
	float position[3];		// raw game units, each within +/-AUDIO_COORDINATE_MAX
	float velocity[3];		// raw game units per second (Doppler), same bound
	float minDistance;		// raw game units; finite, in (0, AUDIO_DISTANCE_MAX] when 3D
	float maxDistance;		// raw game units; finite, >= minDistance when 3D
} AudioPlayRequest;

// The audio listener is the service-owned mapping target for the game camera:
// the game publishes the camera/listener pose, the service normalizes forward
// and up to unit vectors before handing them to FMOD. Both orientations must be
// non-zero and linearly independent (velocity may stay zero). The service
// orthogonalizes up against forward. The listener is single-thread confined: it
// is written on the simulation/audio-service thread and read by the service,
// never by an FMOD callback or mixer thread.
typedef struct
{
	float position[3];
	float velocity[3];
	float forward[3];
	float up[3];
} AudioListener;

// A voice slot plus the generation it was acquired at. A zeroed handle
// { 0, 0 } is invalid: acquired generations are never 0.
typedef struct
{
	unsigned short index;
	unsigned short generation;
} AudioVoiceHandle;

// Fixed voice table owned by the audio service. Voice stealing and sample
// budgets are service concerns; this table only tracks slot ownership. It is
// single-thread confined: only the simulation/audio-service thread may call
// these functions, never an FMOD callback or mixer thread. Acquire uses a
// first-fit slot scan, which is an implementation detail the service must not
// rely on - key FMOD channels by handle, never by slot index.
typedef struct
{
	unsigned short generation[AUDIO_VOICE_MAX];
	unsigned char inUse[AUDIO_VOICE_MAX];
} AudioVoiceTable;

#ifdef __cplusplus
extern "C" {
#endif

// Packs a stable event id. Returns 0 (the reserved invalid id) when the kind is
// not an AudioKind or primary/secondary are outside their documented ranges.
unsigned int AudioContract_MakeEventId(int kind, int primary, int secondary);

// Unpacks an event id. EventKind returns AUDIO_KIND_INVALID for the reserved id
// 0 and for ids whose kind field is not an AudioKind. EventPrimary/EventSecondary
// return -1 for the reserved id 0 and the packed field otherwise.
int AudioContract_EventKind(unsigned int eventId);
int AudioContract_EventPrimary(unsigned int eventId);
int AudioContract_EventSecondary(unsigned int eventId);

// Stable names for logs and tests: "invalid", "sfx", "music", "dialogue", or
// "unknown" for values that are not an AudioKind.
const char* AudioContract_KindName(int kind);

// Stable name for an AUDIO_CONTRACT_* result ("ok", "volume", ... ; "unknown"
// for values that are not a validation result).
const char* AudioContract_ResultName(int result);

// Fills a request with explicit defaults: no event, volume 1.0, pitch 1.0, all
// flags clear. A spatialized request must also set minDistance/maxDistance and
// (optionally) position/velocity before validation.
void AudioContract_InitPlayRequest(AudioPlayRequest* request);

// Checks a request. Returns AUDIO_CONTRACT_OK (0) when it is playable, or the
// first named AUDIO_CONTRACT_ERROR_* with a diagnostic in `error` (which may be
// NULL/0 when the caller does not need the text). Checks the event id, kind
// agreement, flag bits, volume, pitch, always the position/velocity bounds and,
// for spatialized requests, the distance bounds.
int AudioContract_ValidateRequest(const AudioPlayRequest* request, char* error, int errorCapacity);

// Checks converted loop metadata (frames, inclusive end; see the module
// comment). A one-frame loop (loopEnd == loopStart) is contract-valid; the
// service rejects equal SDK endpoints rather than silently changing the loop.
// Returns AUDIO_CONTRACT_OK (0) or AUDIO_CONTRACT_ERROR_LOOP_* with a diagnostic
// in `error` (which may be NULL/0).
int AudioContract_ValidateLoop(int sampleFrames, int loopStart, int loopEnd, char* error, int errorCapacity);

// Checks a listener pose. Position/velocity/forward/up components must be finite
// and within +/-AUDIO_COORDINATE_MAX; forward and up must additionally be
// non-zero and linearly independent (the service orthonormalizes them).
// Returns AUDIO_CONTRACT_OK (0) or
// AUDIO_CONTRACT_ERROR_LISTENER / _LISTENER_ORIENTATION with a diagnostic in
// `error` (which may be NULL/0).
int AudioContract_ValidateListener(const AudioListener* listener, char* error, int errorCapacity);

// Resets the table to all slots free and every generation to 0. Handles are
// era-scoped: the caller must discard every held handle here, because the first
// post-Reset acquire of a slot returns generation 1 and can systematically
// compare equal to a pre-Reset handle (see the module comment).
void AudioVoiceTable_Reset(AudioVoiceTable* table);

// Acquires a free slot, writing its handle to `out`. Returns 0 on success or -1
// when the table is NULL/full; on failure `out` is set to the invalid zero
// handle when it is not NULL.
int AudioVoiceTable_Acquire(AudioVoiceTable* table, AudioVoiceHandle* out);

// Releases a valid handle. Returns 0 on success or -1 for a stale or invalid
// handle (already released, wrong generation, out-of-range index). The slot's
// generation is incremented so the old handle stays stale.
int AudioVoiceTable_Release(AudioVoiceTable* table, AudioVoiceHandle handle);

// Returns 1 while the handle is the live owner of its slot, else 0.
int AudioVoiceTable_IsValid(const AudioVoiceTable* table, AudioVoiceHandle handle);

// Number of acquired slots; 0 for a NULL table.
int AudioVoiceTable_Count(const AudioVoiceTable* table);

#ifdef __cplusplus
}
#endif

#endif
