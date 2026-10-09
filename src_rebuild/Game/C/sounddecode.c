#include "sounddecode.h"

#include <math.h>

// PSX SPU ADPCM coefficients, matching the project's own decoder
// (PsyCross/src/audio/PsyX_SPUAL.cpp). The same coefficients and running
// predictor are used per sample; only the loop mapping deliberately diverges
// (see sounddecode.h). This module includes no PsyCross header so it builds and
// tests without the SDK.
static const float kFilter0[5] = { 0.0f, 0.9375f, 1.796875f, 1.53125f, 1.90625f };
static const float kFilter1[5] = { 0.0f, 0.0f, -0.8125f, -0.859375f, -0.9375f };

static void SetError(char* error, int capacity, const char* text)
{
	int i;

	if (!error || capacity <= 0)
		return;

	for (i = 0; text[i] && i < capacity - 1; ++i)
		error[i] = text[i];
	error[i] = '\0';
}

// Decodes one 4-bit sample with the running predictor. The predictor carries
// across blocks exactly as the SPU and PsyX_SPUAL.cpp do.
static short DecodeNibble(int nibble, int filter, int shift, float* prev1, float* prev2)
{
	if (nibble > 7)
		nibble -= 16;

	const float data = (float)nibble * (float)pow(2.0, (double)(12 - shift));
	const float result = data + (*prev1) * kFilter0[filter] + (*prev2) * kFilter1[filter];

	*prev2 = *prev1;
	*prev1 = result;

	int value = (int)round(result);
	if (value > 32767)
		value = 32767;
	if (value < -32768)
		value = -32768;
	return (short)value;
}

int SoundDecode_SpuAdpcm(const unsigned char* blob, int blobSize,
	const SoundDecodeSample* sample, short* pcm16, int pcmCapacity,
	SoundDecodeInfo* info, char* error, int errorCapacity)
{
	if (!blob || !sample || !pcm16 || !info)
	{
		SetError(error, errorCapacity, "the decoder needs a blob, sample, output and info");
		return SOUND_DECODE_ERROR_NULL;
	}
	if (blobSize < 0 || (int)sample->samplerate <= 0)
	{
		SetError(error, errorCapacity, "the blob size must be >= 0 and the sample rate must be positive");
		return SOUND_DECODE_ERROR_ARGUMENT;
	}
	if (sample->length < SOUND_DECODE_BLOCK_BYTES)
	{
		SetError(error, errorCapacity, "the sample is shorter than one 16-byte SPU block");
		return SOUND_DECODE_ERROR_SHORT;
	}
	if (sample->length % SOUND_DECODE_BLOCK_BYTES != 0)
	{
		SetError(error, errorCapacity, "the sample length is not a multiple of 16 bytes");
		return SOUND_DECODE_ERROR_TRUNCATED;
	}
	if ((long long)sample->address + (long long)sample->length > (long long)blobSize)
	{
		SetError(error, errorCapacity, "the sample address/length is outside the blob");
		return SOUND_DECODE_ERROR_RANGE;
	}

	// Frame/capacity arithmetic is done in 64 bits: a malformed length can make
	// blockCount * 28 overflow a 32-bit int even though the address/length check
	// above already bounds it against the blob.
	const long long blockCount64 = (long long)(sample->length / SOUND_DECODE_BLOCK_BYTES);
	const long long frameCount64 = blockCount64 * SOUND_DECODE_FRAMES_PER_BLOCK;
	if (frameCount64 > (long long)pcmCapacity)
	{
		SetError(error, errorCapacity, "the sample needs more output frames than the buffer holds");
		return SOUND_DECODE_ERROR_OVERSIZED;
	}
	const int blockCount = (int)blockCount64;	// <= blobSize / 16, fits int
	const int frameCount = (int)frameCount64;	// <= pcmCapacity, fits int

	// Pre-scan headers so every failure happens before any PCM is written.
	const unsigned char* region = blob + sample->address;
	int loopStartBlock = -1;
	int loopEndBlock = -1;
	int loopStartCount = 0;
	int loopEndCount = 0;
	for (int block = 0; block < blockCount; ++block)
	{
		const unsigned char header = region[block * SOUND_DECODE_BLOCK_BYTES];
		if (((header >> 4) & 0x0F) > 4)
		{
			SetError(error, errorCapacity, "a block uses an invalid ADPCM filter");
			return SOUND_DECODE_ERROR_BLOCK_FORMAT;
		}
		const unsigned char flags = region[block * SOUND_DECODE_BLOCK_BYTES + 1];
		if (flags & 0x04)
		{
			loopStartBlock = block;
			++loopStartCount;
		}
		if (flags & 0x01)
		{
			loopEndBlock = block;
			++loopEndCount;
		}
	}

	// Loop flags are only meaningful for a looping record. Non-looping samples
	// routinely carry several end markers and must decode regardless.
	int loopStartFrame = 0;
	int loopEndFrame = -1;
	if (sample->loop)
	{
		if (loopStartCount != 1 || loopEndCount != 1)
		{
			SetError(error, errorCapacity, "a looping sample needs exactly one loop-start and one loop-end block flag");
			return SOUND_DECODE_ERROR_LOOP_SHAPE;
		}
		if (loopEndBlock < loopStartBlock)
		{
			SetError(error, errorCapacity, "the loop-end block precedes the loop-start block");
			return SOUND_DECODE_ERROR_LOOP_SHAPE;
		}
		const unsigned char endFlags = region[loopEndBlock * SOUND_DECODE_BLOCK_BYTES + 1];
		if (!(endFlags & 0x02))
		{
			SetError(error, errorCapacity, "the loop-end block does not request a repeat");
			return SOUND_DECODE_ERROR_LOOP_SHAPE;
		}
		// PSX block-start convention: the loop starts at the first frame of the
		// LoopStart block. This deliberately diverges from the fork reference
		// (PsyX_SPUAL.cpp:486, marked FIXME), which uses (block + 1) * 28.
		loopStartFrame = loopStartBlock * SOUND_DECODE_FRAMES_PER_BLOCK;
		// loopEndBlock < blockCount, so loopEndFrame <= (blockCount - 1) * 28 + 27
		// == frameCount - 1; it can never leave the decoded sample.
		loopEndFrame = loopEndBlock * SOUND_DECODE_FRAMES_PER_BLOCK + (SOUND_DECODE_FRAMES_PER_BLOCK - 1);
	}

	// Decode. All validation is done, so this cannot fail partway.
	float prev1 = 0.0f;
	float prev2 = 0.0f;
	int frame = 0;
	for (int block = 0; block < blockCount; ++block)
	{
		const unsigned char header = region[block * SOUND_DECODE_BLOCK_BYTES];
		const int shift = header & 0x0F;
		const int filter = (header >> 4) & 0x0F;
		const unsigned char* nibbles = region + block * SOUND_DECODE_BLOCK_BYTES + 2;
		for (int i = 0; i < SOUND_DECODE_BLOCK_BYTES - 2; ++i)
		{
			pcm16[frame++] = DecodeNibble(nibbles[i] & 0x0F, filter, shift, &prev1, &prev2);
			pcm16[frame++] = DecodeNibble((nibbles[i] >> 4) & 0x0F, filter, shift, &prev1, &prev2);
		}
	}

	info->frameCount = frameCount;
	info->sampleRate = (int)sample->samplerate;
	info->loop = sample->loop ? 1 : 0;
	info->loopStart = loopStartFrame;
	info->loopEnd = loopEndFrame;
	return SOUND_DECODE_OK;
}
