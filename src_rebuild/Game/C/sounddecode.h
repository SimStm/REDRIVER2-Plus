#ifndef SOUND_DECODE_H
#define SOUND_DECODE_H

// E04 pure PSX SPU ADPCM (VAG) decoder.
//
// Decodes one bounded sample region from a sound-bank blob into PCM16 mono plus
// the loop metadata the audio service needs. It is deliberately SDK-free: no
// PsyCross, GPU, FMOD or game-state include and no mutable global state, so a
// translation unit can unit-test it without an audio device. The per-sample
// recursion uses the same predictor coefficients and the same running predictor
// across blocks as the project's own PSX ADPCM decode
// (PsyX_SPUAL.cpp:398-431), so PCM stays consistent with the existing
// emulation path.
//
// Loop mapping is a deliberate divergence from that reference. This module
// reports the loop start at the first frame of the block carrying the SPU
// LoopStart flag (loopStartBlock * 28, the PSX block-start convention) and the
// inclusive last frame of the LoopEnd block. The fork reference instead
// computes loopStrt = k + 26 = (block + 1) * 28 and is explicitly marked FIXME
// in PsyX_SPUAL.cpp:486-497. Migrating a looping sample through this module
// therefore moves its loop start one block later than the current OpenAL/SPU
// path; that change needs an explicit acceptance decision when environment or
// other looping samples migrate. It does not affect the non-looping FESound
// slice.
//
// `address`/`length` are the byte offset/length of the sample inside `blob`
// (the SAMPLE_DATA fields). `length` must be a positive multiple of the 16-byte
// SPU block, every decoded frame must fit the caller's `pcmCapacity`, and a
// malformed region produces a named negative error with zero partial output.

#define SOUND_DECODE_ERROR_CAPACITY		192

#define SOUND_DECODE_BLOCK_BYTES			16
#define SOUND_DECODE_FRAMES_PER_BLOCK	28

// Named results. SOUND_DECODE_OK is 0; every other value is a negative named
// failure with a diagnostic in `error` when a buffer is supplied.
enum
{
	SOUND_DECODE_OK = 0,
	SOUND_DECODE_ERROR_NULL = -1,			// NULL blob/sample/output/info
	SOUND_DECODE_ERROR_ARGUMENT = -2,		// negative blob size or non-positive sample rate
	SOUND_DECODE_ERROR_SHORT = -3,			// length below one 16-byte block
	SOUND_DECODE_ERROR_TRUNCATED = -4,		// length is not a multiple of 16 bytes
	SOUND_DECODE_ERROR_RANGE = -5,			// address/length outside the blob
	SOUND_DECODE_ERROR_OVERSIZED = -6,		// decoded frames exceed pcmCapacity
	SOUND_DECODE_ERROR_BLOCK_FORMAT = -7,	// filter nibble outside 0..4
	SOUND_DECODE_ERROR_LOOP_SHAPE = -8,		// unsupported or inconsistent loop flags
};

// The subset of SAMPLE_DATA the decoder consumes. `loop` is the record's loop
// boolean; the actual loop endpoints come from the block flags.
typedef struct
{
	unsigned int address;		// byte offset inside the blob
	unsigned int length;		// sample byte length (multiple of 16)
	unsigned int loop;			// 1 when the record loops
	unsigned int samplerate;	// source sample rate in Hz
} SoundDecodeSample;

// Decoded metadata. `loopEnd` is the last looping frame (inclusive) and is -1
// when the sample does not loop.
typedef struct
{
	int frameCount;
	int sampleRate;
	int loop;
	int loopStart;
	int loopEnd;
} SoundDecodeInfo;

#ifdef __cplusplus
extern "C" {
#endif

// Decodes `sample` out of `blob` into `pcm16` (mono, at least
// (length / 16) * 28 frames). On success writes `info` and returns
// SOUND_DECODE_OK. On failure writes no PCM and no metadata and returns a
// negative SOUND_DECODE_ERROR_* with a diagnostic in `error`.
int SoundDecode_SpuAdpcm(const unsigned char* blob, int blobSize,
	const SoundDecodeSample* sample, short* pcm16, int pcmCapacity,
	SoundDecodeInfo* info, char* error, int errorCapacity);

#ifdef __cplusplus
}
#endif

#endif
