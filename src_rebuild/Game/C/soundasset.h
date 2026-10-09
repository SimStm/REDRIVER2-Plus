#ifndef SOUND_ASSET_H
#define SOUND_ASSET_H

// E02 sound asset record (pure data module).
//
// A converted sound is more than an anonymous WAV: it carries the playback
// metadata the original engine needs - sample rate, channel count, bit depth,
// frame count, loop points, a pitch reference, the bank/voice ids it came from
// and, for music, the bank/program/sequence ids. The record is validated and
// serialized canonically so a package can reference it and two identical
// conversions produce equal output. It never touches PsyCross, the GPU or game
// state.

#define SOUND_ASSET_ID_CAPACITY			96
#define SOUND_ASSET_ERROR_CAPACITY		160
#define SOUND_ASSET_SERIALIZED_CAPACITY	512

typedef struct
{
	char id[SOUND_ASSET_ID_CAPACITY];

	int bankId;			// source bank id (-1 when not from a bank)
	int voiceId;		// source voice/tone id (-1 when not from a bank)

	int sampleRate;		// Hz
	int channels;		// 1 (mono) or 2 (stereo)
	int bitDepth;		// 8 or 16
	int frameCount;		// frames (samples per channel)

	int loop;			// 1 when the sound loops
	int loopStart;		// first looping frame
	int loopEnd;		// last looping frame (-1 when not looping)

	int pitchReferenceHz;	// reference pitch in Hz (e.g. the base sample rate)

	// Music metadata (CD-XA / sequenced music). -1 when the sound is not music.
	int musicBank;
	int musicProgram;
	int musicSequence;
} SoundAsset;

#ifdef __cplusplus
extern "C" {
#endif

// Fills a record with explicit, non-music defaults.
void SoundAsset_Init(SoundAsset* asset);

// Sets the stable id. Returns 1 on success, 0 when empty or too long.
int SoundAsset_SetId(SoundAsset* asset, const char* id);

// Sets the loop range (both frames must be valid when validated).
void SoundAsset_SetLoop(SoundAsset* asset, int startFrame, int endFrame);

// Checks the record. Returns 1 when valid, or 0 with a diagnostic in `error`.
int SoundAsset_Validate(const SoundAsset* asset, char* error, int errorCapacity);

// Writes a canonical JSON record. Returns the byte count, or -1 when the buffer
// is too small.
int SoundAsset_Serialize(const SoundAsset* asset, char* out, int capacity);

#ifdef __cplusplus
}
#endif

#endif
